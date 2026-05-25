#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

#define BUFFER_SIZE 4096

// FIXME do not use typedef

// IEEE 802.11 Frame
// TODO change from packed to alignment
typedef struct {
    	unsigned short frame_control;
    	unsigned short duration;
    	unsigned char addr1[6];
    	unsigned char addr2[6];
    	unsigned char addr3[6];
    	unsigned short seq_ctrl;
} __attribute__((packed)) ieee80211_header;

typedef struct {
    	unsigned long long timestamp;
    	unsigned short beacon_interval;
    	unsigned short capability_info;
} __attribute__((packed)) beacon_fixed;

void get_frame_type(unsigned short fc, char *type_str) {
    	unsigned char type = (fc >> 2) & 0x3;
    	unsigned char subtype = (fc >> 4) & 0xF;
    
    	if (type == 0) {  // Management frame
        	switch(subtype) {
            		case 0x8: sprintf(type_str, "Beacon"); break;
            		case 0x0: sprintf(type_str, "Association Request"); break;
            		case 0x1: sprintf(type_str, "Association Response"); break;
            		case 0x4: sprintf(type_str, "Probe Request"); break;
            		case 0x5: sprintf(type_str, "Probe Response"); break;
            		default: sprintf(type_str, "Management(%d)", subtype);
        	}
    	} else if (type == 1) {  // Control frame
        	sprintf(type_str, "Control(%d)", subtype);
    	} else if (type == 2) {  // Data frame
        	sprintf(type_str, "Data(%d)", subtype);
    	} else {
        	sprintf(type_str, "Unknown(%d)", subtype);
    	}
}

char* extract_ssid(unsigned char *packet, int offset, int len) {
    	static char ssid[33];
    	memset(ssid, 0, 33);
    
    	int pos = offset;
    	while (pos < len) {
        	unsigned char tag = packet[pos++];
        	unsigned char tag_len = packet[pos++];
        
        	if (tag == 0 && tag_len > 0) {  	// SSID tag
            		int copy_len = tag_len < 32 ? tag_len : 32;
            		memcpy(ssid, &packet[pos], copy_len);
            		ssid[copy_len] = '\0';
            		break;
        	}
        	pos += tag_len;
    	}
    	return ssid;
}

// Supported Rates
void extract_rates(unsigned char *packet, int offset, int len) {
    	int pos = offset;
    	printf("Supported Rates: ");
    
    	while (pos < len) {
        	unsigned char tag = packet[pos++];
        	unsigned char tag_len = packet[pos++];
        
        	if (tag == 1 && tag_len > 0) {  // Supported Rates tag
            		for (int i = 0; i < tag_len; i++) {
                		printf("%.1f Mbps ", (packet[pos + i] & 0x7F) * 0.5);
            		}
            		break;
        	}
        	pos += tag_len;
    	}
    	printf("\n");
}

void print_mac(unsigned char *mac) {
    	printf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int main(int argc, char *argv[]) {
    	int sockfd;
    	unsigned char buffer[BUFFER_SIZE];
    	struct sockaddr_ll addr;
    	socklen_t addr_len = sizeof(addr);
    	char iface[16] = "wlan0";
    
    	if (argc > 1) {
        	strncpy(iface, argv[1], 15);
        	iface[15] = '\0';
    	}
    
    	printf("Starting Beacon Frame Analyzer on interface: %s\n", iface);
    
    	sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (sockfd < 0) {
        	perror("socket");
        	return 1;
    	}
    
    	struct ifreq ifr;
    	memset(&ifr, 0, sizeof(ifr));
    	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    	if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        	perror("ioctl");
        	close(sockfd);
        	return 1;
    	}
    
    	memset(&addr, 0, sizeof(addr));
    	addr.sll_family = AF_PACKET;
    	addr.sll_ifindex = ifr.ifr_ifindex;
    	addr.sll_protocol = htons(ETH_P_ALL);
    
    	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        	perror("bind");
        	close(sockfd);
        	return 1;
    	}
    
    	printf("Listening for beacon frames...\n\n");
    
    	int frame_count = 0;
    
    	while (1) {
        	memset(buffer, 0, BUFFER_SIZE);
        	int len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&addr, &addr_len);
        
        	if (len < 0) {
            		perror("recvfrom");
            		break;
        	}
        
        	// 802.11 frames do not have the Ethernet header
        	if (len < sizeof(ieee80211_header)) {
            		continue;
        	}
        
        	ieee80211_header *mac_header = (ieee80211_header*)buffer;
        	unsigned short fc = ntohs(mac_header->frame_control);
        
        	char frame_type[32];
        	get_frame_type(fc, frame_type);
        
        	if (strcmp(frame_type, "Beacon") == 0) {
            		frame_count++;
            
            		// Skipping header to the beacon
            		int header_len = sizeof(ieee80211_header);
            		if (len < header_len + sizeof(beacon_fixed)) {
                		continue;
            		}
            
            		beacon_fixed *beacon = (beacon_fixed*)(buffer + header_len);
            
            		printf("\n-----\n");
            		printf("Beacon Frame #%d\n", frame_count);
            
            		printf("BSSID (Source): ");
            		print_mac(mac_header->addr2);
            		printf("\n");
            
            		printf("Destination: ");
            		print_mac(mac_header->addr1);
            		printf("\n");
            
            		printf("Source: ");
            		print_mac(mac_header->addr2);
            		printf("\n");
            
            		printf("Beacon Interval: %d\n", ntohs(beacon->beacon_interval));
            		printf("Timestamp: %llu\n", beacon->timestamp);
            		printf("Capability: 0x%04x\n", ntohs(beacon->capability_info));
            
            		int tagged_params_offset = header_len + sizeof(beacon_fixed);
            		if (tagged_params_offset < len) {
                		// SSID
                		char *ssid = extract_ssid(buffer, tagged_params_offset, len);
                		if (strlen(ssid) > 0) {
                    			printf("SSID: %s\n", ssid);
                		} else {
                    			printf("SSID: [Hidden/Broadcast]\n");
                		}
                
                		// Supported Rates
                		extract_rates(buffer, tagged_params_offset, len);
                
                		// def_encr
                		int pos = tagged_params_offset;
                		int wpa_found = 0, rsn_found = 0;
                		while (pos < len) {
                    			unsigned char tag = buffer[pos++];
                    			unsigned char tag_len = buffer[pos++];
                    			if (tag == 48) {  // RSN Information
                        			rsn_found = 1;
                    			} else if (tag == 221) {  // Vendor specific (WPA)
                        			if (tag_len >= 4 && buffer[pos] == 0x00 && buffer[pos+1] == 0x50 && buffer[pos+2] == 0xf2) {
                            				wpa_found = 1;
                        			}
                    			}
                    			pos += tag_len;
                		}
                
                		if (rsn_found || wpa_found) {
                    			printf("Encryption: WPA/WPA2\n");
                		} else if (ntohs(beacon->capability_info) & 0x0010) {
                    			printf("Encryption: WEP\n");
                		} else {
                    			printf("Encryption: Open\n");
                		}
                
                		// DS
                		pos = tagged_params_offset;
                		while (pos < len) {
                    			unsigned char tag = buffer[pos++];
                    			unsigned char tag_len = buffer[pos++];
                    			if (tag == 3 && tag_len == 1) {  // DS Parameter Set
                        			printf("Channel: %d\n", buffer[pos]);
                        			break;
                    			}
                    			pos += tag_len;
                		}
            		}
            
            		printf("Frame length: %d bytes\n", len);
        	}
    	}
    
    	close(sockfd);
    	return 0;
}
