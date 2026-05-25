// Summary: FML
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>

#define BUFFER_SIZE 65536
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

// DHCP options
#define DHCP_MSG_TYPE 53
#define DHCP_REQUESTED_IP 50
#define DHCP_SERVER_ID 54

// DHCP messanges
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_DECLINE 4
#define DHCP_ACK 5
#define DHCP_NAK 6
#define DHCP_RELEASE 7

typedef struct dhcp_binding {
    	uint8_t mac[6];
   	uint32_t ip;
    	char iface[16];
    	time_t lease_time;
    	time_t expiration;
    	struct dhcp_binding *next;
} dhcp_binding_t;

dhcp_binding_t *bindings = NULL;
int raw_sock = -1;
volatile int running = 1;

// DHCP header (after UDP)
typedef struct {
    	uint8_t op;
    	uint8_t htype;
    	uint8_t hlen;
    	uint8_t hops;
    	uint32_t xid;
    	uint16_t secs;
    	uint16_t flags;
    	uint32_t ciaddr;
    	uint32_t yiaddr;
    	uint32_t siaddr;
    	uint32_t giaddr;
    	uint8_t chaddr[16];
    	uint8_t sname[64];
    	uint8_t file[128];
    	uint8_t options[312];
} __attribute__((packed)) dhcp_header_t;

void signal_handler(int sig) {
    	if (sig == SIGINT || sig == SIGTERM) {
        	printf("\nShutting down DHCP Snooping...\n");
        	running = 0;
        	if (raw_sock >= 0) close(raw_sock);
    	}
}

void add_binding(uint8_t *mac, uint32_t ip, const char *iface, uint32_t lease) {
    	// Checking is binding already exist
    	dhcp_binding_t *current = bindings;
    	while (current) {
        	if (memcmp(current->mac, mac, 6) == 0 && current->ip == ip) {
            		current->expiration = time(NULL) + lease;
            		struct in_addr addr;
            		addr.s_addr = ip;
            		printf("Binding updated: %s on %s (lease +%us)\n", inet_ntoa(addr), iface, lease);
            		return;
        	}
        	current = current->next;
    	}
    
    	dhcp_binding_t *new_binding = malloc(sizeof(dhcp_binding_t));
    	if (!new_binding) return;
    
    	memcpy(new_binding->mac, mac, 6);
    	new_binding->ip = ip;
    	strcpy(new_binding->iface, iface);
    	new_binding->lease_time = lease;
    	new_binding->expiration = time(NULL) + lease;
    	new_binding->next = bindings;
    	bindings = new_binding;
    
    	struct in_addr addr;
    	addr.s_addr = ip;
    	printf("BINDING ADDED: %02x:%02x:%02x:%02x:%02x:%02x -> %s on %s (lease %us, expires in %lds)\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], inet_ntoa(addr), iface, lease, (long)lease);
}

// Checking the validation of the bind
int check_binding(uint8_t *mac, uint32_t ip, const char *iface) {
    	dhcp_binding_t *current = bindings;
    	time_t now = time(NULL);
    
    	while (current) {
        	if (memcmp(current->mac, mac, 6) == 0) {
            		if (current->ip == ip && strcmp(current->iface, iface) == 0) {
                		if (current->expiration > now) {
                    			return 1;
                		} else {
                    			printf("[!] Binding expired for %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                		}
            		}
        	}
        	current = current->next;
    	}
    	return 0;
}

uint8_t *get_dhcp_option(uint8_t *options, int opt_len, uint8_t opt_code) {
    	int i = 0;
    	while (i < opt_len) {
        	if (options[i] == 0) {
            		i++;
            		continue;
        	}
        	if (options[i] == 255) break;
        	if (options[i] == opt_code && i + 1 < opt_len) {
            		return &options[i + 2];
        	}
        	if (i + options[i + 1] + 2 <= opt_len) {
            		i += options[i + 1] + 2;
        	} else {
            		break;
        	}
    	}
    	return NULL;
}

void mac_to_str(uint8_t *mac, char *str) {
    	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Ethernet
void process_dhcp_packet_eth(uint8_t *packet, int len, const char *iface, int trusted_port) {
    	if (len < sizeof(struct ether_header)) return;
    
    	struct ether_header *eth = (struct ether_header *)packet;
    	if (ntohs(eth->ether_type) != ETHERTYPE_IP) return;
    
    	int ip_offset = sizeof(struct ether_header);
    	if (len < ip_offset + sizeof(struct iphdr)) return;
    
    	struct iphdr *ip = (struct iphdr *)(packet + ip_offset);
    	if (ip->protocol != IPPROTO_UDP) return;
    
    	int ip_header_len = ip->ihl * 4;
    	int udp_offset = ip_offset + ip_header_len;
    
    	if (len < udp_offset + sizeof(struct udphdr)) return;
    
    	struct udphdr *udp = (struct udphdr *)(packet + udp_offset);
    
    	// Checking out DHCP ports
    	if (ntohs(udp->dest) != DHCP_SERVER_PORT && ntohs(udp->source) != DHCP_CLIENT_PORT) return;
    
    	int dhcp_offset = udp_offset + sizeof(struct udphdr);
    	if (len < dhcp_offset + sizeof(dhcp_header_t)) return;
    
    	dhcp_header_t *dhcp = (dhcp_header_t *)(packet + dhcp_offset);
    
    	// Type of the DHCP mes
    	uint8_t *msg_type_opt = get_dhcp_option(dhcp->options, 312, DHCP_MSG_TYPE);
    	if (!msg_type_opt) return;
    
    	uint8_t dhcp_msg_type = msg_type_opt[0];
    	char mac_str[18];
    	mac_to_str(dhcp->chaddr, mac_str);
    
    	struct in_addr addr;
    	// char ip_str[16];
    
    	// Logs
    	printf("\n[%s] %s port | DHCP ", iface, trusted_port ? "TRUSTED" : "UNTRUSTED");
    	switch(dhcp_msg_type) {
        	case DHCP_DISCOVER: printf("DISCOVER"); break;
        	case DHCP_OFFER: printf("OFFER"); break;
        	case DHCP_REQUEST: printf("REQUEST"); break;
        	case DHCP_ACK: printf("ACK"); break;
        	case DHCP_NAK: printf("NAK"); break;
        	case DHCP_RELEASE: printf("RELEASE"); break;
        	default: printf("TYPE%d", dhcp_msg_type);
    	}
    	printf(" from MAC %s", mac_str);
    
    	if (dhcp->yiaddr) {
        	addr.s_addr = dhcp->yiaddr;
        	printf(" (offered IP: %s)", inet_ntoa(addr));
    	}
    
    	// blocking untrusted ports
    	if (!trusted_port) {
        	if (dhcp_msg_type == DHCP_OFFER || dhcp_msg_type == DHCP_ACK) {
            		printf("BLOCKED (server messages not allowed on untrusted port: %d)\n", trusted_port);
            		return;
        	}
        
        	// Checking out the validation of Request
		if (dhcp_msg_type == DHCP_REQUEST) {
            		uint32_t requested_ip = 0;
            		uint8_t *req_ip_opt = get_dhcp_option(dhcp->options, 312, DHCP_REQUESTED_IP);
            		if (req_ip_opt) {
                		memcpy(&requested_ip, req_ip_opt, 4);
                		addr.s_addr = requested_ip;
                
                		if (!check_binding(dhcp->chaddr, requested_ip, iface)) {
                    			printf("BLOCKED (invalid binding request for %s)\n", inet_ntoa(addr));
                    			return;
                		} else {
                    			printf("VALID request for %s", inet_ntoa(addr));
                		}
            		}
        	}
        
        	if (dhcp_msg_type == DHCP_RELEASE) {
            		// RELEASE is always allowed but removing the bind
            		printf("RELEASE accepted");
            		dhcp_binding_t *current = bindings;
            		dhcp_binding_t *prev = NULL;
            		while (current) {
                		if (memcmp(current->mac, dhcp->chaddr, 6) == 0) {
                    			if (prev) prev->next = current->next;
                    			else bindings = current->next;
                    			free(current);
                    			printf(" (binding removed)");
                    			break;
                		}
                		prev = current;
                		current = current->next;
            		}
        	}
    	}
    
    	printf("ALLOWED\n");
    
    	// Binding ACK to the trusted port
    	if (dhcp_msg_type == DHCP_ACK && trusted_port) {
        	uint32_t lease = 3600; // Default: 1 hour
        	uint8_t *lease_opt = get_dhcp_option(dhcp->options, 312, 51); // Option 51 - lease time
        	if (lease_opt) {
            		memcpy(&lease, lease_opt, 4);
            		lease = ntohl(lease);
        	}
        	add_binding(dhcp->chaddr, dhcp->yiaddr, iface, lease);
    	}
}

// Loopback (hold in mind that there is no Ethernet header)
void process_dhcp_packet_loopback(uint8_t *packet, int len, const char *iface) {
    	if (len < sizeof(struct iphdr)) return;
    
    	struct iphdr *ip = (struct iphdr *)packet;
    	if (ip->protocol != IPPROTO_UDP) return;
    
    	int ip_header_len = ip->ihl * 4;
    	struct udphdr *udp = (struct udphdr *)(packet + ip_header_len);
    
    	if (ntohs(udp->dest) != DHCP_SERVER_PORT && ntohs(udp->source) != DHCP_CLIENT_PORT) return;
    
    	int dhcp_offset = ip_header_len + sizeof(struct udphdr);
    	if (len < dhcp_offset + sizeof(dhcp_header_t)) return;
    
    	dhcp_header_t *dhcp = (dhcp_header_t *)(packet + dhcp_offset);
    
    	uint8_t *msg_type_opt = get_dhcp_option(dhcp->options, 312, DHCP_MSG_TYPE);
    	if (!msg_type_opt) return;
    
    	char mac_str[18];
    	mac_to_str(dhcp->chaddr, mac_str);
    
    	printf("\n[%s] LOOPBACK | DHCP message type %d from MAC %s (simulation only)\n", iface, msg_type_opt[0], mac_str);
}

void show_bindings() {
    	dhcp_binding_t *current = bindings;
    	if (!current) {
        	printf("\nNo active bindings in the binding table\n");
        	return;
    	}
    
    	printf("\n---DHCP Snooping Binding Table---\n");
    	printf("%-20s %-16s %-10s %-20s\n", "MAC Address", "IP Address", "Interface", "Expires in");
    
    	time_t now = time(NULL);
    	while (current) {
        	struct in_addr addr;
        	addr.s_addr = current->ip;
        	char mac_str[18];
        	mac_to_str(current->mac, mac_str);
        
        	long remaining = (long)(current->expiration - now);
        	if (remaining < 0) remaining = 0;
        
        	printf("%-20s %-16s %-10s %-20ld\n", mac_str, inet_ntoa(addr), current->iface, remaining);
        	current = current->next;
    	}
    	printf("-----\n\n");
}

int setup_raw_socket(const char *iface) {
    	int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (sock < 0) {
        	perror("socket");
        	return -1;
    	}
    
    	struct ifreq ifr;
    	memset(&ifr, 0, sizeof(ifr));
    	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    
    	// Index of the interface
    	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        	perror("ioctl SIOCGIFINDEX");
        	close(sock);
        	return -1;
    	}
    
    	// Bind
    	struct sockaddr_ll saddr;
    	memset(&saddr, 0, sizeof(saddr));
    	saddr.sll_family = AF_PACKET;
    	saddr.sll_protocol = htons(ETH_P_ALL);
    	saddr.sll_ifindex = ifr.ifr_ifindex;
    
    	if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        	perror("bind");
        	close(sock);
        	return -1;
    	}
    
    	return sock;
}

int setup_loopback_socket() {
    	int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (sock < 0) {
        	perror("socket");
        	return -1;
    	}
    
    	struct ifreq ifr;
    	memset(&ifr, 0, sizeof(ifr));
    	strcpy(ifr.ifr_name, "lo");
    
    	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        	perror("ioctl SIOCGIFINDEX for lo");
        	close(sock);
        	return -1;
    	}
    
    	struct sockaddr_ll saddr;
    	memset(&saddr, 0, sizeof(saddr));
    	saddr.sll_family = AF_PACKET;
    	saddr.sll_protocol = htons(ETH_P_ALL);
    	saddr.sll_ifindex = ifr.ifr_ifindex;
    
    	if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        	perror("bind for lo");
        	close(sock);
        	return -1;
    	}
    
    	return sock;
}

int main(int argc, char *argv[]) {
    	if (argc < 2) {
        	printf("Usage: %s <trusted_interface> [untrusted]\n", argv[0]);
        	exit(1);
    	}
    
    	signal(SIGINT, signal_handler);
    	signal(SIGTERM, signal_handler);
    
    	char trusted_iface[IFNAMSIZ];
    	strncpy(trusted_iface, argv[1], IFNAMSIZ - 1);
    
    	printf("Trusted interface: %s\n", trusted_iface);
    
    	if (strcmp(trusted_iface, "lo") == 0) {
        	raw_sock = setup_loopback_socket();
    	} else {
        	raw_sock = setup_raw_socket(trusted_iface);
    	}
    
    	if (raw_sock < 0) {
        	printf("was in the functuns that returned raw_sock");
		exit(1);
    	}
    
    	printf("Monitoring DHCP traffic on %s...\n", trusted_iface);
    	printf("Press Ctrl+C to stop and show bindings\n\n");
    
    	uint8_t buffer[BUFFER_SIZE];
    	struct sockaddr_ll addr;
    	socklen_t addr_len = sizeof(addr);
    
    	// Timer to periodic show of stats
    	time_t last_show = time(NULL);
    
    	while (running) {
        	fd_set fds;
        	FD_ZERO(&fds);
        	FD_SET(raw_sock, &fds);
        
        	struct timeval tv;
        	tv.tv_sec = 5;
        	tv.tv_usec = 0;
        
        	int ret = select(raw_sock + 1, &fds, NULL, NULL, &tv);
        	if (ret < 0) {
            		if (running) perror("select");
            		continue;
        	}
        
        	if (ret == 0) {
            		// Showing table every 30 sec
			if (time(NULL) - last_show >= 30) {
                		show_bindings();
                		last_show = time(NULL);
            		}
            		continue;
        	}
        
        	int len = recvfrom(raw_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addr_len);
        	if (len < 0) {
            		if (running) perror("recvfrom");
            		continue;
        	}
        
        	char iface_name[IFNAMSIZ];
        	if (if_indextoname(addr.sll_ifindex, iface_name) == NULL) {
            		strcpy(iface_name, "unknown");
        	}
        
        	// Def trusted port
		int trusted = (strcmp(iface_name, trusted_iface) == 0);
        
        	// Specific for loopback
		if (strcmp(iface_name, "lo") == 0) {
            		process_dhcp_packet_loopback(buffer, len, iface_name);
        	} else {
            		process_dhcp_packet_eth(buffer, len, iface_name, trusted);
        	}
    	}
    
    	close(raw_sock);
    	show_bindings();
    
    	return 0;
}
