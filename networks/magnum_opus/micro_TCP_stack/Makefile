CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -lpthread -O2

all: clean compile
clean:
	rm -f main 

compile: main.c
	$(CC) $(CFLAGS) -o main main.c

run:
	sudo iptables -I OUTPUT -p tcp --tcp-flags RST RST -j DROP
	sudo ./main
remove_new_rule:
	sudo iptables -D OUTPUT -p tcp --tcp-flags RST RST -j DROP

valgrind-run: main
	sudo valgrind --leak-check=full ./main

.PHONY: all clean run valgrind-run
