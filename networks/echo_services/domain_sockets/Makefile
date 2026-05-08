CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2

all: clean server client
clean:
	rm -f  unix_server unix_client

server: unix_server.c
	$(CC) $(CFLAGS) -o unix_server unix_server.c 
client: unix_client.c
	$(CC) $(CFLAGS) -o unix_client unix_client.c

run-server:
	./unix_server
run-client:
	./unix_client

valgrind-server: server
	valgrind --leak-check=full ./unix_server

valgrind-client: client
	valgrind --leak-check=full ./unix_client

.PHONY: all clean run-server run-client valgrind-server valgrind-client
