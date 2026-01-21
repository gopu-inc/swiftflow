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
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] VARIABLE SYSTEM
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
    char* module;
    bool is_exported;
} Variable;

static Variable vars[500];
static int var_count = 0;
static int scope_level = 0;

// ======================================================
// [SECTION] FUNCTION SYSTEM
// ======================================================
typedef struct {
    char name[100];
    ASTNode* params;      // Linked list of parameters
    ASTNode* body;        // Function body
    int param_count;
    char** param_names;   // Parameter names
} Function;

static Function functions[100];
static int func_count = 0;

// ======================================================
// [SECTION] IMPORT SYSTEM
// ======================================================
typedef struct {
    char* name;
    char* file_path;
    bool is_loaded;
} ImportedModule;

static ImportedModule imports[100];
static int import_count = 0;
static char current_working_dir[PATH_MAX];

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static void initWorkingDir(const char* filename) {
    if (filename && strcmp(filename, "REPL") != 0) {
        char abs_path[PATH_MAX];
        if (realpath(filename, abs_path)) {
            char* dir = dirname(strdup(abs_path));
            strncpy(current_working_dir, dir, sizeof(current_working_dir) - 1);
            free(dir);
        }
    } else {
        getcwd(current_working_dir, sizeof(current_working_dir));
    }
}

static int calculateVariableSize(TokenKind type) {
    switch (type) {
        case TK_VAR: return (rand() % 5) + 1;
        case TK_NET: return (rand() % 8) + 1;
        case TK_CLOG: return (rand() % 25) + 1;
        case TK_DOS: return (rand() % 1024) + 1;
        case TK_SEL: return (rand() % 128) + 1;
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

static void registerFunction(const char* name, ASTNode* params, ASTNode* body, int param_count) {
    if (func_count < 100) {
        Function* func = &functions[func_count];
        strncpy(func->name, name, 99);
        func->name[99] = '\0';
        func->params = params;
        func->body = body;
        func->param_count = param_count;
        
        // Extract parameter names
        if (param_count > 0) {
            func->param_names = malloc(param_count * sizeof(char*));
            ASTNode* param = params;
            int i = 0;
            while (param && i < param_count) {
                if (param->type == NODE_IDENT && param->data.name) {
                    func->param_names[i] = str_copy(param->data.name);
                } else {
                    func->param_names[i] = NULL;
                }
                param = param->right;
                i++;
            }
        } else {
            func->param_names = NULL;
        }
        
        func_count++;
        printf("[FUNC] Function '%s' registered (%d parameters)\n", name, param_count);
    }
}

static Function* findFunction(const char* name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            return &functions[i];
        }
    }
    return NULL;
}

static bool isLocalImport(const char* import_path) {
    return import_path[0] == '.' || import_path[0] == '/';
}

static char* resolveImportPath(const char* import_path, const char* from_package) {
    char* resolved = malloc(512);
    if (!resolved) return NULL;
    
    // Handle local imports (./ or ../)
    if (isLocalImport(import_path)) {
        if (import_path[0] == '/') {
            strncpy(resolved, import_path, 511);
            resolved[511] = '\0';
        } else if (strncmp(import_path, "./", 2) == 0) {
            snprintf(resolved, 511, "%s/%s", current_working_dir, import_path + 2);
        } else if (strncmp(import_path, "../", 3) == 0) {
            char parent_dir[512];
            strncpy(parent_dir, current_working_dir, 511);
            parent_dir[511] = '\0';
            char* last_slash = strrchr(parent_dir, '/');
            if (last_slash) *last_slash = '\0';
            snprintf(resolved, 511, "%s/%s", parent_dir, import_path + 3);
        }
        
        if (!strstr(resolved, ".swf")) {
            strncat(resolved, ".swf", 511 - strlen(resolved));
        }
        
        return resolved;
    }
    
    // Handle package imports (from "stdlib")
    if (from_package) {
        char svlib_path[512];
        snprintf(svlib_path, sizeof(svlib_path), 
                "/usr/local/lib/swift/%s/%s.svlib", from_package, from_package);
        
        FILE* f = fopen(svlib_path, "r");
        if (f) {
            char line[256];
            char export_file[100], export_alias[100];
            
            while (fgets(line, sizeof(line), f)) {
                if (line[0] == '/' && line[1] == '/') continue;
                
                if (sscanf(line, "export \"%[^\"]\" as \"%[^\"]\";", 
                          export_file, export_alias) == 2) {
                    if (strcmp(export_alias, import_path) == 0) {
                        snprintf(resolved, 511, "/usr/local/lib/swift/%s/%s", 
                                from_package, export_file);
                        fclose(f);
                        return resolved;
                    }
                }
            }
            fclose(f);
        }
        
        snprintf(resolved, 511, "/usr/local/lib/swift/%s/%s.swf", 
                from_package, import_path);
        return resolved;
    }
    
    snprintf(resolved, 511, "/usr/local/lib/swift/modules/%s.swf", import_path);
    return resolved;
}

// ======================================================
// [SECTION] EXECUTION FUNCTION (DECLARED EARLY)
// ======================================================
static void execute(ASTNode* node);

static bool loadAndExecuteModule(const char* import_path, const char* from_package) {
    char* full_path = resolveImportPath(import_path, from_package);
    if (!full_path) {
        printf("[ERROR] Failed to resolve import path: %s\n", import_path);
        return false;
    }
    
    printf("  🔍 Looking for: %s\n", full_path);
    
    FILE* f = fopen(full_path, "r");
    if (!f) {
        printf("  ❌ Not found: %s\n", full_path);
        free(full_path);
        return false;
    }
    
    printf("  ✅ Found: %s\n", full_path);
    
    // Read module source
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        free(full_path);
        return false;
    }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Save module info
    if (import_count < 100) {
        imports[import_count].name = str_copy(import_path);
        imports[import_count].file_path = str_copy(full_path);
        imports[import_count].is_loaded = true;
        import_count++;
    }
    
    // Save current directory
    char old_dir[PATH_MAX];
    strncpy(old_dir, current_working_dir, sizeof(old_dir));
    
    // Change to module directory for relative imports
    char module_path[PATH_MAX];
    strncpy(module_path, full_path, sizeof(module_path));
    char* module_dir = dirname(module_path);
    strncpy(current_working_dir, module_dir, sizeof(current_working_dir));
    
    // Execute module
    printf("  🚀 Executing module...\n");
    
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (nodes) {
        for (int i = 0; i < count; i++) {
            if (nodes[i] && nodes[i]->type != NODE_FUNC) {
                execute(nodes[i]);
            }
            // Functions are registered during parsing
        }
        // Cleanup nodes
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                if (nodes[i]->type == NODE_STRING && nodes[i]->data.str_val) {
                    free(nodes[i]->data.str_val);
                }
                if (nodes[i]->type == NODE_IDENT && nodes[i]->data.name) {
                    free(nodes[i]->data.name);
                }
                free(nodes[i]);
            }
        }
        free(nodes);
    }
    
    // Restore directory
    strncpy(current_working_dir, old_dir, sizeof(current_working_dir));
    
    free(source);
    free(full_path);
    return true;
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
                } else {
                    return (double)vars[idx].value.int_val;
                }
            }
            printf("[ERROR] Undefined variable: %s\n", node->data.name);
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
                case TK_POW: return pow(left, right);
                case TK_CONCAT: return 0.0; // String operation
                case TK_EQ: return left == right ? 1.0 : 0.0;
                case TK_NEQ: return left != right ? 1.0 : 0.0;
                case TK_GT: return left > right ? 1.0 : 0.0;
                case TK_LT: return left < right ? 1.0 : 0.0;
                case TK_GTE: return left >= right ? 1.0 : 0.0;
                case TK_LTE: return left <= right ? 1.0 : 0.0;
                case TK_AND: return (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
                case TK_OR: return (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
                case TK_IN: return 0.0; // Not implemented
                case TK_IS: return left == right ? 1.0 : 0.0;
                case TK_ISNOT: return left != right ? 1.0 : 0.0;
                default: return 0.0;
            }
        }
            
        case NODE_UNARY: {
            double operand = evalFloat(node->left);
            switch (node->op_type) {
                case TK_MINUS: return -operand;
                case TK_NOT: return operand == 0.0 ? 1.0 : 0.0;
                case TK_BIT_NOT: return ~(int64_t)operand;
                default: return operand;
            }
        }
            
        case NODE_FUNC_CALL: {
            Function* func = findFunction(node->data.name);
            if (func) {
                printf("[FUNC EXEC] Executing %s()\n", node->data.name);
                
                // Save current scope
                int old_scope = scope_level;
                
                // New scope for function
                scope_level++;
                
                // Execute function body
                if (func->body) {
                    execute(func->body);
                }
                
                // Restore scope
                scope_level = old_scope;
                
                // For now, return 0
                return 0.0;
            }
            
            printf("[ERROR] Function not found: %s\n", node->data.name);
            return 0.0;
        }
            
        case NODE_NULL:
        case NODE_UNDEFINED:
            return 0.0;
            
        default:
            printf("[NOTE] Node type %d not fully implemented in evalFloat\n", node->type);
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
            
        case NODE_INT: {
            result = malloc(32);
            if (result) sprintf(result, "%lld", node->data.int_val);
            break;
        }
            
        case NODE_FLOAT: {
            result = malloc(32);
            if (result) {
                double val = node->data.float_val;
                if (val == (int64_t)val) {
                    sprintf(result, "%lld", (int64_t)val);
                } else {
                    char temp[32];
                    sprintf(temp, "%.10f", val);
                    char* dot = strchr(temp, '.');
                    if (dot) {
                        char* end = temp + strlen(temp) - 1;
                        while (end > dot && *end == '0') {
                            *end-- = '\0';
                        }
                        if (end == dot) {
                            *dot = '\0';
                        }
                    }
                    strcpy(result, temp);
                }
            }
            break;
        }
            
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
                    if (result) {
                        double val = vars[idx].value.float_val;
                        if (val == (int64_t)val) {
                            sprintf(result, "%lld", (int64_t)val);
                        } else {
                            char temp[32];
                            sprintf(temp, "%.10f", val);
                            char* dot = strchr(temp, '.');
                            if (dot) {
                                char* end = temp + strlen(temp) - 1;
                                while (end > dot && *end == '0') {
                                    *end-- = '\0';
                                }
                                if (end == dot) {
                                    *dot = '\0';
                                }
                            }
                            strcpy(result, temp);
                        }
                    }
                } else {
                    result = malloc(32);
                    if (result) sprintf(result, "%lld", vars[idx].value.int_val);
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
                if (left_str && right_str) {
                    result = malloc(strlen(left_str) + strlen(right_str) + 1);
                    if (result) {
                        strcpy(result, left_str);
                        strcat(result, right_str);
                    }
                } else {
                    result = str_copy("");
                }
                free(left_str);
                free(right_str);
            } else {
                double val = evalFloat(node);
                result = malloc(32);
                if (result) {
                    if (val == (int64_t)val) {
                        sprintf(result, "%lld", (int64_t)val);
                    } else {
                        char temp[32];
                        sprintf(temp, "%.10f", val);
                        char* dot = strchr(temp, '.');
                        if (dot) {
                            char* end = temp + strlen(temp) - 1;
                            while (end > dot && *end == '0') {
                                *end-- = '\0';
                            }
                            if (end == dot) {
                                *dot = '\0';
                            }
                        }
                        strcpy(result, temp);
                    }
                }
            }
            break;
        }
            
        case NODE_NULL:
            result = str_copy("null");
            break;
            
        case NODE_UNDEFINED:
            result = str_copy("undefined");
            break;
            
        case NODE_FUNC_CALL: {
            Function* func = findFunction(node->data.name);
            if (func) {
                printf("[FUNC EXEC] Executing %s() for string\n", node->data.name);
                
                int old_scope = scope_level;
                scope_level++;
                
                if (func->body) {
                    execute(func->body);
                }
                
                scope_level = old_scope;
                
                result = str_copy("(function executed)");
            } else {
                result = str_copy("undefined");
            }
            break;
        }
            
        default:
            result = str_copy("");
            break;
    }
    
    if (!result) {
        result = str_copy("");
    }
    
    return result;
}

// ======================================================
// [SECTION] EXECUTION FUNCTION IMPLEMENTATION
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
                var->module = NULL;
                var->is_exported = false;
                
                if (node->left) {
                    var->is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        var->is_string = true;
                        var->is_float = false;
                        var->value.str_val = str_copy(node->left->data.str_val);
                        printf("[DECL] %s %s = \"%s\" (%d bytes)\n", 
                               getTypeName(var_type), var->name, var->value.str_val, 
                               var->size_bytes);
                    } 
                    else if (node->left->type == NODE_FLOAT) {
                        var->is_float = true;
                        var->is_string = false;
                        var->value.float_val = evalFloat(node->left);
                        printf("[DECL] %s %s = %g (%d bytes)\n", 
                               getTypeName(var_type), var->name, var->value.float_val, 
                               var->size_bytes);
                    }
                    else if (node->left->type == NODE_BOOL) {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = node->left->data.bool_val ? 1 : 0;
                        printf("[DECL] %s %s = %s (%d bytes)\n", 
                               getTypeName(var_type), var->name, 
                               node->left->data.bool_val ? "true" : "false", 
                               var->size_bytes);
                    }
                    else {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = (int64_t)evalFloat(node->left);
                        printf("[DECL] %s %s = %lld (%d bytes)\n", 
                               getTypeName(var_type), var->name, var->value.int_val, 
                               var->size_bytes);
                    }
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->is_string = false;
                    var->value.int_val = 0;
                    printf("[DECL] %s %s (uninitialized, %d bytes)\n", 
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
                    printf("[ERROR] Cannot assign to constant '%s'\n", node->data.name);
                    return;
                }
                
                if (node->left) {
                    vars[idx].is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        if (vars[idx].value.str_val) free(vars[idx].value.str_val);
                        vars[idx].is_string = true;
                        vars[idx].is_float = false;
                        vars[idx].value.str_val = str_copy(node->left->data.str_val);
                        printf("[ASSIGN] %s = \"%s\"\n", node->data.name, vars[idx].value.str_val);
                    }
                    else if (node->left->type == NODE_FLOAT) {
                        vars[idx].is_float = true;
                        vars[idx].is_string = false;
                        vars[idx].value.float_val = evalFloat(node->left);
                        printf("[ASSIGN] %s = %g\n", node->data.name, vars[idx].value.float_val);
                    }
                    else if (node->left->type == NODE_BOOL) {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].value.int_val = node->left->data.bool_val ? 1 : 0;
                        printf("[ASSIGN] %s = %s\n", node->data.name, 
                               node->left->data.bool_val ? "true" : "false");
                    }
                    else {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].value.int_val = (int64_t)evalFloat(node->left);
                        printf("[ASSIGN] %s = %lld\n", node->data.name, vars[idx].value.int_val);
                    }
                }
            } else {
                printf("[ERROR] Variable '%s' not found\n", node->data.name);
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
            
        case NODE_WELD: {
            if (node->left) {
                char* str = evalString(node->left);
                printf("⚡ WELD: %s\n", str);
                free(str);
            } else {
                printf("⚡ WELD: (no message)\n");
            }
            break;
        }
            
        case NODE_PASS:
            // Do nothing
            break;
            
        case NODE_SIZEOF: {
            int idx = findVar(node->data.size_info.var_name);
            if (idx >= 0) {
                printf("%d bytes\n", vars[idx].size_bytes);
            } else {
                printf("[ERROR] Variable '%s' not found\n", 
                       node->data.size_info.var_name);
            }
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
            
        case NODE_FOR: {
            if (node->data.loop.init) execute(node->data.loop.init);
            while (evalFloat(node->data.loop.condition) != 0) {
                execute(node->data.loop.body);
                if (node->data.loop.update) execute(node->data.loop.update);
            }
            break;
        }
            
        case NODE_RETURN: {
            if (node->left) {
                char* str = evalString(node->left);
                printf("[RETURN] %s\n", str);
                free(str);
            }
            break;
        }
            
        case NODE_BREAK:
            printf("[BREAK] Break statement executed\n");
            // In a real implementation, this would break out of loops
            break;
            
        case NODE_CONTINUE:
            printf("[CONTINUE] Continue statement executed\n");
            // In a real implementation, this would continue to next iteration
            break;
            
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
            printf("\n[EXEC] Starting main()...\n");
            for (int i = 0; i < 60; i++) printf("=");
            printf("\n");
            
            if (node->left) execute(node->left);
            
            printf("\n[EXEC] main() finished\n");
            break;
        }
            
        case NODE_DBVAR: {
            printf("\n╔════════════════════════════════════════════════════════════════╗\n");
            printf("║                   VARIABLE TABLE (dbvar)                       ║\n");
            printf("╠════════════════════════════════════════════════════════════════╣\n");
            printf("║  Type    │ Name        │ Size     │ Value       │ Initialized ║\n");
            printf("╠════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < var_count; i++) {
                Variable* var = &vars[i];
                char value_str[50];
                
                if (var->is_string && var->value.str_val) {
                    snprintf(value_str, sizeof(value_str), "\"%s\"", var->value.str_val);
                } else if (var->is_float) {
                    snprintf(value_str, sizeof(value_str), "%g", var->value.float_val);
                } else {
                    snprintf(value_str, sizeof(value_str), "%lld", var->value.int_val);
                }
                
                printf("║ %-8s │ %-11s │ %-8d │ %-11s │ %-11s ║\n",
                       getTypeName(var->type),
                       var->name,
                       var->size_bytes,
                       value_str,
                       var->is_initialized ? "✓" : "✗");
            }
            
            if (var_count == 0) {
                printf("║                   No variables declared                       ║\n");
            }
            
            printf("║\n║                    FUNCTION TABLE                             ║\n");
            printf("╠════════════════════════════════════════════════════════════════╣\n");
            printf("║  Name        │ Parameters │ Defined                            ║\n");
            printf("╠════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < func_count; i++) {
                Function* func = &functions[i];
                printf("║ %-11s │ %-10d │ %-34s ║\n",
                       func->name,
                       func->param_count,
                       func->body ? "✓" : "✗");
            }
            
            if (func_count == 0) {
                printf("║                   No functions defined                        ║\n");
            }
            
            printf("╚════════════════════════════════════════════════════════════════╝\n");
            break;
        }
            
        case NODE_IMPORT: {
            printf("[IMPORT] ");
            
            if (node->data.imports.from_module) {
                printf("from package '%s': ", node->data.imports.from_module);
            } else {
                printf("local import: ");
            }
            
            for (int i = 0; i < node->data.imports.module_count; i++) {
                printf("%s", node->data.imports.modules[i]);
                if (i < node->data.imports.module_count - 1) printf(", ");
            }
            printf("\n");
            
            // Load each module
            for (int i = 0; i < node->data.imports.module_count; i++) {
                char* module_name = node->data.imports.modules[i];
                char* package_name = node->data.imports.from_module;
                
                if (!loadAndExecuteModule(module_name, package_name)) {
                    printf("  ❌ Failed to import: %s\n", module_name);
                }
            }
            break;
        }
            
        case NODE_FUNC: {
            printf("[FUNC DEF] Function definition: %s\n", node->data.name);
            
            // Count parameters
            int param_count = 0;
            ASTNode* param = node->left;
            while (param) {
                param_count++;
                param = param->right;
            }
            
            registerFunction(node->data.name, node->left, node->right, param_count);
            break;
        }
            
        case NODE_FUNC_CALL: {
            Function* func = findFunction(node->data.name);
            if (func) {
                printf("[FUNC CALL] Calling %s()\n", node->data.name);
                
                // Save current scope
                int old_scope = scope_level;
                
                // New scope for function
                scope_level++;
                
                // Execute function body
                if (func->body) {
                    execute(func->body);
                }
                
                // Restore scope
                scope_level = old_scope;
                
                printf("[FUNC END] %s() finished\n", node->data.name);
            } else {
                printf("[ERROR] Function '%s' not found\n", node->data.name);
                
                // Check if it's a variable
                int idx = findVar(node->data.name);
                if (idx >= 0) {
                    printf("  It's a variable: ");
                    if (vars[idx].is_string) {
                        printf("\"%s\"\n", vars[idx].value.str_val);
                    } else if (vars[idx].is_float) {
                        printf("%g\n", vars[idx].value.float_val);
                    } else {
                        printf("%lld\n", vars[idx].value.int_val);
                    }
                }
            }
            break;
        }
            
        case NODE_BINARY: {
            double result = evalFloat(node);
            printf("%g\n", result);
            break;
        }
            
        case NODE_UNARY: {
            double result = evalFloat(node);
            printf("%g\n", result);
            break;
        }
            
        case NODE_LIST: {
            printf("[LIST] List literal (display not implemented)\n");
            break;
        }
            
        case NODE_GLOBAL:
            printf("[GLOBAL] Global statement (not implemented)\n");
            break;
            
        case NODE_TRY:
            printf("[TRY] Try statement (not implemented)\n");
            break;
            
        case NODE_LAMBDA:
            printf("[LAMBDA] Lambda expression (not implemented)\n");
            break;
            
        case NODE_WITH:
            printf("[WITH] With statement (not implemented)\n");
            break;
            
        case NODE_YIELD:
            printf("[YIELD] Yield statement (not implemented)\n");
            break;
            
        case NODE_APPEND:
            printf("[APPEND] Append operation (not implemented)\n");
            break;
            
        case NODE_WRITE:
            printf("[WRITE] Write operation (not implemented)\n");
            break;
            
        case NODE_READ:
            printf("[READ] Read operation (not implemented)\n");
            break;
            
        case NODE_LEARN:
            printf("[LEARN] Learn statement (not implemented)\n");
            break;
            
        case NODE_LOCK:
            printf("[LOCK] Lock statement (not implemented)\n");
            break;
            
        case NODE_EXPORT:
            printf("[EXPORT] Export statement: %s as %s\n", 
                   node->data.export.symbol, node->data.export.alias);
            break;
            
        case NODE_NULL:
        case NODE_UNDEFINED:
            printf("null/undefined\n");
            break;
            
        case NODE_EMPTY:
            // Empty node, do nothing
            break;
            
        default:
            printf("[NOTE] Node type %d not fully implemented yet\n", node->type);
            break;
    }
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source, const char* filename) {
    initWorkingDir(filename);
    
    printf("📁 Working directory: %s\n", current_working_dir);
    
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) {
        printf("[ERROR] Parsing failed\n");
        return;
    }
    
    ASTNode* main_node = NULL;
    for (int i = 0; i < count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_MAIN) {
            main_node = nodes[i];
            break;
        }
    }
    
    if (main_node) {
        printf("\n[EXEC] Starting main()...\n");
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n");
        
        execute(main_node);
        
        printf("\n[EXEC] main() finished\n");
    } else {
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                execute(nodes[i]);
            }
        }
    }
    
    // Cleanup nodes
    for (int i = 0; i < count; i++) {
        if (nodes[i]) {
            if (nodes[i]->type == NODE_STRING && nodes[i]->data.str_val) {
                free(nodes[i]->data.str_val);
            }
            if (nodes[i]->type == NODE_IDENT && nodes[i]->data.name) {
                free(nodes[i]->data.name);
            }
            if (nodes[i]->type == NODE_SIZEOF && nodes[i]->data.size_info.var_name) {
                free(nodes[i]->data.size_info.var_name);
            }
            if (nodes[i]->type == NODE_IMPORT) {
                for (int j = 0; j < nodes[i]->data.imports.module_count; j++) {
                    free(nodes[i]->data.imports.modules[j]);
                }
                free(nodes[i]->data.imports.modules);
                if (nodes[i]->data.imports.from_module) {
                    free(nodes[i]->data.imports.from_module);
                }
            }
            if (nodes[i]->type == NODE_EXPORT) {
                free(nodes[i]->data.export.symbol);
                free(nodes[i]->data.export.alias);
            }
            free(nodes[i]);
        }
    }
    free(nodes);
    
    // Cleanup variables
    for (int i = 0; i < var_count; i++) {
        if (vars[i].is_string && vars[i].value.str_val) {
            free(vars[i].value.str_val);
        }
        if (vars[i].module) free(vars[i].module);
    }
    var_count = 0;
    scope_level = 0;
    
    // Cleanup functions
    for (int i = 0; i < func_count; i++) {
        if (functions[i].param_names) {
            for (int j = 0; j < functions[i].param_count; j++) {
                free(functions[i].param_names[j]);
            }
            free(functions[i].param_names);
        }
    }
    func_count = 0;
    
    // Cleanup imports
    for (int i = 0; i < import_count; i++) {
        free(imports[i].name);
        free(imports[i].file_path);
    }
    import_count = 0;
}

// ======================================================
// [SECTION] REPL
// ======================================================
static void repl() {
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                SwiftFlow - Complete Edition                     ║\n");
    printf("║           Advanced Import System with Local & Remote            ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    
    printf("\n📦 Import Examples:\n");
    printf("  import \"./file\";                      # Local file\n");
    printf("  import \"file\" from \"stdlib\";         # From package\n");
    printf("  import \"file, io, math\" from \"stdlib\"; # Multiple imports\n");
    printf("  import \"../parent/module\";           # Parent directory\n");
    printf("  import \"/absolute/path/module\";      # Absolute path\n");
    
    printf("\nCommands: exit, dbvar, clear\n\n");
    
    char line[1024];
    while (1) {
        printf("swift>> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "dbvar") == 0) {
            ASTNode node;
            memset(&node, 0, sizeof(node));
            node.type = NODE_DBVAR;
            execute(&node);
            continue;
        }
        if (strlen(line) == 0) continue;
        
        run(line, "REPL");
    }
    
    printf("\n[REPL] Goodbye!\n");
}

// ======================================================
// [SECTION] FILE LOADING
// ======================================================
static char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("[ERROR] Cannot open: %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    return source;
}

// ======================================================
// [SECTION] MAIN
// ======================================================
int main(int argc, char* argv[]) {
    srand(time(NULL));
    
    printf("\n");
    printf("   _____ _       __ _   _____ _                 \n");
    printf("  / ____(_)     / _| | |  __ (_)                \n");
    printf(" | (___  _ _ __| |_| |_| |__) | _____      __   \n");
    printf("  \\___ \\| | '__|  _| __|  ___/ |/ _ \\ \\ /\\ / /  \n");
    printf("  ____) | | |  | | | |_| |   | | (_) \\ V  V /   \n");
    printf(" |_____/|_|_|  |_|  \\__|_|   |_|\\___/ \\_/\\_/    \n");
    printf("                                                \n");
    printf("         Complete Edition - All Features        \n");
    printf("         Advanced Import System v2.0            \n");
    printf("\n");
    
    if (argc < 2) {
        repl();
    } else {
        char* source = loadFile(argv[1]);
        if (!source) return 1;
        
        printf("[LOAD]>> %s\n", argv[1]);
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n\n");
        
        run(source, argv[1]);
        free(source);
    }
    
    return 0;
}