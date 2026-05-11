CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2

all: clean server client
clean:
	rm -f server client


server: server.c
	$(CC) $(CFLAGS) -lpthread -o server server.c
client: client.c
	$(CC) $(CFLAGS) -o client client.c

run-server:
	./server 8080
run-client:
	./client myfile.txt 127.0.0.1 8080

.PHONY: all clean run-client run-server
