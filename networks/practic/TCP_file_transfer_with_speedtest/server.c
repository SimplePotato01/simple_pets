// server.c (исправленная версия)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <endian.h>

#define BUFFER_SIZE 8192
#define UPLOAD_DIR "uploads"
#define SPEED_INTERVAL 3  // seconds

typedef struct {
    	int client_fd;
    	struct sockaddr_in addr;
} client_info_t;

typedef struct {
    	int client_fd;
    	pthread_t thread_id;
    	struct timespec start_time;
    	struct timespec last_print_time;
    	uint64_t total_bytes_received;
    	uint64_t last_bytes_received;
    	int active;
    	int finished;
    	pthread_mutex_t mutex;
} client_stats_t;

#define MAX_CLIENTS 100
client_stats_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void ensure_upload_dir() {
    	struct stat st = {0};
    	if (stat(UPLOAD_DIR, &st) == -1) {
        	mkdir(UPLOAD_DIR, 0755);
    	}
}

void print_speed(client_stats_t *client, struct timespec *now) {
    	double elapsed_total = (now->tv_sec - client->start_time.tv_sec) + (now->tv_nsec - client->start_time.tv_nsec) / 1e9;
    	double elapsed_interval = (now->tv_sec - client->last_print_time.tv_sec) + (now->tv_nsec - client->last_print_time.tv_nsec) / 1e9;
    
    	if (elapsed_interval < 0.001) elapsed_interval = 0.001;
    
    	uint64_t bytes_interval = client->total_bytes_received - client->last_bytes_received;
    	double instant_speed = bytes_interval / elapsed_interval;
    	double avg_speed = client->total_bytes_received / elapsed_total;
    
    	printf("[Client %d] Instant speed: %.2f B/s (%.2f KB/s, %.2f MB/s), Average speed: %.2f B/s (%.2f KB/s, %.2f MB/s)\n",
		       	client->client_fd, instant_speed, instant_speed/1024, instant_speed/(1024*1024), avg_speed, avg_speed/1024, avg_speed/(1024*1024));
    
    	client->last_bytes_received = client->total_bytes_received;
    	clock_gettime(CLOCK_MONOTONIC, &client->last_print_time);
}

void *speed_printer(void *arg) {
    	(void)arg;
    	while (1) {
        	sleep(SPEED_INTERVAL);
        	pthread_mutex_lock(&clients_mutex);
        	for (int i = 0; i < MAX_CLIENTS; i++) {
            		if (clients[i] != NULL && (clients[i]->active || clients[i]->finished)) {
                		pthread_mutex_lock(&clients[i]->mutex);
                		struct timespec now;
                		clock_gettime(CLOCK_MONOTONIC, &now);
                
                		double elapsed_interval = (now.tv_sec - clients[i]->last_print_time.tv_sec) + (now.tv_nsec - clients[i]->last_print_time.tv_nsec) / 1e9;
                
                		// Print if interval >= 3 seconds OR if transfer just finished
                		if (elapsed_interval >= SPEED_INTERVAL - 0.01 || (clients[i]->finished && clients[i]->total_bytes_received > clients[i]->last_bytes_received)) {
                    			print_speed(clients[i], &now);
                		}
                		pthread_mutex_unlock(&clients[i]->mutex);
            		}
        	}
        	pthread_mutex_unlock(&clients_mutex);
    	}
    	return NULL;
}

void *handle_client(void *arg) {
    	client_info_t *info = (client_info_t *)arg;
    	int fd = info->client_fd;
    	char filename[4096];
    	uint32_t name_len;
    	uint64_t file_size;
    	uint64_t received = 0;
    	char safe_path[8192];
    
    	// Find or create stats entry
    	pthread_mutex_lock(&clients_mutex);
    	int stats_idx = -1;
    	for (int i = 0; i < MAX_CLIENTS; i++) {
        	if (clients[i] == NULL) {
            		// TODO think may it could be easier
			clients[i] = calloc(1, sizeof(client_stats_t));
            		clients[i]->client_fd = fd;
            		clients[i]->active = 1;
            		clients[i]->finished = 0;
            		clients[i]->total_bytes_received = 0;
            		clients[i]->last_bytes_received = 0;
            		clock_gettime(CLOCK_MONOTONIC, &clients[i]->start_time);
            		clients[i]->last_print_time = clients[i]->start_time;
            		pthread_mutex_init(&clients[i]->mutex, NULL);
            		stats_idx = i;
            		break;
        	}
    	}
    	pthread_mutex_unlock(&clients_mutex);
    
    	if (stats_idx == -1) {
        	close(fd);
        	free(info);
        	return NULL;
    	}
    
    	// Receive name length
    	if (recv(fd, &name_len, sizeof(name_len), 0) != sizeof(name_len)) {
        	goto error;
    	}
    	name_len = ntohl(name_len);
    	if (name_len > 4096) goto error;
    
    	// Receive filename
    	if (recv(fd, filename, name_len, 0) != name_len) goto error;
    	filename[name_len] = '\0';
    
    	// Receive file size
    	if (recv(fd, &file_size, sizeof(file_size), 0) != sizeof(file_size)) goto error;
    	file_size = be64toh(file_size);
    
    	printf("[Client %d] Receiving file: %s (size: %lu bytes)\n", fd, filename, file_size);
    
    	// Sanitize filename
    	if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
        	goto error;
    	}
    	snprintf(safe_path, sizeof(safe_path), "%s/%s", UPLOAD_DIR, filename);
    	FILE *out = fopen(safe_path, "wb");
    	if (!out) goto error;
    
    	// Receive file data
    	char buf[BUFFER_SIZE];
    	uint64_t remaining = file_size;
    	struct timespec last_update;
    	clock_gettime(CLOCK_MONOTONIC, &last_update);
    
    	while (remaining > 0) {
        	size_t to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        	ssize_t got = recv(fd, buf, to_read, 0);
        	if (got <= 0) {
            		fclose(out);
            		unlink(safe_path);
            		goto error;
        	}
        	fwrite(buf, 1, got, out);
        	received += got;
        	remaining -= got;
        
        	pthread_mutex_lock(&clients[stats_idx]->mutex);
        	clients[stats_idx]->total_bytes_received += got;
        	pthread_mutex_unlock(&clients[stats_idx]->mutex);
    	}
    	fclose(out);
    
    	// Verify size
    	int success = (received == file_size);
    	uint8_t status = success ? 1 : 0;
    	send(fd, &status, sizeof(status), 0);
    
    	pthread_mutex_lock(&clients[stats_idx]->mutex);
    	clients[stats_idx]->active = 0;
    	clients[stats_idx]->finished = 1;
    	pthread_mutex_unlock(&clients[stats_idx]->mutex);
    
    	// Print final speed if needed
    	struct timespec now;
    	clock_gettime(CLOCK_MONOTONIC, &now);
    	pthread_mutex_lock(&clients[stats_idx]->mutex);
    	double elapsed_interval = (now.tv_sec - clients[stats_idx]->last_print_time.tv_sec) + (now.tv_nsec - clients[stats_idx]->last_print_time.tv_nsec) / 1e9;
    	if (elapsed_interval < SPEED_INTERVAL - 0.01 && clients[stats_idx]->total_bytes_received > clients[stats_idx]->last_bytes_received) {
        	print_speed(clients[stats_idx], &now);
    	}
    	pthread_mutex_unlock(&clients[stats_idx]->mutex);
    	printf("[Client %d] Transfer completed: %s (%s)\n", fd, filename, success ? "SUCCESS" : "FAILED");
    
    	close(fd);
    	free(info);
    	return NULL;
    
error:
    	close(fd);
    	pthread_mutex_lock(&clients_mutex);
    	if (stats_idx >= 0 && clients[stats_idx]) {
        	clients[stats_idx]->active = 0;
        	clients[stats_idx]->finished = 1;
    	}
    	pthread_mutex_unlock(&clients_mutex);
    	free(info);
    	return NULL;
}

int main(int argc, char *argv[]) {
    	if (argc != 2) {
        	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        	exit(1);
    	}
    	int port = atoi(argv[1]);
    	ensure_upload_dir();
    
    	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    	int opt = 1;
    	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    	struct sockaddr_in addr = {0};
    	addr.sin_family = AF_INET;
    	addr.sin_addr.s_addr = INADDR_ANY;
    	addr.sin_port = htons(port);
    
    	if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        	perror("bind");
        	exit(1);
    	}
    	listen(listen_fd, 10);
    
    	printf("Server listening on port %d\n", port);
    
    	pthread_t printer_thread;
    	pthread_create(&printer_thread, NULL, speed_printer, NULL);
    	pthread_detach(printer_thread);
    
    	while (1) {
        	struct sockaddr_in client_addr;
        	socklen_t len = sizeof(client_addr);
        	int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len);
        	if (client_fd < 0) continue;
        
        	printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        	client_info_t *info = malloc(sizeof(client_info_t));
        	info->client_fd = client_fd;
        	info->addr = client_addr;
        
        	pthread_t thread;
        	pthread_create(&thread, NULL, handle_client, info);
        	pthread_detach(thread);
    	}
    	close(listen_fd);
    	return 0;
}
