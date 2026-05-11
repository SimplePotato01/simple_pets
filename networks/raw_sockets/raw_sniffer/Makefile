CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2

all: clean compile
clean:
	rm -f raw_sniffer 


compile: raw_sniffer.c
	$(CC) $(CFLAGS) -o raw_sniffer -g raw_sniffer.c

run:
	sudo ./raw_sniffer 

valgrind-run: raw_sniffer
	sudo valgrind --leak-check=full ./raw_sniffer

.PHONY: all clean run valgrind-run
