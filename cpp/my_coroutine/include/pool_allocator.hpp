#ifndef POOL_ALLOCATOR_HPP
#define POOL_ALLOCATOR_HPP

#include <array>
#include <bitset>
#include <cstddef>
#include <stdexcept>

class StaticStackPool {
public:
    static constexpr std::size_t STACK_SIZE = 64 * 1024;
    static constexpr std::size_t MAX_COROUTINES = 16;

    using Stack = std::array<std::byte, STACK_SIZE>;

    static void* allocate() {
        for (std::size_t i = 0; i < MAX_COROUTINES; ++i) {
            if (!used_[i]) {
                used_.set(i);
                return stacks_[i].data();
            }
        }
        throw std::runtime_error("StaticStackPool: no free stacks available");
    }

    static void deallocate(void* ptr) {
        if (ptr == nullptr) return;
        for (std::size_t i = 0; i < MAX_COROUTINES; ++i) {
            if (stacks_[i].data() == ptr) {
                used_.reset(i);
                return;
            }
        }
    }

    static std::size_t get_used_count() {
        return used_.count();
    }

private:
    static inline std::array<Stack, MAX_COROUTINES> stacks_;
    static inline std::bitset<MAX_COROUTINES> used_;
};

#endif
