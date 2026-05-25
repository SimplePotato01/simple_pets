#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#define BUFFER_SIZE 1024
#define HEARTBEAT_INTERVAL 2
#define TIMEOUT 5
#define MAX_PEERS 64

// FIXME make more stable shutting down 
// cause i forgot to use O_NONBLOCK and so recv is blocking the thread even in ctrl+c
//

typedef struct {
	char ip[INET6_ADDRSTRLEN];
	time_t last_seen;
	int is_ipv6;
} Peer;

Peer peers[MAX_PEERS];
int peer_count = 0;
volatile int running = 1;
int sock = -1;
pthread_t recv_thread, send_thread, clean_thread;

void print_peers() {
    	printf("\nLiving copies:\n");
    	for (int i = 0; i < peer_count; i++) {
        	printf("  %s\n", peers[i].ip);
    	}
    	printf("Total: %d\n\n", peer_count);
}

void add_or_update_peer(const char* ip, int is_ipv6) {
    	time_t now = time(NULL);
    
    	for (int i = 0; i < peer_count; i++) {
        	if (strcmp(peers[i].ip, ip) == 0) {
            		peers[i].last_seen = now;
            		return;
        	}
    	}
    
    	if (peer_count < MAX_PEERS) {
        	strcpy(peers[peer_count].ip, ip);
        	peers[peer_count].last_seen = now;
        	peers[peer_count].is_ipv6 = is_ipv6;
        	peer_count++;
        	printf("\n[+] NEW PEER: %s\n", ip);
        	print_peers();
    	}
}

void remove_old_peers() {
    	time_t now = time(NULL);
    	int changed = 0;
    
    	for (int i = 0; i < peer_count; i++) {
        	if (now - peers[i].last_seen > TIMEOUT) {
            		printf("\n[-] PEER LOST: %s\n", peers[i].ip);
            		for (int j = i; j < peer_count - 1; j++) {
                		peers[j] = peers[j + 1];
            		}
            		peer_count--;
            		changed = 1;
            		i--;
        	}
    	}
    
    	if (changed) {
        	print_peers();
    	}
}

int is_own_ip(const char* ip) {
    	struct ifaddrs *ifaces, *ifa;
    	int result = 0;
    
    	if (getifaddrs(&ifaces) == 0) {
        	for (ifa = ifaces; ifa != NULL; ifa = ifa->ifa_next) {
            		if (ifa->ifa_addr == NULL) continue;
            
            		char self_ip[INET6_ADDRSTRLEN];
            		if (ifa->ifa_addr->sa_family == AF_INET) {
                		struct sockaddr_in* s = (struct sockaddr_in*)ifa->ifa_addr;
                		inet_ntop(AF_INET, &(s->sin_addr), self_ip, sizeof(self_ip));
                		if (strcmp(self_ip, ip) == 0) {
                    			result = 1;
                    			break;
                		}
            		} else if (ifa->ifa_addr->sa_family == AF_INET6) {
                		struct sockaddr_in6* s = (struct sockaddr_in6*)ifa->ifa_addr;
                		if (IN6_IS_ADDR_LOOPBACK(&s->sin6_addr)) continue;
                		inet_ntop(AF_INET6, &(s->sin6_addr), self_ip, sizeof(self_ip));
                		if (strcmp(self_ip, ip) == 0) {
                    			result = 1;
                    			break;
                		}
        		}
        	}
        	freeifaddrs(ifaces);
    	}
    
    	if (strcmp(ip, "127.0.0.1") == 0 || strcmp(ip, "::1") == 0) {
        	result = 1;
    	}
    
    	return result;
}

void* receiver_thread(void* arg) {
    	char buffer[BUFFER_SIZE];
    	struct sockaddr_storage sender_addr;
    	socklen_t addr_len;
    	char ip_str[INET6_ADDRSTRLEN];
    
    	printf("Receiver thread started!\n");
    
    	while (running) {
        	memset(buffer, 0, BUFFER_SIZE);
        	addr_len = sizeof(sender_addr);
        
        	ssize_t bytes = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&sender_addr, &addr_len);
        
        	if (bytes > 0) {
            		buffer[bytes] = '\0';
            
            		void* addr;
            		int is_ipv6 = 0;
            
            		if (sender_addr.ss_family == AF_INET) {
                		struct sockaddr_in* s = (struct sockaddr_in*)&sender_addr;
                		addr = &(s->sin_addr);
                		is_ipv6 = 0;
            		} else {
                		struct sockaddr_in6* s = (struct sockaddr_in6*)&sender_addr;
                		addr = &(s->sin6_addr);
                		is_ipv6 = 1;
            		}
            
            		inet_ntop(sender_addr.ss_family, addr, ip_str, sizeof(ip_str));
            
            		printf("Received from %s: %s\n", ip_str, buffer);
            
            		if (!is_own_ip(ip_str)) {
                		add_or_update_peer(ip_str, is_ipv6);
            		} else {
                		printf("Ignoring own message from %s\n", ip_str);
            		}
        	} else if (bytes < 0 && running) {
            		perror("recvfrom error");
        	}
    	}
    
    	printf("Receiver thread stopping\n");
    	return NULL;
}

void* sender_thread(void* arg) {
    	const char* group = (const char*)arg;
    	struct sockaddr_storage group_addr;
    	socklen_t addr_len;
    	int is_ipv6;
    	char message[BUFFER_SIZE];
    	char hostname[256];
    
    	printf("Sender thread started!\n");
    
    	struct addrinfo hints, *res;
    	memset(&hints, 0, sizeof(hints));
    	hints.ai_family = AF_UNSPEC;
    	hints.ai_socktype = SOCK_DGRAM;
    
    	if (getaddrinfo(group, NULL, &hints, &res) != 0) {
        	fprintf(stderr, "Invalid multicast group address\n");
        	return NULL;
    	}
    
    	is_ipv6 = (res->ai_family == AF_INET6);
    
    	if (is_ipv6) {
        	struct sockaddr_in6* addr = (struct sockaddr_in6*)&group_addr;
        	memset(addr, 0, sizeof(struct sockaddr_in6));
        	addr->sin6_family = AF_INET6;
        	addr->sin6_port = htons(9999);
        	memcpy(&addr->sin6_addr, &((struct sockaddr_in6*)res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
        	addr_len = sizeof(struct sockaddr_in6);
    	} else {
        	struct sockaddr_in* addr = (struct sockaddr_in*)&group_addr;
        	memset(addr, 0, sizeof(struct sockaddr_in));
        	addr->sin_family = AF_INET;
        	addr->sin_port = htons(9999);
        	memcpy(&addr->sin_addr, &((struct sockaddr_in*)res->ai_addr)->sin_addr, sizeof(struct in_addr));
        	addr_len = sizeof(struct sockaddr_in);
    	}
    
    	freeaddrinfo(res);
    
    	gethostname(hostname, sizeof(hostname));
    
    	while (running) {
        	snprintf(message, BUFFER_SIZE, "HELLO from %s with pid: %d", hostname, getpid());
        
        	printf("Sending: %s to group %s\n", message, group);
        
        	ssize_t sent = sendto(sock, message, strlen(message), 0, (struct sockaddr*)&group_addr, addr_len);
        	if (sent < 0) {
            		perror("sendto error");
        	}
        
        	sleep(HEARTBEAT_INTERVAL);
    	}
    
    	printf("Sender thread stopping\n");
    	return NULL;
}

void* cleanup_thread(void* arg) {
    	printf("Cleanup thread started\n");
    	while (running) {
        	sleep(1);
        	remove_old_peers();
    	}
    	return NULL;
}

void signal_handler(int sig) {
    	if (sig == SIGINT || sig == SIGTERM) {
        	printf("\n\nSignal received, shutting down...\n");
        	running = 0;
        	if (sock != -1) {
            		shutdown(sock, SHUT_RDWR);
			close(sock);
            		sock = -1;
        	}
    	}
}

int main(int argc, char* argv[]) {
    	if (argc != 2) {
        	fprintf(stderr, "Usage: %s <multicast_group>\n", argv[0]);
        	fprintf(stderr, "\nFor local testing use: %s 224.0.0.1\n", argv[0]);
        	return 1;
    	}
    
    	const char* multicast_group = argv[1];
    	int is_ipv6 = 0;
    	int reuse = 1;
    	struct sockaddr_storage local_addr;
    
    	printf("Starting multicast peer discovery\n");
    	printf("Group: %s\n", multicast_group);
    
    	struct addrinfo hints, *res;
    	memset(&hints, 0, sizeof(hints));
    	hints.ai_family = AF_UNSPEC;
    	hints.ai_socktype = SOCK_DGRAM;
    
    	if (getaddrinfo(multicast_group, NULL, &hints, &res) != 0) {
        	fprintf(stderr, "Error: Invalid multicast group address\n");
        	return 1;
    	}
    
    	is_ipv6 = (res->ai_family == AF_INET6);
    	printf("Protocol: %s\n", is_ipv6 ? "IPv6" : "IPv4");
    
    	sock = socket(res->ai_family, SOCK_DGRAM, 0);
    	if (sock < 0) {
        	perror("socket");
        	freeaddrinfo(res);
        	return 1;
    	}
    
    	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        	perror("setsockopt SO_REUSEADDR");
    	}
    
    	memset(&local_addr, 0, sizeof(local_addr));
    
    	if (is_ipv6) {
        	struct sockaddr_in6* addr = (struct sockaddr_in6*)&local_addr;
        	addr->sin6_family = AF_INET6;
        	addr->sin6_port = htons(9999);
        	addr->sin6_addr = in6addr_any;
        
        	if (bind(sock, (struct sockaddr*)addr, sizeof(struct sockaddr_in6)) < 0) {
            		perror("bind");
            		close(sock);
            		freeaddrinfo(res);
            		return 1;
        	}
        
        	struct ipv6_mreq mreq6;
        	memcpy(&mreq6.ipv6mr_multiaddr, &((struct sockaddr_in6*)res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
        	mreq6.ipv6mr_interface = 0;
        
        	if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0) {
            		perror("setsockopt IPV6_ADD_MEMBERSHIP");
            		close(sock);
            		freeaddrinfo(res);
            		return 1;
        	}
        
        	int loop = 1;
        	setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));
        
    	} else {
        	struct sockaddr_in* addr = (struct sockaddr_in*)&local_addr;
        	addr->sin_family = AF_INET;
        	addr->sin_port = htons(9999);
        	addr->sin_addr.s_addr = INADDR_ANY;
        
        	if (bind(sock, (struct sockaddr*)addr, sizeof(struct sockaddr_in)) < 0) {
            		perror("bind");
            		close(sock);
            		freeaddrinfo(res);
            		return 1;
        	}
        
        	struct ip_mreq mreq4;
        	memcpy(&mreq4.imr_multiaddr, &((struct sockaddr_in*)res->ai_addr)->sin_addr, sizeof(struct in_addr));
        	mreq4.imr_interface.s_addr = INADDR_ANY;
        
        	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq4, sizeof(mreq4)) < 0) {
            		perror("setsockopt IP_ADD_MEMBERSHIP");
            		printf("Check if multicast is supported on your system\n");
            		close(sock);
            		freeaddrinfo(res);
            		return 1;
        	}
        
        	unsigned char loop = 1;
        	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
        
        	unsigned char ttl = 1;
        	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    	}
    
    	freeaddrinfo(res);
    
    	struct sigaction sa;
    	memset(&sa, 0, sizeof(sa));
    	sa.sa_handler = signal_handler;
    	sigaction(SIGINT, &sa, NULL);
    	sigaction(SIGTERM, &sa, NULL);
    
    	printf("\n[INFO] Listening on %s:9999\n", multicast_group);
    	printf("[INFO] Starting threads...\n\n");
    
    	pthread_create(&recv_thread, NULL, receiver_thread, NULL);
    	pthread_create(&send_thread, NULL, sender_thread, (void*)multicast_group);
    	pthread_create(&clean_thread, NULL, cleanup_thread, NULL);
    
    	pthread_join(recv_thread, NULL);
    	pthread_join(send_thread, NULL);
    	pthread_join(clean_thread, NULL);
    
    	printf("\n[INFO] Program terminated\n");
    	return 0;
}
