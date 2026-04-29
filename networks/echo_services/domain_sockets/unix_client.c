#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/unix_socket"
#define BUFFER_SIZE 1024

int main() {
    	int sockfd;
    	struct sockaddr_un server_addr;
    	char buffer[BUFFER_SIZE] = {0};
    	char message[] = "Hello from Unix socket client!";
    
    	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        	perror("socket:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	memset(&server_addr, 0, sizeof(server_addr));
    	server_addr.sun_family = AF_UNIX;
    	strcpy(server_addr.sun_path, SOCKET_PATH);
    
    	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        	perror("connect:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	send(sockfd, message, strlen(message), 0);
    	printf("Server: %s\n", message);
    
    	read(sockfd, buffer, BUFFER_SIZE);
    	printf("Recived: %s\n", buffer);
    
    	close(sockfd);
    	return 0;
}
