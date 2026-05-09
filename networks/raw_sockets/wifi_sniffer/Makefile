CC = gcc
CFLAGS = -Wall -Werror -O2

all: clean compile
clean:
	rm -f main


compile: main.c 
	$(CC) $(CFLAGS) -o main main.c

run:
	sudo ./main

valgrind-run: main
	sudo valgrind --leak-check=full ./main

.PHONY: all clean run valgrind-run
