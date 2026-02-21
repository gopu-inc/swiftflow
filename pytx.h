// pytx.h
#ifndef PYTX_H
#define PYTX_H

#include <python3.9/Python.h>
#include "common.h"

typedef struct {
    PyObject* py_module;      // Module Python importé
    char* module_name;         // Nom du module
    PyObject* py_dict;         // Dictionnaire des fonctions/méthodes
    bool is_loaded;            
} PyTxModule;

// Cache des modules Python importés
static PyTxModule py_modules[100];
static int py_module_count = 0;

// Initialisation de l'interpréteur Python
void init_pytx(void) {
    Py_Initialize();
    // Ajouter le chemin des modules système
    PyRun_SimpleString("import sys; sys.path.append('.')");
}

// Import d'un module Python
PyTxModule* pytx_import(const char* module_name) {
    // Vérifier le cache
    for (int i = 0; i < py_module_count; i++) {
        if (strcmp(py_modules[i].module_name, module_name) == 0) {
            return &py_modules[i];
        }
    }
    
    // Importer le module Python
    PyObject* py_name = PyUnicode_FromString(module_name);
    PyObject* py_module = PyImport_Import(py_name);
    Py_DECREF(py_name);
    
    if (!py_module) {
        PyErr_Print();
        return NULL;
    }
    
    // Stocker dans le cache
    PyTxModule* mod = &py_modules[py_module_count++];
    mod->py_module = py_module;
    mod->module_name = str_copy(module_name);
    mod->py_dict = PyModule_GetDict(py_module);
    mod->is_loaded = true;
    
    return mod;
}
