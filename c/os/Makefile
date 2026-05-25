CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -lpthread -O2

all: clean compile
clean:
	rm -f main 

compile: main.c
	$(CC) $(CFLAGS) -o main main.c

run:
	sudo ./main
run-test:
	dig @127.0.0.1 -p 8080 google.com
valgrind-run: main
	sudo valgrind --leak-check=full ./main

.PHONY: all clean run run-test valgrind-run
