# SwiftFlow Interpreter with Libraries Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include -I./libs/include
LDFLAGS = -lm -lgc -lpcre -ljansson -lreadline -ltommath -ltomcrypt

# Directories
SRC_DIR = src
INCLUDE_DIR = include
LIB_DIR = libs
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/frontend/lexer.c \
       $(SRC_DIR)/frontend/parser.c \
       $(SRC_DIR)/frontend/ast.c \
       $(SRC_DIR)/runtime/swf.c \
       $(SRC_DIR)/interpreter/interpreter.c \
       $(SRC_DIR)/libs/mathlib.c \
       $(SRC_DIR)/libs/jsonlib.c \
       $(SRC_DIR)/libs/regexlib.c \
       $(SRC_DIR)/libs/repl.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Binary
TARGET = $(BIN_DIR)/swiftflow

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR)/frontend
	@mkdir -p $(OBJ_DIR)/interpreter
	@mkdir -p $(OBJ_DIR)/runtime
	@mkdir -p $(OBJ_DIR)/libs
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(LIB_DIR)/include

# Download and build libraries
libs: directories
	@echo "Downloading and building libraries..."
	@if [ ! -f "$(LIB_DIR)/include/gc.h" ]; then \
		echo "Downloading Boehm GC..."; \
		wget -q -O gc.tar.gz https://github.com/ivmai/bdwgc/releases/download/v8.2.2/gc-8.2.2.tar.gz; \
		tar -xzf gc.tar.gz && rm gc.tar.gz; \
		cd gc-8.2.2 && ./configure --prefix=$(PWD)/$(LIB_DIR) --disable-threads && make install; \
		cd .. && rm -rf gc-8.2.2; \
	fi
	@if [ ! -f "$(LIB_DIR)/include/jansson.h" ]; then \
		echo "Downloading Jansson..."; \
		wget -q -O jansson.tar.gz https://github.com/akheron/jansson/releases/download/v2.14/jansson-2.14.tar.gz; \
		tar -xzf jansson.tar.gz && rm jansson.tar.gz; \
		cd jansson-2.14 && cmake -DCMAKE_INSTALL_PREFIX=$(PWD)/$(LIB_DIR) -DJANSSON_BUILD_DOCS=OFF . && make install; \
		cd .. && rm -rf jansson-2.14; \
	fi
	@if [ ! -f "$(LIB_DIR)/include/tommath.h" ]; then \
		echo "Downloading LibTomMath..."; \
		git clone --depth 1 https://github.com/libtom/libtommath.git; \
		cd libtommath && make && cp tommath.h $(PWD)/$(LIB_DIR)/include/; \
		cp libtommath.a $(PWD)/$(LIB_DIR)/lib/; \
		cd .. && rm -rf libtommath; \
	fi

# Link executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) -L$(LIB_DIR)/lib

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

clean-all: clean
	rm -rf $(LIB_DIR)

# Run tests
test: all
	@echo "Running tests..."
	@./$(BIN_DIR)/swiftflow examples/hello.swf

# Install
install: all
	cp $(TARGET) /usr/local/bin/
	@echo "SwiftFlow interpreter installed to /usr/local/bin"

# REPL mode
repl: all
	@./$(BIN_DIR)/swiftflow -

# Development mode
dev: CFLAGS += -DDEBUG -fsanitize=address
dev: LDFLAGS += -fsanitize=address
dev: clean all

# Release mode
release: CFLAGS = -Wall -Wextra -std=c99 -O2 -I./include -I./libs/include
release: clean all

# Phony targets
.PHONY: all clean clean-all test install repl dev release directories libs

# Dependencies
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/frontend/lexer.o: $(SRC_DIR)/frontend/lexer.c $(INCLUDE_DIR)/lexer.h $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/frontend/parser.o: $(SRC_DIR)/frontend/parser.c $(INCLUDE_DIR)/parser.h $(INCLUDE_DIR)/ast.h $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/frontend/ast.o: $(SRC_DIR)/frontend/ast.c $(INCLUDE_DIR)/ast.h $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/runtime/swf.o: $(SRC_DIR)/runtime/swf.c $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/interpreter/interpreter.o: $(SRC_DIR)/interpreter/interpreter.c $(INCLUDE_DIR)/interpreter.h $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/libs/mathlib.o: $(SRC_DIR)/libs/mathlib.c $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/libs/jsonlib.o: $(SRC_DIR)/libs/jsonlib.c $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/libs/regexlib.o: $(SRC_DIR)/libs/regexlib.c $(INCLUDE_DIR)/common.h
$(OBJ_DIR)/libs/repl.o: $(SRC_DIR)/libs/repl.c $(INCLUDE_DIR)/common.h
