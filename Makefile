# Dans ton Makefile
CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-format-truncation
LIBS = -lm -lsqlite3

all: swift

swift: swf.o lexer.o parser.o io.o
	$(CC) $(CFLAGS) -o swift swf.o lexer.o parser.o io.o $(LIBS)

swf.o: swf.c common.h io.h
	$(CC) $(CFLAGS) -c swf.c -o swf.o

lexer.o: lexer.c common.h
	$(CC) $(CFLAGS) -c lexer.c -o lexer.o

parser.o: parser.c common.h
	$(CC) $(CFLAGS) -c parser.c -o parser.o

io.o: io.c common.h io.h
	$(CC) $(CFLAGS) -c io.c -o io.o

clean:
	rm -f *.o swift swift.db
