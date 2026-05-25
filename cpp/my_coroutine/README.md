# Stackless Coroutine (Generator) in C++ without std::coroutine
Implementation of a simple **stackful coroutine** (generator) using `swapcontext` for context switching. Stacks are allocated from a static pool without using `new`/`delete`.
## Features
Template class `Generator<T>` with `yield()` and `next()` methods  
Static memory pool for coroutine stacks (64 KiB each, max 16 coroutines)  
No dynamic memory allocation (except `std::function` internal allocation)  
Performance benchmark comparing naive loop vs generator  
Unit tests  
## Build
```bash
make run        # Demo
make run_test
make clean
