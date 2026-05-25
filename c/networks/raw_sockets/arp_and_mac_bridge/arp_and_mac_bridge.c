#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <netpacket/packet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define MAX_PORTS 8
#define MAC_TABLE_SIZE 256
#define ARP_TABLE_SIZE 128
#define BUFFER_SIZE 65536
#define MAC_AGING_TIME 300
#define ARP_AGING_TIME 600

struct arp_header {
    	uint16_t htype;
    	uint16_t ptype;
    	uint8_t  hlen;
    	uint8_t  plen;
    	uint16_t oper;
    	unsigned char sha[6];
    	uint32_t spa;
    	unsigned char tha[6];
    	uint32_t tpa;
} __attribute__((packed));

// TODO Write without typedef
typedef struct {
    	unsigned char mac[6];
    	int port;
    	time_t last_seen;
    	int valid;
} mac_table_entry_t;

// TODO Write without typedef
typedef struct {
    	uint32_t ip;
    	unsigned char mac[6];
    	time_t last_seen;
    	int valid;
} arp_table_entry_t;

int sockets[MAX_PORTS];
char if_names[MAX_PORTS][IFNAMSIZ];
int num_ports = 0;
mac_table_entry_t mac_table[MAC_TABLE_SIZE];
arp_table_entry_t arp_table[ARP_TABLE_SIZE];

unsigned int mac_hash(const unsigned char *mac) {
    	unsigned int hash = 0;
    	for (int i = 0; i < 6; i++) hash = (hash << 5) + hash + mac[i];
    	return hash % MAC_TABLE_SIZE;
}

unsigned int arp_hash(uint32_t ip) {
    	return ip % ARP_TABLE_SIZE;
}

void add_mac_entry(const unsigned char *mac, int port) {
    	unsigned int hash = mac_hash(mac);
    
    	for (int i = 0; i < MAC_TABLE_SIZE; i++) {
        	int idx = (hash + i) % MAC_TABLE_SIZE;
        	if (!mac_table[idx].valid || memcmp(mac_table[idx].mac, mac, 6) == 0) {
            		memcpy(mac_table[idx].mac, mac, 6);
            		mac_table[idx].port = port;
            		mac_table[idx].last_seen = time(NULL);
            		mac_table[idx].valid = 1;
            
            		printf("MAC Learned: %02x:%02x:%02x:%02x:%02x:%02x -> Port %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], port);
            		return;
        	}
    	}
    	printf("MAC table is full!\n");
}

int find_mac_port(const unsigned char *mac) {
    	unsigned int hash = mac_hash(mac);
    
    	for (int i = 0; i < MAC_TABLE_SIZE; i++) {
        	int idx = (hash + i) % MAC_TABLE_SIZE;
        	if (mac_table[idx].valid && memcmp(mac_table[idx].mac, mac, 6) == 0) {
			mac_table[idx].last_seen = time(NULL);
            		return mac_table[idx].port;
        	}
    	}
    	return -1;
}

void add_arp_entry(uint32_t ip, const unsigned char *mac) {
    	unsigned int hash = arp_hash(ip);
    
    	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        	int idx = (hash + i) % ARP_TABLE_SIZE;
        	if (!arp_table[idx].valid || arp_table[idx].ip == ip) {
      			arp_table[idx].ip = ip;
            		memcpy(arp_table[idx].mac, mac, 6);
            		arp_table[idx].last_seen = time(NULL);
            		arp_table[idx].valid = 1;
            
            		struct in_addr addr;
            		addr.s_addr = ip;
            		printf("ARP Learned: %s -> %02x:%02x:%02x:%02x:%02x:%02x\n", inet_ntoa(addr), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            		return;
        	}
    	}
}

void aging_check() {
    	time_t now = time(NULL);
    
    	for (int i = 0; i < MAC_TABLE_SIZE; i++) {
        	if (mac_table[i].valid && (now - mac_table[i].last_seen) > MAC_AGING_TIME) {
            		printf("MAC entry expired and removed\n");
            		mac_table[i].valid = 0;
        	}
    	}
    
    	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        	if (arp_table[i].valid && (now - arp_table[i].last_seen) > ARP_AGING_TIME) {
            		arp_table[i].valid = 0;
        	}
    	}
}

void process_arp(const unsigned char *packet, int packet_len, int incoming_port) {
    	if (packet_len < sizeof(struct ethhdr) + sizeof(struct arp_header)) {
        	return;
    	}
    
    	struct arp_header *arp = (struct arp_header*)(packet + sizeof(struct ethhdr));
    
    	if (ntohs(arp->htype) != 1 || ntohs(arp->ptype) != 0x0800) {
        	return;
    	}
    
    	add_arp_entry(arp->spa, arp->sha);
    
    	printf("ARP %s: ", ntohs(arp->oper) == 1 ? "Request" : "Reply");
    	struct in_addr addr;
    	addr.s_addr = arp->spa;
    	printf("%s -> ", inet_ntoa(addr));
    	addr.s_addr = arp->tpa;
    	printf("%s\n", inet_ntoa(addr));
}

int create_raw_socket(const char *ifname) {
    	int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (sock == -1) {
        	perror("socket:\n");
        	return -1;
    	}
    
    	struct ifreq ifr;
    	memset(&ifr, 0, sizeof(ifr));
    	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    
    	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) == -1) {
        	perror("setsockopt:\n");
        	close(sock);
        	return -1;
    	}
    
    	return sock;
}

void send_packet(int port_idx, const unsigned char *buffer, int len) {
    	struct sockaddr_ll dest_addr;
    	memset(&dest_addr, 0, sizeof(dest_addr));
    	dest_addr.sll_family = AF_PACKET;
    	dest_addr.sll_ifindex = sockets[port_idx];
    	dest_addr.sll_protocol = htons(ETH_P_ALL);
    
    	if (sendto(sockets[port_idx], buffer, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) == -1) {
        	perror("sendto:\n");
    	}
}

void forward_packet(const unsigned char *buffer, int len, int incoming_port) {
    	struct ethhdr *eth = (struct ethhdr*)buffer;
    
    	int is_broadcast = 1;
    	for (int i = 0; i < 6; i++) {
        	if (eth->h_dest[i] != 0xff) {
            		is_broadcast = 0;
            		break;
        	}
    	}
    
    	add_mac_entry(eth->h_source, incoming_port);
    
    	if (ntohs(eth->h_proto) == 0x0806) {
        	process_arp(buffer, len, incoming_port);
    	}
    
    	int dest_port = find_mac_port(eth->h_dest);
    
    	if (is_broadcast || dest_port == -1) {
        	printf("FLOOD: MAC ");
        	for(int i = 0; i < 6; i++) printf("%02x", eth->h_dest[i]);
        	printf("	not found, flooding to all ports except %d\n", incoming_port);
        
        	for (int i = 0; i < num_ports; i++) {
            		if (i != incoming_port) send_packet(i, buffer, len);
        	}
    	} else {
        	printf("FORWARD: MAC found on port %d\n", dest_port);
        	send_packet(dest_port, buffer, len);
    	}
}

void print_tables() {
    	printf("\n=== MAC TABLE ===\n");
    	for (int i = 0; i < MAC_TABLE_SIZE; i++) {
        	if (mac_table[i].valid) {
            		printf("%02x:%02x:%02x:%02x:%02x:%02x -> Port %d\n",
                   		mac_table[i].mac[0],
			       	mac_table[i].mac[1],
                   		mac_table[i].mac[2],
			       	mac_table[i].mac[3],
                   		mac_table[i].mac[4],
			       	mac_table[i].mac[5],
                   		mac_table[i].port);
        	}
    	}
    
    	printf("\n=== ARP TABLE ===\n");
    	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        	if (arp_table[i].valid) {
            		struct in_addr addr;
            		addr.s_addr = arp_table[i].ip;
            		printf("%s -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                   		inet_ntoa(addr),
                   		arp_table[i].mac[0],
			       	arp_table[i].mac[1],
                   		arp_table[i].mac[2],
			       	arp_table[i].mac[3],
                   		arp_table[i].mac[4],
			       	arp_table[i].mac[5]);
        	}
    	}
    	printf("\n");
}

int running = 1;
void sigint_handler(int sig) {
    	running = 0;
}

int main(int argc, char *argv[]) {
    	if (argc < 3) {
        	fprintf(stderr, "Usage: %s <interface1> <interface2> ...\n", argv[0]);
        	exit(EXIT_FAILURE);
    	}
    
    	num_ports = argc - 1;
    	if (num_ports > MAX_PORTS) {
        	fprintf(stderr, "Maximum %d ports\n", MAX_PORTS);
        	exit(EXIT_FAILURE);
    	}
    
    	memset(mac_table, 0, sizeof(mac_table));
    	memset(arp_table, 0, sizeof(arp_table));
    
    	for (int i = 0; i < num_ports; i++) {
        	strcpy(if_names[i], argv[i+1]);
        	sockets[i] = create_raw_socket(if_names[i]);
        	if (sockets[i] == -1) {
            		fprintf(stderr, "Cannot open interface %s\n", if_names[i]);
            		exit(EXIT_FAILURE);
        	}
        	printf("Port %d: %s (socket fd: %d)\n", i, if_names[i], sockets[i]);
    	}
    
    	signal(SIGINT, sigint_handler);
    
    	printf("\nL2 Switch running. Press Ctrl+C to exit.\n");
    	printf("Press 's' to view tables\n\n");
    
    	fd_set read_fds;
    	unsigned char buffer[BUFFER_SIZE];
    	struct timeval tv;
    	time_t last_aging = time(NULL);
    
    	while (running) {
        	FD_ZERO(&read_fds);
        	int max_fd = 0;
        
        	for (int i = 0; i < num_ports; i++) {
            		FD_SET(sockets[i], &read_fds);
            		if (sockets[i] > max_fd) max_fd = sockets[i];
        	}
        
        	FD_SET(STDIN_FILENO, &read_fds);
        	if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        
        	tv.tv_sec = 1;
        	tv.tv_usec = 0;
        
        	int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        	if (activity < 0 && running) {
            		perror("select:\n");
            		break;
        	}
        
        	if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            		char cmd = getchar();
            		if (cmd == 's' || cmd == 'S') print_tables();
        	}
        
        	for (int i = 0; i < num_ports; i++) {
            		if (FD_ISSET(sockets[i], &read_fds)) {
                		struct sockaddr_ll saddr;
                		socklen_t saddr_len = sizeof(saddr);
                		ssize_t len = recvfrom(sockets[i], buffer, BUFFER_SIZE, 0, (struct sockaddr*)&saddr, &saddr_len);
                
                		if (len > 0) forward_packet(buffer, len, i);
            		}
        	}
        
        	time_t now = time(NULL);
        	if (now - last_aging >= 30) {
            		aging_check();
            		last_aging = now;
        	}
    	}
    
    	for (int i = 0; i < num_ports; i++) close(sockets[i]);
    
    	printf("\nL2 Switch stopped\n");
    	return 0;
}
