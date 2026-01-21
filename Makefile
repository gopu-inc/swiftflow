CC = gcc
CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wno-unused-function -Wno-format-truncation -Wno-implicit-function-declaration
LDFLAGS = -lm

SRCS = lexer.c parser.c swf.c
OBJS = $(SRCS:.c=.o)
TARGET = swift

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	chmod +x $(TARGET)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)