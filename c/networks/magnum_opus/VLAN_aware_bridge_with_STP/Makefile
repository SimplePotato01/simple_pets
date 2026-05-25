CC = gcc
CFLAGS = -Wall -Werror -lpthread -g -O2

all: clean compile
clean:
	rm -f main 

compile: main.c
	$(CC) $(CFLAGS) -o main main.c

run:
	sudo ip link add veth0 type veth peer name veth0_peer
	sudo ip link add veth1 type veth peer name veth1_peer
	sudo ip link add veth2 type veth peer name veth2_peer
	sudo ip link add veth3 type veth peer name veth3_peer
	sudo ip link set veth0 up
	sudo ip link set veth1 up
	sudo ip link set veth2 up
	sudo ip link set veth3 up
	
	sudo ./main
	
	sudo ip link delete veth0
	sudo ip link delete veth1
	sudo ip link delete veth2
	sudo ip link delete veth3

remove_new_rule:
	sudo ip link delete veth0
	sudo ip link delete veth1
	sudo ip link delete veth2
	sudo ip link delete veth3

valgrind-run: main
	sudo ip link add veth0 type veth peer name veth0_peer
	sudo ip link add veth1 type veth peer name veth1_peer
	sudo ip link add veth2 type veth peer name veth2_peer
	sudo ip link add veth3 type veth peer name veth3_peer
	sudo ip link set veth0 up
	sudo ip link set veth1 up
	sudo ip link set veth2 up
	sudo ip link set veth3 up
	sudo valgrind --leak-check=full ./main

.PHONY: all clean run remove_new_rule valgrind-run
