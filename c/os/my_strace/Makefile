CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -O2

all: clean compile
clean:
	rm -f main test_code 

compile: main.c test_code.c
	$(CC) $(CFLAGS) -o main main.c
	$(CC) $(CFLAGS) -o test_code test_code.c

run:
	./main ./test_code 
valgrind-run: main
	sudo valgrind --leak-check=full ./main ./test_code

.PHONY: all clean run valgrind-run
