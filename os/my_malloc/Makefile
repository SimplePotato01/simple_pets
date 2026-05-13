CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2

all: clean compile
clean:
	rm -f main

compile: avl_tree.c  avl_tree.h  heap.c  heap.h  main.c
	$(CC) $(CFLAGS) -o main avl_tree.c  avl_tree.h  heap.c  heap.h  main.c

run:
	./main 
valgrind-run: main
	valgrind --leak-check=full ./main

.PHONY: all clean run valgrind-run
