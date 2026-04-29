#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>

#define BUFFER_SIZE 65536	// 64 KB

void print_mac(const unsigned char *mac) {
	printf("%02x:%02x:%02x:%02x:%02x:%02x", 
			mac[0], 
			mac[1], 
			mac[2], 
			mac[3], 
			mac[4], 
			mac[5]);
}

int main(int argc, char *argv[]) {
    	int raw_socket;
    	unsigned char *buffer = malloc(BUFFER_SIZE);
    	struct sockaddr_ll saddr;
    	socklen_t saddr_len = sizeof(saddr);
    	ssize_t data_size;
    
    	if (argc != 2) {
        	fprintf(stderr, "Using: %s <interface>\n", argv[0]);
        	exit(EXIT_FAILURE);
    	}
    
    	raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (raw_socket == -1) {
        	perror("socket:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	struct ifreq ifr;
    	memset(&ifr, 0, sizeof(ifr));
    	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ - 1);
    
   	 if (setsockopt(raw_socket, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) == -1) {
        	perror("setsockopt:\n");
        	close(raw_socket);
        	exit(EXIT_FAILURE);
    	}
    
    	printf("Listening the %s interface...\n", argv[1]);
    
    	unsigned long packet_count = 0;
    
    	while (1) {
        	data_size = recvfrom(raw_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&saddr, &saddr_len);
        	if (data_size < 0) {
            		perror("recvfrom:\n");
            		continue;
        	}
        	struct ethhdr *eth = (struct ethhdr *)buffer; // Pointer to the ethernet-header
        
        	printf("[%lu] Interface: %d | ", ++packet_count, saddr.sll_ifindex);
        	printf("Src: ");
        	print_mac(eth->h_source);
        	printf(" -> Dst: ");
        	print_mac(eth->h_dest);
        
        	// Def type of the protocol
        	printf(" | Type: 0x%04x", ntohs(eth->h_proto));
        
        	// Checking VLAN (802.1Q)
        	if (ntohs(eth->h_proto) == 0x8100) {
            		struct vlan_hdr {
                		uint16_t tpid;
                		uint16_t tci;
            		} __attribute__((packed));
            
            		struct vlan_hdr *vlan = (struct vlan_hdr*)(buffer + sizeof(struct ethhdr));
            		uint16_t vid = ntohs(vlan->tci) & 0x0FFF;
            		printf(" | VLAN: %d", vid);
        	}
        
        	printf("\n");
    	}
    
    	free(buffer);
    	close(raw_socket);
    	return 0;
}
