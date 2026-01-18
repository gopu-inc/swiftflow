#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// Déclaration forward pour éviter l'erreur
void run(const char* source);

// Simple VM avec support d'import
typedef struct {
    char name[50];
    int value;
} Variable;

typedef struct {
    Variable vars[100];
    int count;
    char* import_path;
} VM;

VM vm = {0};

// Version simple de strdup pour iSH
static char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    if (d) strcpy(d, s);
    return d;
}

// Définir le chemin d'import
void setImportPath(const char* path) {
    if (vm.import_path) free(vm.import_path);
    vm.import_path = my_strdup(path);
}

// Chercher un fichier dans le chemin d'import
char* findImportFile(const char* module) {
    if (!vm.import_path) {
        vm.import_path = my_strdup("./");
    }
    
    // Si le module commence par . ou /, c'est un chemin relatif/absolu
    if (module[0] == '.' || module[0] == '/') {
        return my_strdup(module);
    }
    
    // Sinon chercher dans le chemin d'import
    char* path = malloc(strlen(vm.import_path) + strlen(module) + 10);
    if (!path) return NULL;
    
    // Essayer avec extension .swf
    sprintf(path, "%s%s.swf", vm.import_path, module);
    
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return path;
    }
    
    // Essayer sans extension
    sprintf(path, "%s%s", vm.import_path, module);
    f = fopen(path, "r");
    if (f) {
        fclose(f);
        return path;
    }
    
    free(path);
    return NULL;
}

// Importer et exécuter un module
void importModule(const char* module) {
    printf("[IMPORT] Loading: %s\n", module);
    
    char* path = findImportFile(module);
    if (!path) {
        printf("[ERROR] Cannot find module: %s\n", module);
        return;
    }
    
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("[ERROR] Cannot open: %s\n", path);
        free(path);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        free(path);
        return;
    }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Exécuter le module
    run(source);
    
    free(source);
    free(path);
}

int findVar(const char* name) {
    for (int i = 0; i < vm.count; i++) {
        if (strcmp(vm.vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int eval(ASTNode* node) {
    if (!node) return 0;
    
    switch (node->type) {
        case NODE_INT:
            return (int)node->data.int_val;
            
        case NODE_BINARY: {
            int left = eval(node->left);
            int right = eval(node->right);
            
            switch ((TokenKind)node->data.int_val) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0 ? left / right : 0;
                default: return 0;
            }
        }
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            return idx >= 0 ? vm.vars[idx].value : 0;
        }
            
        default:
            return 0;
    }
}

void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                printf("Error: Variable '%s' already exists\n", node->data.name);
                return;
            }
            
            if (vm.count < 100) {
                strncpy(vm.vars[vm.count].name, node->data.name, 49);
                vm.vars[vm.count].name[49] = '\0';
                vm.vars[vm.count].value = node->left ? eval(node->left) : 0;
                vm.count++;
                printf("[DEBUG] Variable created: %s = %d\n", 
                       vm.vars[vm.count-1].name, vm.vars[vm.count-1].value);
            }
            break;
        }
            
        case NODE_ASSIGN: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                vm.vars[idx].value = eval(node->left);
                printf("[DEBUG] Variable updated: %s = %d\n", 
                       node->data.name, vm.vars[idx].value);
            } else {
                printf("Error: Variable '%s' not found\n", node->data.name);
            }
            break;
        }
            
        case NODE_PRINT: {
            int value = eval(node->left);
            printf("%d\n", value);
            break;
        }
            
        case NODE_IMPORT: {
            // Importer tous les modules listés
            for (int i = 0; i < node->import_count; i++) {
                if (node->data.imports[i]) {
                    importModule(node->data.imports[i]);
                }
            }
            break;
        }
            
        case NODE_BLOCK:
            execute(node->left);
            break;
            
        default:
            printf("[WARN] Unsupported node type: %d\n", node->type);
            break;
    }
}

void run(const char* source) {
    int count;
    ASTNode** nodes = parse(source, &count);
    
    if (nodes) {
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                execute(nodes[i]);
            }
        }
        
        // Cleanup
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                // Nettoyer les imports
                if (nodes[i]->type == NODE_IMPORT) {
                    for (int j = 0; j < nodes[i]->import_count; j++) {
                        free(nodes[i]->data.imports[j]);
                    }
                    free(nodes[i]->data.imports);
                }
                // Nettoyer les noms
                if (nodes[i]->type == NODE_IDENT || 
                    nodes[i]->type == NODE_VAR || 
                    nodes[i]->type == NODE_ASSIGN) {
                    free(nodes[i]->data.name);
                }
                free(nodes[i]);
            }
        }
        free(nodes);
    }
}

void repl() {
    printf("SwiftFlow avec Imports v1.0 (iSH)\n");
    printf("Type 'exit' to quit, 'path' to set import path\n\n");
    
    // Définir le chemin d'import par défaut
    setImportPath("./");
    
    char line[256];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "vars") == 0) {
            printf("Variables (%d):\n", vm.count);
            for (int i = 0; i < vm.count; i++) {
                printf("  %s = %d\n", vm.vars[i].name, vm.vars[i].value);
            }
            continue;
        }
        if (strncmp(line, "path ", 5) == 0) {
            setImportPath(line + 5);
            printf("Import path set to: %s\n", vm.import_path);
            continue;
        }
        
        run(line);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        repl();
    } else {
        FILE* f = fopen(argv[1], "r");
        if (!f) {
            printf("Cannot open file: %s\n", argv[1]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        if (!source) {
            fclose(f);
            return 1;
        }
        
        fread(source, 1, size, f);
        source[size] = '\0';
        fclose(f);
        
        // Définir le chemin d'import basé sur le fichier
        char* dir = my_strdup(argv[1]);
        if (dir) {
            char* last_slash = strrchr(dir, '/');
            if (last_slash) {
                *last_slash = '\0';
                setImportPath(dir);
            } else {
                setImportPath("./");
            }
            free(dir);
        }
        
        run(source);
        free(source);
    }
    
    if (vm.import_path) free(vm.import_path);
    return 0;
}