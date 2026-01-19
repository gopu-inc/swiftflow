#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] VM STATE
// ======================================================
typedef struct {
    char name[100];
    TokenKind type;
    int size_bytes;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
    } value;
    bool is_float;
    bool is_string;
    bool is_initialized;
    bool is_constant;
    int scope_level;
} Variable;

static Variable vars[500];
static int var_count = 0;
static int scope_level = 0;

// ======================================================
// [SECTION] DBVAR TABLE - STRUCTURE UNIQUE
// ======================================================
typedef struct {
    char name[100];
    char type[20];
    int size_bytes;
    char value_str[100];
    bool initialized;
} DBVarEntry;

static DBVarEntry dbvars[500];
static int dbvar_count = 0;

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static int calculateVariableSize(TokenKind type) {
    switch (type) {
        case TK_VAR: return (rand() % 5) + 1;       // 1-5 bytes
        case TK_NET: return (rand() % 8) + 1;       // 1-8 bytes
        case TK_CLOG: return (rand() % 25) + 1;     // 1-25 bytes
        case TK_DOS: return (rand() % 1024) + 1;    // 1-1024 bytes
        case TK_SEL: return (rand() % 128) + 1;     // 1-128 bytes
        default: return 4;
    }
}

static const char* getTypeName(TokenKind type) {
    switch (type) {
        case TK_VAR: return "var";
        case TK_NET: return "net";
        case TK_CLOG: return "clog";
        case TK_DOS: return "dos";
        case TK_SEL: return "sel";
        case TK_TYPE_INT: return "int";
        case TK_TYPE_FLOAT: return "float";
        case TK_TYPE_STR: return "string";
        case TK_TYPE_BOOL: return "bool";
        default: return "unknown";
    }
}

static int findVar(const char* name) {
    for (int i = var_count - 1; i >= 0; i--) {
        if (strcmp(vars[i].name, name) == 0 && vars[i].scope_level <= scope_level) {
            return i;
        }
    }
    return -1;
}

static void addToDBVar(const char* name, TokenKind type, int size_bytes, 
                       const char* value_str, bool initialized) {
    if (dbvar_count < 500) {
        DBVarEntry* entry = &dbvars[dbvar_count++];
        strncpy(entry->name, name, 99);
        entry->name[99] = '\0';
        strncpy(entry->type, getTypeName(type), 19);
        entry->type[19] = '\0';
        entry->size_bytes = size_bytes;
        if (value_str) {
            strncpy(entry->value_str, value_str, 99);
            entry->value_str[99] = '\0';
        } else {
            strcpy(entry->value_str, "N/A");
        }
        entry->initialized = initialized;
    }
}

static void updateDBVar(const char* name, const char* value_str) {
    for (int i = 0; i < dbvar_count; i++) {
        if (strcmp(dbvars[i].name, name) == 0) {
            if (value_str) {
                strncpy(dbvars[i].value_str, value_str, 99);
                dbvars[i].value_str[99] = '\0';
            }
            dbvars[i].initialized = true;
            break;
        }
    }
}

// ======================================================
// [SECTION] EXPRESSION EVALUATION
// ======================================================
static double evalFloat(ASTNode* node) {
    if (!node) return 0.0;
    
    switch (node->type) {
        case NODE_INT:
            return (double)node->data.int_val;
            
        case NODE_FLOAT:
            return node->data.float_val;
            
        case NODE_BOOL:
            return node->data.bool_val ? 1.0 : 0.0;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_float) {
                    return vars[idx].value.float_val;
                } else if (vars[idx].is_string) {
                    return 0.0;
                } else {
                    return (double)vars[idx].value.int_val;
                }
            }
            printf(RED "[ERROR]" RESET " Undefined variable: %s\n", node->data.name);
            return 0.0;
        }
            
        case NODE_BINARY: {
            double left = evalFloat(node->left);
            double right = evalFloat(node->right);
            
            switch (node->op_type) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0 ? left / right : 0.0;
                case TK_MOD: return right != 0 ? fmod(left, right) : 0.0;
                case TK_EQ: return left == right ? 1.0 : 0.0;
                case TK_NEQ: return left != right ? 1.0 : 0.0;
                case TK_GT: return left > right ? 1.0 : 0.0;
                case TK_LT: return left < right ? 1.0 : 0.0;
                case TK_GTE: return left >= right ? 1.0 : 0.0;
                case TK_LTE: return left <= right ? 1.0 : 0.0;
                default: return 0.0;
            }
        }
            
        case NODE_UNARY: {
            double operand = evalFloat(node->left);
            switch (node->op_type) {
                case TK_MINUS: return -operand;
                case TK_NOT: return operand == 0.0 ? 1.0 : 0.0;
                default: return operand;
            }
        }
            
        default:
            return 0.0;
    }
}

static char* evalString(ASTNode* node) {
    if (!node) return str_copy("");
    
    char* result = NULL;
    
    switch (node->type) {
        case NODE_STRING:
            result = str_copy(node->data.str_val);
            break;
            
        case NODE_INT:
            result = malloc(32);
            sprintf(result, "%lld", node->data.int_val);
            break;
            
        case NODE_FLOAT:
            result = malloc(32);
            sprintf(result, "%.2f", node->data.float_val);
            break;
            
        case NODE_BOOL:
            result = str_copy(node->data.bool_val ? "true" : "false");
            break;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_string && vars[idx].value.str_val) {
                    result = str_copy(vars[idx].value.str_val);
                } else if (vars[idx].is_float) {
                    result = malloc(32);
                    sprintf(result, "%.2f", vars[idx].value.float_val);
                } else {
                    result = malloc(32);
                    sprintf(result, "%lld", vars[idx].value.int_val);
                }
            } else {
                result = str_copy("undefined");
            }
            break;
        }
            
        case NODE_BINARY: {
            if (node->op_type == TK_CONCAT) {
                char* left_str = evalString(node->left);
                char* right_str = evalString(node->right);
                result = malloc(strlen(left_str) + strlen(right_str) + 1);
                if (result) {
                    strcpy(result, left_str);
                    strcat(result, right_str);
                }
                free(left_str);
                free(right_str);
            } else {
                double val = evalFloat(node);
                result = malloc(32);
                sprintf(result, "%.2f", val);
            }
            break;
        }
            
        default:
            result = str_copy("");
            break;
    }
    
    return result;
}

// ======================================================
// [SECTION] EXECUTION
// ======================================================
static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL: {
            TokenKind var_type = TK_VAR;
            if (node->type == NODE_NET_DECL) var_type = TK_NET;
            else if (node->type == NODE_CLOG_DECL) var_type = TK_CLOG;
            else if (node->type == NODE_DOS_DECL) var_type = TK_DOS;
            else if (node->type == NODE_SEL_DECL) var_type = TK_SEL;
            else if (node->type == NODE_CONST_DECL) var_type = TK_CONST;
            
            if (var_count < 500) {
                Variable* var = &vars[var_count];
                strncpy(var->name, node->data.name, 99);
                var->name[99] = '\0';
                var->type = var_type;
                var->size_bytes = calculateVariableSize(var_type);
                var->scope_level = scope_level;
                var->is_constant = (var_type == TK_CONST);
                
                if (node->left) {
                    var->is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        var->is_string = true;
                        var->is_float = false;
                        var->value.str_val = str_copy(node->left->data.str_val);
                        addToDBVar(var->name, var_type, var->size_bytes, 
                                  var->value.str_val, true);
                        printf(GREEN "[DECL]" RESET " %s %s = \"%s\" (%d bytes)\n", 
                               getTypeName(var_type), var->name, var->value.str_val, var->size_bytes);
                    } 
                    else if (node->left->type == NODE_FLOAT) {
                        var->is_float = true;
                        var->is_string = false;
                        var->value.float_val = evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%.2f", var->value.float_val);
                        addToDBVar(var->name, var_type, var->size_bytes, value_str, true);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %.2f (%d bytes)\n", 
                               getTypeName(var_type), var->name, var->value.float_val, var->size_bytes);
                    }
                    else if (node->left->type == NODE_BOOL) {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = node->left->data.bool_val ? 1 : 0;
                        
                        addToDBVar(var->name, var_type, var->size_bytes, 
                                  node->left->data.bool_val ? "true" : "false", true);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %s (%d bytes)\n", 
                               getTypeName(var_type), var->name, 
                               node->left->data.bool_val ? "true" : "false", var->size_bytes);
                    }
                    else {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = (int64_t)evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%lld", var->value.int_val);
                        addToDBVar(var->name, var_type, var->size_bytes, value_str, true);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %lld (%d bytes)\n", 
                               getTypeName(var_type), var->name, var->value.int_val, var->size_bytes);
                    }
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->is_string = false;
                    var->value.int_val = 0;
                    
                    addToDBVar(var->name, var_type, var->size_bytes, "uninitialized", false);
                    
                    printf(GREEN "[DECL]" RESET " %s %s (uninitialized, %d bytes)\n", 
                           getTypeName(var_type), var->name, var->size_bytes);
                }
                
                var_count++;
            }
            break;
        }
            
        case NODE_ASSIGN: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_constant) {
                    printf(RED "[ERROR]" RESET " Cannot assign to constant '%s'\n", node->data.name);
                    return;
                }
                
                if (node->left) {
                    vars[idx].is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        if (vars[idx].value.str_val) free(vars[idx].value.str_val);
                        vars[idx].is_string = true;
                        vars[idx].is_float = false;
                        vars[idx].value.str_val = str_copy(node->left->data.str_val);
                        updateDBVar(node->data.name, vars[idx].value.str_val);
                    }
                    else if (node->left->type == NODE_FLOAT) {
                        vars[idx].is_float = true;
                        vars[idx].is_string = false;
                        vars[idx].value.float_val = evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%.2f", vars[idx].value.float_val);
                        updateDBVar(node->data.name, value_str);
                    }
                    else if (node->left->type == NODE_BOOL) {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].value.int_val = node->left->data.bool_val ? 1 : 0;
                        
                        updateDBVar(node->data.name, node->left->data.bool_val ? "true" : "false");
                    }
                    else {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].value.int_val = (int64_t)evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%lld", vars[idx].value.int_val);
                        updateDBVar(node->data.name, value_str);
                    }
                    
                    printf(CYAN "[ASSIGN]" RESET " %s = ", node->data.name);
                    if (vars[idx].is_string) {
                        printf("\"%s\"\n", vars[idx].value.str_val);
                    } else if (vars[idx].is_float) {
                        printf("%.2f\n", vars[idx].value.float_val);
                    } else {
                        printf("%lld\n", vars[idx].value.int_val);
                    }
                }
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", node->data.name);
            }
            break;
        }
            
        case NODE_PRINT: {
            if (node->left) {
                char* str = evalString(node->left);
                printf("%s", str);
                free(str);
            }
            printf("\n");
            break;
        }
            
        case NODE_SIZEOF: {
            int idx = findVar(node->data.size_info.var_name);
            if (idx >= 0) {
                printf("%d bytes", vars[idx].size_bytes);
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", 
                       node->data.size_info.var_name);
            }
            break;
        }
            
        case NODE_DBVAR: {
            printf(CYAN "\n╔════════════════════════════════════════════════════════════════════╗\n");
            printf("║                   TABLE DES VARIABLES (dbvar)                    ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            printf("║  Type    │ Nom          │ Taille     │ Valeur         │ Init     ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < dbvar_count; i++) {
                DBVarEntry* entry = &dbvars[i];
                printf("║ %-8s │ %-12s │ %-10d │ %-14s │ %-8s ║\n",
                       entry->type,
                       entry->name,
                       entry->size_bytes,
                       entry->value_str,
                       entry->initialized ? "✓" : "✗");
            }
            
            if (dbvar_count == 0) {
                printf("║                      No variables declared                        ║\n");
            }
            
            printf("╚════════════════════════════════════════════════════════════════════╝\n" RESET);
            break;
        }
            
        case NODE_IF: {
            double condition = evalFloat(node->left);
            if (condition != 0) {
                execute(node->right);
            } else if (node->third) {
                execute(node->third);
            }
            break;
        }
            
        case NODE_WHILE: {
            while (evalFloat(node->left) != 0) {
                execute(node->right);
            }
            break;
        }
            
        case NODE_BLOCK: {
            scope_level++;
            ASTNode* current = node->left;
            while (current) {
                execute(current);
                current = current->right;
            }
            scope_level--;
            break;
        }
            
        case NODE_MAIN: {
            printf(CYAN "\n[EXEC]" RESET " Starting main()...\n");
            for (int i = 0; i < 60; i++) printf("=");
            printf("\n");
            
            if (node->left) execute(node->left);
            
            printf("\n" CYAN "[EXEC]" RESET " main() finished successfully\n");
            break;
        }
            
        case NODE_RETURN: {
            if (node->left) {
                char* str = evalString(node->left);
                printf(CYAN "[RETURN]" RESET " %s\n", str);
                free(str);
            } else {
                printf(CYAN "[RETURN]" RESET " (no value)\n");
            }
            break;
        }
            
        case NODE_BINARY: {
            // Évaluation directe d'une expression binaire
            double result = evalFloat(node);
            printf("%.2f\n", result);
            break;
        }
            
        default:
            break;
    }
}

// ======================================================
// [SECTION] CLEANUP FUNCTIONS
// ======================================================
static void cleanupVariables() {
    for (int i = 0; i < var_count; i++) {
        if (vars[i].is_string && vars[i].value.str_val) {
            free(vars[i].value.str_val);
        }
    }
    var_count = 0;
    scope_level = 0;
}

static void cleanupDBVar() {
    dbvar_count = 0;
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source) {
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) {
        printf(RED "[ERROR]" RESET " Failed to parse source code\n");
        return;
    }
    
    // Cleanup avant nouvelle exécution
    cleanupVariables();
    cleanupDBVar();
    
    // Chercher d'abord la fonction main
    ASTNode* main_node = NULL;
    for (int i = 0; i < count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_MAIN) {
            main_node = nodes[i];
            break;
        }
    }
    
    if (main_node) {
        execute(main_node);
    } else {
        // Exécuter toutes les déclarations dans l'ordre
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                execute(nodes[i]);
            }
        }
    }
    
    // Cleanup des nœuds AST
    for (int i = 0; i < count; i++) {
        if (nodes[i]) {
            free(nodes[i]);
        }
    }
    free(nodes);
}

// ======================================================
// [SECTION] REPL
// ======================================================
static void repl() {
    printf(GREEN "╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                SwiftFlow v2.0 - REPL Mode                        ║\n");
    printf("║           Types CLAIR & SYM Fusion - Complete Language           ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n" RESET);
    printf("\n");
    printf("Available commands:\n");
    printf("  " CYAN "exit" RESET "      - Quit the REPL\n");
    printf("  " CYAN "dbvar" RESET "     - Show variable table\n");
    printf("  " CYAN "clear" RESET "     - Clear screen\n");
    printf("  " CYAN "reset" RESET "     - Reset all variables\n");
    printf("\n");
    printf("Example statements:\n");
    printf("  " YELLOW "var x = 10;" RESET "\n");
    printf("  " YELLOW "net y = 20.5;" RESET "\n");
    printf("  " YELLOW "print(x + y);" RESET "\n");
    printf("  " YELLOW "if (x > 5) print(\"x is big\");" RESET "\n");
    printf("\n");
    
    char line[1024];
    while (1) {
        printf(GREEN "swift> " RESET);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "dbvar") == 0) {
            printf(CYAN "\n╔════════════════════════════════════════════════════════════════════╗\n");
            printf("║                   TABLE DES VARIABLES (dbvar)                    ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            printf("║  Type    │ Nom          │ Taille     │ Valeur         │ Init     ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < dbvar_count; i++) {
                DBVarEntry* entry = &dbvars[i];
                printf("║ %-8s │ %-12s │ %-10d │ %-14s │ %-8s ║\n",
                       entry->type,
                       entry->name,
                       entry->size_bytes,
                       entry->value_str,
                       entry->initialized ? "✓" : "✗");
            }
            
            if (dbvar_count == 0) {
                printf("║                      No variables declared                        ║\n");
            }
            
            printf("╚════════════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        if (strcmp(line, "reset") == 0) {
            cleanupVariables();
            cleanupDBVar();
            printf(GREEN "[INFO]" RESET " All variables reset\n");
            continue;
        }
        if (strlen(line) == 0) continue;
        
        run(line);
    }
    
    printf(CYAN "\n[REPL]" RESET " Exiting SwiftFlow. Goodbye!\n");
}

// ======================================================
// [SECTION] FILE LOADING
// ======================================================
static char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf(RED "[ERROR]" RESET " Cannot open file: %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        printf(RED "[ERROR]" RESET " Memory allocation failed\n");
        return NULL;
    }
    
    size_t bytes_read = fread(source, 1, size, f);
    source[bytes_read] = '\0';
    fclose(f);
    
    return source;
}

// ======================================================
// [SECTION] MAIN
// ======================================================
int main(int argc, char* argv[]) {
    // Initialisation
    srand(time(NULL));
    
    printf(CYAN "\n");
    printf("   _____ _       __ _   _____ _                 \n");
    printf("  / ____(_)     / _| | |  __ (_)                \n");
    printf(" | (___  _ _ __| |_| |_| |__) | _____      __   \n");
    printf("  \\___ \\| | '__|  _| __|  ___/ |/ _ \\ \\ /\\ / /  \n");
    printf("  ____) | | |  | | | |_| |   | | (_) \\ V  V /   \n");
    printf(" |_____/|_|_|  |_|  \\__|_|   |_|\\___/ \\_/\\_/    \n");
    printf("                                                \n");
    printf("         v2.0 - Fusion CLAIR & SYM              \n");
    printf(RESET "\n");
    
    if (argc < 2) {
        // Mode REPL
        repl();
    } else {
        // Mode fichier
        char* source = loadFile(argv[1]);
        if (!source) {
            return 1;
        }
        
        printf(GREEN "[SUCCESS]" RESET ">> Executing %s\n", argv[1]);
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n\n");
        
        run(source);
        free(source);
    }
    
    // Nettoyage final
    cleanupVariables();
    
    return 0;
}
