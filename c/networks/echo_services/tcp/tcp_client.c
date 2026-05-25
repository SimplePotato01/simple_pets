#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    	int sock = 0;
    	struct sockaddr_in serv_addr;
    	char buffer[BUFFER_SIZE];
    	char message[] = "Echo from TCP client";
    
    	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        	perror("socket:\n");
        	return -1;
    	}
    
    	serv_addr.sin_family = AF_INET;
    	serv_addr.sin_port = htons(PORT);
    
    	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        	perror("Invalid address:\n");
        	close(sock);
        	return -1;
    	}
    
    	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        	perror("connection failed:\n");
        	close(sock);
        	return -1;
    	}
    	printf("Connected to the server\n");
    
    	ssize_t bytes_sent = send(sock, message, strlen(message), 0);
    	if (bytes_sent < 0) {
        	perror("send failed:\n");
        	close(sock);
        	return -1;
    	}
    	printf("Sended %zd bytes: %s\n", bytes_sent, message);
    
    	memset(buffer, 0, BUFFER_SIZE);
    	ssize_t bytes_read = read(sock, buffer, BUFFER_SIZE - 1);
    
    	if (bytes_read < 0) {
        	perror("read failed:\n");
    	} else if (bytes_read == 0) {
        	printf("Server closed the connection\n");
    	} else {
        	printf("Recived %zd bytes: %s\n", bytes_read, buffer);
    	}
    
    	close(sock);
    	printf("Connection was closed\n");
    	return 0;
}
