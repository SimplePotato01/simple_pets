CC = gcc
CFLAGS = -O2

all: clean compile
clean:
	rm -f main


compile: main.c 
	$(CC) $(CFLAGS) -o main -g main.c

run:
	sudo ./main wlp2s0

valgrind-run: main
	sudo valgrind --leak-check=full ./main wlp2s0

.PHONY: all clean run valgrind-run
