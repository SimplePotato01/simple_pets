CC = gcc
CFLAGS = -Wall -pthread -O2

all: server client

server: server.c
	$(CC) $(CFLAGS) server.c -o server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f server client

run-server:
	./server

run-client:
	./client 127.0.0.1

.PHONY: all clean run-server run-client
