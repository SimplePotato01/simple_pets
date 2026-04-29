clear
gcc avl_tree.c  avl_tree.h  heap.c  heap.h  main.c -o main
valgrind --leak-check=full ./main
