// vm.h - Interface de la VM
#ifndef VM_H
#define VM_H

#include "common.h"

// Structure Value complète
struct Value {
    ValueType type;
    union {
        int intVal;
        float floatVal;
        bool boolVal;
        char* stringVal;
    } as;
};

// Structure VM
typedef struct VM {
    Value stack[STACK_SIZE];
    int stackTop;
    Value* globals[MAX_SYMBOLS];
    char* globalNames[MAX_SYMBOLS];
    int globalCount;
    bool hadError;
    bool debugMode;
} VM;

// Fonctions de la VM
VM* createVM();
void freeVM(VM* vm);
void interpret(VM* vm, ASTNode* program);
void runFile(const char* filename, bool debug);
void repl();

#endif