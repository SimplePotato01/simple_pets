CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2

all: clean compile
clean:
	rm -f main

compile: main.c 
	$(CC) $(CFLAGS) -o main main.c 

run: main
	./main 
valgrind-run: main
	valgrind --leak-check=full ./main

.PHONY: all clean run valgrind-run
