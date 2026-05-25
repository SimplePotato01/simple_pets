#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8081
#define BUFFER_SIZE 1024
#define MESSAGE_SIZE 20

int main() {
    	int sockfd;
    	char buffer[BUFFER_SIZE];
    	struct sockaddr_in server_addr;
    	socklen_t addr_len = sizeof(server_addr);
    	char* message = malloc(sizeof(char) * MESSAGE_SIZE);
   	char stop_message[] = "/q\n";

    	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        	perror("socket:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	memset(&server_addr, 0, sizeof(server_addr));
    	server_addr.sin_family = AF_INET;
    	server_addr.sin_port = htons(PORT);
    	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	printf("Ready for sending message (/q to stop)\n");
	while (1){
		printf("Message: ");
		if(fgets(message, MESSAGE_SIZE, stdin) == NULL) {
			perror("fgets:\n");
			exit(EXIT_FAILURE);
		}
		if(strcmp(message, stop_message) == 0) break;

    		ssize_t sent = sendto(sockfd, message, strlen(message), 0, (const struct sockaddr *)&server_addr, addr_len);
    
    		if (sent < 0) {
        		perror("sendto failed:\n");
        		close(sockfd);
        		exit(EXIT_FAILURE);
    		}
    		printf("Sended %zd bytes: %s\n", sent, message);
    
    		memset(buffer, 0, BUFFER_SIZE);
    		ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&server_addr, &addr_len);
    
    		if (n < 0) perror("recvfrom failed:\n");
    		else printf("Recived %zd bytes: %s\n", n, buffer);
    	}
    	close(sockfd);
    	free(message);
	return 0;
}
