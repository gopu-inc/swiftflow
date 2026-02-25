#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "class.h"

static ClassDef* classes[6400];
static int class_count = 0;

void init_class_system() {
    class_count = 0;
}

ClassDef* create_class_def(const char* name, const char* parent) {
    ClassDef* def = malloc(sizeof(ClassDef));
    if (!def) return NULL;
    
    def->name = str_copy(name);
    def->parent_name = parent ? str_copy(parent) : NULL;
    
        return def;
}

void register_class(ClassDef* def) {
    if (class_count < 100) {
        classes[class_count++] = def;
        
    } else {
        fprintf(stderr, "Erreur : you are defield max class (max 100).\n");
    }
}

ClassDef* find_class(const char* name) {
    for (int i = 0; i < class_count; i++) {
        if (strcmp(classes[i]->name, name) == 0) return classes[i];
    }
    return NULL;
}

