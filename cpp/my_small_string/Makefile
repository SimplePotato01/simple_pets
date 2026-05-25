CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -pthread

SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = obj

SOURCES = $(SRC_DIR)/small_string.cpp $(SRC_DIR)/pool_allocator.cpp
OBJECTS = $(OBJ_DIR)/small_string.o $(OBJ_DIR)/pool_allocator.o

TEST_SRC = $(TEST_DIR)/test_small_string.cpp
TEST_BIN = run_tests

.PHONY: all clean test

all: $(TEST_BIN)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(OBJECTS) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(OBJECTS) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	@echo "========== Running tests =========="
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR) $(TEST_BIN)
	@echo "Cleanup complete"
