CC = gcc
CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-format-truncation

# Python configuration
PYTHON_VERSION = 3.9
PYTHON_INCLUDE = -I/usr/include/python$(PYTHON_VERSION)
PYTHON_LIB = -lpython$(PYTHON_VERSION) -lpthread -ldl -lutil -lm

OBJS = swf.o lexer.o parser.o io.o net.o sys.o http.o json.o stdlib.o pytx.o

swift: $(OBJS)
	$(CC) $(CFLAGS) -o swift $(OBJS) -lm -lsqlite3 -lcurl $(PYTHON_LIB)

%.o: %.c
	$(CC) $(CFLAGS) $(PYTHON_INCLUDE) -c $< -o $@

clean:
	rm -f *.o swift

.PHONY: clean

