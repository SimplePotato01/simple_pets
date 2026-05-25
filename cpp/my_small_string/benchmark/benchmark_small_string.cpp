#include <benchmark/benchmark.h>
#include "small_string.h"
#include <string>

static void BM_StdStringShortConstruct(benchmark::State& state) {
    for (auto _ : state) {
        std::string s("hello");
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_StdStringShortConstruct);

static void BM_SmallStringShortConstruct(benchmark::State& state) {
    for (auto _ : state) {
        SmallString s("hello");
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_SmallStringShortConstruct);

static void BM_StdStringLongConstruct(benchmark::State& state) {
    const char* long_str = "this is a very long string that definitely exceeds the SSO capacity of typical implementations";
    for (auto _ : state) {
        std::string s(long_str);
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_StdStringLongConstruct);

static void BM_SmallStringLongConstruct(benchmark::State& state) {
    const char* long_str = "this is a very long string that definitely exceeds the SSO capacity of typical implementations";
    for (auto _ : state) {
        SmallString s(long_str);
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_SmallStringLongConstruct);

static void BM_StdStringPushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::string s;
        for (int i = 0; i < 100; ++i) s.push_back('a');
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_StdStringPushBack);

static void BM_SmallStringPushBack(benchmark::State& state) {
    for (auto _ : state) {
        SmallString s;
        for (int i = 0; i < 100; ++i) s.push_back('a');
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_SmallStringPushBack);

BENCHMARK_MAIN();
