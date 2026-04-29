#!/bin/bash
make

sudo insmod chardev.ko
sudo chmod 666 /dev/mychardev

echo ""
echo "Test 1: write and read"
echo "Hello Kernel!" > /dev/mychardev
echo "cat /dev/mychardev:"
cat /dev/mychardev

echo ""
echo "Test 2: multiple writes"
echo "First line" > /dev/mychardev
echo "Second line" >> /dev/mychardev
echo "Third line" >> /dev/mychardev
cat /dev/mychardev

echo -e "\nKernel logs:"
sudo dmesg | tail -20

echo ""
echo "Info:"
lsmod | grep chardev
modinfo chardev.ko | grep -E "description|author|license"

sudo rmmod chardev
