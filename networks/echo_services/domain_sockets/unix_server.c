#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/unix_socket"
#define BUFFER_SIZE 1024

int main() {
    	int server_fd, client_fd;
    	struct sockaddr_un server_addr, client_addr;
    	socklen_t addr_len = sizeof(client_addr);
    	char buffer[BUFFER_SIZE] = {0};
    
    	unlink(SOCKET_PATH);
    
    	if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        	perror("socket:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	memset(&server_addr, 0, sizeof(server_addr));
    	server_addr.sun_family = AF_UNIX;
    	strcpy(server_addr.sun_path, SOCKET_PATH);
    
    	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        	perror("bind:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	if (listen(server_fd, 5) == -1) {
        	perror("listen:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	printf("Unix domain server listening: %s\n", SOCKET_PATH);
    
    	while (1) {
        	if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
            		perror("accept:\n");
            		continue;
        	}
        
        	read(client_fd, buffer, BUFFER_SIZE);
        	printf("Recived: %s\n", buffer);
        	send(client_fd, buffer, strlen(buffer), 0);
        	close(client_fd);
    	}
    
    	close(server_fd);
    	unlink(SOCKET_PATH);
    	return 0;
}
