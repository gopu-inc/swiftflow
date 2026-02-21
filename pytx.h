// pytx.h
#ifndef PYTX_H
#define PYTX_H

#include <python3.9/Python.h>
#include <stdbool.h>

// Structure pour stocker un module Python importé
typedef struct {
    PyObject* py_module;
    char* module_name;
    PyObject* py_dict;
    bool is_loaded;
} PyTxModule;

// Fonctions du module pytx
void init_pytx(void);
PyTxModule* pytx_import(const char* module_name);
void pytx_cleanup(void);

#endif
