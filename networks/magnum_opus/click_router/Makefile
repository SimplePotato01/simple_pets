CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -lpthread -O2

all: clean compile
clean:
	rm -f main 

compile: main.c
	$(CC) $(CFLAGS) -o main main.c

run:
	sudo ./main router.conf
valgrind-run: main
	sudo valgrind --leak-check=full ./main router.conf

.PHONY: all clean run valgrind-run
