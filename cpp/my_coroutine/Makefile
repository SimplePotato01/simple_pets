CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -Iinclude
LDFLAGS = 

TARGET_DEMO = demo
TARGET_TEST = test_runner
TARGET_BENCH = bench_runner

SRC_DIR = src
TEST_DIR = tests
BENCH_DIR = bench

all: $(TARGET_DEMO) $(TARGET_TEST) $(TARGET_BENCH)

$(TARGET_DEMO): $(SRC_DIR)/main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(TARGET_TEST): $(TEST_DIR)/test_generator.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(TARGET_BENCH): $(BENCH_DIR)/bench_generator.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

run: $(TARGET_DEMO)
	./$(TARGET_DEMO)

run_test: $(TARGET_TEST)
	./$(TARGET_TEST)

run_bench: $(TARGET_BENCH)
	./$(TARGET_BENCH)

clean:
	rm -f $(TARGET_DEMO) $(TARGET_TEST) $(TARGET_BENCH)

.PHONY: all run run_test run_bench clean
