# SwiftFlow Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include
LDFLAGS = -lm
TARGET = swiftflow

# Liste des fichiers source
SOURCES = src/main.c src/lexer.c src/parser.c src/ast.c src/interpreter.c \
          src/jsonlib.c src/swf.c src/llvm_backend.c
OBJECTS = $(SOURCES:.c=.o)

# Détection des bibliothèques optionnelles
HAS_PCRE = $(shell pkg-config --exists libpcre 2>/dev/null && echo 1 || echo 0)
HAS_TOMMATH = $(shell pkg-config --exists libtommath 2>/dev/null && echo 1 || echo 0)
HAS_READLINE = $(shell pkg-config --exists readline 2>/dev/null && echo 1 || echo 0)

ifeq ($(HAS_PCRE),1)
LDFLAGS += -lpcre
CFLAGS += -DHAVE_PCRE
SOURCES += src/regexlib.c
endif

ifeq ($(HAS_TOMMATH),1)
LDFLAGS += -ltommath
CFLAGS += -DHAVE_TOMMATH
SOURCES += src/mathlib.c
endif

ifeq ($(HAS_READLINE),1)
LDFLAGS += -lreadline
CFLAGS += -DHAVE_READLINE
SOURCES += src/repl.c
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Règle pour compiler les fichiers .c dans src/
src/%.o: src/%.c include/%.h
	$(CC) $(CFLAGS) -c $< -o $@

# Pour les fichiers sans .h correspondant
src/main.o: src/main.c include/common.h include/lexer.h include/parser.h include/ast.h include/interpreter.h
	$(CC) $(CFLAGS) -c $< -o $@

src/lexer.o: src/lexer.c include/lexer.h include/common.h
	$(CC) $(CFLAGS) -c $< -o $@

src/parser.o: src/parser.c include/parser.h include/common.h include/ast.h
	$(CC) $(CFLAGS) -c $< -o $@

src/ast.o: src/ast.c include/ast.h include/common.h
	$(CC) $(CFLAGS) -c $< -o $@

src/interpreter.o: src/interpreter.c include/interpreter.h include/common.h
	$(CC) $(CFLAGS) -c $< -o $@

src/jsonlib.o: src/jsonlib.c include/jsonlib.h include/common.h include/interpreter.h
	$(CC) $(CFLAGS) -c $< -o $@

src/swf.o: src/swf.c include/common.h
	$(CC) $(CFLAGS) -c $< -o $@

src/llvm_backend.o: src/llvm_backend.c include/backend.h include/common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	mkdir -p /usr/local/include/swiftflow
	cp include/*.h /usr/local/include/swiftflow/

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	rm -rf /usr/local/include/swiftflow

test: $(TARGET)
	./$(TARGET) test.swf

# Nettoyer les balises des fichiers
clean-tags:
	find src include -type f -name "*.c" -o -name "*.h" | xargs sed -i '/^\[file name\]:/d;/^\[file content begin\]/d;/^\[file content end\]/d'

.PHONY: all clean install uninstall test clean-tags
