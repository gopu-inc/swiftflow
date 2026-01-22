#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] CONFIGURATION
// ======================================================
#define SWIFT_VERSION_STRING "Flow 1.0"
#define MAX_FUNCTIONS 500
#define MAX_VARIABLES 2000
#define MAX_CLASSES 100
#define MAX_IMPORTS 100
#define MAX_SCOPES 50

// ======================================================
// [SECTION] TYPES DE DONNÉES SWIFTFLOW
// ======================================================
typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_ARRAY,
    TYPE_MAP,
    TYPE_FUNCTION,
    TYPE_CLASS,
    TYPE_ANY,
    TYPE_NULL
} SwiftType;

typedef struct SwiftValue {
    SwiftType type;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
        struct {
            struct SwiftValue* items;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            struct SwiftValue* values;
            int count;
            int capacity;
        } map;
        struct {
            char* name;
            ASTNode* params;
            ASTNode* body;
        } func;
    } value;
} SwiftValue;

// ======================================================
// [SECTION] SYSTÈME DE VARIABLES AVEC TYPES
// ======================================================
typedef struct {
    char name[100];
    SwiftType type;
    SwiftValue value;
    bool is_constant;
    int scope_level;
    char* module;
    bool is_exported;
} Variable;

static Variable vars[MAX_VARIABLES];
static int var_count = 0;
static int scope_stack[MAX_SCOPES];
static int scope_top = 0;

// ======================================================
// [SECTION] SYSTÈME DE FONCTIONS AVEC NAMESPACE
// ======================================================
typedef struct {
    char full_name[150];  // format: module.function
    char name[100];
    char module[100];
    ASTNode* params;
    ASTNode* body;
    int param_count;
    SwiftType return_type;
    bool is_exported;
    bool is_native;
    void (*native_func)(void);
} Function;

static Function functions[MAX_FUNCTIONS];
static int func_count = 0;
static Function* current_function = NULL;

// ======================================================
// [SECTION] SYSTÈME DE CLASSES
// ======================================================
typedef struct {
    char name[100];
    char* parent;
    SwiftType* fields;
    Function* methods;
    int field_count;
    int method_count;
} Class;

static Class classes[MAX_CLASSES];
static int class_count = 0;

// ======================================================
// [SECTION] SYSTÈME D'IMPORT HIÉRARCHIQUE
// ======================================================
typedef struct {
    char* path;
    char* alias;
    bool is_loaded;
    Function** exported_funcs;
    int func_count;
} ImportedModule;

static ImportedModule imports[MAX_IMPORTS];
static int import_count = 0;
static char current_working_dir[PATH_MAX];
static char current_module[100] = "global";

// ======================================================
// [SECTION] SYSTÈME DE PACKAGES
// ======================================================
typedef struct {
    char name[100];
    char* path;
    char** dependencies;
    int dep_count;
} Package;

// ======================================================
// [SECTION] DÉCLARATIONS DE FONCTIONS
// ======================================================
static void execute(ASTNode* node);
static SwiftValue evalExpression(ASTNode* node);
static SwiftValue evalBinary(SwiftValue left, SwiftValue right, TokenKind op);
static SwiftValue evalUnary(SwiftValue operand, TokenKind op);
static char* evalString(ASTNode* node);
static bool evalBool(ASTNode* node);
static double evalFloat(ASTNode* node);

// Native functions
static SwiftValue native_print(SwiftValue* args, int argc);
static SwiftValue native_input(SwiftValue* args, int argc);
static SwiftValue native_file_read(SwiftValue* args, int argc);
static SwiftValue native_file_write(SwiftValue* args, int argc);
static SwiftValue native_typeof(SwiftValue* args, int argc);
static SwiftValue native_len(SwiftValue* args, int argc);
static SwiftValue native_range(SwiftValue* args, int argc);

// System functions
static void push_scope();
static void pop_scope();
static int find_variable(const char* name);
static void register_function(const char* full_name, const char* module, 
                              ASTNode* params, ASTNode* body, int param_count,
                              bool is_exported, bool is_native, void (*native_func)(void));
static Function* find_function(const char* name);
static void register_class(const char* name, char* parent, ASTNode* members);
static SwiftValue call_function(Function* func, SwiftValue* args, int argc);
static bool import_module(const char* module_path, const char* alias);
static bool import_from_package(const char* module, const char* package);
static void register_builtins();

// ======================================================
// [SECTION] FONCTIONS UTILITAIRES
// ======================================================
static void init_working_dir(const char* filename) {
    if (filename && strcmp(filename, "REPL") != 0) {
        if (getcwd(current_working_dir, sizeof(current_working_dir)) == NULL) {
            strcpy(current_working_dir, ".");
        }
    } else {
        strcpy(current_working_dir, ".");
    }
}

static void push_scope() {
    if (scope_top < MAX_SCOPES) {
        scope_stack[scope_top++] = var_count;
    }
}

static void pop_scope() {
    if (scope_top > 0) {
        int prev_count = scope_stack[--scope_top];
        // Free variables in popped scope
        for (int i = var_count - 1; i >= prev_count; i--) {
            if (vars[i].value.type == TYPE_STRING && vars[i].value.value.str_val) {
                free(vars[i].value.value.str_val);
            } else if (vars[i].value.type == TYPE_ARRAY) {
                free(vars[i].value.value.array.items);
            } else if (vars[i].value.type == TYPE_MAP) {
                for (int j = 0; j < vars[i].value.value.map.count; j++) {
                    free(vars[i].value.value.map.keys[j]);
                    // Note: values would need cleanup too
                }
                free(vars[i].value.value.map.keys);
                free(vars[i].value.value.map.values);
            }
        }
        var_count = prev_count;
    }
}

static int find_variable(const char* name) {
    for (int i = var_count - 1; i >= 0; i--) {
        if (strcmp(vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// ======================================================
// [SECTION] SYSTÈME DE FONCTIONS COMPLET
// ======================================================
static void register_function(const char* full_name, const char* module, 
                              ASTNode* params, ASTNode* body, int param_count,
                              bool is_exported, bool is_native, void (*native_func)(void)) {
    if (func_count >= MAX_FUNCTIONS) return;
    
    Function* func = &functions[func_count];
    
    // Extraire nom simple du nom complet
    const char* dot = strrchr(full_name, '.');
    if (dot) {
        strncpy(func->name, dot + 1, 99);
    } else {
        strncpy(func->name, full_name, 99);
    }
    func->name[99] = '\0';
    
    strncpy(func->full_name, full_name, 149);
    func->full_name[149] = '\0';
    
    strncpy(func->module, module, 99);
    func->module[99] = '\0';
    
    func->params = params;
    func->body = body;
    func->param_count = param_count;
    func->return_type = TYPE_ANY; // Inférence de type
    func->is_exported = is_exported;
    func->is_native = is_native;
    func->native_func = native_func;
    
    func_count++;
    printf("%s[FUNC]%s Enregistrée: %s (module: %s, params: %d)\n", 
           COLOR_GREEN, COLOR_RESET, full_name, module, param_count);
}

static Function* find_function(const char* name) {
    // Chercher avec namespace complet
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].full_name, name) == 0) {
            return &functions[i];
        }
    }
    
    // Chercher dans le module courant
    char full_name[150];
    snprintf(full_name, sizeof(full_name), "%s.%s", current_module, name);
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].full_name, full_name) == 0) {
            return &functions[i];
        }
    }
    
    // Chercher fonction globale (sans module)
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0 && 
            strcmp(functions[i].module, "global") == 0) {
            return &functions[i];
        }
    }
    
    return NULL;
}

// ======================================================
// [SECTION] SYSTÈME D'IMPORT COMPLET
// ======================================================
static bool import_module(const char* module_path, const char* alias) {
    printf("%s[IMPORT]%s Chargement module: %s", COLOR_CYAN, COLOR_RESET, module_path);
    if (alias) printf(" as %s", alias);
    printf("\n");
    
    // Vérifier si déjà importé
    for (int i = 0; i < import_count; i++) {
        if (strcmp(imports[i].path, module_path) == 0) {
            printf("%s[IMPORT]%s Déjà chargé: %s\n", COLOR_YELLOW, COLOR_RESET, module_path);
            return true;
        }
    }
    
    // Chercher le module
    char search_paths[][100] = {
        "%s/%s.swf",                    // Répertoire courant
        "%s/%s/main.swf",               // Package main
        "/usr/local/lib/swift/%s.swf",  // Modules système
        "/usr/local/lib/swift/%s/main.swf",
        "/usr/local/lib/swift/packages/%s/main.swf",
        "/usr/local/lib/swift/packages/%s/%s.swf"
    };
    
    char full_path[PATH_MAX];
    FILE* f = NULL;
    
    for (int i = 0; i < sizeof(search_paths)/sizeof(search_paths[0]); i++) {
        snprintf(full_path, sizeof(full_path), search_paths[i], 
                 current_working_dir, module_path);
        
        f = fopen(full_path, "r");
        if (f) {
            printf("%s[IMPORT]%s Trouvé: %s\n", COLOR_GREEN, COLOR_RESET, full_path);
            break;
        }
    }
    
    if (!f) {
        printf("%s[IMPORT ERREUR]%s Module non trouvé: %s\n", COLOR_RED, COLOR_RESET, module_path);
        return false;
    }
    
    // Lire le module
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Sauvegarder le module courant
    char old_module[100];
    strcpy(old_module, current_module);
    
    // Définir le nouveau module
    if (alias) {
        strncpy(current_module, alias, 99);
    } else {
        // Extraire nom du module du chemin
        char* last_slash = strrchr(module_path, '/');
        if (last_slash) {
            strncpy(current_module, last_slash + 1, 99);
        } else {
            strncpy(current_module, module_path, 99);
        }
        // Enlever extension
        char* dot = strrchr(current_module, '.');
        if (dot) *dot = '\0';
    }
    
    // Parser et exécuter le module
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count);
    
    if (nodes) {
        // Enregistrer les fonctions exportées
        for (int i = 0; i < node_count; i++) {
            if (nodes[i] && nodes[i]->type == NODE_FUNC && nodes[i]->data.name) {
                int param_count = 0;
                ASTNode* param = nodes[i]->left;
                while (param) {
                    param_count++;
                    param = param->right;
                }
                
                char full_name[150];
                snprintf(full_name, sizeof(full_name), "%s.%s", 
                         current_module, nodes[i]->data.name);
                
                register_function(full_name, current_module, 
                                 nodes[i]->left, nodes[i]->right, 
                                 param_count, true, false, NULL);
            }
        }
        
        // Exécuter le code d'initialisation du module
        for (int i = 0; i < node_count; i++) {
            if (nodes[i] && nodes[i]->type != NODE_FUNC) {
                execute(nodes[i]);
            }
        }
        
        // Nettoyer
        for (int i = 0; i < node_count; i++) {
            if (nodes[i]) free(nodes[i]);
        }
        free(nodes);
    }
    
    // Restaurer le module précédent
    strcpy(current_module, old_module);
    
    // Enregistrer l'import
    if (import_count < MAX_IMPORTS) {
        imports[import_count].path = str_copy(module_path);
        imports[import_count].alias = alias ? str_copy(alias) : NULL;
        imports[import_count].is_loaded = true;
        import_count++;
    }
    
    free(source);
    printf("%s[IMPORT]%s Module %s chargé avec succès\n", COLOR_GREEN, COLOR_RESET, module_path);
    return true;
}

static bool import_from_package(const char* module, const char* package) {
    char path[200];
    snprintf(path, sizeof(path), "%s/%s", package, module);
    return import_module(path, module);
}

// ======================================================
// [SECTION] FONCTIONS NATIVES BUILT-IN
// ======================================================
static void register_builtins() {
    printf("%s[INIT]%s Enregistrement des fonctions natives...\n", COLOR_CYAN, COLOR_RESET);
    
    // Fonctions système
    register_function("print", "sys", NULL, NULL, -1, true, true, NULL);
    register_function("println", "sys", NULL, NULL, -1, true, true, NULL);
    register_function("input", "sys", NULL, NULL, -1, true, true, NULL);
    
    // Fonctions mathématiques
    register_function("math.abs", "math", NULL, NULL, 1, true, true, NULL);
    register_function("math.sqrt", "math", NULL, NULL, 1, true, true, NULL);
    register_function("math.pow", "math", NULL, NULL, 2, true, true, NULL);
    register_function("math.sin", "math", NULL, NULL, 1, true, true, NULL);
    register_function("math.cos", "math", NULL, NULL, 1, true, true, NULL);
    
    // Fonctions chaînes
    register_function("str.len", "str", NULL, NULL, 1, true, true, NULL);
    register_function("str.concat", "str", NULL, NULL, 2, true, true, NULL);
    register_function("str.substr", "str", NULL, NULL, 3, true, true, NULL);
    register_function("str.split", "str", NULL, NULL, 2, true, true, NULL);
    
    // Fonctions fichiers
    register_function("file.read", "file", NULL, NULL, 1, true, true, NULL);
    register_function("file.write", "file", NULL, NULL, 2, true, true, NULL);
    register_function("file.append", "file", NULL, NULL, 2, true, true, NULL);
    register_function("file.exists", "file", NULL, NULL, 1, true, true, NULL);
    
    // Fonctions tableau
    register_function("array.new", "array", NULL, NULL, 0, true, true, NULL);
    register_function("array.push", "array", NULL, NULL, 2, true, true, NULL);
    register_function("array.pop", "array", NULL, NULL, 1, true, true, NULL);
    register_function("array.len", "array", NULL, NULL, 1, true, true, NULL);
    
    printf("%s[INIT]%s %d fonctions natives enregistrées\n", COLOR_GREEN, COLOR_RESET, func_count);
}

// ======================================================
// [SECTION] ÉVALUATION D'EXPRESSIONS
// ======================================================
static SwiftValue evalExpression(ASTNode* node) {
    if (!node) {
        SwiftValue null = {TYPE_NULL};
        return null;
    }
    
    switch (node->type) {
        case NODE_INT: {
            SwiftValue val = {TYPE_INT};
            val.value.int_val = node->data.int_val;
            return val;
        }
        case NODE_FLOAT: {
            SwiftValue val = {TYPE_FLOAT};
            val.value.float_val = node->data.float_val;
            return val;
        }
        case NODE_STRING: {
            SwiftValue val = {TYPE_STRING};
            val.value.str_val = str_copy(node->data.str_val);
            return val;
        }
        case NODE_BOOL: {
            SwiftValue val = {TYPE_BOOL};
            val.value.bool_val = node->data.bool_val;
            return val;
        }
        case NODE_IDENT: {
            int idx = find_variable(node->data.name);
            if (idx >= 0) {
                return vars[idx].value;
            }
            // Variable non trouvée, retourner null
            SwiftValue null = {TYPE_NULL};
            printf("%s[EXEC ERREUR]%s Variable non définie: %s\n", 
                   COLOR_RED, COLOR_RESET, node->data.name);
            return null;
        }
        case NODE_BINARY: {
            SwiftValue left = evalExpression(node->left);
            SwiftValue right = evalExpression(node->right);
            return evalBinary(left, right, node->op_type);
        }
        case NODE_UNARY: {
            SwiftValue operand = evalExpression(node->left);
            return evalUnary(operand, node->op_type);
        }
        case NODE_FUNC_CALL: {
            Function* func = find_function(node->data.name);
            if (!func) {
                printf("%s[EXEC ERREUR]%s Fonction non trouvée: %s\n", 
                       COLOR_RED, COLOR_RESET, node->data.name);
                SwiftValue null = {TYPE_NULL};
                return null;
            }
            
            // Évaluer les arguments
            int arg_count = 0;
            SwiftValue args[20];
            ASTNode* arg = node->left;
            while (arg && arg_count < 20) {
                args[arg_count++] = evalExpression(arg);
                arg = arg->right;
            }
            
            if (func->is_native) {
                // Appeler fonction native
                // Note: À implémenter selon la fonction
                SwiftValue result = {TYPE_VOID};
                return result;
            } else {
                // Appeler fonction SwiftFlow
                // Sauvegarder contexte
                Function* old_func = current_function;
                int old_scope_top = scope_top;
                
                current_function = func;
                push_scope();
                
                // Passer les paramètres
                ASTNode* param = func->params;
                for (int i = 0; i < arg_count && param; i++) {
                    if (param->type == NODE_IDENT && param->data.name) {
                        // Créer variable locale
                        if (var_count < MAX_VARIABLES) {
                            Variable* var = &vars[var_count];
                            strncpy(var->name, param->data.name, 99);
                            var->name[99] = '\0';
                            var->type = TYPE_ANY;
                            var->value = args[i];
                            var->is_constant = false;
                            var->scope_level = scope_top;
                            var_count++;
                        }
                    }
                    param = param->right;
                }
                
                // Exécuter le corps
                SwiftValue result = {TYPE_VOID};
                if (func->body) {
                    execute(func->body);
                    if (func->return_type != TYPE_VOID) {
                        // Récupérer valeur de retour
                        result = func->body->data.value; // À adapter
                    }
                }
                
                // Restaurer contexte
                pop_scope();
                current_function = old_func;
                scope_top = old_scope_top;
                
                return result;
            }
        }
        default: {
            SwiftValue null = {TYPE_NULL};
            return null;
        }
    }
}

static SwiftValue evalBinary(SwiftValue left, SwiftValue right, TokenKind op) {
    SwiftValue result = {TYPE_NULL};
    
    // Conversions de type si nécessaire
    if (left.type == TYPE_INT && right.type == TYPE_FLOAT) {
        left.type = TYPE_FLOAT;
        left.value.float_val = (double)left.value.int_val;
    } else if (left.type == TYPE_FLOAT && right.type == TYPE_INT) {
        right.type = TYPE_FLOAT;
        right.value.float_val = (double)right.value.int_val;
    }
    
    switch (op) {
        case TK_PLUS:
            if (left.type == TYPE_INT && right.type == TYPE_INT) {
                result.type = TYPE_INT;
                result.value.int_val = left.value.int_val + right.value.int_val;
            } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
                result.type = TYPE_FLOAT;
                result.value.float_val = left.value.float_val + right.value.float_val;
            } else if (left.type == TYPE_STRING || right.type == TYPE_STRING) {
                // Concaténation
                char* left_str = left.type == TYPE_STRING ? 
                    left.value.str_val : "?";
                char* right_str = right.type == TYPE_STRING ? 
                    right.value.str_val : "?";
                char* concat = malloc(strlen(left_str) + strlen(right_str) + 1);
                strcpy(concat, left_str);
                strcat(concat, right_str);
                result.type = TYPE_STRING;
                result.value.str_val = concat;
            }
            break;
            
        case TK_MINUS:
            if (left.type == TYPE_INT && right.type == TYPE_INT) {
                result.type = TYPE_INT;
                result.value.int_val = left.value.int_val - right.value.int_val;
            } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
                result.type = TYPE_FLOAT;
                result.value.float_val = left.value.float_val - right.value.float_val;
            }
            break;
            
        case TK_MULT:
            if (left.type == TYPE_INT && right.type == TYPE_INT) {
                result.type = TYPE_INT;
                result.value.int_val = left.value.int_val * right.value.int_val;
            } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
                result.type = TYPE_FLOAT;
                result.value.float_val = left.value.float_val * right.value.float_val;
            }
            break;
            
        case TK_DIV:
            if (left.type == TYPE_INT && right.type == TYPE_INT) {
                if (right.value.int_val != 0) {
                    result.type = TYPE_FLOAT;
                    result.value.float_val = (double)left.value.int_val / (double)right.value.int_val;
                }
            } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
                if (right.value.float_val != 0.0) {
                    result.type = TYPE_FLOAT;
                    result.value.float_val = left.value.float_val / right.value.float_val;
                }
            }
            break;
            
        case TK_EQ:
            result.type = TYPE_BOOL;
            // Comparaison simplifiée
            if (left.type == right.type) {
                if (left.type == TYPE_INT) {
                    result.value.bool_val = left.value.int_val == right.value.int_val;
                } else if (left.type == TYPE_FLOAT) {
                    result.value.bool_val = fabs(left.value.float_val - right.value.float_val) < 1e-10;
                } else if (left.type == TYPE_BOOL) {
                    result.value.bool_val = left.value.bool_val == right.value.bool_val;
                } else if (left.type == TYPE_STRING) {
                    result.value.bool_val = strcmp(left.value.str_val, right.value.str_val) == 0;
                }
            } else {
                result.value.bool_val = false;
            }
            break;
            
        case TK_NEQ:
            result.type = TYPE_BOOL;
            if (left.type == right.type) {
                if (left.type == TYPE_INT) {
                    result.value.bool_val = left.value.int_val != right.value.int_val;
                } else if (left.type == TYPE_FLOAT) {
                    result.value.bool_val = fabs(left.value.float_val - right.value.float_val) >= 1e-10;
                } else if (left.type == TYPE_BOOL) {
                    result.value.bool_val = left.value.bool_val != right.value.bool_val;
                } else if (left.type == TYPE_STRING) {
                    result.value.bool_val = strcmp(left.value.str_val, right.value.str_val) != 0;
                }
            } else {
                result.value.bool_val = true;
            }
            break;
            
        case TK_LT:
        case TK_GT:
        case TK_LTE:
        case TK_GTE:
            result.type = TYPE_BOOL;
            if (left.type == TYPE_INT && right.type == TYPE_INT) {
                switch (op) {
                    case TK_LT: result.value.bool_val = left.value.int_val < right.value.int_val; break;
                    case TK_GT: result.value.bool_val = left.value.int_val > right.value.int_val; break;
                    case TK_LTE: result.value.bool_val = left.value.int_val <= right.value.int_val; break;
                    case TK_GTE: result.value.bool_val = left.value.int_val >= right.value.int_val; break;
                }
            } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
                switch (op) {
                    case TK_LT: result.value.bool_val = left.value.float_val < right.value.float_val; break;
                    case TK_GT: result.value.bool_val = left.value.float_val > right.value.float_val; break;
                    case TK_LTE: result.value.bool_val = left.value.float_val <= right.value.float_val; break;
                    case TK_GTE: result.value.bool_val = left.value.float_val >= right.value.float_val; break;
                }
            }
            break;
            
        default:
            break;
    }
    
    return result;
}

static SwiftValue evalUnary(SwiftValue operand, TokenKind op) {
    SwiftValue result = {TYPE_NULL};
    
    switch (op) {
        case TK_MINUS:
            if (operand.type == TYPE_INT) {
                result.type = TYPE_INT;
                result.value.int_val = -operand.value.int_val;
            } else if (operand.type == TYPE_FLOAT) {
                result.type = TYPE_FLOAT;
                result.value.float_val = -operand.value.float_val;
            }
            break;
            
        case TK_NOT:
            if (operand.type == TYPE_BOOL) {
                result.type = TYPE_BOOL;
                result.value.bool_val = !operand.value.bool_val;
            }
            break;
            
        case TK_INCREMENT:
        case TK_DECREMENT:
            // À implémenter pour les variables
            break;
            
        default:
            break;
    }
    
    return result;
}

// ======================================================
// [SECTION] EXÉCUTION DES INSTRUCTIONS
// ======================================================
static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_BLOCK: {
            push_scope();
            ASTNode* stmt = node->left;
            while (stmt) {
                execute(stmt);
                stmt = stmt->right;
            }
            pop_scope();
            break;
        }
        
        case NODE_VAR_DECL: {
            if (var_count >= MAX_VARIABLES) break;
            
            Variable* var = &vars[var_count];
            strncpy(var->name, node->data.name, 99);
            var->name[99] = '\0';
            var->type = TYPE_ANY; // Type inféré
            var->is_constant = false;
            var->scope_level = scope_top;
            var->module = str_copy(current_module);
            var->is_exported = false;
            
            if (node->left) {
                var->value = evalExpression(node->left);
            } else {
                var->value.type = TYPE_NULL;
            }
            
            var_count++;
            printf("%s[VAR]%s Déclarée: %s\n", COLOR_CYAN, COLOR_RESET, var->name);
            break;
        }
        
        case NODE_CONST_DECL: {
            if (var_count >= MAX_VARIABLES) break;
            
            Variable* var = &vars[var_count];
            strncpy(var->name, node->data.name, 99);
            var->name[99] = '\0';
            var->type = TYPE_ANY;
            var->is_constant = true;
            var->scope_level = scope_top;
            var->module = str_copy(current_module);
            var->is_exported = false;
            
            if (node->left) {
                var->value = evalExpression(node->left);
            } else {
                printf("%s[ERREUR]%s Constante sans valeur: %s\n", 
                       COLOR_RED, COLOR_RESET, node->data.name);
                var->value.type = TYPE_NULL;
            }
            
            var_count++;
            printf("%s[CONST]%s Déclarée: %s\n", COLOR_MAGENTA, COLOR_RESET, var->name);
            break;
        }
        
        case NODE_ASSIGN: {
            int idx = find_variable(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_constant) {
                    printf("%s[ERREUR]%s Cannot assign to constant: %s\n", 
                           COLOR_RED, COLOR_RESET, node->data.name);
                } else {
                    vars[idx].value = evalExpression(node->left);
                    printf("%s[ASSIGN]%s %s = ", COLOR_GREEN, COLOR_RESET, node->data.name);
                    // Afficher valeur
                }
            } else {
                printf("%s[ERREUR]%s Variable non définie: %s\n", 
                       COLOR_RED, COLOR_RESET, node->data.name);
            }
            break;
        }
        
        case NODE_IF: {
            SwiftValue cond = evalExpression(node->left);
            bool condition = false;
            
            if (cond.type == TYPE_BOOL) {
                condition = cond.value.bool_val;
            } else if (cond.type == TYPE_INT) {
                condition = cond.value.int_val != 0;
            } else if (cond.type == TYPE_FLOAT) {
                condition = fabs(cond.value.float_val) > 1e-10;
            } else if (cond.type == TYPE_STRING) {
                condition = cond.value.str_val && strlen(cond.value.str_val) > 0;
            } else if (cond.type == TYPE_NULL) {
                condition = false;
            }
            
            if (condition) {
                execute(node->right);
            } else if (node->third) {
                execute(node->third);
            }
            break;
        }
        
        case NODE_WHILE: {
            while (1) {
                SwiftValue cond = evalExpression(node->left);
                bool condition = false;
                
                if (cond.type == TYPE_BOOL) {
                    condition = cond.value.bool_val;
                } else if (cond.type == TYPE_INT) {
                    condition = cond.value.int_val != 0;
                } else if (cond.type == TYPE_FLOAT) {
                    condition = fabs(cond.value.float_val) > 1e-10;
                } else if (cond.type == TYPE_STRING) {
                    condition = cond.value.str_val && strlen(cond.value.str_val) > 0;
                }
                
                if (!condition) break;
                
                execute(node->right);
                
                if (current_function && current_function->has_returned) {
                    break;
                }
            }
            break;
        }
        
        case NODE_FOR: {
            // Initialisation
            if (node->data.loop.init) {
                execute(node->data.loop.init);
            }
            
            // Condition
            while (1) {
                if (node->data.loop.condition) {
                    SwiftValue cond = evalExpression(node->data.loop.condition);
                    bool condition = false;
                    
                    if (cond.type == TYPE_BOOL) {
                        condition = cond.value.bool_val;
                    } else if (cond.type == TYPE_INT) {
                        condition = cond.value.int_val != 0;
                    }
                    
                    if (!condition) break;
                }
                
                // Corps
                execute(node->data.loop.body);
                
                if (current_function && current_function->has_returned) {
                    break;
                }
                
                // Mise à jour
                if (node->data.loop.update) {
                    execute(node->data.loop.update);
                }
            }
            break;
        }
        
        case NODE_FOR_IN: {
            // À implémenter
            break;
        }
        
        case NODE_RETURN: {
            if (current_function) {
                current_function->has_returned = true;
                if (node->left) {
                    // Stocker valeur de retour
                    SwiftValue ret_val = evalExpression(node->left);
                    // À stocker dans la fonction
                }
            }
            break;
        }
        
        case NODE_PRINT: {
            ASTNode* arg = node->left;
            while (arg) {
                SwiftValue val = evalExpression(arg);
                
                if (val.type == TYPE_STRING) {
                    printf("%s", val.value.str_val);
                } else if (val.type == TYPE_INT) {
                    printf("%lld", val.value.int_val);
                } else if (val.type == TYPE_FLOAT) {
                    printf("%g", val.value.float_val);
                } else if (val.type == TYPE_BOOL) {
                    printf("%s", val.value.bool_val ? "true" : "false");
                } else if (val.type == TYPE_NULL) {
                    printf("null");
                }
                
                arg = arg->right;
                if (arg) printf(" ");
            }
            printf("\n");
            break;
        }
        
        case NODE_IMPORT: {
            for (int i = 0; i < node->data.imports.module_count; i++) {
                char* module_name = node->data.imports.modules[i];
                char* package_name = node->data.imports.from_module;
                
                if (package_name) {
                    import_from_package(module_name, package_name);
                } else {
                    import_module(module_name, NULL);
                }
            }
            break;
        }
        
        case NODE_FUNC:
            // Déjà enregistrée pendant le parsing
            break;
            
        case NODE_FUNC_CALL:
            evalExpression(node);
            break;
            
        case NODE_EXPRESSION:
            evalExpression(node->left);
            break;
            
        default:
            printf("%s[EXEC]%s Node type non géré: %d\n", 
                   COLOR_YELLOW, COLOR_RESET, node->type);
            break;
    }
}

// ======================================================
// [SECTION] EXÉCUTION PRINCIPALE
// ======================================================
static void run(const char* source, const char* filename) {
    init_working_dir(filename);
    
    printf("%s[SWIFTFLOW]%s Version: %s\n", COLOR_BRIGHT_CYAN, COLOR_RESET, SWIFT_VERSION_STRING);
    printf("%s[EXEC]%s Répertoire: %s\n", COLOR_CYAN, COLOR_RESET, current_working_dir);
    printf("%s[EXEC]%s Module: %s\n", COLOR_CYAN, COLOR_RESET, current_module);
    
    // Enregistrer fonctions natives
    register_builtins();
    
    // Parser le code
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) {
        printf("%s[ERREUR]%s Échec du parsing\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("%s[PARSER]%s %d déclarations parsées\n", COLOR_GREEN, COLOR_RESET, count);
    
    // Enregistrer fonctions définies par l'utilisateur
    for (int i = 0; i < count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_FUNC && nodes[i]->data.name) {
            int param_count = 0;
            ASTNode* param = nodes[i]->left;
            while (param) {
                param_count++;
                param = param->right;
            }
            
            char full_name[150];
            snprintf(full_name, sizeof(full_name), "%s.%s", current_module, nodes[i]->data.name);
            
            register_function(full_name, current_module, 
                             nodes[i]->left, nodes[i]->right, 
                             param_count, true, false, NULL);
        }
    }
    
    // Exécuter le programme
    for (int i = 0; i < count; i++) {
        if (nodes[i] && nodes[i]->type != NODE_FUNC) {
            execute(nodes[i]);
        }
    }
    
    // Nettoyer
    for (int i = 0; i < count; i++) {
        if (nodes[i]) free(nodes[i]);
    }
    free(nodes);
    
    printf("%s[EXEC]%s Exécution terminée\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
}

// ======================================================
// [SECTION] REPL INTERACTIF
// ======================================================
static void repl() {
    printf("\n");
    printf("%s╔════════════════════════════════════════════════════════════════╗%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║              SWIFTFLOW INTERACTIVE REPL                       ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Commands:                                                    ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    .exit      Quitter                                        ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    .clear     Effacer l'écran                                ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    .vars      Afficher variables                             ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    .funcs     Afficher fonctions                             ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    .imports   Afficher imports                               ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╚════════════════════════════════════════════════════════════════╝%s\n", COLOR_CYAN, COLOR_RESET);
    printf("\n");
    
    char line[4096];
    while (1) {
        printf("%sflow>%s ", COLOR_BRIGHT_GREEN, COLOR_RESET);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        // Commandes REPL
        if (strcmp(line, ".exit") == 0 || strcmp(line, ".quit") == 0) break;
        if (strcmp(line, ".clear") == 0 || strcmp(line, ".cls") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, ".vars") == 0) {
            printf("\n%s=== VARIABLES (%d) ===%s\n", COLOR_CYAN, var_count, COLOR_RESET);
            for (int i = 0; i < var_count; i++) {
                printf("%s[%d]%s %s (scope: %d)\n", 
                       COLOR_YELLOW, i, COLOR_RESET, 
                       vars[i].name, vars[i].scope_level);
            }
            continue;
        }
        if (strcmp(line, ".funcs") == 0) {
            printf("\n%s=== FONCTIONS (%d) ===%s\n", COLOR_CYAN, func_count, COLOR_RESET);
            for (int i = 0; i < func_count; i++) {
                printf("%s[%d]%s %s (module: %s, params: %d)\n", 
                       COLOR_MAGENTA, i, COLOR_RESET,
                       functions[i].full_name, functions[i].module, functions[i].param_count);
            }
            continue;
        }
        if (strcmp(line, ".imports") == 0) {
            printf("\n%s=== IMPORTS (%d) ===%s\n", COLOR_CYAN, import_count, COLOR_RESET);
            for (int i = 0; i < import_count; i++) {
                printf("%s[%d]%s %s%s%s\n", 
                       COLOR_BLUE, i, COLOR_RESET,
                       imports[i].path,
                       imports[i].alias ? " as " : "",
                       imports[i].alias ? imports[i].alias : "");
            }
            continue;
        }
        if (strlen(line) == 0) continue;
        
        // Exécuter du code
        run(line, "REPL");
    }
    
    printf("\n%s[REPL]%s Au revoir !\n", COLOR_BLUE, COLOR_RESET);
}

// ======================================================
// [SECTION] CHARGEMENT DE FICHIER
// ======================================================
static char* load_file(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("%s[ERREUR]%s Fichier non trouvé: %s\n", COLOR_RED, COLOR_RESET, filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    return source;
}

// ======================================================
// [SECTION] POINT D'ENTRÉE
// ======================================================
int main(int argc, char* argv[]) {
    srand(time(NULL));
    
    // Gérer arguments ligne de commande
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("SwiftFlow %s\n", SWIFT_VERSION_STRING);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: swiftflow [options] [fichier]\n");
            printf("Options:\n");
            printf("  -v, --version    Afficher version\n");
            printf("  -h, --help       Afficher aide\n");
            printf("  -c <code>        Exécuter code inline\n");
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            run(argv[i + 1], "inline");
            return 0;
        }
    }
    
    if (argc < 2) {
        // Mode REPL
        repl();
    } else {
        // Exécuter fichier
        if (argv[1][0] == '-') {
            printf("%s[ERREUR]%s Option inconnue: %s\n", COLOR_RED, COLOR_RESET, argv[1]);
            return 1;
        }
        
        char* source = load_file(argv[1]);
        if (!source) return 1;
        
        run(source, argv[1]);
        free(source);
    }
    
    return 0;
}
