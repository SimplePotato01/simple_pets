#include "avl_tree.h"
#include <stdio.h>
#include <stdlib.h>

int avl_height(block_header_t *node) {
    	return node ? node->height : 0;
}

void avl_update_height(block_header_t *node) {
    	if (node) {
        	int left_height = avl_height(node->left);
        	int right_height = avl_height(node->right);
        	node->height = (left_height > right_height ? left_height : right_height) + 1;
    	}
}

int avl_balance_factor(block_header_t *node) {
    	return node ? avl_height(node->left) - avl_height(node->right) : 0;
}

block_header_t* avl_rotate_right(block_header_t *y) {
    	if (!y || !y->left) return y;
    
    	block_header_t *x = y->left;
    	block_header_t *T2 = x->right;
    
    	x->right = y;
    	y->left = T2;
    
    	avl_update_height(y);
    	avl_update_height(x);
    
    	return x;
}

block_header_t* avl_rotate_left(block_header_t *x) {
    	if (!x || !x->right) return x;
    
    	block_header_t *y = x->right;
    	block_header_t *T2 = y->left;
    
    	y->left = x;
    	x->right = T2;
    
    	avl_update_height(x);
    	avl_update_height(y);
    
    	return y;
}

block_header_t* avl_balance(block_header_t *node) {
    	if (!node) return NULL;
    
    	avl_update_height(node);
    	int balance = avl_balance_factor(node);
    
    	// L is heavier
    	if (balance > 1) {
        	// LR
        	if (avl_balance_factor(node->left) < 0) {
            		node->left = avl_rotate_left(node->left);
        	}
        	// LL 
        	return avl_rotate_right(node);
    	}
    
        // R is heavier	
	if (balance < -1) {
        	// RL
        	if (avl_balance_factor(node->right) > 0) {
            		node->right = avl_rotate_right(node->right);
        	}
        	// RR 
        	return avl_rotate_left(node);
    	}
    
    	return node;
}

block_header_t* avl_insert(block_header_t *root, block_header_t *block) {
    	if (!root) {
        	block->left = NULL;
        	block->right = NULL;
        	block->height = 1;
        	return block;
    	}
    
    	if (block->size < root->size) {
        	root->left = avl_insert(root->left, block);
    	} else if (block->size > root->size) {
        	root->right = avl_insert(root->right, block);
    	} else {
        	if ((uintptr_t)block < (uintptr_t)root) {
            		root->left = avl_insert(root->left, block);
        	} else {
            		root->right = avl_insert(root->right, block);
        	}
    	}
    
    	return avl_balance(root);
}

block_header_t* avl_find_min(block_header_t *node) {
    	while (node && node->left) {
        	node = node->left;
    	}
    	return node;
}

block_header_t* avl_delete_node(block_header_t *root, block_header_t *block) {
    	if (!root) return NULL;
    
    	if (block->size < root->size) {
        	root->left = avl_delete_node(root->left, block);
    	} else if (block->size > root->size) {
        	root->right = avl_delete_node(root->right, block);
    	} else {
        	if (root == block) {
            		if (!root->left || !root->right) {
                		// Node has 0-1 child 
                		block_header_t *temp = root->left ? root->left : root->right;
                		return temp;
            		} else {
                		// Node hav 2 children 
                		block_header_t *temp = avl_find_min(root->right);
                		
                		root->size = temp->size;
                		root->is_free = temp->is_free;
                		root->next = temp->next;
                		root->prev = temp->prev;
                	
                		root->right = avl_delete_node(root->right, temp);
            		}
        	} else {
            		// Continue search
            		root->right = avl_delete_node(root->right, block);
        	}
    	}
    
    	return avl_balance(root);
}

block_header_t* avl_delete(block_header_t *root, block_header_t *block) {
    	if (!root || !block) return NULL;
    	return avl_delete_node(root, block);
}

block_header_t* avl_find_first_fit(block_header_t *root, size_t requested_size) {
    	if (!root) return NULL;
    
    	block_header_t *best = NULL;
    
    	if (root->size >= requested_size) {
        	best = root;
        	block_header_t *left_best = avl_find_first_fit(root->left, requested_size);
        	if (left_best) {
            		best = left_best;
        	}
    	} else {
        	best = avl_find_first_fit(root->right, requested_size);
    	}
    
    	return best;
}

static block_header_t* avl_find_by_ptr_recursive(block_header_t *root, void *ptr) {
    	if (!root) return NULL;
    
    	void *data_start = (void *)((char *)root + sizeof(block_header_t));
    	void *data_end = (void *)((char *)root + root->size);
    
    	if (ptr >= data_start && ptr < data_end) return root;
    
    	block_header_t *left_result = avl_find_by_ptr_recursive(root->left, ptr);
    	if (left_result) return left_result;
    
    	return avl_find_by_ptr_recursive(root->right, ptr);
}

block_header_t* avl_find_by_ptr(block_header_t *root, void *ptr) {
    	if (!root || !ptr) return NULL;
    	return avl_find_by_ptr_recursive(root, ptr);
}

void avl_inorder_traversal(block_header_t *root, void (*callback)(block_header_t *)) {
    	if (!root) return;
    	avl_inorder_traversal(root->left, callback);
    	if (callback) callback(root);
    	avl_inorder_traversal(root->right, callback);
}

void avl_free_tree(block_header_t *root) {
    	if (!root) return;
    	avl_free_tree(root->left);
    	avl_free_tree(root->right);
    	
    	root->left = NULL;
    	root->right = NULL;
}
