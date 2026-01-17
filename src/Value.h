#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>

// ===== TYPES DE DONNÉES =====
typedef enum {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, 
    VAL_STRING, VAL_FUNCTION, VAL_NATIVE, VAL_RETURN_SIG,
    VAL_ARRAY, VAL_OBJECT, VAL_CLOSURE, VAL_GENERATOR,
    VAL_PROMISE, VAL_ERROR, VAL_REGEX, VAL_BUFFER,
    VAL_DATE, VAL_SYMBOL, VAL_BIGINT, VAL_ITERATOR,
    VAL_BREAK, VAL_CONTINUE, VAL_EXPR
} ValueType;

// Déclarations anticipées
typedef struct ASTNode ASTNode;
typedef struct Environment Environment;
typedef struct Value Value;

// Structure Value complète
struct Value {
    ValueType type;
    union {
        int boolean;
        int64_t integer;
        double number;
        char* string;
        struct {
            Value* items;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            Value* values;
            int count;
            int capacity;
        } object;
        struct {
            char* name;
            Value (*fn)(Value*, int, Environment*);
        } native;
        struct {
            ASTNode* declaration;
            Environment* closure;
        } function;
        struct {
            ASTNode* generator;
            Environment* closure;
            int state;
        } generator;
        struct {
            int resolved;
            Value value;
        } promise;
        struct {
            char* message;
            Value data;
        } error;
    };
};

// Fonctions utilitaires pour Value
Value make_number(double value);
Value make_string(const char* str);
Value make_bool(int b);
Value make_nil();
Value make_array();
Value make_object();

void array_push(Value* array, Value item);
void object_set(Value* obj, const char* key, Value value);

#endif
