#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8082
#define MAX_EVENTS 100
#define BUFFER_SIZE 1024

void set_nonblocking(int fd) {
    	int flags = fcntl(fd, F_GETFL, 0);
    	if (flags == -1) {
        	perror("fcntl F_GETFL:\n");
        	return;
    	}
    	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        	perror("fcntl F_SETFL:\n");
    	}
}

int main() {
    	int server_fd, epoll_fd;
    	struct sockaddr_in address;
    	struct epoll_event ev, events[MAX_EVENTS];
    	int opt = 1;
    
    	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        	perror("socket:\n");
        	exit(EXIT_FAILURE);
    	}
    
    	set_nonblocking(server_fd);
    
    	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        	perror("setsockopt:\n");
        	close(server_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	address.sin_family = AF_INET;
    	address.sin_addr.s_addr = INADDR_ANY;
    	address.sin_port = htons(PORT);
    
    	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        	perror("bind:\n");
        	close(server_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	if (listen(server_fd, MAX_EVENTS) < 0) {
        	perror("listen:\n");
        	close(server_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	epoll_fd = epoll_create1(0);
    	if (epoll_fd == -1) {
        	perror("epoll_create1:\n");
        	close(server_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	ev.events = EPOLLIN;
    	ev.data.fd = server_fd;
    	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        	perror("epoll_ctl:\n");
        	close(server_fd);
        	close(epoll_fd);
        	exit(EXIT_FAILURE);
    	}
    
    	printf("Epoll TCP server was binded on %d port\n", PORT);
    
    	while (1) {
        	int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        	if (nfds == -1) {
            		if (errno == EINTR) continue;
            		perror("epoll_wait:\n");
            		break;
        	}
        
        	for (int i = 0; i < nfds; i++) {
            		if (events[i].data.fd == server_fd) {
                		// New connection
                		struct sockaddr_in client_addr;
                		socklen_t client_len = sizeof(client_addr);
                		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                
                		if (client_fd == -1) {
                    			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                    			perror("accept:\n");
                    			continue;
                		}
                
                		set_nonblocking(client_fd);
                
                		ev.events = EPOLLIN | EPOLLET;
                		ev.data.fd = client_fd;
                		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    			perror("epoll_ctl: client_fd:\n");
                    			close(client_fd);
                		} else {
                    			printf("New connection %s:%d (fd=%d)\n",
                           		inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);
                		}
            		} else {
                		// Data from a client
                		int client_fd = events[i].data.fd;
                		char buffer[BUFFER_SIZE];
                
                		while (1) {
                    			ssize_t bytes = read(client_fd, buffer, BUFFER_SIZE);
                    
                    			if (bytes < 0) {
                        			if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            				// No data to read
                            				break;
                        			}
                        			perror("read error:\n");
                        			close(client_fd);
                        			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        			break;
                    			} else if (bytes == 0) {
                        			printf("Client fd=%d closed the connection\n", client_fd);
                        			close(client_fd);
                        			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        			break;
                    			} else {
                        			buffer[bytes] = '\0';
                        			printf("Recived %zd bytes from fd=%d: %s\n", bytes, client_fd, buffer);
                        
                        			ssize_t sent = send(client_fd, buffer, bytes, 0);
                        			if (sent < 0) {
                            				perror("send error:\n");
                            				close(client_fd);
                            				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            				break;
                        			} else if (sent != bytes) {
                            				printf("Sended: %zd of %zd bytes\n", sent, bytes);
                        			}
                    			}
                		}
            		}
        	}
    	}
    
    	close(server_fd);
	close(epoll_fd);
    	return 0;
}
