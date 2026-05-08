CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2

clean:
	rm -f tcp_client tcp_server
all: server client

server: tcp_server.c
	$(CC) $(CFLAGS) -o tcp_server tcp_server.c
client: tcp_client.c
	$(CC) $(CFLAGS) -o tcp_client tcp_client.c

run-server:
	./tcp_server
run-client:
	./tcp_client

valgrind-server: server
	valgrind --leak-check=full ./tcp_server

valgrind-client: client
	valgrind --leak-check=full ./tcp_client

.PHONY: all clean run-server run-client valgrind-server valgrind-client
