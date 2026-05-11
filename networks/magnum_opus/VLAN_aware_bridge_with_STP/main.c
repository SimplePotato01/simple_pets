#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/ethernet.h>

#define NUM_PORTS 4
#define VLAN_COUNT 3
#define BPDU_INTERVAL 2  // seconds
#define MAC_ADDR_LEN 6

// VLAN IDs
const int vlans[] = {10, 20, 30};

#define ETH_P_8021Q 0x8100
#define ETH_P_STP   0x8888

// Port states according to RSTP
typedef enum {
    	STATE_DISCARDING = 0,
    	STATE_LEARNING = 1,
    	STATE_FORWARDING = 2
} port_state_t;

typedef struct {
    	char name[16];
    	int socket_fd;
    	port_state_t state;
    	int vlan_membership[VLAN_COUNT];
    	time_t last_bpdu_time;
    	int root_path_cost;
    	int designated_root;
} port_t;

// Bridge
typedef struct {
    	port_t ports[NUM_PORTS];
    	int bridge_id;
    	int root_bridge_id;
    	int running;
    	pthread_mutex_t mutex;
    	time_t learning_start[NUM_PORTS];
} bridge_t;

bridge_t bridge;

// BPDU structure
#pragma pack(push, 1)
typedef struct {
    	uint8_t dst_mac[6];
    	uint8_t src_mac[6];
    	uint16_t ethertype;
    	uint16_t bpdu_type;
    	uint8_t flags;
    	uint32_t root_id;
    	uint32_t bridge_id;
    	uint16_t port_id;
    	uint16_t message_age;
    	uint16_t max_age;
    	uint16_t hello_time;
    	uint16_t forward_delay;
} bpdu_t;

typedef struct {
    	uint16_t tpid;	// Tag Protocol ID (0x8100)
    	uint16_t tci;   // Tag Control Info (PCP + DEI + VID)
} vlan_header_t;
#pragma pack(pop)

// Initialize raw socket for a port
int init_raw_socket(const char *ifname) {
    	int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (sock < 0) {
        	perror("socket");
        	return -1;
    	}
    
    	struct ifreq ifr;
    	memset(&ifr, 0, sizeof(ifr));
    	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    
    	if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        	perror("setsockopt BINDTODEVICE");
        	close(sock);
        	return -1;
    	}
    
    	return sock;
}

void send_bpdu(port_t *port, int port_id) {
    	bpdu_t bpdu;
    	memset(&bpdu, 0, sizeof(bpdu));
    
    	// STP multicast MAC
    	bpdu.dst_mac[0] = 0x01;
    	bpdu.dst_mac[1] = 0x80;
    	bpdu.dst_mac[2] = 0xC2;
    	bpdu.dst_mac[3] = 0x00;
    	bpdu.dst_mac[4] = 0x00;
    	bpdu.dst_mac[5] = 0x00;
    
    	// Fake source MAC
    	for (int i = 0; i < MAC_ADDR_LEN; i++)
        	bpdu.src_mac[i] = 0x02 + i;
    
    	bpdu.ethertype = htons(ETH_P_STP);
    	bpdu.bpdu_type = htons(1);
    	bpdu.flags = (port->state == STATE_FORWARDING) ? 0x01 : 0x00;
    	bpdu.root_id = htonl(bridge.root_bridge_id);
    	bpdu.bridge_id = htonl(bridge.bridge_id);
    	bpdu.port_id = htons(port_id);
    	bpdu.hello_time = htons(BPDU_INTERVAL);
    	bpdu.max_age = htons(20);
    	bpdu.forward_delay = htons(15);
    
    	struct sockaddr_ll addr;
    	memset(&addr, 0, sizeof(addr));
    	addr.sll_family = AF_PACKET;
    	addr.sll_protocol = htons(ETH_P_ALL);
    	addr.sll_ifindex = if_nametoindex(port->name);
    
    	ssize_t sent = sendto(port->socket_fd, &bpdu, sizeof(bpdu), 0, (struct sockaddr*)&addr, sizeof(addr));
    
    	if (sent < 0) {
        	perror("sendto BPDU");
    	} else {
        	printf("[Port %s] BPDU sent (state: %s)\n", port->name, 
				port->state == STATE_DISCARDING ? "DISCARDING" :
				port->state == STATE_LEARNING ? "LEARNING" : "FORWARDING");
    	}
}

void process_bpdu(port_t *port, const bpdu_t *bpdu, int port_id) {
    	uint32_t received_root = ntohl(bpdu->root_id);
    
    	pthread_mutex_lock(&bridge.mutex);
    
    	if (received_root < bridge.root_bridge_id) {
        	printf("\n[STP] Better root bridge found: %d (was %d)\n", received_root, bridge.root_bridge_id);
        	bridge.root_bridge_id = received_root;
        
        	// Transition all ports to DISCARDING
        	for (int i = 0; i < NUM_PORTS; i++) {
            		if (bridge.ports[i].state != STATE_DISCARDING) {
                		port_state_t old = bridge.ports[i].state;
                		bridge.ports[i].state = STATE_DISCARDING;
                		bridge.learning_start[i] = 0;
                		printf("[Port %s] %s -> DISCARDING\n", bridge.ports[i].name, old == STATE_LEARNING ? "LEARNING" : "FORWARDING");
            		}
        	}
        
        	// Root port transitions to LEARNING after a BPDU
        	if (port_id == 0) {
            		sleep(2);
            		bridge.ports[port_id].state = STATE_LEARNING;
            		bridge.learning_start[port_id] = time(NULL);
            		printf("[Port %s] DISCARDING -> LEARNING\n", port->name);
        	}
    	}
    
    	port->root_path_cost = ntohs(bpdu->message_age) + 10;
    	port->last_bpdu_time = time(NULL);
    
    	pthread_mutex_unlock(&bridge.mutex);
}

int extract_vlan_id(const uint8_t *packet, int len) {
    	struct ether_header *eth = (struct ether_header*)packet;
    
    	// Check if packet has VLAN tag
    	if (ntohs(eth->ether_type) == ETH_P_8021Q) {
        	if (len >= (int)(sizeof(struct ether_header) + sizeof(vlan_header_t))) {
            		vlan_header_t *vlan = (vlan_header_t*)(packet + sizeof(struct ether_header));
            		return ntohs(vlan->tci) & 0x0FFF;  // Extract VID (12 bits)
        	}
    	}
    
    	return -1;  // Untagged
}

int is_port_vlan_member(port_t *port, int vlan_id) {
    	for (int i = 0; i < VLAN_COUNT; i++) {
        	if (vlans[i] == vlan_id && port->vlan_membership[i]) return 1;
    	}
    	return 0;
}

void forward_packet(port_t *src_port, port_t *dst_port, uint8_t *packet, int len, int vlan_id) {
    	if (!is_port_vlan_member(dst_port, vlan_id)) {
        	printf("[Filter] Port %s not in VLAN %d, dropping\n", dst_port->name, vlan_id);
        	return;
    	}
    
    	if (dst_port->state != STATE_FORWARDING) {
        	printf("[Block] Port %s in %s state\n", dst_port->name, dst_port->state == STATE_DISCARDING ? "DISCARDING" : "LEARNING");
        	return;
    	}
    
    	struct sockaddr_ll addr;
    	memset(&addr, 0, sizeof(addr));
    	addr.sll_family = AF_PACKET;
    	addr.sll_protocol = htons(ETH_P_ALL);
    	addr.sll_ifindex = if_nametoindex(dst_port->name);
    
    	ssize_t sent = sendto(dst_port->socket_fd, packet, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    
    	if (sent > 0) {
        	printf("[Forward] VLAN %d: %s -> %s\n", vlan_id, src_port->name, dst_port->name);
    	}
}

void *port_state_machine(void *arg) {
    	while (bridge.running) {
        	sleep(1);
        
        	pthread_mutex_lock(&bridge.mutex);
        
        	for (int i = 0; i < NUM_PORTS; i++) {
            		if (bridge.ports[i].state == STATE_LEARNING) {
                		if (bridge.learning_start[i] == 0)
                    			bridge.learning_start[i] = time(NULL);
                
                		// After forward_delay (15 sec) move to FORWARDING
                		if (time(NULL) - bridge.learning_start[i] >= 15) {
                    			bridge.ports[i].state = STATE_FORWARDING;
                    			printf("[Port %s] LEARNING -> FORWARDING (VLANs: ", bridge.ports[i].name);
                    			for (int v = 0; v < VLAN_COUNT; v++) {
                        			if (bridge.ports[i].vlan_membership[v])
                            			printf("%d ", vlans[v]);
                    			}
                    			printf(")\n");
                    			bridge.learning_start[i] = 0;
                		}
            		}
        	}
        
        	pthread_mutex_unlock(&bridge.mutex);
    	}
    	return NULL;
}

// Main packet processing loop
void *port_loop(void *arg) {
    	int port_idx = *(int*)arg;
    	free(arg);
    	port_t *port = &bridge.ports[port_idx];
    	uint8_t buffer[2048];
    
    	while (bridge.running) {
        	struct sockaddr_ll src_addr;
        	socklen_t addr_len = sizeof(src_addr);
        
        	ssize_t len = recvfrom(port->socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &addr_len);
        
        	if (len < (int)sizeof(struct ether_header)) continue;
        
        	// Check if BPDU dest MAC is STP multicast
        	if (buffer[0] == 0x01 && 
				buffer[1] == 0x80 && 
				buffer[2] == 0xC2 && 
				buffer[3] == 0x00 && 
				buffer[4] == 0x00 && 
				buffer[5] == 0x00) {
            		if (len >= (int)sizeof(bpdu_t)) process_bpdu(port, (bpdu_t*)buffer, port_idx);
            		continue;
        	}
        
        	// Extract VLAN ID
        	int vlan_id = extract_vlan_id(buffer, len);
        
        	if (port->state == STATE_DISCARDING) {
            		static time_t last_log = 0;
            		if (time(NULL) - last_log > 10) {
                		printf("[%s] DISCARDING state, dropping packet (VLAN: %s)\n", port->name, vlan_id == -1 ? "untagged" : "tagged");
                		last_log = time(NULL);
            		}
            		continue;
        	}
        
        	if (port->state == STATE_LEARNING) {
            		printf("[%s] LEARNING state, learning MAC (VLAN: %s)\n", port->name, vlan_id == -1 ? "untagged" : "tagged");
            		continue;
        	}
        
        	// Forwarding state
        	if (port->state == STATE_FORWARDING) {
            		printf("[%s] FORWARDING packet (VLAN: %s, len: %zd)\n", port->name, vlan_id == -1 ? "untagged" : "tagged", len);
            
            		for (int i = 0; i < NUM_PORTS; i++) {
                		if (i == port_idx) continue;
                
                		port_t *dst = &bridge.ports[i];
                
                		if (vlan_id == -1) {
                    			// Untagged - forward to all VLANs (simplified)
                    			for (int v = 0; v < VLAN_COUNT; v++) {
                        			if (is_port_vlan_member(dst, vlans[v])) forward_packet(port, dst, buffer, len, vlans[v]);
                    			}
                		} else {
                    			// Tagged
                    			if (is_port_vlan_member(dst, vlan_id) && dst->state == STATE_FORWARDING) 
						forward_packet(port, dst, buffer, len, vlan_id);
                		}
            		}
        	}
    	}
    	return NULL;
}

void *bpdu_sender(void *arg) {
    	while (bridge.running) {
        	sleep(BPDU_INTERVAL);
        
        	pthread_mutex_lock(&bridge.mutex);
        
        	for (int i = 0; i < NUM_PORTS; i++) {
            		send_bpdu(&bridge.ports[i], i);
        	}
        
        	pthread_mutex_unlock(&bridge.mutex);
    	}
    	return NULL;
}

void signal_handler(int sig) {
    	printf("\n\nShutting down bridge...\n");
    	bridge.running = 0;
}

int main() {
    	printf("VLAN-Aware Bridge with RSTP\n");
    	printf("Bridge ID: 1\n");
    	printf("VLANs: 10, 20, 30\n\n");
    
    	signal(SIGINT, signal_handler);
    
    	// Init
    	memset(&bridge, 0, sizeof(bridge));
    	bridge.bridge_id = 1;
    	bridge.root_bridge_id = 1;
    	bridge.running = 1;
    	pthread_mutex_init(&bridge.mutex, NULL);
    
    	const char *port_names[NUM_PORTS] = {"veth0", "veth1", "veth2", "veth3"};
    	printf("Initializing ports...\n");
    	for (int i = 0; i < NUM_PORTS; i++) {
        	strncpy(bridge.ports[i].name, port_names[i], 15);
        	bridge.ports[i].state = STATE_DISCARDING;
        	bridge.learning_start[i] = 0;
        	bridge.ports[i].socket_fd = init_raw_socket(bridge.ports[i].name);
        
        	if (bridge.ports[i].socket_fd < 0) {
            		fprintf(stderr, "\nERROR: Failed to initialize %s\n", bridge.ports[i].name);
            		fprintf(stderr, "\nPlease create veth pairs first:\n");
            		for (int j = 0; j < NUM_PORTS; j++) {
                		printf("  sudo ip link add %s type veth peer name %s_peer\n", port_names[j], port_names[j]);
                		printf("  sudo ip link set %s up\n", port_names[j]);
            		}
            		printf("\n  sudo ip link set %s_peer up (for testing)\n", port_names[0]);
            		return 1;
        	}
        
        	// Configure VLAN membership
        	for (int v = 0; v < VLAN_COUNT; v++) {
            		// Port 0: VLAN 10, 20 
			// Port 1: VLAN 10, 30 
			// Port 2: VLAN 20 
			// Port 3: VLAN 30
            		if (i == 0 && (v == 0 || v == 1)) bridge.ports[i].vlan_membership[v] = 1;
            		else if (i == 1 && (v == 0 || v == 2)) bridge.ports[i].vlan_membership[v] = 1;
            		else if (i == 2 && v == 1) bridge.ports[i].vlan_membership[v] = 1;
            		else if (i == 3 && v == 2) bridge.ports[i].vlan_membership[v] = 1;
        	}
        
        	printf("  %s: VLANs {", bridge.ports[i].name);
        	for (int v = 0; v < VLAN_COUNT; v++) {
            		if (bridge.ports[i].vlan_membership[v])
                		printf("%d ", vlans[v]);
        	}
        	printf("}, state: DISCARDING\n");
    	}
    
    	printf("\nStarting RSTP protocol...\n");
    	printf("Ports will transition: DISCARDING -> LEARNING -> FORWARDING\n\n");
    
    	// Time to stabilize
    	sleep(2);
    
    	pthread_t sender_thread, state_thread, port_threads[NUM_PORTS];
    	pthread_create(&sender_thread, NULL, bpdu_sender, NULL);
    	pthread_create(&state_thread, NULL, port_state_machine, NULL);
    
    	for (int i = 0; i < NUM_PORTS; i++) {
        	int *idx = malloc(sizeof(int));
        	*idx = i;
        	pthread_create(&port_threads[i], NULL, port_loop, idx);
    	}
    
    	// Simulate RSTP convergence
    	printf("[STP] Bridge ID: %d, Root ID: %d\n", bridge.bridge_id, bridge.root_bridge_id);
    	printf("[STP] Port veth0 selected as root port\n");
    	printf("[Port veth0] DISCARDING -> LEARNING\n");
    	bridge.ports[0].state = STATE_LEARNING;
    	bridge.learning_start[0] = time(NULL);
    
    	printf("\nBridge is running. Press Ctrl+C to stop.\n");
    	printf("-----\n\n");
    
    	// Monitoring loop
    	while (bridge.running) {
        	sleep(5);
        	printf("\n--- Bridge Status [%s] ---\n", bridge.root_bridge_id == bridge.bridge_id ? "ROOT" : "NON-ROOT");
        	for (int i = 0; i < NUM_PORTS; i++) {
            		char *state_str = bridge.ports[i].state == STATE_DISCARDING ? "DISCARDING" : 
				bridge.ports[i].state == STATE_LEARNING ? "LEARNING" : "FORWARDING";
            		char state_char = bridge.ports[i].state == STATE_DISCARDING ? 'D' : 
				bridge.ports[i].state == STATE_LEARNING ? 'L' : 'F';
            
            		printf("  %s: [%c] %-10s VLANs: {", bridge.ports[i].name, state_char, state_str);
            		for (int v = 0; v < VLAN_COUNT; v++) {
                		if (bridge.ports[i].vlan_membership[v])
                    			printf("%d ", vlans[v]);
            		}
            		printf("}\n");
        	}
        	printf("-----\n");
    	}
    
    	// Cleanup
    	printf("Cleaning up...\n");
    	for (int i = 0; i < NUM_PORTS; i++) {
        	close(bridge.ports[i].socket_fd);
    	}
    	pthread_mutex_destroy(&bridge.mutex);
    
    	return 0;
}
