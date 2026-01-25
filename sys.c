#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "sys.h"

static int g_argc;
static char** g_argv;

void init_sys_module(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
    printf("%s[SYS MODULE]%s Initialized (%d args)\n", COLOR_CYAN, COLOR_RESET, argc);
}

// Récupère l'argument à l'index donné.
// Note : Index 0 correspond au premier argument APRES le nom du script
// Usage : ./swift zarch.swf install (install = index 0)
char* sys_get_argv(int index) {
    // Offset de 2 : [0]=./swift, [1]=zarch.swf, [2]=Arg0
    int real_idx = index + 2;
    
    if (real_idx < g_argc) {
        return g_argv[real_idx]; // Retourne le pointeur direct (lecture seule)
    }
    return NULL;
}

// Exécute une commande shell et retourne le code de sortie
int sys_exec_int(const char* cmd) {
    if (!cmd) return -1;
    printf("%s[SYS EXEC]%s %s\n", COLOR_YELLOW, COLOR_RESET, cmd);
    int res = system(cmd);
    
    // WEXITSTATUS pour avoir le vrai code de retour Linux (0-255)
    if (res == -1) return -1;
    return (res >> 8) & 0xFF; 
}
