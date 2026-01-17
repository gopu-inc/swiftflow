CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I./src
LIBS = -lm -lpthread -lcurl -lsqlite3
SRC_DIR = src
OBJ_DIR = obj

SRCS = $(SRC_DIR)/main.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

TARGET = swift

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

test:
	./$(TARGET) test

repl:
	./$(TARGET) repl

.PHONY: all clean test repl
