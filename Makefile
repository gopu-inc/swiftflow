CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g
LDFLAGS = -lm

SOURCES = lexer.c parser.c swf.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = swf

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

test: $(TARGET)
	./$(TARGET) examples/test.sf

repl: $(TARGET)
	./$(TARGET)

.PHONY: all clean test repl
