# Makefile simple pour SwiftVelox
CC = cc
CFLAGS = -Os -s -static -Wall
TARGET = swiftvelox

.PHONY: all clean

all: $(TARGET)

$(TARGET): src/main.c src/compiler.c src/runtime.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c src/compiler.c src/runtime.c

clean:
	rm -f $(TARGET) *.o examples/*.out

test: $(TARGET)
	./$(TARGET) examples/hello.svx
