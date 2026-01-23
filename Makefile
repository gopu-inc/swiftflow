# SwiftFlow Interpreter Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include
LDFLAGS = -lm

# Détection du système d'exploitation
ifeq ($(OS),Windows_NT)
    CFLAGS += -D_WIN32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CFLAGS += -D_POSIX_C_SOURCE=199309L
        LDLIBS += -lrt
    endif
    ifeq ($(UNAME_S),Darwin)
        CFLAGS += -D_DARWIN_C_SOURCE
    endif
endif

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/lexer.c \
       $(SRC_DIR)/parser.c \
       $(SRC_DIR)/ast.c \
       $(SRC_DIR)/swf.c \
       $(SRC_DIR)/jsonlib.c \
       $(SRC_DIR)/interpreter.c

# Object files
OBJS = $(OBJ_DIR)/main.o \
       $(OBJ_DIR)/lexer.o \
       $(OBJ_DIR)/parser.o \
       $(OBJ_DIR)/ast.o \
       $(OBJ_DIR)/swf.o \
       $(OBJ_DIR)/jsonlib.o \
       $(OBJ_DIR)/interpreter.o

# Binary
TARGET = $(BIN_DIR)/swift

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# Link executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# Compile each source file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Test
test: all
	@echo "Testing..."
	@./$(BIN_DIR)/swift

# Install
install: all
	cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin"

# Phony targets
.PHONY: all clean test install directories
