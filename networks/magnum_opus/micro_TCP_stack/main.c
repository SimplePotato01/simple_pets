#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/if_arp.h>
#include <ifaddrs.h>

#define BUFFER_SIZE         (64 * 1024)
#define ARP_TABLE_SIZE      16
#define ARP_TIMEOUT_SEC     300
#define TCP_WINDOW          65535
#define RETRANSMIT_USEC     100000
#define MAX_TCP_SOCKETS     8
#define MAX_FRAGMENTS       16
#define MAX_PACKET_SIZE     2048
#define ARP_LOCALHOST       -2

#define DEBUG_TCP 1

#if DEBUG_TCP
#define debug_print(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug_print(fmt, ...)
#endif

typedef struct {
    	uint32_t ip;
    	uint8_t mac[6];
    	time_t last_seen;
    	int valid;
} arp_entry_t;

typedef struct {
    	uint8_t data[BUFFER_SIZE];
    	int len;
    	int sent_bytes;
    	uint32_t seq, ack;
    	uint8_t flags;
    	time_t last_send;
} tcp_buffer_t;

typedef struct {
    	int state;  // 0=CLOSED, 1=LISTEN, 2=SYN_SENT, 3=SYN_RCVD, 4=ESTAB, 5=FIN_WAIT
    	uint32_t local_ip, remote_ip;
    	uint16_t local_port, remote_port;
    	uint32_t seq, ack;
    	tcp_buffer_t tx_buf, rx_buf;
    	pthread_mutex_t lock;
} tcp_socket_t;

typedef struct {
    	uint16_t id;
    	int total_len;
    	int received;
    	uint8_t *buffer;
    	time_t timestamp;
    	int in_progress;
} ip_fragment_t;

void icmp_handle(uint8_t *packet, int len);
void tcp_handle(uint8_t *packet, int len);
void process_packet(uint8_t *buffer, int len);

static int raw_sock;
static int raw_sock_lo;
static char ifname[16];
static uint8_t my_mac[6];
static uint32_t my_ip;
static arp_entry_t arp_table[ARP_TABLE_SIZE];
static tcp_socket_t tcp_sockets[MAX_TCP_SOCKETS];
static ip_fragment_t fragments[MAX_FRAGMENTS];
static pthread_mutex_t arp_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t frag_lock = PTHREAD_MUTEX_INITIALIZER;

// UTILS
uint16_t checksum(uint16_t *ptr, int nbytes) {
    	uint32_t sum = 0;
    	while (nbytes > 1) { 
        	sum += *ptr++; 
        	nbytes -= 2; 
    	}
    	if (nbytes) 
        	sum += *(uint8_t*)ptr;
    	while (sum >> 16) 
        	sum = (sum & 0xFFFF) + (sum >> 16);
    	return ~sum;
}

void send_raw_frame(int sock, uint8_t *buf, int len, int ifindex, uint8_t *dst_mac) {
    	struct sockaddr_ll dest;
    	memset(&dest, 0, sizeof(dest));
    	dest.sll_family = AF_PACKET;
    	dest.sll_ifindex = ifindex;
    	dest.sll_halen = ETH_ALEN;
    	memcpy(dest.sll_addr, dst_mac, ETH_ALEN);
    
    	if (sendto(sock, buf, len, 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        	perror("sendto");
    	}
}

// ARP
void arp_request(uint32_t target_ip) {
    	uint8_t packet[64];
    	struct ethhdr *eth = (struct ethhdr*)packet;
    	struct arphdr *arp = (struct arphdr*)(packet + sizeof(struct ethhdr));
    	uint8_t *arp_data = (uint8_t*)(packet + sizeof(struct ethhdr) + sizeof(struct arphdr));
    
    	memset(eth->h_dest, 0xFF, ETH_ALEN);
    	memcpy(eth->h_source, my_mac, ETH_ALEN);
    	eth->h_proto = htons(ETH_P_ARP);
    
    	arp->ar_hrd = htons(ARPHRD_ETHER);
    	arp->ar_pro = htons(ETH_P_IP);
    	arp->ar_hln = 6;
    	arp->ar_pln = 4;
    	arp->ar_op = htons(ARPOP_REQUEST);
    
    	memcpy(arp_data, my_mac, 6);
    	memcpy(arp_data + 6, &my_ip, 4);
    	memset(arp_data + 10, 0, 6);
    	memcpy(arp_data + 16, &target_ip, 4);
    
    	struct ifreq ifr;
    	strcpy(ifr.ifr_name, ifname);
    	ioctl(raw_sock, SIOCGIFINDEX, &ifr);
    	send_raw_frame(raw_sock, packet, sizeof(struct ethhdr) + sizeof(struct arphdr) + 20, ifr.ifr_ifindex, eth->h_dest);
}

int arp_resolve(uint32_t ip) {
    	if (ip == my_ip || ip == htonl(INADDR_LOOPBACK)) {
        	return ARP_LOCALHOST;
    	}
    
    	pthread_mutex_lock(&arp_lock);
    	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        	if (arp_table[i].valid && arp_table[i].ip == ip && 
            		time(NULL) - arp_table[i].last_seen < ARP_TIMEOUT_SEC) {
            		pthread_mutex_unlock(&arp_lock);
            		return i;
        	}
    	}
    	pthread_mutex_unlock(&arp_lock);
    
    	arp_request(ip);
    	for (int wait = 0; wait < 20; wait++) {
        	usleep(10000);
        	pthread_mutex_lock(&arp_lock);
        	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
            		if (arp_table[i].valid && arp_table[i].ip == ip) {
                		pthread_mutex_unlock(&arp_lock);
                		return i;
            		}
        	}
        	pthread_mutex_unlock(&arp_lock);
    	}
    	return -1;
}

void handle_arp(uint8_t *packet, int len) {
    	(void)len;
    	struct ethhdr *eth = (struct ethhdr*)packet;
    	struct arphdr *arp = (struct arphdr*)(packet + sizeof(struct ethhdr));
    	uint8_t *arp_data = (uint8_t*)(packet + sizeof(struct ethhdr) + sizeof(struct arphdr));
    
    	if (arp->ar_op == htons(ARPOP_REQUEST)) {
        	uint32_t target_ip;
        	memcpy(&target_ip, arp_data + 16, 4);
        
        	if (target_ip == my_ip) {
            		uint8_t reply[64];
            		struct ethhdr *reth = (struct ethhdr*)reply;
            		struct arphdr *rarph = (struct arphdr*)(reply + sizeof(struct ethhdr));
            		uint8_t *rarp_data = (uint8_t*)(reply + sizeof(struct ethhdr) + sizeof(struct arphdr));
            
            		memcpy(reth->h_dest, eth->h_source, ETH_ALEN);
            		memcpy(reth->h_source, my_mac, ETH_ALEN);
            		reth->h_proto = htons(ETH_P_ARP);
            
            		*rarph = *arp;
            		rarph->ar_op = htons(ARPOP_REPLY);
            
            		memcpy(rarp_data, my_mac, 6);
            		memcpy(rarp_data + 6, &my_ip, 4);
            		memcpy(rarp_data + 10, arp_data, 6);
            		memcpy(rarp_data + 16, arp_data + 6, 4);
            
            		struct ifreq ifr;
            		strcpy(ifr.ifr_name, ifname);
            		ioctl(raw_sock, SIOCGIFINDEX, &ifr);
            		send_raw_frame(raw_sock, reply, sizeof(struct ethhdr) + sizeof(struct arphdr) + 20, ifr.ifr_ifindex, reth->h_dest);
            		debug_print("ARP: Sent reply to %08x\n", target_ip);
        	}
    	}
    
    	uint32_t src_ip;
    	memcpy(&src_ip, arp_data + 6, 4);
    	uint8_t *src_mac = arp_data;
    
    	pthread_mutex_lock(&arp_lock);
    	for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        	if (!arp_table[i].valid || arp_table[i].ip == src_ip) {
            		arp_table[i].ip = src_ip;
            		memcpy(arp_table[i].mac, src_mac, 6);
            		arp_table[i].last_seen = time(NULL);
            		arp_table[i].valid = 1;
            		break;
        	}
    	}
    	pthread_mutex_unlock(&arp_lock);
}

// IP Fragmentation/Defrag
void ip_defrag_process(struct iphdr *iph, uint8_t *payload, int len) {
    	uint16_t frag_off = ntohs(iph->frag_off);
    	int offset = (frag_off & 0x1FFF) * 8;
    	int more = frag_off & 0x2000;
    	uint16_t ip_id = ntohs(iph->id);
    
    	pthread_mutex_lock(&frag_lock);
    	ip_fragment_t *f = NULL;
    	for (int i = 0; i < MAX_FRAGMENTS; i++) {
        	if (fragments[i].in_progress && fragments[i].id == ip_id) {
            		f = &fragments[i];
            		break;
        	}
    	}
    	if (!f && offset == 0) {
        	for (int i = 0; i < MAX_FRAGMENTS; i++) {
            		if (!fragments[i].in_progress) {
                		f = &fragments[i];
                		f->id = ip_id;
                		f->total_len = ntohs(iph->tot_len) - (iph->ihl*4) + offset + len;
                		f->received = 0;
                		f->buffer = malloc(f->total_len);
                		f->timestamp = time(NULL);
                		f->in_progress = 1;
                		break;
            		}
        	}
    	}
    	if (f && offset + len <= f->total_len) {
        	memcpy(f->buffer + offset, payload, len);
        	f->received += len;
        	if (!more && f->received >= f->total_len) {
            		uint8_t *whole = malloc(f->total_len + (iph->ihl*4));
            		memcpy(whole, iph, iph->ihl*4);
            		memcpy(whole + (iph->ihl*4), f->buffer, f->total_len);
            		uint8_t proto = iph->protocol;
            		if (proto == IPPROTO_ICMP) 
				icmp_handle(whole, f->total_len + (iph->ihl*4));
            		else if (proto == IPPROTO_TCP) 
				tcp_handle(whole, f->total_len + (iph->ihl*4));
            		free(whole);
            		free(f->buffer);
            		f->in_progress = 0;
        	}
    	}
    	pthread_mutex_unlock(&frag_lock);
}

// ICMP
void icmp_handle(uint8_t *packet, int len) {
    	// struct ethhdr *eth = (struct ethhdr*)packet;
    	struct iphdr *iph = (struct iphdr*)(packet + sizeof(struct ethhdr));
    	struct icmphdr *icmp = (struct icmphdr*)(packet + sizeof(struct ethhdr) + (iph->ihl*4));
    
    	if (icmp->type == ICMP_ECHO) {
        	uint8_t reply[MAX_PACKET_SIZE];
        	struct ethhdr *reth = (struct ethhdr*)reply;
        	struct iphdr *rip = (struct iphdr*)(reply + sizeof(struct ethhdr));
        	struct icmphdr *ricmp = (struct icmphdr*)(reply + sizeof(struct ethhdr) + sizeof(struct iphdr));
        
        	int arp_idx = arp_resolve(iph->saddr);
        	if (arp_idx < 0 && arp_idx != ARP_LOCALHOST) return;
        
        	// Defining the interface for sending
        	int use_loopback = (iph->saddr == my_ip || iph->saddr == htonl(INADDR_LOOPBACK));
        
        	if (use_loopback) {
            		memcpy(reth->h_dest, my_mac, ETH_ALEN);
        	} else if (arp_idx >= 0) {
            		memcpy(reth->h_dest, arp_table[arp_idx].mac, ETH_ALEN);
        	} else {
            		return;
        	}
        
        	memcpy(reth->h_source, my_mac, ETH_ALEN);
        	reth->h_proto = htons(ETH_P_IP);
        
        	rip->version = 4;
        	rip->ihl = 5;
        	rip->tos = 0;
        	int icmp_len = len - sizeof(struct ethhdr) - (iph->ihl*4);
        	rip->tot_len = htons(sizeof(struct iphdr) + icmp_len);
        	rip->id = 0;
        	rip->frag_off = 0;
        	rip->ttl = 64;
        	rip->protocol = IPPROTO_ICMP;
        	rip->saddr = my_ip;
        	rip->daddr = iph->saddr;
        	rip->check = 0;
        	rip->check = checksum((uint16_t*)rip, sizeof(struct iphdr));
        
        	memcpy(ricmp, icmp, icmp_len);
        	ricmp->type = ICMP_ECHOREPLY;
        	ricmp->checksum = 0;
        	ricmp->checksum = checksum((uint16_t*)ricmp, icmp_len);
        
        	struct ifreq ifr;
        	if (use_loopback) {
            		strcpy(ifr.ifr_name, "lo");
            		ioctl(raw_sock_lo, SIOCGIFINDEX, &ifr);
            		send_raw_frame(raw_sock_lo, reply, sizeof(struct ethhdr) + sizeof(struct iphdr) + icmp_len, ifr.ifr_ifindex, reth->h_dest);
        	} else {
            		strcpy(ifr.ifr_name, ifname);
            		ioctl(raw_sock, SIOCGIFINDEX, &ifr);
            		send_raw_frame(raw_sock, reply, sizeof(struct ethhdr) + sizeof(struct iphdr) + icmp_len, ifr.ifr_ifindex, reth->h_dest);
        	}
        
        	debug_print("ICMP: Sent reply to %s\n", inet_ntoa(*(struct in_addr*)&iph->saddr));
    	}
}

// TCP
void tcp_send_segment(tcp_socket_t *sock, uint8_t flags, uint32_t seq, uint32_t ack, uint8_t *data, int datalen) {
    	uint8_t packet[MAX_PACKET_SIZE];
    	struct ethhdr *eth = (struct ethhdr*)packet;
    	struct iphdr *ip = (struct iphdr*)(packet + sizeof(struct ethhdr));
    	struct tcphdr *tcp = (struct tcphdr*)(packet + sizeof(struct ethhdr) + sizeof(struct iphdr));
    
    	int use_loopback = (sock->remote_ip == my_ip || sock->remote_ip == htonl(INADDR_LOOPBACK));
    	int arp_idx = -1;
    
    	if (!use_loopback) {
        	arp_idx = arp_resolve(sock->remote_ip);
        	if (arp_idx < 0) {
            		debug_print("TCP: ARP resolution failed\n");
            		return;
        	}
    	}
    
    	if (use_loopback) {
        	memcpy(eth->h_dest, my_mac, ETH_ALEN);
    	} else {
        	memcpy(eth->h_dest, arp_table[arp_idx].mac, ETH_ALEN);
    	}
    	memcpy(eth->h_source, my_mac, ETH_ALEN);
    	eth->h_proto = htons(ETH_P_IP);
    
    	ip->version = 4;
    	ip->ihl = 5;
    	ip->tos = 0;
    	ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr) + datalen);
    	ip->id = rand() & 0xFFFF;
    	ip->frag_off = 0;
    	ip->ttl = 64;
    	ip->protocol = IPPROTO_TCP;
    	ip->saddr = my_ip;
    	ip->daddr = sock->remote_ip;
    	ip->check = 0;
    	ip->check = checksum((uint16_t*)ip, sizeof(struct iphdr));
    
    	tcp->source = htons(sock->local_port);
    	tcp->dest = htons(sock->remote_port);
    	tcp->seq = htonl(seq);
    	tcp->ack_seq = htonl(ack);
    	tcp->doff = 5;
    	tcp->res1 = 0;
    	tcp->cwr = tcp->ece = tcp->urg = 0;
    	tcp->ack = (flags & 0x10) ? 1 : 0;
    	tcp->psh = (flags & 0x08) ? 1 : 0;
    	tcp->rst = (flags & 0x04) ? 1 : 0;
    	tcp->syn = (flags & 0x02) ? 1 : 0;
    	tcp->fin = (flags & 0x01) ? 1 : 0;
    	tcp->window = htons(TCP_WINDOW);
    	tcp->urg_ptr = 0;
    
    	struct {
        	uint32_t src, dst;
        	uint8_t zero, proto;
        	uint16_t len;
    	} ph;
    	ph.src = my_ip;
    	ph.dst = sock->remote_ip;
    	ph.zero = 0;
    	ph.proto = IPPROTO_TCP;
    	ph.len = htons(sizeof(struct tcphdr) + datalen);
    
    	int csum_len = sizeof(ph) + sizeof(struct tcphdr) + datalen;
    	uint8_t *csum_buf = malloc(csum_len);
    	if (!csum_buf) return;
    
    	memcpy(csum_buf, &ph, sizeof(ph));
    	memcpy(csum_buf + sizeof(ph), tcp, sizeof(struct tcphdr));
    	if (data && datalen > 0) {
        	memcpy(csum_buf + sizeof(ph) + sizeof(struct tcphdr), data, datalen);
    	}
    	tcp->check = 0;
    	tcp->check = checksum((uint16_t*)csum_buf, csum_len);
    	free(csum_buf);
    
    	if (data && datalen > 0) {
        	uint8_t *payload = packet + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct tcphdr);
        	memcpy(payload, data, datalen);
    	}
    
    	struct ifreq ifr;
    	if (use_loopback) {
        	strcpy(ifr.ifr_name, "lo");
        	ioctl(raw_sock_lo, SIOCGIFINDEX, &ifr);
        	send_raw_frame(raw_sock_lo, packet, sizeof(struct ethhdr) + ntohs(ip->tot_len), ifr.ifr_ifindex, eth->h_dest);
    	} else {
        	strcpy(ifr.ifr_name, ifname);
        	ioctl(raw_sock, SIOCGIFINDEX, &ifr);
        	send_raw_frame(raw_sock, packet, sizeof(struct ethhdr) + ntohs(ip->tot_len), ifr.ifr_ifindex, eth->h_dest);
    	}
    
    	debug_print("TCP: Sent segment flags=%02x seq=%u ack=%u len=%d\n", flags, seq, ack, datalen);
}

void *tcp_retransmit_thread(void *arg) {
    	(void)arg;
    	while (1) {
        	usleep(RETRANSMIT_USEC);
        	for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
            		tcp_socket_t *s = &tcp_sockets[i];
            		pthread_mutex_lock(&s->lock);
            		if (s->state == 4 && 
					s->tx_buf.len > 0 && 
					s->tx_buf.sent_bytes < s->tx_buf.len && 
					time(NULL) - s->tx_buf.last_send > RETRANSMIT_USEC / 1000000) {
                		int remaining = s->tx_buf.len - s->tx_buf.sent_bytes;
                		tcp_send_segment(s, 0x18, s->tx_buf.seq + s->tx_buf.sent_bytes, s->ack, s->tx_buf.data + s->tx_buf.sent_bytes, remaining);
                		s->tx_buf.last_send = time(NULL);
            		}
            		pthread_mutex_unlock(&s->lock);
        	}
    	}
    	return NULL;
}

void tcp_handle(uint8_t *packet, int len) {
    	// struct ethhdr *eth = (struct ethhdr*)packet;
    	struct iphdr *ip = (struct iphdr*)(packet + sizeof(struct ethhdr));
    	struct tcphdr *tcp = (struct tcphdr*)(packet + sizeof(struct ethhdr) + (ip->ihl*4));
    	uint8_t *payload = (uint8_t*)tcp + (tcp->doff * 4);
    	int payload_len = len - (sizeof(struct ethhdr) + (ip->ihl*4) + (tcp->doff * 4));
    
    	uint16_t src_port = ntohs(tcp->source);
    	uint16_t dst_port = ntohs(tcp->dest);
    	uint32_t seq = ntohl(tcp->seq);
    	uint32_t ack_num = ntohl(tcp->ack_seq);
    
    	printf("TCP: %s:%d -> %s:%d ", 
			inet_ntoa(*(struct in_addr*)&ip->saddr), src_port,
			inet_ntoa(*(struct in_addr*)&ip->daddr), dst_port);
    	printf("flags: %s%s%s%s%s seq=%u ack=%u len=%d\n",
			tcp->syn ? "SYN " : "",
			tcp->ack ? "ACK " : "",
			tcp->fin ? "FIN " : "",
			tcp->rst ? "RST " : "",
			tcp->psh ? "PSH " : "",
			seq, ack_num, payload_len);
    
    	if (dst_port != 1234) return;
    
    	tcp_socket_t *sock = NULL;
    
    	// Find existing connection
    	for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
        	pthread_mutex_lock(&tcp_sockets[i].lock);
        	if (tcp_sockets[i].state > 1 && 
				tcp_sockets[i].local_port == dst_port &&
				tcp_sockets[i].remote_ip == ip->saddr && 
				tcp_sockets[i].remote_port == src_port) {
			sock = &tcp_sockets[i];
            		pthread_mutex_unlock(&tcp_sockets[i].lock);
            		break;
        	}
        	pthread_mutex_unlock(&tcp_sockets[i].lock);
    	}
    
    	// Retransmitted SYN
    	if (!sock && tcp->syn && !tcp->ack) {
        	for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
            		pthread_mutex_lock(&tcp_sockets[i].lock);
            		if (tcp_sockets[i].state == 3 && 
					tcp_sockets[i].local_port == dst_port &&
					tcp_sockets[i].remote_ip == ip->saddr && 
					tcp_sockets[i].remote_port == src_port) {
				sock = &tcp_sockets[i];
                		pthread_mutex_unlock(&tcp_sockets[i].lock);
                		break;
            		}
            		pthread_mutex_unlock(&tcp_sockets[i].lock);
        	}
        	if (sock) {
            		printf("TCP: Retransmitted SYN, resending SYN+ACK\n");
            		tcp_send_segment(sock, 0x12, sock->seq, sock->ack, NULL, 0);
            		return;
        	}
    	}
    
    	// New connection
    	if (!sock && tcp->syn && !tcp->ack) {
        	printf("TCP: New SYN connection request on port %d from %s:%d\n", dst_port, inet_ntoa(*(struct in_addr*)&ip->saddr), src_port);
        
        	int new_sock_index = -1;
        	for (int i = 1; i < MAX_TCP_SOCKETS; i++) {
            		pthread_mutex_lock(&tcp_sockets[i].lock);
            		if (tcp_sockets[i].state == 0) {
                		new_sock_index = i;
                		pthread_mutex_unlock(&tcp_sockets[i].lock);
                		break;
            	}
            	pthread_mutex_unlock(&tcp_sockets[i].lock);
        	}
        
        	if (new_sock_index == -1) {
            		printf("TCP: No free sockets, sending RST\n");
            		tcp_socket_t tmp_sock;
            		memset(&tmp_sock, 0, sizeof(tmp_sock));
            		tmp_sock.local_port = dst_port;
            		tmp_sock.remote_ip = ip->saddr;
            		tmp_sock.remote_port = src_port;
            		tmp_sock.seq = 0;
            		tmp_sock.ack = seq + 1;
            		tcp_send_segment(&tmp_sock, 0x04, 0, tmp_sock.ack, NULL, 0);
            		return;
        	}
        
        	pthread_mutex_lock(&tcp_sockets[new_sock_index].lock);
        	tcp_sockets[new_sock_index].state = 3;
        	tcp_sockets[new_sock_index].local_ip = my_ip;
        	tcp_sockets[new_sock_index].local_port = dst_port;
        	tcp_sockets[new_sock_index].remote_ip = ip->saddr;
        	tcp_sockets[new_sock_index].remote_port = src_port;
        	tcp_sockets[new_sock_index].seq = rand();
        	tcp_sockets[new_sock_index].ack = seq + 1;
        	memset(&tcp_sockets[new_sock_index].tx_buf, 0, sizeof(tcp_buffer_t));
        	memset(&tcp_sockets[new_sock_index].rx_buf, 0, sizeof(tcp_buffer_t));
        
        	printf("TCP: Created new socket %d, sending SYN+ACK (seq=%u, ack=%u)\n", 
				new_sock_index, tcp_sockets[new_sock_index].seq, tcp_sockets[new_sock_index].ack);
		tcp_send_segment(&tcp_sockets[new_sock_index], 0x12, 
				tcp_sockets[new_sock_index].seq, 
				tcp_sockets[new_sock_index].ack, NULL, 0);
		pthread_mutex_unlock(&tcp_sockets[new_sock_index].lock);
        	return;
    	}
    
    	if (!sock) return;
    
    	pthread_mutex_lock(&sock->lock);
    
    	if (tcp->rst) {
        	printf("TCP: RST received, closing connection\n");
        	sock->state = 0;
        	pthread_mutex_unlock(&sock->lock);
        	return;
    	}
    
    	if (sock->state == 3 && tcp->syn && tcp->ack) {
        	sock->state = 4;
        	sock->seq = ack_num;
        	sock->ack = seq + 1;
        	printf("TCP: Handshake complete, ESTABLISHED\n");
        	pthread_mutex_unlock(&sock->lock);
        	return;
    	}
    
    	if (sock->state == 3 && tcp->ack && !tcp->syn) {
        	sock->state = 4;
        	sock->seq = ack_num;
        	sock->ack = seq;
        	printf("TCP: Handshake complete via ACK, ESTABLISHED\n");
        	pthread_mutex_unlock(&sock->lock);
        	return;
    	}
    
    	if (sock->state == 4) {
        	if (payload_len > 0 || tcp->fin) {
            		if (seq == sock->ack) {
                		sock->ack = seq + payload_len;
                		if (payload_len > 0) {
                    			if (sock->rx_buf.len + payload_len <= BUFFER_SIZE) {
                        			memcpy(sock->rx_buf.data + sock->rx_buf.len, payload, payload_len);
                        			sock->rx_buf.len += payload_len;
                        			printf("TCP: Received %d bytes, echoing\n", payload_len);
                    			}
                    			tcp_send_segment(sock, 0x10, sock->seq, sock->ack, NULL, 0);
                    			tcp_send_segment(sock, 0x18, sock->seq, sock->ack, payload, payload_len);
                    			sock->seq += payload_len;
                		}
                		
				if (tcp->fin) {
                    			printf("TCP: FIN received, closing\n");
                    			sock->ack = seq + payload_len + 1;
                    			tcp_send_segment(sock, 0x11, sock->seq, sock->ack, NULL, 0);
                    			sock->state = 0;
                		}
            		} else {
                		printf("TCP: Seq mismatch exp=%u got=%u\n", sock->ack, seq);
                		tcp_send_segment(sock, 0x10, sock->seq, sock->ack, NULL, 0);
            		}
        	}
    	}
    
    	pthread_mutex_unlock(&sock->lock);
}

// PACKET PROCESSING
void process_packet(uint8_t *buffer, int len) {
    	struct ethhdr *eth = (struct ethhdr*)buffer;
    	if (ntohs(eth->h_proto) == ETH_P_ARP) {
        	handle_arp(buffer, len);
    	} else if (ntohs(eth->h_proto) == ETH_P_IP) {
        	struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
        
        	if (ip->daddr != my_ip && ip->daddr != htonl(INADDR_LOOPBACK)) {
            		return;
        	}
        
        	uint16_t frag_off = ntohs(ip->frag_off);
        	if ((frag_off & 0x3FFF) == 0) {
            		if (ip->protocol == IPPROTO_ICMP) {
                		icmp_handle(buffer, len);
            		} else if (ip->protocol == IPPROTO_TCP) {
                		tcp_handle(buffer, len);
            		}
        	} else {
            		ip_defrag_process(ip, buffer + sizeof(struct ethhdr) + (ip->ihl*4), len - (sizeof(struct ethhdr) + (ip->ihl*4)));
        	}
    	}
}

// HELPER FUNCTIONS
int find_network_interface(char *iface, size_t iface_size) {
    	struct ifaddrs *ifaddr, *ifa;
    	int found = 0;
    
    	if (getifaddrs(&ifaddr) == -1) {
        	return -1;
    	}
    
    	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        	if (ifa->ifa_addr == NULL) continue;
        
        	if (ifa->ifa_addr->sa_family == AF_INET && 
				!(ifa->ifa_flags & IFF_LOOPBACK) &&
				(ifa->ifa_flags & IFF_UP) &&
				(ifa->ifa_flags & IFF_RUNNING)) {
			strncpy(iface, ifa->ifa_name, iface_size - 1);
            		iface[iface_size - 1] = '\0';
            		found = 1;
            		break;
        	}
    	}
    
    	freeifaddrs(ifaddr);
    	return found ? 0 : -1;
}

// Main
int main(int argc, char **argv) {
    	if (argc == 2) {
        	strcpy(ifname, argv[1]);
    	} else {
        	if (find_network_interface(ifname, sizeof(ifname)) < 0) {
            		fprintf(stderr, "Could not find active network interface\n");
            		fprintf(stderr, "Usage: %s <iface>\n", argv[0]);
            		exit(1);
        	}
        	printf("Auto-detected interface: %s\n", ifname);
    	}
    
    	struct ifreq ifr;
    	raw_sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (raw_sock < 0) {
        	perror("socket");
        	exit(1);
    	}
    
    	strcpy(ifr.ifr_name, ifname);
    	if (ioctl(raw_sock, SIOCGIFHWADDR, &ifr) < 0) {
        	perror("ioctl MAC");
        	close(raw_sock);
        	exit(1);
    	}
    	memcpy(my_mac, ifr.ifr_hwaddr.sa_data, 6);
    
    	int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    	if (udp_sock < 0) {
        	perror("socket UDP");
        	close(raw_sock);
        	exit(1);
    	}
    
    	ifr.ifr_addr.sa_family = AF_INET;
    	strcpy(ifr.ifr_name, ifname);
    	if (ioctl(udp_sock, SIOCGIFADDR, &ifr) < 0) {
        	perror("ioctl IP");
        	close(udp_sock);
        	close(raw_sock);
        	exit(1);
    	}
    	my_ip = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
    	close(udp_sock);
    
    	raw_sock_lo = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	if (raw_sock_lo < 0) {
        	perror("socket lo");
        	raw_sock_lo = -1;
    	} else {
        	struct ifreq ifr_lo;
        	strcpy(ifr_lo.ifr_name, "lo");
        	if (ioctl(raw_sock_lo, SIOCGIFINDEX, &ifr_lo) == 0) {
            		struct sockaddr_ll sll;
            		memset(&sll, 0, sizeof(sll));
            		sll.sll_family = AF_PACKET;
            		sll.sll_ifindex = ifr_lo.ifr_ifindex;
            		sll.sll_protocol = htons(ETH_P_ALL);
            
            		if (bind(raw_sock_lo, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
                		perror("bind lo");
                		close(raw_sock_lo);
                		raw_sock_lo = -1;
            		} else {
                		printf("And also listening on loopback interface\n");
            		}
        	} else {
            		close(raw_sock_lo);
            		raw_sock_lo = -1;
        	}
    	}
    
    	printf("Interface: %s\n", ifname);
    	printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
			my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
    	printf("IP: %s\n", inet_ntoa(*(struct in_addr*)&my_ip));
    	printf("Listening on port 1234...\n");
    
    	memset(tcp_sockets, 0, sizeof(tcp_sockets));
    	for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
        	tcp_sockets[i].state = 0;
        	pthread_mutex_init(&tcp_sockets[i].lock, NULL);
    	}
    	tcp_sockets[0].state = 1; // LISTEN
    	tcp_sockets[0].local_port = 1234;
    	tcp_sockets[0].local_ip = my_ip;
    
    	memset(fragments, 0, sizeof(fragments));
    
    	pthread_t retrans_thread;
    	pthread_create(&retrans_thread, NULL, tcp_retransmit_thread, NULL);
    
    	uint8_t buffer[MAX_PACKET_SIZE];
    	fd_set read_fds;
    	int max_fd = raw_sock;
    	if (raw_sock_lo > max_fd) max_fd = raw_sock_lo;
    
    	while (1) {
        	FD_ZERO(&read_fds);
        	FD_SET(raw_sock, &read_fds);
        	if (raw_sock_lo >= 0) {
            		FD_SET(raw_sock_lo, &read_fds);
        	}
        
        	if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            		continue;
        	}
        
        	if (FD_ISSET(raw_sock, &read_fds)) {
            		int len = recv(raw_sock, buffer, sizeof(buffer), 0);
            		if (len > 0) {
                		process_packet(buffer, len);
            		}
        	}
        
        	if (raw_sock_lo >= 0 && FD_ISSET(raw_sock_lo, &read_fds)) {
            		int len = recv(raw_sock_lo, buffer, sizeof(buffer), 0);
            		if (len > 0) {
                		process_packet(buffer, len);
            		}
        	}
    	}
    
    	close(raw_sock);
    	if (raw_sock_lo >= 0) close(raw_sock_lo);
    	return 0;
}
