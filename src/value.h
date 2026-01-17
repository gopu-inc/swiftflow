#ifndef VALUE_H
#define VALUE_H

#include "common.h"

// Déclarations anticipées
typedef struct ASTNode ASTNode;
typedef struct Environment Environment;

// Structure Value complète
typedef struct Value {
    ValueType type;
    union {
        int boolean;
        int64_t integer;
        double number;
        char* string;
        struct {
            struct Value* items;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            struct Value* values;
            int count;
            int capacity;
        } object;
        struct {
            char* name;
            struct Value (*fn)(struct Value*, int, Environment*);
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
            struct Value value;
        } promise;
        struct {
            char* message;
            struct Value data;
        } error;
    };
} Value;

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
