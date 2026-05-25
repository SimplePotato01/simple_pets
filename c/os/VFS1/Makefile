CC = gcc
CFLAGS = -Werror -g -O2

all: clean compile
clean:
	rm -fr main elpmaxe

compile: main.c boilerplate.h files_functions.c
	$(CC) $(CFLAGS) -o main main.c boilerplate.h files_functions.c

run: main
	./main example
valgrind-run: main
	valgrind --leak-check=full ./main example

.PHONY: all clean run valgrind-run
