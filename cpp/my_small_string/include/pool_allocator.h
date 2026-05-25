#pragma once

#include <cstddef>
#include <vector>

class PoolAllocator {
public:
    static PoolAllocator& instance();

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);

    ~PoolAllocator();

private:
    PoolAllocator();

    struct FreeListNode {
        FreeListNode* next;
    };

    struct Pool {
        size_t block_size;
        FreeListNode* free_list;
        std::vector<void*> blocks;  // all allocated blocks (for cleanup)
    };

    std::vector<Pool> pools_;

    static constexpr size_t BLOCK_SIZES[] = {32, 64, 128, 256, 512, 1024, 2048};
    static constexpr size_t NUM_BLOCK_SIZES = sizeof(BLOCK_SIZES) / sizeof(BLOCK_SIZES[0]);

    size_t get_pool_index(size_t size) const;
    void* allocate_from_pool(size_t index);
    void deallocate_to_pool(size_t index, void* ptr);
};
