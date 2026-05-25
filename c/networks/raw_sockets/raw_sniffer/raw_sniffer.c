#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/if_packet.h>
#include <net/if.h>

#define BUFFER_SIZE 65536	// 64 KB

int main() {
    	int raw_socket;
    	char buffer[BUFFER_SIZE];
    	struct sockaddr_ll saddr;
    	socklen_t saddr_len = sizeof(saddr);
    	int bytes;
    
    	if ((raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        	perror("socket:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	printf("Capture all Ethernet frames...\n");
    	printf("Press Ctrl+C to exit\n\n");
    
    	while (1) {
        	bytes = recvfrom(raw_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&saddr, &saddr_len);
        
        	if (bytes < 0) {
            		perror("recvfrom:\n");
            		continue;
        	}
        
        	char ifname[IFNAMSIZ];
        	if_indextoname(saddr.sll_ifindex, ifname);
        
        	struct ethhdr *eth = (struct ethhdr*)buffer;
        
        	printf("Interface: %s\n", ifname);
        	printf("MAC src: %02x:%02x:%02x:%02x:%02x:%02x\n", 
				eth->h_source[0], 
				eth->h_source[1], 
				eth->h_source[2], 
				eth->h_source[3], 
				eth->h_source[4], 
				eth->h_source[5]);
        	printf("MAC dst: %02x:%02x:%02x:%02x:%02x:%02x\n", 
				eth->h_dest[0], 
				eth->h_dest[1], 
				eth->h_dest[2], 
				eth->h_dest[3], 
				eth->h_dest[4], 
				eth->h_dest[5]);
        
        	// Ethernet
        	printf("Type: 0x%04x\n", ntohs(eth->h_proto));
        
        	if (ntohs(eth->h_proto) == ETH_P_IP) {
            		// IP packet
            		struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
            		struct sockaddr_in source, dest;
            		source.sin_addr.s_addr = ip->saddr;
            		dest.sin_addr.s_addr = ip->daddr;
            
            		printf("IP src: %s\n", inet_ntoa(source.sin_addr));
            		printf("IP dst: %s\n", inet_ntoa(dest.sin_addr));
            		printf("Protocol: %d\n", ip->protocol);
            
            		if (ip->protocol == IPPROTO_TCP) {
                		struct tcphdr *tcp = (struct tcphdr*)(buffer + sizeof(struct ethhdr) + (ip->ihl * 4));
                		printf("TCP port src: %d\n", ntohs(tcp->source));
                		printf("TCP port dst: %d\n", ntohs(tcp->dest));
            		}
        	}
        
        	printf("---\n");
    	}
    
    	close(raw_socket);
    	return 0;
}
