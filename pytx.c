#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pytx.h"
#include "common.h"

static PyTxModule py_modules[100];
static int py_module_count = 0;

void init_pytx(void) {
    Py_Initialize();
    // Ajouter le chemin courant au path Python
    PyRun_SimpleString("import sys; sys.path.append('.')");
    
}

PyTxModule* pytx_import(const char* module_name) {
    if (!module_name) return NULL;
    
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
        printf("%s[PYTX ERROR]%s Failed to import Python module: %s\n", 
               COLOR_RED, COLOR_RESET, module_name);
        return NULL;
    }
    
    // Stocker dans le cache
    if (py_module_count < 100) {
        PyTxModule* mod = &py_modules[py_module_count++];
        mod->py_module = py_module;
        mod->module_name = strdup(module_name);
        mod->py_dict = PyModule_GetDict(py_module);
        mod->is_loaded = true;
        
        printf("%s[PYTX]%s Imported Python module: %s\n", 
               COLOR_GREEN, COLOR_RESET, module_name);
        return mod;
    }
    
    Py_DECREF(py_module);
    return NULL;
}
char* pytx_execute(const char* python_code) {
    if (!python_code) return NULL;
    
    // Obtenir le module principal et son dictionnaire
    PyObject* main_module = PyImport_AddModule("__main__");
    PyObject* main_dict = PyModule_GetDict(main_module);
    
    // Exécuter le code Python
    PyObject* result = PyRun_String(python_code, Py_file_input, main_dict, main_dict);
    
    if (!result) {
        PyErr_Print();
        return strdup("");  // Retourne chaîne vide en cas d'erreur
    }
    
    Py_DECREF(result);
    return strdup("[Python code executed successfully]");
}
void pytx_cleanup(void) {
    for (int i = 0; i < py_module_count; i++) {
        if (py_modules[i].py_module) {
            Py_DECREF(py_modules[i].py_module);
        }
        if (py_modules[i].module_name) {
            free(py_modules[i].module_name);
        }
    }
}
