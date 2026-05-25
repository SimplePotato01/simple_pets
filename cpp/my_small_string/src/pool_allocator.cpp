#include "pool_allocator.h"
#include <cstdlib>
#include <cassert>

PoolAllocator::PoolAllocator() {
    pools_.reserve(NUM_BLOCK_SIZES);
    for (size_t i = 0; i < NUM_BLOCK_SIZES; ++i) {
        Pool pool{BLOCK_SIZES[i], nullptr, {}};
        pools_.push_back(pool);
    }
}

PoolAllocator::~PoolAllocator() {
    for (auto& pool : pools_) {
        for (void* block : pool.blocks) {
            std::free(block);
        }
    }
}

PoolAllocator& PoolAllocator::instance() {
    static PoolAllocator alloc;
    return alloc;
}

size_t PoolAllocator::get_pool_index(size_t size) const {
    for (size_t i = 0; i < NUM_BLOCK_SIZES; ++i) {
        if (size <= BLOCK_SIZES[i])
            return i;
    }
    return NUM_BLOCK_SIZES;  // too large
}

void* PoolAllocator::allocate(size_t size) {
    size_t idx = get_pool_index(size);
    if (idx < NUM_BLOCK_SIZES) {
        return allocate_from_pool(idx);
    } else {
        return std::malloc(size);
    }
}

void* PoolAllocator::allocate_from_pool(size_t index) {
    Pool& pool = pools_[index];
    if (pool.free_list) {
        void* ptr = pool.free_list;
        pool.free_list = pool.free_list->next;
        return ptr;
    } else {
        void* new_block = std::malloc(pool.block_size);
        if (!new_block) throw std::bad_alloc();
        pool.blocks.push_back(new_block);
        return new_block;
    }
}

void PoolAllocator::deallocate(void* ptr, size_t size) {
    if (!ptr) return;
    size_t idx = get_pool_index(size);
    if (idx < NUM_BLOCK_SIZES) {
        deallocate_to_pool(idx, ptr);
    } else {
        std::free(ptr);
    }
}

void PoolAllocator::deallocate_to_pool(size_t index, void* ptr) {
    Pool& pool = pools_[index];
    auto* node = static_cast<FreeListNode*>(ptr);
    node->next = pool.free_list;
    pool.free_list = node;
}
