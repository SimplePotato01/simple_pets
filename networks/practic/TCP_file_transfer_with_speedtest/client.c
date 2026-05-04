// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <sys/stat.h>
#include <endian.h>

#define BUFFER_SIZE 8192

int main(int argc, char *argv[]) {
    	if (argc != 4) {
        	fprintf(stderr, "Usage: %s <file_path> <server_ip> <port>\n", argv[0]);
        	exit(1);
    	}
    	char *filepath = argv[1];
    	char *server_ip = argv[2];
    	int port = atoi(argv[3]);
    
    	struct stat st;
    	if (stat(filepath, &st) != 0) {
        	perror("stat");
        	exit(1);
    	}
    	uint64_t file_size = st.st_size;
    
    	FILE *f = fopen(filepath, "rb");
    	if (!f) {
        	perror("fopen");
        	exit(1);
    	}
    
    	int sock = socket(AF_INET, SOCK_STREAM, 0);
    	struct sockaddr_in addr;
    	addr.sin_family = AF_INET;
    	addr.sin_port = htons(port);
    	if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
        	struct hostent *he = gethostbyname(server_ip);
        	if (!he) {
            		fprintf(stderr, "Cannot resolve hostname\n");
            		exit(1);
        	}
        	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    	}
    
    	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        	perror("connect");
        	fclose(f);
        	exit(1);
    	}
    
    	// Send filename length and name
    	const char *filename = strrchr(filepath, '/');
    	if (filename == NULL) filename = filepath;
    	else filename++;
    	uint32_t name_len = strlen(filename);
    	uint32_t net_name_len = htonl(name_len);
    	send(sock, &net_name_len, sizeof(net_name_len), 0);
    	send(sock, filename, name_len, 0);
    
    	// Send file size
    	uint64_t net_size = htobe64(file_size);
    	send(sock, &net_size, sizeof(net_size), 0);
    
    	// Send the file
    	char buf[BUFFER_SIZE];
    	size_t n;
    	while ((n = fread(buf, 1, BUFFER_SIZE, f)) > 0) {
        	send(sock, buf, n, 0);
    	}
    	fclose(f);
    
    	// Receive status
    	uint8_t status;
    	if (recv(sock, &status, sizeof(status), 0) == sizeof(status)) {
        	if (status == 1) printf("File transmitted successfully\n");
        	else printf("File transmission failed (size mismatch or server error)\n");
    	} else {
        	printf("Server did not respond with status\n");
    	}
    	close(sock);
    	return 0;
}
