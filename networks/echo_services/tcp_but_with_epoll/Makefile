CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2

all: clean server client
clean:
	rm -f  epoll_server tcp_client

server: epoll_server.c
	$(CC) $(CFLAGS) -o epoll_server epoll_server.c
client: tcp_client.c
	$(CC) $(CFLAGS) -o tcp_client tcp_client.c

run-server:
	./epoll_server
run-client:
	./tcp_client

valgrind-server: server
	valgrind --leak-check=full ./epoll_server

valgrind-client: client
	valgrind --leak-check=full ./tcp_client

.PHONY: all clean run-server run-client valgrind-server valgrind-client
