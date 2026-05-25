#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
	int server_fd, client_fd;
	struct sockaddr_in address;
	int opt = 1;
	socklen_t addrlen = sizeof(address);
	char buffer[BUFFER_SIZE];
    
    	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        	perror("socket failed:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        	perror("setsockopt:\n");
        	close(server_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	address.sin_family = AF_INET;
    	address.sin_addr.s_addr = INADDR_ANY;
    	address.sin_port = htons(PORT);
    
    	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        	perror("bind failed:\n");
        	close(server_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	if (listen(server_fd, 3) < 0) {
        	perror("listen:\n");
        	close(server_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	printf("TCP server is listening %d port\n", PORT);
    
    	while (1) {
        	if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            		if (errno == EINTR) continue;
            		perror("accept:\n");
            		continue;
        	}
		// pthread_create(&thread, NULL, handle_client, &client_fd);
        	printf("New connection from %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
        	while (1) {
            		memset(buffer, 0, BUFFER_SIZE);
            		ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
            
            		if (bytes_read < 0) {
                		if (errno == EINTR) continue;
                		perror("read error:\n");
                		break;
            		} else if (bytes_read == 0) {
                		printf("Client %s:%d closed the connection\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                		break;
            		}
            
            		printf("Recived: %zd байт: %s\n", bytes_read, buffer);
            	
            		ssize_t bytes_sent = send(client_fd, buffer, bytes_read, 0);
            		if (bytes_sent < 0) {
                		perror("send error:\n");
                		break;
            		} else if (bytes_sent != bytes_read) {
                		printf("Sended: %zd of %zd bytes\n", bytes_sent, bytes_read);
            		}
        	}
        
        	close(client_fd);
        	printf("Connection was closed\n");
    	}
    
    	close(server_fd);
    	return 0;
}
