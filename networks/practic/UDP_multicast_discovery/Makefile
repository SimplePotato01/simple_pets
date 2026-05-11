CC = gcc
CFLAGS = -Wall -Werror -lpthread -O2

all: clean compile
clean:
	rm -f main 


compile: main.c
	$(CC) $(CFLAGS) -o main main.c

run-ipv4:
	sudo ./main 224.0.0.1
run-ipv6:
	sudo ./main ff02::1

valgrind-run: main
	sudo valgrind --leak-check=full ./main 224.0.0.1

.PHONY: all clean run-ipv4 run-ipv6 valgrind-run
