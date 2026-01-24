CC = gcc
CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra
LDFLAGS = -lm
TARGET = swift
SOURCES = swf.c lexer.c parser.c io.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: all
	./$(TARGET)

test: all
	./$(TARGET) test.swf

.PHONY: all clean run test
