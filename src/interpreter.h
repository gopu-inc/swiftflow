#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "value.h"

// Environnement
typedef struct Environment {
    struct Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
} Environment;

Environment* new_environment(Environment* enclosing);
void env_define(Environment* env, char* name, Value value);
int env_get(Environment* env, char* name, Value* out);

// Variables globales
extern Environment* global_env;

// Fonctions d'évaluation
Value eval(void* node, Environment* env);  // Prototype corrigé

#endif
