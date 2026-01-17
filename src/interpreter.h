#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "value.h"

// Environnement
struct Environment {
    struct Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
};

typedef struct Environment Environment;

// Fonctions d'environnement
Environment* new_environment(Environment* enclosing);
void env_define(Environment* env, char* name, Value value);
int env_get(Environment* env, char* name, Value* out);

// Variables globales
extern Environment* global_env;

#endif
