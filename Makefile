CC = gcc
CFLAGS = -std=c99 -Wall -Os -I.  # -Os pour optimisation taille
TARGET = swf
SOURCES = swf.c lexer.c parser.c

all: $(TARGET)

$(TARGET): $(SOURCES) common.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) *.o

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run