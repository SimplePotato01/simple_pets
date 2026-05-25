#include "generator.hpp"
#include <cassert>
#include <iostream>

void test_sequence() {
    std::cout << "Testing: sequence..." << std::endl;
    Generator<int> gen([](Generator<int>& self) {
        for (int i = 1; i <= 3; ++i) {
            self.yield(i);
        }
    });
    
    assert(gen.next() == 1);
    assert(gen.next() == 2);
    assert(gen.next() == 3);
    assert(gen.next() == std::nullopt);
    std::cout << "  OK" << std::endl;
}

void test_pool_reuse() {
    std::cout << "Testing: pool reuse..." << std::endl;
    for (int i = 0; i < 20; ++i) {
        Generator<int> gen([](Generator<int>& self) {
            self.yield(42);
        });
        assert(gen.next() == 42);
        assert(gen.next() == std::nullopt);
    }
    std::cout << "  OK" << std::endl;
}

void test_early_exit() {
    std::cout << "Testing: early exit..." << std::endl;
    Generator<int> gen([](Generator<int>& self) {
        self.yield(1);
    });
    assert(gen.next() == 1);
    assert(gen.next() == std::nullopt);
    std::cout << "  OK" << std::endl;
}

int main() {
    std::cout << "Running tests..." << std::endl;
    test_sequence();
    test_pool_reuse();
    test_early_exit();
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
