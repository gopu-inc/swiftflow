CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I./src
LIBS = -lm -lpthread -lcurl -lsqlite3
SRCS = src/main.c
TARGET = swiftvelox

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
