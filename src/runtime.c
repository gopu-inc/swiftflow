#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// Fonctions de base
void sv_print(const char* msg) {
    printf("%s\n", msg);
}

void sv_print_int(int value) {
    printf("%d\n", value);
}

void sv_print_float(double value) {
    printf("%f\n", value);
}

// Gestion mémoire simple
void* sv_malloc(size_t size) {
    return malloc(size);
}

void sv_free(void* ptr) {
    free(ptr);
}

// Fonctions mathématiques
double sv_sin(double x) { return sin(x); }
double sv_cos(double x) { return cos(x); }
double sv_tan(double x) { return tan(x); }
double sv_sqrt(double x) { return sqrt(x); }
double sv_pow(double x, double y) { return pow(x, y); }
double sv_abs(double x) { return fabs(x); }

// Fonctions système
int sv_time() {
    return (int)time(NULL);
}

void sv_exit(int code) {
    exit(code);
}

// Manipulation de chaînes
char* sv_str_concat(const char* a, const char* b) {
    char* result = malloc(strlen(a) + strlen(b) + 1);
    strcpy(result, a);
    strcat(result, b);
    return result;
}

int sv_str_length(const char* str) {
    return strlen(str);
}
