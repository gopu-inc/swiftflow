// string.h
#ifndef SWIFT_STRING_H
#define SWIFT_STRING_H

#include "common.h"

// Initialisation du module (si besoin de constantes globales)
void init_string_module(void);

// Fonctions natives exposées
int   str_native_len(const char* str);
char* str_native_upper(const char* str);
char* str_native_lower(const char* str);
char* str_native_sub(const char* str, int start, int end);
char* str_native_replace(const char* str, const char* search, const char* replace);
int   str_native_find(const char* str, const char* search);

#endif

