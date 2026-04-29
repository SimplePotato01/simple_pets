#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_SIZE (1024 * 1024)  
#define ALIGNMENT 8                   

#define RED "\033[31m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define PURP "\033[35m"
#define LBLUE "\033[36m"
#define RESET "\033[0m"

// Structure of a memory block header
typedef struct block_header {
        size_t size;                      // Block size (including header)
        int is_free;                      // Whether the block is free
        struct block_header *next;        // For linked list
        struct block_header *prev;        // For linked list

        struct block_header *left;        // Left child in AVL tree
        struct block_header *right;       // Right child in AVL tree
        int height;                       // Node height in AVL tree
} block_header_t;

int init_heap(void);
void *my_malloc(size_t size);
void my_free(void *ptr);
void dump_heap(void);
void heap_stats(void);
size_t get_heap_size(void);

#endif
