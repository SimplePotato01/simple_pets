# SmallString – SSO string with custom pool allocator
`SmallString` is a C++17 implementation of a string class
## Features
**Small String Optimization (SSO)** – strings up to 22 bytes are stored inline, no heap allocation.  
**Custom pool allocator** – long strings use a fixed‑block memory pool (sizes 32, 64, …, 2048 bytes) to reduce allocation overhead.  
**Full STL‑like interface** – constructors, `push_back`, `append`, iterators, comparisons, etc.  
**Performance benchmarks** – compared against `std::string` using Google Benchmark.  
## Build
```bash
make  
make help
make test
