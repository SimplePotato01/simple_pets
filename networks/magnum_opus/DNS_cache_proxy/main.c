#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>

#define DNS_PORT 8080
#define UPSTREAM_DNS "8.8.8.8"
#define UPSTREAM_PORT 53
#define CACHE_SIZE 1024
#define BUFFER_SIZE 512

typedef struct {
    	unsigned short id;
    	unsigned short flags;
    	unsigned short qdcount;
    	unsigned short ancount;
    	unsigned short nscount;
    	unsigned short arcount;
} dns_header_t;

typedef struct cache_entry {
    	char domain[256];
    	char ip[16];
    	time_t expire_time;
    	struct cache_entry *next;
} cache_entry_t;

cache_entry_t *cache_table[CACHE_SIZE];
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned int hash(const char *str) {
    	unsigned int hash = 5381;
    	int c;
    	while ((c = *str++))
        	hash = ((hash << 5) + hash) + c;
    	return hash % CACHE_SIZE;
}

int parse_dns_name(const unsigned char *buffer, int offset, char *name, int name_len) {
    	int i = 0;
    	int pos = offset;
    
    	while (1) {
        	unsigned char len = buffer[pos++];
        	if (len == 0) break;
        	if (len & 0xC0) {
            		int new_offset = ((len & 0x3F) << 8) | buffer[pos++];
            		parse_dns_name(buffer, new_offset, name + i, name_len - i);
            		return pos - offset;
        	}
        	if (i + len + 1 >= name_len) {
            		return -1;
        	}
        
        	memcpy(name + i, buffer + pos, len);
        	i += len;
        	name[i++] = '.';
        	pos += len;
    	}
    
    	if (i > 0) name[i-1] = '\0';
    	else name[0] = '\0';
    
    	return pos - offset;
}

cache_entry_t* cache_find(const char *domain) {
    	unsigned int h = hash(domain);
    	cache_entry_t *entry = cache_table[h];
    
    	while (entry) {
        	if (strcmp(entry->domain, domain) == 0) {
            		if (time(NULL) < entry->expire_time) {
                		return entry;
            		} else {
                		return NULL;
            		}
        	}
        	entry = entry->next;
    	}
    	return NULL;
}

void cache_add(const char *domain, const char *ip, int ttl) {
    	if (ttl <= 0) ttl = 60;
    
    	unsigned int h = hash(domain);
    	cache_entry_t *entry = malloc(sizeof(cache_entry_t));
    	if (!entry) return;
    
    	strcpy(entry->domain, domain);
    	strcpy(entry->ip, ip);
    	entry->expire_time = time(NULL) + ttl;
    	entry->next = cache_table[h];
    	cache_table[h] = entry;
}

int extract_ip_from_dns(const unsigned char *response, /*int len,*/ char *ip) {
    	dns_header_t *header = (dns_header_t*)response;
    	int offset = 12;
    	char domain[256];
    
    	parse_dns_name(response, offset, domain, sizeof(domain));
    	offset += strlen(domain) + 2 + 4;
    
    	for (int i = 0; i < ntohs(header->ancount); i++) {
        	parse_dns_name(response, offset, domain, sizeof(domain));
        	offset += strlen(domain) + 2;
        
        	unsigned short type = (response[offset] << 8) | response[offset+1];
        	unsigned short class = (response[offset+2] << 8) | response[offset+3];
        	int ttl = (response[offset+4] << 24) | (response[offset+5] << 16) | (response[offset+6] << 8) | response[offset+7];
        	unsigned short rdlength = (response[offset+8] << 8) | response[offset+9];
        	offset += 10;
        
        	if (type == 1 && class == 1 && rdlength == 4) {
            		sprintf(ip, "%d.%d.%d.%d", response[offset], response[offset+1], response[offset+2], response[offset+3]);
            
            		char qdomain[256];
            		parse_dns_name(response, 12, qdomain, sizeof(qdomain));
            		pthread_mutex_lock(&cache_mutex);
            		cache_add(qdomain, ip, ttl);
            		pthread_mutex_unlock(&cache_mutex);
            		return 1;
        	}
        	offset += rdlength;
    	}
    	return 0;
}

int forward_to_upstream(const unsigned char *request, int req_len, unsigned char *response) {
    	int sock = socket(AF_INET, SOCK_DGRAM, 0);
    	if (sock < 0) return -1;
    
    	struct timeval tv;
    	tv.tv_sec = 5;
    	tv.tv_usec = 0;
    	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    	struct sockaddr_in upstream;
    	memset(&upstream, 0, sizeof(upstream));
    	upstream.sin_family = AF_INET;
    	upstream.sin_port = htons(UPSTREAM_PORT);
    	inet_pton(AF_INET, UPSTREAM_DNS, &upstream.sin_addr);
    
    	sendto(sock, request, req_len, 0, (struct sockaddr*)&upstream, sizeof(upstream));
    
    	socklen_t addr_len = sizeof(upstream);
    	int res_len = recvfrom(sock, response, BUFFER_SIZE, 0, (struct sockaddr*)&upstream, &addr_len);
    
    	close(sock);
    	return res_len;
}

int build_response_from_cache(const unsigned char *request, int req_len, unsigned char *response, const char *ip) {
    	memcpy(response, request, req_len);
    	dns_header_t *header = (dns_header_t*)response;
    
    	header->flags = htons(0x8180);
    	header->ancount = htons(1);
    	header->nscount = 0;
    	header->arcount = 0;
    
    	int offset = req_len;
    	struct in_addr addr;
    	inet_pton(AF_INET, ip, &addr);
    
    	unsigned char *ptr = response + offset;
    	*ptr++ = 0xC0;
    	*ptr++ = 0x0C;
    	*ptr++ = 0x00; *ptr++ = 0x01;
    	*ptr++ = 0x00; *ptr++ = 0x01;
    	*ptr++ = 0x00; *ptr++ = 0x00;
    	*ptr++ = 0x00; *ptr++ = 0x3C;
    	*ptr++ = 0x00; *ptr++ = 0x04;
    	memcpy(ptr, &addr, 4);
    
    	return offset + 16;
}

// A structure for transferring data to a stream
typedef struct {
    	int server_sock;  // The original server socket
    	struct sockaddr_in client_addr;
    	unsigned char buffer[BUFFER_SIZE];
    	int req_len;
} request_data_t;

void* handle_dns_request(void *arg) {
    	request_data_t *data = (request_data_t*)arg;
    
    	char domain[256];
    	parse_dns_name(data->buffer, 12, domain, sizeof(domain));
    
    	pthread_mutex_lock(&cache_mutex);
    	cache_entry_t *cached = cache_find(domain);
    	pthread_mutex_unlock(&cache_mutex);
    
    	unsigned char response[BUFFER_SIZE];
    	int resp_len;
    
    	if (cached) {
        	printf("CACHE HIT: %s -> %s\n", domain, cached->ip);
        	resp_len = build_response_from_cache(data->buffer, data->req_len, response, cached->ip);
    	} else {
        	printf("CACHE MISS: %s - forwarding to 8.8.8.8\n", domain);
        	resp_len = forward_to_upstream(data->buffer, data->req_len, response);
        	if (resp_len > 0) {
            		char ip[16];
            		if (extract_ip_from_dns(response, /*resp_len,*/ ip)) {
                		printf("Cached: %s -> %s\n", domain, ip);
            		}
        	}
    	}
    
    	if (resp_len > 0) {
        	// Send the response through the same socket that the server is listening on
        	sendto(data->server_sock, response, resp_len, 0, (struct sockaddr*)&data->client_addr, sizeof(data->client_addr));
        	printf("Response sent to %s:%d\n", inet_ntoa(data->client_addr.sin_addr), ntohs(data->client_addr.sin_port));
    	} else {
        	printf("Failed to handle request for %s\n", domain);
    	}
    
    	free(data);
    	return NULL;
}

int main() {
    	int server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    	if (server_sock < 0) {
        	perror("socket");
        	return 1;
    	}
    
    	int reuse = 1;
    	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    	struct sockaddr_in server_addr;
    	memset(&server_addr, 0, sizeof(server_addr));
    	server_addr.sin_family = AF_INET;
    	server_addr.sin_addr.s_addr = INADDR_ANY;
    	server_addr.sin_port = htons(DNS_PORT);
    
    	if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        	perror("bind");
        	close(server_sock);
        	return 1;
    	}
    
    	printf("DNS Proxy listening on UDP port %d\n", DNS_PORT);
    	printf("Upstream DNS: %s\n", UPSTREAM_DNS);
    
    	memset(cache_table, 0, sizeof(cache_table));
    
    	while (1) {
        	struct sockaddr_in client_addr;
        	socklen_t addr_len = sizeof(client_addr);
        	unsigned char buffer[BUFFER_SIZE];
        
        	int req_len = recvfrom(server_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        
        	if (req_len > 0) {
            		printf("\n[MAIN] Received %d bytes from %s:%d\n", req_len, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            		request_data_t *data = malloc(sizeof(request_data_t));
            		if (data) {
                		data->server_sock = server_sock;  // Passing the server socket
                		data->client_addr = client_addr;
                		data->req_len = req_len;
                		memcpy(data->buffer, buffer, req_len);
                
                		pthread_t thread;
                		pthread_create(&thread, NULL, handle_dns_request, data);
                		pthread_detach(thread);
            		} else {
                		perror("malloc failed");
            		}
        	}
    	}
    
    	close(server_sock);
    	return 0;
}
