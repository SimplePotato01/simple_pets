#include "generator.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>

constexpr long long N = 10'000'000;

// Обычная функция – просто суммируем числа
long long sum_range_naive() {
    long long sum = 0;
    for (long long i = 0; i < N; ++i) {
        sum += i;
    }
    return sum;
}

// Генератор, который yield'ит каждое число
long long sum_range_generator() {
    long long sum = 0;
    Generator<long long> gen([](Generator<long long>& self) {
        for (long long i = 0; i < N; ++i) {
            self.yield(i);
        }
    });
    while (auto val = gen.next()) {
        sum += *val;
    }
    return sum;
}

template<typename Func>
double measure(Func f) {
    auto start = std::chrono::high_resolution_clock::now();
    volatile auto result = f(); // предотвращаем оптимизацию
    (void)result;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "BENCHMARK: Sum of 0.." << N-1 << std::endl;
    std::cout << "==========================================" << std::endl;
    
    double t_naive = measure(sum_range_naive);
    double t_gen = measure(sum_range_generator);
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Naive loop:          " << t_naive << " sec" << std::endl;
    std::cout << "Generator:           " << t_gen << " sec" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Overhead ratio:      " << (t_gen / t_naive) << "x" << std::endl;
    std::cout << "Absolute overhead:   " << (t_gen - t_naive) << " sec" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    return 0;
}
