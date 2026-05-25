#include <iostream>
#include "generator.hpp"

int main() {
    std::cout << "=== Generator Demo ===" << std::endl;
    
    // Простой генератор от 0 до 4
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

    // Генератор чисел Фибоначчи
    std::cout << "\n=== Fibonacci (first 10) ===" << std::endl;
    Generator<int> fib([](Generator<int>& self) {
        int a = 0, b = 1;
        for (int i = 0; i < 10; ++i) {
            self.yield(a);
            int next = a + b;
            a = b;
            b = next;
        }
    });

    int count = 0;
    while (auto val = fib.next()) {
        std::cout << "F" << count++ << " = " << *val << std::endl;
    }

    // Демонстрация работы пула
    std::cout << "\n=== Pool usage demonstration ===" << std::endl;
    for (int i = 0; i < 5; ++i) {
        Generator<int> temp([](Generator<int>& self) {
            self.yield(42);
        });
        auto val = temp.next();
        std::cout << "Generator " << i << " produced: " << *val << std::endl;
    }

    return 0;
}
