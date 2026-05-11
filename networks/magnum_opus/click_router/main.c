#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#define MAX_PACKET_SIZE 2048
#define RING_BUFFER_SIZE 1024
#define MAX_ELEMENTS 32
#define MAX_CONFIG_LINE 512
#define SIMULATED_PACKETS 20

typedef enum {
    	ELEM_FROM_DEVICE,
    	ELEM_CLASSIFIER,
    	ELEM_IP_LOOKUP,
    	ELEM_QUEUE,
    	ELEM_TO_DEVICE,
    	ELEM_SIMULATED_SOURCE  // Added simulated source 
} ElementType;

typedef struct Packet {
    	uint8_t data[MAX_PACKET_SIZE];
    	int length;
    	uint32_t src_ip;
    	uint32_t dst_ip;
    	uint16_t ether_type;
    	int packet_id;
} Packet;

typedef struct {
    	Packet* buffer[RING_BUFFER_SIZE];
    	int head;
    	int tail;
    	int count;
    	pthread_mutex_t mutex;
    	pthread_cond_t not_empty;
    	pthread_cond_t not_full;
} RingBuffer;

typedef struct Element {
    	ElementType type;
    	char name[64];
    
    	RingBuffer* input;
    	RingBuffer* output;
    
    	pthread_t thread;
    	int running;
    
    	union {
        	struct {
            		char device[64];
        	} from_device;
        
        	struct {
            		uint16_t ether_type;
        	} classifier;
        
        	struct {
            		struct {
                		uint32_t network;
                		int mask;
                		int port;
            		} routes[32];
            		int route_count;
        	} ip_lookup;
        
        	struct {
            		int max_size;
        	} queue;
        
        	struct {
            		char device[64];
        	} to_device;
        
        	struct {
            		int packet_count;
        	} simulated_source;
    	} data;
    
    	struct Element* next;
} Element;

// Global pipeline
typedef struct {
    	Element* head;
    	Element* tail;
    	Element* elements[MAX_ELEMENTS];
    	int element_count;
    	int running;
} Pipeline;

Pipeline* g_pipeline = NULL;

// Init ring buffer
RingBuffer* ring_buffer_create() {
    	RingBuffer* rb = (RingBuffer*)calloc(1, sizeof(RingBuffer));
    	rb->head = 0;
    	rb->tail = 0;
    	rb->count = 0;
    	pthread_mutex_init(&rb->mutex, NULL);
    	pthread_cond_init(&rb->not_empty, NULL);
    	pthread_cond_init(&rb->not_full, NULL);
    	return rb;
}

int ring_buffer_push(RingBuffer* rb, Packet* packet) {
    	pthread_mutex_lock(&rb->mutex);
    
    	while (rb->count >= RING_BUFFER_SIZE && g_pipeline && g_pipeline->running) {
        	pthread_cond_wait(&rb->not_full, &rb->mutex);
    	}
    
    	if (!g_pipeline || !g_pipeline->running) {
        	pthread_mutex_unlock(&rb->mutex);
        	return 0;
    	}
    
    	rb->buffer[rb->tail] = packet;
    	rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    	rb->count++;
    
    	pthread_cond_signal(&rb->not_empty);
    	pthread_mutex_unlock(&rb->mutex);
    	return 1;
}

Packet* ring_buffer_pop(RingBuffer* rb) {
    	pthread_mutex_lock(&rb->mutex);
    
    	while (rb->count == 0 && g_pipeline && g_pipeline->running) {
        	pthread_cond_wait(&rb->not_empty, &rb->mutex);
    	}
    
    	if (!g_pipeline || !g_pipeline->running) {
        	pthread_mutex_unlock(&rb->mutex);
        	return NULL;
    	}
    
    	Packet* packet = rb->buffer[rb->head];
    	rb->head = (rb->head + 1) % RING_BUFFER_SIZE;
    	rb->count--;
    
    	pthread_cond_signal(&rb->not_full);
    	pthread_mutex_unlock(&rb->mutex);
    	return packet;
}

// Simulation of packet generation
Packet* generate_simulated_packet(int id) {
    	// Generating different IP addresses (for routing testing)
    	static uint32_t src_ips[] = {
        	0x0a000001,  // 10.0.0.1
        	0x0a000002,
        	0x0a000003,
        	0x0a000004
    	};
    
    	static uint32_t dst_ips[] = {
        	0x0a000064,  // 10.0.0.100 (10.0.0.0/24 -> port 1)
        	0x0a000065,  // 10.0.0.101 
        	0xc0a80101,  // 192.168.1.1 (default -> порт 2)
        	0xc0a80102   // 192.168.1.2 
    	};
    
    	Packet* pkt = (Packet*)malloc(sizeof(Packet));
    	if (!pkt) return NULL;
    
    	memset(pkt, 0, sizeof(Packet));
    	pkt->packet_id = id;
    	pkt->length = 64;
    	pkt->ether_type = 0x0800; // IPv4
    	pkt->src_ip = src_ips[id % 4];
    	pkt->dst_ip = dst_ips[id % 4];
    
    	return pkt;
}

// Simulation of receiving a packet from a network device
Packet* receive_from_device(const char* device, int* should_continue) {
    	static int packet_counter = 0;
    
    	if (packet_counter >= SIMULATED_PACKETS) {
        	*should_continue = 0;
        	return NULL;
    	}
    
    	Packet* pkt = generate_simulated_packet(packet_counter);
    
    	printf("[%s] Received packet #%d: %u.%u.%u.%u -> %u.%u.%u.%u\n",
			device,
			pkt->packet_id,
			(pkt->src_ip >> 24) & 0xFF, (pkt->src_ip >> 16) & 0xFF,
			(pkt->src_ip >> 8) & 0xFF, pkt->src_ip & 0xFF,
			(pkt->dst_ip >> 24) & 0xFF, (pkt->dst_ip >> 16) & 0xFF,
			(pkt->dst_ip >> 8) & 0xFF, pkt->dst_ip & 0xFF);
    
    	packet_counter++;
    	return pkt;
}

void send_to_device(const char* device, Packet* packet) {
    	printf("[%s] Sent packet #%d: %u.%u.%u.%u -> %u.%u.%u.%u\n",
			device,
			packet->packet_id,
			(packet->src_ip >> 24) & 0xFF, 
			(packet->src_ip >> 16) & 0xFF,
			(packet->src_ip >> 8) & 0xFF, 
			packet->src_ip & 0xFF,
			(packet->dst_ip >> 24) & 0xFF, 
			(packet->dst_ip >> 16) & 0xFF,
			(packet->dst_ip >> 8) & 0xFF, 
			packet->dst_ip & 0xFF);
	free(packet);
}

// Element FromDevice
void* from_device_thread(void* arg) {
    	Element* elem = (Element*)arg;
    	RingBuffer* output = elem->output;
    	int should_continue = 1;
    
    	printf("[%s] Thread started (simulated mode)\n", elem->data.from_device.device);
    
    	while (elem->running && g_pipeline && g_pipeline->running && should_continue) {
        	Packet* packet = receive_from_device(elem->data.from_device.device, &should_continue);
        	if (!packet) break;
        
        	if (!ring_buffer_push(output, packet)) {
            		free(packet);
            		break;
        	}
        	usleep(100000); // 100ms delay
    	}
    
    	printf("[%s] Thread finished\n", elem->data.from_device.device);
    	return NULL;
}

// Element Classifier
void* classifier_thread(void* arg) {
    	Element* elem = (Element*)arg;
    	RingBuffer* input = elem->input;
    	RingBuffer* output = elem->output;
    	int accepted = 0, rejected = 0;
    
    	printf("[Classifier] Thread started (filtering ether_type=0x%04x)\n", elem->data.classifier.ether_type);
    
    	while (elem->running && g_pipeline && g_pipeline->running) {
        	Packet* packet = ring_buffer_pop(input);
        	if (!packet) continue;
        
        	if (packet->ether_type == elem->data.classifier.ether_type) {
            		accepted++;
            		printf("[Classifier] WELL Packet #%d accepted (ether_type=0x%04x)\n", packet->packet_id, packet->ether_type);
			ring_buffer_push(output, packet);
        	} else {
            		rejected++;
            		printf("[Classifier] BAD Packet #%d rejected (ether_type=0x%04x != 0x%04x)\n",
					packet->packet_id, packet->ether_type, elem->data.classifier.ether_type);
            		free(packet);
        	}
    	}
    
    	printf("[Classifier] Thread finished (accepted=%d, rejected=%d)\n", accepted, rejected);
    	return NULL;
}

// Element IPLookup
void* ip_lookup_thread(void* arg) {
    	Element* elem = (Element*)arg;
    	RingBuffer* input = elem->input;
    	RingBuffer* output = elem->output;
    
    	printf("[IPLookup] Thread started with %d routes\n", elem->data.ip_lookup.route_count);
    
    	// IP route table
    	for (int i = 0; i < elem->data.ip_lookup.route_count; i++) {
        	if (elem->data.ip_lookup.routes[i].mask == 0) {
            		printf("[IPLookup] Route: default -> port %d\n", elem->data.ip_lookup.routes[i].port);
        	} else {
            		printf("[IPLookup] Route: %u.%u.%u.%u/%d -> port %d\n",
					(elem->data.ip_lookup.routes[i].network >> 24) & 0xFF,
					(elem->data.ip_lookup.routes[i].network >> 16) & 0xFF,
					(elem->data.ip_lookup.routes[i].network >> 8) & 0xFF,
					elem->data.ip_lookup.routes[i].network & 0xFF,
					elem->data.ip_lookup.routes[i].mask,
					elem->data.ip_lookup.routes[i].port);
        	}
    	}
    
    	while (elem->running && g_pipeline && g_pipeline->running) {
        	Packet* packet = ring_buffer_pop(input);
        	if (!packet) continue;
        
        	int port = -1;
        
        	// Route search - longest prefix match
        	for (int i = 0; i < elem->data.ip_lookup.route_count; i++) {
            		uint32_t route_net = elem->data.ip_lookup.routes[i].network;
            		int mask = elem->data.ip_lookup.routes[i].mask;
            		uint32_t masked_dst = packet->dst_ip & (0xFFFFFFFF << (32 - mask));
            
            		if (mask == 0 || masked_dst == route_net) {
                		port = elem->data.ip_lookup.routes[i].port;
                		break;
            		}
        	}
        
        	printf("[IPLookup] Packet #%d: %u.%u.%u.%u -> port %d\n",
				packet->packet_id,
				(packet->dst_ip >> 24) & 0xFF, 
				(packet->dst_ip >> 16) & 0xFF,
				(packet->dst_ip >> 8) & 0xFF, 
				packet->dst_ip & 0xFF, port);
        
        	ring_buffer_push(output, packet);
    	}
    
    	printf("[IPLookup] Thread finished\n");
    	return NULL;
}

// Element Queue
void* queue_thread(void* arg) {
    	Element* elem = (Element*)arg;
    	RingBuffer* input = elem->input;
    	RingBuffer* output = elem->output;
    	int queued = 0;
    
    	printf("[Queue] Thread started (max_size=%d)\n", elem->data.queue.max_size);
    
    	while (elem->running && g_pipeline && g_pipeline->running) {
        	Packet* packet = ring_buffer_pop(input);
        	if (!packet) continue;
        
        	queued++;
        	printf("[Queue] Packet #%d queued (total queued=%d)\n", packet->packet_id, queued);
        	ring_buffer_push(output, packet);
    	}
    
    	printf("[Queue] Thread finished (processed=%d packets)\n", queued);
    	return NULL;
}

// Element ToDevice
void* to_device_thread(void* arg) {
    	Element* elem = (Element*)arg;
    	RingBuffer* input = elem->input;
    	int sent = 0;
    
    	printf("[%s] Thread started (simulated mode)\n", elem->data.to_device.device);
    
    	while (elem->running && g_pipeline && g_pipeline->running) {
        	Packet* packet = ring_buffer_pop(input);
        	if (!packet) continue;
        
        	sent++;
        	send_to_device(elem->data.to_device.device, packet);
    	}
    
    	printf("[%s] Thread finished (sent=%d packets)\n", elem->data.to_device.device, sent);
    	return NULL;
}

// Removing spaces
void trim(char* str) {
    	char* start = str;
    	char* end;
    
    	while (isspace((unsigned char)*start)) start++;
    
    	if (*start == 0) {
        	str[0] = '\0';
        	return;
    	}
    
    	end = start + strlen(start) - 1;
    	while (end > start && isspace((unsigned char)*end)) end--;
    
    	memmove(str, start, end - start + 1);
    	str[end - start + 1] = '\0';
}

Element* create_element(const char* token) {
    	Element* elem = (Element*)calloc(1, sizeof(Element));
    	if (!elem) return NULL;
    
    	elem->running = 1;
    
    	char* open_paren = strchr(token, '(');
    	if (!open_paren) {
        	free(elem);
        	return NULL;
    	}
    
    	char* close_paren = strrchr(token, ')');
    	if (!close_paren) {
        	free(elem);
        	return NULL;
    	}
    
    	int type_len = open_paren - token;
    	char type[64];
    	strncpy(type, token, type_len);
    	type[type_len] = '\0';
    	trim(type);
    
    	int params_len = close_paren - open_paren - 1;
    	char params[256];
    	strncpy(params, open_paren + 1, params_len);
    	params[params_len] = '\0';
    	trim(params);
    
    	printf("  Creating: type='%s', params='%s'\n", type, params);
    
    	if (strcmp(type, "FromDevice") == 0) {
        	elem->type = ELEM_FROM_DEVICE;
        	strcpy(elem->data.from_device.device, params);
    	}
    	else if (strcmp(type, "Classifier") == 0) {
        	elem->type = ELEM_CLASSIFIER;
        	unsigned int ether_type;
        	sscanf(params, "ether proto %x", &ether_type);
        	elem->data.classifier.ether_type = ether_type;
    	}
    	else if (strcmp(type, "IPLookup") == 0) {
        	elem->type = ELEM_IP_LOOKUP;
        
        	char* routes_str = strdup(params);
        	char* route = strtok(routes_str, ",");
        	while (route) {
            		trim(route);
            
            		if (strstr(route, "default")) {
                		int port;
                		sscanf(route, "default -> %d", &port);
                		elem->data.ip_lookup.routes[elem->data.ip_lookup.route_count].network = 0;
                		elem->data.ip_lookup.routes[elem->data.ip_lookup.route_count].mask = 0;
                		elem->data.ip_lookup.routes[elem->data.ip_lookup.route_count].port = port;
                		elem->data.ip_lookup.route_count++;
            		} else {
                		char network[32];
                		int mask, port;
                		if (sscanf(route, "%[^/]/%d -> %d", network, &mask, &port) == 3) {
                    			unsigned int a, b, c, d;
                    			sscanf(network, "%u.%u.%u.%u", &a, &b, &c, &d);
                    			uint32_t ip = (a << 24) | (b << 16) | (c << 8) | d;
                    			elem->data.ip_lookup.routes[elem->data.ip_lookup.route_count].network = ip;
                    			elem->data.ip_lookup.routes[elem->data.ip_lookup.route_count].mask = mask;
                    			elem->data.ip_lookup.routes[elem->data.ip_lookup.route_count].port = port;
                    			elem->data.ip_lookup.route_count++;
                		}
            		}
            		route = strtok(NULL, ",");
        	}
        	free(routes_str);
    	} else if (strcmp(type, "Queue") == 0) {
        	elem->type = ELEM_QUEUE;
        	sscanf(params, "%d", &elem->data.queue.max_size);
    	} else if (strcmp(type, "ToDevice") == 0) {
        	elem->type = ELEM_TO_DEVICE;
        	strcpy(elem->data.to_device.device, params);
    	} else {
        	free(elem);
        	return NULL;
    	}
    
    	return elem;
}

void pipeline_start(Pipeline* pipe) {
    	Element* current = pipe->head;
    	Element* prev = NULL;
    	int elem_index = 0;
    
    	while (current) {
        	if (current->output == NULL && current->next != NULL) {
            		current->output = ring_buffer_create();
        	}
        
        	if (prev && prev->output) {
            		current->input = prev->output;
        	}
        
        	printf("Starting element %d...\n", elem_index++);
        
        	switch (current->type) {
            		case ELEM_FROM_DEVICE:
                		pthread_create(&current->thread, NULL, from_device_thread, current);
                		break;
            		case ELEM_CLASSIFIER:
                		pthread_create(&current->thread, NULL, classifier_thread, current);
                		break;
            		case ELEM_IP_LOOKUP:
                		pthread_create(&current->thread, NULL, ip_lookup_thread, current);
                		break;
            		case ELEM_QUEUE:
                		pthread_create(&current->thread, NULL, queue_thread, current);
                		break;
            		case ELEM_TO_DEVICE:
                		pthread_create(&current->thread, NULL, to_device_thread, current);
                		break;
            		default:
                		break;
        	}
        
        	prev = current;
        	current = current->next;
    	}
}

Pipeline* load_config(const char* filename) {
    	FILE* file = fopen(filename, "r");
    	if (!file) {
        	perror("Failed to open config file");
        	return NULL;
    	}
    
    	Pipeline* pipe = (Pipeline*)calloc(1, sizeof(Pipeline));
    	if (!pipe) {
        	fclose(file);
        	return NULL;
    	}
    
    	pipe->running = 1;
    
    	char line[MAX_CONFIG_LINE];
    	if (!fgets(line, sizeof(line), file)) {
        	fclose(file);
        	free(pipe);
        	return NULL;
    	}
    
    	// deleting a newline character
    	line[strcspn(line, "\n")] = '\0';
    
    	printf("Config line: %s\n", line);
    	printf("Parsing elements:\n");
    
    	// Breaking it down by arrows
    	char* token;
    	char* rest = line;
    	Element* prev = NULL;
    
    	// Changind -> by - to make it easier
    	for (char* p = line; *p; p++) {
        	if (*p == '>') *p = '-';
    	}
    
    	while ((token = strtok_r(rest, "-", &rest))) {
        	trim(token);
        
        	if (strlen(token) == 0) continue;
        
        	printf("  Token: '%s'\n", token);
        
        	Element* elem = create_element(token);
        	if (!elem) {
            		fprintf(stderr, "  Failed to create element: %s\n", token);
            		continue;
        	}
        
        	if (!pipe->head) pipe->head = elem;
        	if (prev) prev->next = elem;
        	prev = elem;
        	pipe->elements[pipe->element_count++] = elem;
    	}
    
    	fclose(file);
    	printf("\nLoaded %d elements\n", pipe->element_count);
    	return pipe;
}

void pipeline_stop(Pipeline* pipe) {
    	if (!pipe) return;
    
    	printf("\nStopping pipeline...\n");
    	pipe->running = 0;
    
    	for (int i = 0; i < pipe->element_count; i++) {
        	if (pipe->elements[i]) {
            		pipe->elements[i]->running = 0;
        	}
    	}
    
    	// Signaling all condition variables
    	for (int i = 0; i < pipe->element_count; i++) {
        	Element* elem = pipe->elements[i];
        	if (elem && elem->input) {
            		pthread_cond_signal(&elem->input->not_empty);
            		pthread_cond_signal(&elem->input->not_full);
        	}
        	if (elem && elem->output) {
            		pthread_cond_signal(&elem->output->not_empty);
            		pthread_cond_signal(&elem->output->not_full);
        	}
    	}
    
    	for (int i = 0; i < pipe->element_count; i++) {
        	if (pipe->elements[i]) 
			pthread_join(pipe->elements[i]->thread, NULL);
    	}
}

void signal_handler(/*int sig*/) {
    	if (g_pipeline) {
        	printf("\nInterrupt received (Ctrl+C)\n");
        	pipeline_stop(g_pipeline);
    	}
    	exit(0);
}

int main(int argc, char* argv[]) {
    	const char* config_file = "router.conf";
    	if (argc > 1) {
        	config_file = argv[1];
    	}
    
    	signal(SIGINT, signal_handler);
    
    	printf("---Click Router Simulation---\n");
    	printf("Loading configuration from %s\n\n", config_file);
    
    	Pipeline* pipe = load_config(config_file);
    	if (!pipe || pipe->element_count == 0) {
        	fprintf(stderr, "Failed to load configuration\n");
        	return 1;
    	}
    
    	g_pipeline = pipe;
    
    	printf("\nStarting pipeline with %d elements...\n", pipe->element_count);
    	printf("-----\n\n");
    
    	pipeline_start(pipe);
    
    	// Waiting for all threads to finish
    	sleep(3);
    
    	// Time to process all the packages
    	sleep(2);
    
    	pipeline_stop(pipe);
    
    	printf("\n---Pipeline finished---\n");
    
    	free(pipe);
    	return 0;
}
