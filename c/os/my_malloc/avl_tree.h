#ifndef AVL_TREE_H
#define AVL_TREE_H

#include "heap.h"
#include <stdint.h>

// Node height
int avl_height(block_header_t *node);

// Update node height
void avl_update_height(block_header_t *node);

// Node balance factor
int avl_balance_factor(block_header_t *node);

// Right rotation
block_header_t* avl_rotate_right(block_header_t *y);

// Left rotation
block_header_t* avl_rotate_left(block_header_t *x);

// Balance a node
block_header_t* avl_balance(block_header_t *node);

// Insert a block into the AVL tree (by size)
block_header_t* avl_insert(block_header_t *root, block_header_t *block);

// Delete a block from the AVL tree
block_header_t* avl_delete(block_header_t *root, block_header_t *block);

// Find a block with size >= requested_size (first-fit)
block_header_t* avl_find_first_fit(block_header_t *root, size_t requested_size);

// Find a block by data pointer
block_header_t* avl_find_by_ptr(block_header_t *root, void *ptr);

// In-order tree traversal
void avl_inorder_traversal(block_header_t *root, void (*callback)(block_header_t *));

// Free the tree
void avl_free_tree(block_header_t *root);

#endif // AVL_TREE_H
