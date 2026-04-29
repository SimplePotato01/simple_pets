#include "heap.h"
#include "avl_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void *heap_start = NULL;
static block_header_t *free_tree_root = NULL;
static block_header_t *used_list_head = NULL;
static size_t heap_size = HEAP_SIZE;

size_t get_heap_size(void) {
    	return heap_size;
}

static size_t align_size(size_t size) {
    	return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static int is_ptr_in_heap(void *ptr) {
    	char *heap_start_char = (char *)heap_start;
    	char *heap_end_char = (char *)heap_start + heap_size;
    	char *ptr_char = (char *)ptr;
    
    	return (ptr_char >= heap_start_char && ptr_char < heap_end_char);
}

static block_header_t* get_block_header(void *ptr) {
    	if (!ptr || !heap_start) return NULL;
    
    	if (!is_ptr_in_heap(ptr)) {
        	return NULL;
    	}
    
    	block_header_t *header = (block_header_t *)((char *)ptr - sizeof(block_header_t));
    
    	if (!is_ptr_in_heap(header)) {
        	return NULL;
    	}
    
    	if (header->size > heap_size || header->size < sizeof(block_header_t)) {
        	return NULL;
    	}
    
    	if ((char *)header + header->size > (char *)heap_start + heap_size) {
        	return NULL;
    	}
    
    	if ((char *)ptr != (char *)header + sizeof(block_header_t)) {
        	return NULL;
    	}
    
    	return header;
}

static void split_block(block_header_t *block, size_t requested_size) {
    	size_t remaining_size = block->size - requested_size;
    
    	if (remaining_size > sizeof(block_header_t) + ALIGNMENT) {
        	block_header_t *new_block = (block_header_t *)((char *)block + requested_size);
        	new_block->size = remaining_size;
        	new_block->is_free = 1;
        	new_block->next = NULL;
        	new_block->prev = NULL;
        	new_block->left = NULL;
        	new_block->right = NULL;
        	new_block->height = 1;
        
        	block->size = requested_size;
        
        	free_tree_root = avl_insert(free_tree_root, new_block);
    	}
}

static block_header_t* find_free_block_by_address(block_header_t *root, void *address) {
    	if (!root) return NULL;
    
    	if ((void *)root == address) return root;
    
    	block_header_t *left_result = find_free_block_by_address(root->left, address);
    	if (left_result) return left_result;
    
    	return find_free_block_by_address(root->right, address);
}

static void coalesce_blocks(block_header_t *block) {
    	if (!block || !block->is_free) return;
    
    	block_header_t *next_block = (block_header_t *)((char *)block + block->size);
    	if ((char *)next_block < (char *)heap_start + heap_size) {
        	block_header_t *found_next = find_free_block_by_address(free_tree_root, next_block);
        	if (found_next && found_next->is_free) {
            		block->size += found_next->size;
            		free_tree_root = avl_delete(free_tree_root, found_next);
            		coalesce_blocks(block);
        	}
    	}
}

int init_heap(void) {
    	if (heap_start != NULL) {
        	return 0;
    	}
    
    	heap_start = mmap(NULL, heap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    	if (heap_start == MAP_FAILED) {
        	perror("mmap failed");
        	return -1;
    	}
    
    	block_header_t *initial_block = (block_header_t *)heap_start;
    	initial_block->size = heap_size;
    	initial_block->is_free = 1;
    	initial_block->next = NULL;
    	initial_block->prev = NULL;
    	initial_block->left = NULL;
    	initial_block->right = NULL;
    	initial_block->height = 1;
    
    	free_tree_root = avl_insert(free_tree_root, initial_block);
    	used_list_head = NULL;
    
    	printf("Heap initialized at %p, size: %zu bytes\n", heap_start, heap_size);
    	return 0;
}

void *my_malloc(size_t size) {
    	if (heap_start == NULL) {
        	if (init_heap() != 0) return NULL;
    	}
    
    	if (size == 0) return NULL;
    
    	size_t aligned_size = align_size(size) + sizeof(block_header_t);
    
    	block_header_t *block = avl_find_first_fit(free_tree_root, aligned_size);
    
    	if (block == NULL) {
        	printf(RED "Not enough memory for allocation of %zu bytes\n" RESET, size);
        	return NULL;
    	}
    
    	free_tree_root = avl_delete(free_tree_root, block);
    
    	split_block(block, aligned_size);
    
    	block->is_free = 0;
    
    	block->next = used_list_head;
    	if (used_list_head) {
        	used_list_head->prev = block;
    	}
    	used_list_head = block;
    	block->prev = NULL;
    
    	return (void *)((char *)block + sizeof(block_header_t));
}

void my_free(void *ptr) {
    	if (ptr == NULL || heap_start == NULL) {
        	return;
    	}
    
    	block_header_t *block = get_block_header(ptr);
    
    	if (block == NULL) {
        	printf(RED "Error: Invalid pointer %p - not from our heap or corrupted\n" RESET, ptr);
        	return;
    	}
    
    	if (block->is_free) {
        	printf(RED "Warning: Double free detected at %p\n" RESET, ptr);
        	return;
    	}
    
    	if (block->prev) {
        	block->prev->next = block->next;
    	} else {
        	used_list_head = block->next;
    	}
    
    	if (block->next) {
        	block->next->prev = block->prev;
    	}
    
    	block->is_free = 1;
    	block->next = NULL;
    	block->prev = NULL;
    	block->left = NULL;
    	block->right = NULL;
    
    	coalesce_blocks(block);
    
    	free_tree_root = avl_insert(free_tree_root, block);
}

void dump_heap() {
    	printf(PURP "\n=== Heap State ===\n");
    	printf("Heap start: %p, size: %zu bytes\n", heap_start, heap_size);
    
    	printf("\nUsed blocks:\n");
    	block_header_t *current = used_list_head;
    	int block_num = 0;
    
    	while (current) {
        	char *block_end = (char *)current + current->size;
        	printf("  Block %d: [%p - %p] size=%zu, data at %p\n", block_num++, (void *)current, (void *)block_end,  current->size, (void *)((char *)current + sizeof(block_header_t)));
        	current = current->next;
    	}
    
    	printf("=================\n\n" RESET);
}

static void count_tree_stats(block_header_t *node, size_t *total_free, int *free_blocks) {
    	if (!node) return;
    
    	if (node->is_free) {
        	*total_free += node->size;
        	(*free_blocks)++;
    	}
    
    	count_tree_stats(node->left, total_free, free_blocks);
    	count_tree_stats(node->right, total_free, free_blocks);
}

void heap_stats() {
    	size_t total_free = 0;
    	size_t total_used = 0;
    	int free_blocks = 0;
    	int used_blocks = 0;
    
    	block_header_t *current = used_list_head;
    	while (current) {
        	total_used += current->size;
        	used_blocks++;
        	current = current->next;
    	}
    
    	count_tree_stats(free_tree_root, &total_free, &free_blocks);
    
    	printf(LBLUE "\n=== Heap Statistics ===\n");
    	printf("Total heap size: %zu bytes\n", heap_size);
    	printf("Used memory: %zu bytes (%d blocks)\n", total_used, used_blocks);
    	printf("Free memory: %zu bytes (%d blocks)\n", total_free, free_blocks);
    	if (total_used + total_free > 0) {
        	printf("Memory utilization: %.2f%%\n", (float)total_used / (total_used + total_free) * 100);
    	}
    	printf("=======================\n\n" RESET);
}
