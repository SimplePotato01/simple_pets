#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8081
#define BUFFER_SIZE 1024

int main() {
    	int sockfd;
    	char buffer[BUFFER_SIZE];
    	struct sockaddr_in server_addr, client_addr;
    	socklen_t addr_len = sizeof(client_addr);
    
    	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        	perror("socket:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	memset(&server_addr, 0, sizeof(server_addr));
    	server_addr.sin_family = AF_INET;
    	server_addr.sin_addr.s_addr = INADDR_ANY;
    	server_addr.sin_port = htons(PORT);
    
    	if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        	perror("bind:\n");
        	close(sockfd);
        	exit(EXIT_FAILURE);
    	}
    	printf("UDP server is listening %d port\n", PORT);
    
    	while (1) {
        	memset(buffer, 0, BUFFER_SIZE);
        	ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        	if (n < 0) {
            		if (errno == EINTR) continue;
            		perror("recvfrom error:\n");
            	continue;
        	}
        
        	printf("Recived %zd bytes from %s:%d: %s\n", n, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

		ssize_t sent = sendto(sockfd, buffer, n, 0, (const struct sockaddr *)&client_addr, addr_len);
        
        	if (sent < 0) perror("sendto:\n");
        	else if (sent != n) printf("Sended: %zd of %zd bytes\n", sent, n);
    	}
    
    	close(sockfd);
    	return 0;
}
