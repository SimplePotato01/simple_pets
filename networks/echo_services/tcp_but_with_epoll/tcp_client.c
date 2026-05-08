#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8082
#define BUFFER_SIZE 1024
#define MESSAGE_SIZE 20

int main() {
    	int sock = 0;
    	struct sockaddr_in serv_addr;
    	char buffer[BUFFER_SIZE];
    	char* message = malloc(sizeof(char) * MESSAGE_SIZE);
    	char stop_message[] = "/q\n";
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
        	perror("connection:\n");
        	close(sock);
        	return -1;
    	}
    
    	printf("Connected to the server (/q for quit)\n");
   	while(1) {
		printf("Message:");
		if(fgets(message, MESSAGE_SIZE, stdin) == NULL) {
			perror("fgets:\n");
			exit(EXIT_FAILURE);
		}
		if(strcmp(message, stop_message) == 0) break;
		// Отправка данных
    		ssize_t bytes_sent = send(sock, message, strlen(message), 0);
    		if (bytes_sent < 0) {
        		perror("send failed:\n");
        		close(sock);
        		return -1;
    		}
    		printf("Sended %zd bytes: %s\n", bytes_sent, message);
    
    		// Получение ответа
    		memset(buffer, 0, BUFFER_SIZE);
    		ssize_t bytes_read = read(sock, buffer, BUFFER_SIZE - 1);
    
    		if (bytes_read < 0) {
        		perror("read failed:\n");
    		} else if (bytes_read == 0) {
        		printf("Server closed the connection\n");
			break;
    		} else {
        		printf("Recived %zd bytes: %s\n", bytes_read, buffer);
    		}
	
	}
    	close(sock);
    	printf("The connection was closed\n");
    	return 0;
}
