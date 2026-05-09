CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2

all: clean compile
clean:
	rm -f mac_sniffer


compile: mac_sniffer.c
	$(CC) $(CFLAGS) -o mac_sniffer mac_sniffer.c

run:
	sudo ./mac_sniffer wlp2s0

valgrind-run: mac_sniffer
	sudo valgrind --leak-check=full ./mac_sniffer wlp2s0

.PHONY: all clean run valgrind-run
