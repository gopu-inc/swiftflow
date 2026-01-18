CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = swf
SOURCES = swf.c lexer.c parser.c
HEADERS = common.h
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

test: $(TARGET)
	@echo "=== Testing ==="
	@./$(TARGET) test1.swf

.PHONY: all clean test
