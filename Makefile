# SwiftFlow Makefile
# GoPU.inc © 2026

# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -g -I. -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm -lpthread

# Directories
SRC_DIR = .
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SOURCES = $(SRC_DIR)/lexer.c $(SRC_DIR)/parser.c $(SRC_DIR)/swf.c
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/swiftflow

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Link executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

# Development build with optimizations
dev: CFLAGS += -O2 -DDEBUG
dev: clean all

# Release build with maximum optimizations
release: CFLAGS += -O3 -DNDEBUG
release: clean all

# Debug build with symbols
debug: CFLAGS += -O0 -g3 -DDEBUG -fsanitize=address -fsanitize=undefined
debug: LDFLAGS += -fsanitize=address -fsanitize=undefined
debug: clean all

# Test target
test: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET) test.sfl || true

# REPL mode
repl: $(TARGET)
	@./$(TARGET)

# Install system-wide
install: $(TARGET)
	@echo "Installing SwiftFlow..."
	@sudo cp $(TARGET) /usr/local/bin/swiftflow
	@sudo mkdir -p /usr/local/lib/swiftflow
	@sudo cp -r lib/* /usr/local/lib/swiftflow/ 2>/dev/null || true
	@echo "SwiftFlow installed successfully!"

# Uninstall
uninstall:
	@echo "Uninstalling SwiftFlow..."
	@sudo rm -f /usr/local/bin/swiftflow
	@sudo rm -rf /usr/local/lib/swiftflow
	@echo "SwiftFlow uninstalled!"

# Create package (Debian/Ubuntu)
package: release
	@mkdir -p package/DEBIAN package/usr/local/bin package/usr/local/lib/swiftflow
	@cp $(TARGET) package/usr/local/bin/swiftflow
	@cp -r lib/* package/usr/local/lib/swiftflow/ 2>/dev/null || true
	@cp control package/DEBIAN/
	@dpkg-deb --build package swiftflow_2.0_amd64.deb
	@rm -rf package

# Create distribution tarball
dist: release
	@mkdir -p dist/swiftflow-2.0
	@cp $(TARGET) dist/swiftflow-2.0/
	@cp *.c *.h Makefile README.md LICENSE dist/swiftflow-2.0/
	@cp -r lib examples docs dist/swiftflow-2.0/ 2>/dev/null || true
	@tar -czf swiftflow-2.0.tar.gz -C dist swiftflow-2.0
	@rm -rf dist

# Clean build artifacts
clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR) package dist *.deb *.tar.gz

# Run with Valgrind for memory checking
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Profile with gprof
profile: CFLAGS += -pg
profile: LDFLAGS += -pg
profile: clean $(TARGET)
	./$(TARGET) benchmark.sfl
	gprof $(TARGET) gmon.out > profile.txt

# Format code
format:
	@find . -name "*.c" -o -name "*.h" | xargs clang-format -i

# Static analysis
analyze:
	scan-build make

# Dependencies
deps:
	@echo "Installing dependencies..."
	@sudo apt-get update
	@sudo apt-get install -y build-essential clang valgrind gprof scan-build

# Help
help:
	@echo "SwiftFlow Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build SwiftFlow (default)"
	@echo "  dev       - Development build with optimizations"
	@echo "  release   - Release build with maximum optimizations"
	@echo "  debug     - Debug build with sanitizers"
	@echo "  test      - Run tests"
	@echo "  repl      - Run REPL"
	@echo "  install   - Install system-wide"
	@echo "  uninstall - Uninstall"
	@echo "  package   - Create Debian package"
	@echo "  dist      - Create distribution tarball"
	@echo "  clean     - Clean build artifacts"
	@echo "  valgrind  - Run with Valgrind memory checker"
	@echo "  profile   - Profile with gprof"
	@echo "  format    - Format code with clang-format"
	@echo "  analyze   - Static analysis with scan-build"
	@echo "  deps      - Install build dependencies"
	@echo "  help      - Show this help"

.PHONY: all directories dev release debug test repl install uninstall package dist clean valgrind profile format analyze deps help
