CC = gcc
CFLAGS = -Werror -g -O2

all: clean compile
clean:
	rm -fr main 

compile: main.c boilerplate.h files_functions.c
	$(CC) $(CFLAGS) -o main main.c boilerplate.h files_functions.c

run: main
	./main example

.PHONY: all clean run valgrind-run
