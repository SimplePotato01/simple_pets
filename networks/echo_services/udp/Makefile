CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2

all: clean server client
clean:
	rm -f udp_server udp_client

server: udp_server.c
	$(CC) $(CFLAGS) -o udp_server -g udp_server.c
client: udp_client.c
	$(CC) $(CFLAGS) -o udp_client -g udp_client.c

run-server:
	./udp_server
run-client:
	./udp_client

valgrind-server: server
	valgrind --leak-check=full ./udp_server

valgrind-client: client
	valgrind --leak-check=full ./udp_client

.PHONY: all clean run-server run-client valgrind-server valgrind-client
