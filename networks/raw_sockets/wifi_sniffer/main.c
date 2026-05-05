#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/ioctl.h>

struct ieee80211_frame_ctrl {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t seq_ctrl;
};

// Wi-Fi frame types
#define FRAME_TYPE_MGMT     0x00
#define FRAME_TYPE_CTRL     0x01
#define FRAME_TYPE_DATA     0x02

int get_frame_type(uint16_t frame_control) {
    	return (frame_control & 0x000C) >> 2;
}

void print_mac(const uint8_t *mac) {
    	printf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int main(int argc, char *argv[]) {
    	int raw_socket;
    	unsigned char *buffer = malloc(65536);
    	struct sockaddr_ll saddr;
    	socklen_t saddr_len = sizeof(saddr);
    	ssize_t data_size;
    
    	if (geteuid() != 0) {
        	fprintf(stderr, "geteuid() error\n");
        	return 1;
    	}
    
    	raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (raw_socket == -1) {
        	perror("socket");
        	return 1;
    	}
    
    	printf("Sniffind Wi-Fi frames...\n");
    	printf("Ctrl+C to exit\n\n");
    
    	// Maint cycle of capturing packets
    	while (1) {
        	data_size = recvfrom(raw_socket, buffer, 65536, 0, (struct sockaddr*)&saddr, &saddr_len);
        
        	if (data_size < 0) {
            		perror("recvfrom");
            		continue;
        	}
        
       		// checking is it a wifi frame and so it starts from the begginind and also skipping Radiotap
        	if (data_size >= sizeof(struct ieee80211_frame_ctrl)) {
            		struct ieee80211_frame_ctrl *frame = (struct ieee80211_frame_ctrl*)buffer;
            
            		int frame_type = get_frame_type(ntohs(frame->frame_control));
            
            		// Type
            		const char *type_str;
            		switch(frame_type) {
                		case FRAME_TYPE_MGMT: type_str = "MGMT"; break;
                		case FRAME_TYPE_CTRL: type_str = "CTRL"; break;
                		case FRAME_TYPE_DATA: type_str = "DATA"; break;
                		default: type_str = "UNKN"; break;
            		}
            
            		// Printing info of the frame
            		printf("Type: [%s] ", type_str);
            
            		if (frame_type == FRAME_TYPE_DATA) {
                		printf("SA: "); print_mac(frame->addr2);
                		printf(" -> DA: "); print_mac(frame->addr1);
                		printf(" | Size: %ld bytes\n", data_size);
            		} else if (frame_type == FRAME_TYPE_MGMT) {
                		printf("BSSID: "); print_mac(frame->addr3);
                		printf(" | SA: "); print_mac(frame->addr2);
                		printf("\n");
            		} else {
                		printf("MAC1: "); print_mac(frame->addr1);
                		printf(" MAC2: "); print_mac(frame->addr2);
                		printf("\n");
            		}
        	}
    	}
    
    	close(raw_socket);
    	free(buffer);
    	return 0;
}
