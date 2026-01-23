
# SwiftFlow Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I.
LDFLAGS = -lm -lpcre -ltommath -lreadline
TARGET = swift
SRC = src/
SOURCES = $(SRC)main.c $(SRC)lexer.c $(SRC)parser.c $(SRC)ast.c $(SRC)interpreter.c $(SRC)jsonlib.c \
          $(SRC)mathlib.c $(SRC)regexlib.c $(SRC)repl.c swf.c $(SRC)llvm_backend.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

test: $(TARGET)
	./$(TARGET) test.swf

.PHONY: all clean install uninstall test
