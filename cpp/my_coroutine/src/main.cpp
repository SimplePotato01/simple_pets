#include <iostream>
#include "generator.hpp"

int main() {
    std::cout << "=== Generator Demo ===" << std::endl;
    
    Generator<int> gen([](Generator<int>& self) {
        for (int i = 0; i < 5; ++i) {
            self.yield(i);
        }
    });

    std::cout << "Simple sequence (0-4): ";
    while (auto val = gen.next()) {
        std::cout << *val << " ";
    }
    std::cout << std::endl;

    Generator<int> fib([](Generator<int>& self) {
        int a = 0, b = 1;
        for (int i = 0; i < 10; ++i) {
            self.yield(a);
            int next = a + b;
            a = b;
            b = next;
        }
    });

    std::cout << "\nFibonacci (first 10): ";
    while (auto val = fib.next()) {
        std::cout << *val << " ";
    }
    std::cout << std::endl;

    return 0;
}
