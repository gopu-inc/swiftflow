#ifndef CLASS_H
#define CLASS_H

#include "common.h"

// Structure pour définir une classe (ClassDef)
typedef struct {
    char* name;
    char* parent_name;
    // On pourra ajouter ici des tableaux pour les méthodes plus tard
} ClassDef;

// Prototypes
void init_class_system();
ClassDef* create_class_def(const char* name, const char* parent);
void register_class(ClassDef* def);
ClassDef* find_class(const char* name);

#endif

