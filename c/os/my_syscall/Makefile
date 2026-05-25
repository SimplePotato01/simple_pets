CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2

all: clean compile
clean:
	rm -f main 

compile: hello_my_syscall.c  syscall_wrapper.h
	$(CC) $(CFLAGS) -o main hello_my_syscall.c  syscall_wrapper.h

run:
	./main 

.PHONY: all clean run
