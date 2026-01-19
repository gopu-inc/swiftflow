#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <float.h>
#include "common.h"

extern ASTNode** parse(const char* source, const char* filename, int* count);

// ======================================================
// [SECTION] FORWARD DECLARATIONS
// ======================================================
static void execute(ASTNode* node);
static char* evalString(ASTNode* node);
static double evalFloat(ASTNode* node);
static int64_t evalInt(ASTNode* node);
static bool evalBool(ASTNode* node);

// ======================================================
// [SECTION] VM STATE
// ======================================================
typedef struct {
    char name[256];
    TokenKind type;
    int size_bytes;
    int alignment;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
        void* ptr_val;
    } value;
    bool is_float;
    bool is_string;
    bool is_pointer;
    bool is_initialized;
    bool is_constant;
    bool is_exported;
    int scope_level;
    char* module;
    time_t created_at;
    time_t modified_at;
} Variable;

static Variable vars[1000];
static int var_count = 0;
static int scope_level = 0;
static int max_scope_depth = 100;

// ======================================================
// [SECTION] DBVAR TABLE - STRUCTURE UNIQUE
// ======================================================
typedef struct {
    char name[256];
    char type[32];
    int size_bytes;
    int alignment;
    char value_str[256];
    bool initialized;
    bool constant;
    char module[256];
    time_t created;
    time_t modified;
} DBVarEntry;

static DBVarEntry dbvars[1000];
static int dbvar_count = 0;

// ======================================================
// [SECTION] PACKAGE SYSTEM
// ======================================================
typedef struct {
    char alias[256];
    char module_path[512];
    char package_name[256];
    char symbol_name[256];
    bool is_default;
} ExportEntry;

typedef struct {
    char name[256];
    char version[32];
    char author[256];
    char description[512];
    char license[64];
    ExportEntry exports[100];
    int export_count;
    bool loaded;
    time_t load_time;
    char* manifest_path;
} Package;

static Package packages[100];
static int package_count = 0;

static char* current_module = NULL;
static char* current_package = NULL;
static char* current_dir = NULL;
static char* current_filename = NULL;

// ======================================================
// [SECTION] FUNCTION SYSTEM
// ======================================================
typedef struct {
    char name[256];
    ASTNode* params;
    ASTNode* body;
    char* return_type;
    bool is_async;
    bool is_extern;
    char* module;
    int call_count;
    time_t created_at;
} Function;

static Function functions[500];
static int func_count = 0;

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
        default: return sizeof(int64_t);
    }
}

static int calculateAlignment(TokenKind type) {
    switch (type) {
        case TK_VAR: return 1;
        case TK_NET: return 2;
        case TK_CLOG: return 4;
        case TK_DOS: return 8;
        case TK_SEL: return 16;
        default: return sizeof(void*);
    }
}

static const char* getTypeName(TokenKind type) {
    switch (type) {
        case TK_VAR: return "var";
        case TK_NET: return "net";
        case TK_CLOG: return "clog";
        case TK_DOS: return "dos";
        case TK_SEL: return "sel";
        case TK_CONST: return "const";
        case TK_STATIC: return "static";
        case TK_REF: return "ref";
        case TK_TYPE_INT: return "int";
        case TK_TYPE_FLOAT: return "float";
        case TK_TYPE_STR: return "string";
        case TK_TYPE_BOOL: return "bool";
        case TK_TYPE_CHAR: return "char";
        case TK_TYPE_VOID: return "void";
        case TK_TYPE_ANY: return "any";
        case TK_TYPE_AUTO: return "auto";
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

static int findFunction(const char* name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void addToDBVar(const char* name, TokenKind type, int size_bytes, 
                       int alignment, const char* value_str, bool initialized,
                       bool is_constant, const char* module) {
    if (dbvar_count < 1000) {
        DBVarEntry* entry = &dbvars[dbvar_count++];
        strncpy(entry->name, name, 255);
        entry->name[255] = '\0';
        strncpy(entry->type, getTypeName(type), 31);
        entry->type[31] = '\0';
        entry->size_bytes = size_bytes;
        entry->alignment = alignment;
        
        if (value_str) {
            strncpy(entry->value_str, value_str, 255);
            entry->value_str[255] = '\0';
        } else {
            strcpy(entry->value_str, "N/A");
        }
        
        entry->initialized = initialized;
        entry->constant = is_constant;
        
        if (module) {
            strncpy(entry->module, module, 255);
            entry->module[255] = '\0';
        } else {
            strcpy(entry->module, current_module ? current_module : "global");
        }
        
        entry->created = time(NULL);
        entry->modified = entry->created;
    }
}

static void updateDBVar(const char* name, const char* value_str) {
    for (int i = 0; i < dbvar_count; i++) {
        if (strcmp(dbvars[i].name, name) == 0) {
            if (value_str) {
                strncpy(dbvars[i].value_str, value_str, 255);
                dbvars[i].value_str[255] = '\0';
            }
            dbvars[i].initialized = true;
            dbvars[i].modified = time(NULL);
            break;
        }
    }
}

// ======================================================
// [SECTION] PATH RESOLUTION
// ======================================================
static char* normalizePath(const char* path) {
    if (!path) return NULL;
    
    char* result = strdup(path);
    if (!result) return NULL;
    
    // Replace backslashes with forward slashes
    for (char* p = result; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    
    // Remove trailing slashes
    size_t len = strlen(result);
    while (len > 1 && result[len-1] == '/') {
        result[--len] = '\0';
    }
    
    return result;
}

static char* resolveRelativePath(const char* base_dir, const char* relative_path) {
    if (!relative_path || strlen(relative_path) == 0) {
        return NULL;
    }
    
    // Handle absolute paths
    if (relative_path[0] == '/') {
        return strdup(relative_path);
    }
    
    // Normalize base directory
    char* normalized_base = NULL;
    if (base_dir && strlen(base_dir) > 0) {
        normalized_base = normalizePath(base_dir);
    } else {
        normalized_base = strdup(".");
    }
    
    if (!normalized_base) {
        return NULL;
    }
    
    // Normalize relative path
    char* normalized_rel = normalizePath(relative_path);
    if (!normalized_rel) {
        free(normalized_base);
        return NULL;
    }
    
    // Split paths into components
    char* components[256];
    int comp_count = 0;
    
    // Start with base directory components
    char* token = strtok(normalized_base, "/");
    while (token && comp_count < 255) {
        components[comp_count++] = token;
        token = strtok(NULL, "/");
    }
    
    // Process relative path components
    token = strtok(normalized_rel, "/");
    while (token && comp_count < 255) {
        if (strcmp(token, ".") == 0) {
            // Current directory - do nothing
        } else if (strcmp(token, "..") == 0) {
            // Parent directory
            if (comp_count > 0) {
                comp_count--;
            }
        } else {
            // Regular component
            components[comp_count++] = token;
        }
        token = strtok(NULL, "/");
    }
    
    // Reconstruct path
    char* result = malloc(PATH_MAX);
    if (!result) {
        free(normalized_base);
        free(normalized_rel);
        return NULL;
    }
    
    result[0] = '\0';
    
    if (relative_path[0] == '/') {
        strcat(result, "/");
    }
    
    for (int i = 0; i < comp_count; i++) {
        if (i > 0) strcat(result, "/");
        strcat(result, components[i]);
    }
    
    // Handle empty path
    if (strlen(result) == 0) {
        strcpy(result, ".");
    }
    
    free(normalized_base);
    free(normalized_rel);
    
    return result;
}

// ======================================================
// [SECTION] PACKAGE MANAGEMENT
// ======================================================
static Package* findPackage(const char* name) {
    for (int i = 0; i < package_count; i++) {
        if (strcmp(packages[i].name, name) == 0) {
            return &packages[i];
        }
    }
    return NULL;
}

static Package* createPackage(const char* name) {
    if (package_count >= 100) {
        printf(RED "[ERROR]" RESET " Too many packages (max: 100)\n");
        return NULL;
    }
    
    Package* pkg = &packages[package_count++];
    strncpy(pkg->name, name, 255);
    pkg->name[255] = '\0';
    strcpy(pkg->version, "1.0.0");
    strcpy(pkg->author, "Unknown");
    strcpy(pkg->description, "No description");
    strcpy(pkg->license, "MIT");
    pkg->export_count = 0;
    pkg->loaded = false;
    pkg->load_time = 0;
    pkg->manifest_path = NULL;
    
    return pkg;
}

static void addExportToPackage(const char* package_name, const char* alias, 
                               const char* symbol, const char* module_path, 
                               bool is_default) {
    Package* pkg = findPackage(package_name);
    if (!pkg) {
        pkg = createPackage(package_name);
        if (!pkg) return;
    }
    
    if (pkg->export_count >= 100) {
        printf(RED "[ERROR]" RESET " Too many exports in package %s\n", package_name);
        return;
    }
    
    ExportEntry* exp = &pkg->exports[pkg->export_count++];
    strncpy(exp->alias, alias, 255);
    exp->alias[255] = '\0';
    strncpy(exp->symbol_name, symbol, 255);
    exp->symbol_name[255] = '\0';
    
    if (module_path) {
        strncpy(exp->module_path, module_path, 511);
        exp->module_path[511] = '\0';
    } else {
        exp->module_path[0] = '\0';
    }
    
    strncpy(exp->package_name, package_name, 255);
    exp->package_name[255] = '\0';
    exp->is_default = is_default;
    
    printf(CYAN "[PACKAGE]" RESET " %s exports '%s' as '%s'%s\n", 
           package_name, symbol, alias, is_default ? " (default)" : "");
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
            
            // Check if it's a function
            int func_idx = findFunction(node->data.name);
            if (func_idx >= 0) {
                return functions[func_idx].call_count;
            }
            
            printf(RED "[ERROR]" RESET " Undefined identifier: %s\n", node->data.name);
            return 0.0;
        }
            
        case NODE_BINARY: {
            double left = evalFloat(node->left);
            double right = evalFloat(node->right);
            
            switch (node->op_type) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: 
                    if (right == 0.0) {
                        printf(RED "[ERROR]" RESET " Division by zero\n");
                        return 0.0;
                    }
                    return left / right;
                case TK_MOD: 
                    if (right == 0.0) {
                        printf(RED "[ERROR]" RESET " Modulo by zero\n");
                        return 0.0;
                    }
                    return fmod(left, right);
                case TK_POW: return pow(left, right);
                case TK_EQ: return left == right ? 1.0 : 0.0;
                case TK_NEQ: return left != right ? 1.0 : 0.0;
                case TK_GT: return left > right ? 1.0 : 0.0;
                case TK_LT: return left < right ? 1.0 : 0.0;
                case TK_GTE: return left >= right ? 1.0 : 0.0;
                case TK_LTE: return left <= right ? 1.0 : 0.0;
                case TK_AND: return (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
                case TK_OR: return (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
                default: 
                    printf(YELLOW "[WARNING]" RESET " Unsupported binary operator for float: %d\n", node->op_type);
                    return 0.0;
            }
        }
            
        case NODE_UNARY: {
            double operand = evalFloat(node->left);
            switch (node->op_type) {
                case TK_MINUS: return -operand;
                case TK_PLUS: return +operand;
                case TK_NOT: return operand == 0.0 ? 1.0 : 0.0;
                case TK_INC: return operand + 1.0;
                case TK_DEC: return operand - 1.0;
                default: return operand;
            }
        }
            
        case NODE_TERNARY: {
            double condition = evalFloat(node->left);
            if (condition != 0.0) {
                return evalFloat(node->right);
            } else {
                return evalFloat(node->third);
            }
        }
            
        default:
            printf(YELLOW "[WARNING]" RESET " Cannot convert node type %d to float\n", node->type);
            return 0.0;
    }
}

static int64_t evalInt(ASTNode* node) {
    return (int64_t)evalFloat(node);
}

static bool evalBool(ASTNode* node) {
    return evalFloat(node) != 0.0;
}

static char* evalString(ASTNode* node) {
    if (!node) return str_copy("");
    
    char* result = NULL;
    char buffer[256];
    
    switch (node->type) {
        case NODE_STRING:
            result = str_copy(node->data.str_val);
            break;
            
        case NODE_INT:
            snprintf(buffer, sizeof(buffer), "%lld", node->data.int_val);
            result = str_copy(buffer);
            break;
            
        case NODE_FLOAT: {
            double val = node->data.float_val;
            // Check if it's an integer value
            if (val == (int64_t)val) {
                snprintf(buffer, sizeof(buffer), "%lld", (int64_t)val);
            } else {
                // Format with minimal precision
                char temp[64];
                snprintf(temp, sizeof(temp), "%.10g", val);
                
                // Remove trailing zeros
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
                strcpy(buffer, temp);
            }
            result = str_copy(buffer);
            break;
        }
            
        case NODE_BOOL:
            result = str_copy(node->data.bool_val ? "true" : "false");
            break;
            
        case NODE_NULL:
            result = str_copy("null");
            break;
            
        case NODE_UNDEFINED:
            result = str_copy("undefined");
            break;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_string && vars[idx].value.str_val) {
                    result = str_copy(vars[idx].value.str_val);
                } else if (vars[idx].is_float) {
                    double val = vars[idx].value.float_val;
                    if (val == (int64_t)val) {
                        snprintf(buffer, sizeof(buffer), "%lld", (int64_t)val);
                    } else {
                        char temp[64];
                        snprintf(temp, sizeof(temp), "%.10g", val);
                        
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
                        strcpy(buffer, temp);
                    }
                    result = str_copy(buffer);
                } else {
                    snprintf(buffer, sizeof(buffer), "%lld", vars[idx].value.int_val);
                    result = str_copy(buffer);
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
                if (val == (int64_t)val) {
                    snprintf(buffer, sizeof(buffer), "%lld", (int64_t)val);
                } else {
                    char temp[64];
                    snprintf(temp, sizeof(temp), "%.10g", val);
                    
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
                    strcpy(buffer, temp);
                }
                result = str_copy(buffer);
            }
            break;
        }
            
        default:
            snprintf(buffer, sizeof(buffer), "[object NodeType:%d]", node->type);
            result = str_copy(buffer);
            break;
    }
    
    return result;
}

// ======================================================
// [SECTION] MODULE IMPORT
// ======================================================
static void importModule(const char* module_name, const char* from_package, const char* alias);

static void executeImport(ASTNode* node) {
    if (!node) return;
    
    for (int i = 0; i < node->data.imports.module_count; i++) {
        if (node->data.imports.modules[i]) {
            importModule(node->data.imports.modules[i], 
                        node->data.imports.from_module,
                        node->data.imports.alias);
        }
    }
}

static void importModule(const char* module_name, const char* from_package, const char* alias) {
    if (!module_name) return;
    
    printf(CYAN "[IMPORT]" RESET " %s", module_name);
    if (from_package) printf(" from %s", from_package);
    if (alias) printf(" as %s", alias);
    printf("\n");
    
    // Handle wildcard import
    if (strcmp(module_name, "*") == 0 && from_package) {
        printf(CYAN "[IMPORT]" RESET " Wildcard import from package: %s\n", from_package);
        // In a full implementation, we would import all exports from the package
        return;
    }
    
    // Resolve module path
    char* module_path = NULL;
    
    // Check for local file
    if (module_name[0] == '.' || module_name[0] == '/') {
        // Local path
        module_path = resolveRelativePath(current_dir, module_name);
        if (!module_path) {
            printf(RED "[ERROR]" RESET " Failed to resolve path: %s\n", module_name);
            return;
        }
        
        // Check for .swf extension
        struct stat st;
        if (stat(module_path, &st) != 0) {
            char* with_ext = malloc(strlen(module_path) + 5);
            sprintf(with_ext, "%s.swf", module_path);
            
            if (stat(with_ext, &st) == 0) {
                free(module_path);
                module_path = with_ext;
            } else {
                free(with_ext);
            }
        }
    } else {
        // System module
        char system_path[512];
        snprintf(system_path, sizeof(system_path), 
                "/usr/local/lib/swiftflow/%s.swf", module_name);
        module_path = strdup(system_path);
    }
    
    if (!module_path) {
        printf(RED "[ERROR]" RESET " Failed to resolve module: %s\n", module_name);
        return;
    }
    
    // Check if file exists
    struct stat st;
    if (stat(module_path, &st) != 0) {
        printf(RED "[ERROR]" RESET " Module not found: %s\n", module_path);
        free(module_path);
        return;
    }
    
    // Load and execute module
    FILE* f = fopen(module_path, "r");
    if (!f) {
        printf(RED "[ERROR]" RESET " Cannot open module: %s\n", module_path);
        free(module_path);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        free(module_path);
        printf(RED "[ERROR]" RESET " Memory allocation failed\n");
        return;
    }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Save current state
    char* old_module = current_module ? strdup(current_module) : NULL;
    char* old_package = current_package ? strdup(current_package) : NULL;
    char* old_dir = current_dir ? strdup(current_dir) : NULL;
    char* old_filename = current_filename ? strdup(current_filename) : NULL;
    
    // Set new state
    current_module = strdup(module_name);
    current_filename = strdup(module_path);
    
    // Extract directory from path
    char* last_slash = strrchr(module_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        current_dir = strdup(module_path);
        *last_slash = '/';
    } else {
        current_dir = strdup(".");
    }
    
    // Parse and execute
    printf(CYAN "[MODULE]" RESET " Executing: %s\n", module_name);
    
    int node_count = 0;
    ASTNode** nodes = parse(source, module_path, &node_count);
    
    if (nodes) {
        // First pass: execute imports
        for (int i = 0; i < node_count; i++) {
            if (nodes[i] && nodes[i]->type == NODE_IMPORT) {
                executeImport(nodes[i]);
            }
        }
        
        // Second pass: execute everything else
        for (int i = 0; i < node_count; i++) {
            if (nodes[i] && nodes[i]->type != NODE_IMPORT) {
                execute(nodes[i]);
            }
        }
        
        // Cleanup
        for (int i = 0; i < node_count; i++) {
            if (nodes[i]) free(nodes[i]);
        }
        free(nodes);
    }
    
    // Restore state
    free(current_module);
    free(current_package);
    free(current_dir);
    free(current_filename);
    
    current_module = old_module;
    current_package = old_package;
    current_dir = old_dir;
    current_filename = old_filename;
    
    free(source);
    free(module_path);
}

// ======================================================
// [SECTION] EXECUTION ENGINE
// ======================================================
static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL:
        case NODE_STATIC_DECL:
        case NODE_REF_DECL: {
            TokenKind var_type = TK_VAR;
            switch (node->type) {
                case NODE_NET_DECL: var_type = TK_NET; break;
                case NODE_CLOG_DECL: var_type = TK_CLOG; break;
                case NODE_DOS_DECL: var_type = TK_DOS; break;
                case NODE_SEL_DECL: var_type = TK_SEL; break;
                case NODE_CONST_DECL: var_type = TK_CONST; break;
                case NODE_STATIC_DECL: var_type = TK_STATIC; break;
                case NODE_REF_DECL: var_type = TK_REF; break;
                default: var_type = TK_VAR; break;
            }
            
            if (var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, node->data.name, 255);
                var->name[255] = '\0';
                var->type = var_type;
                var->size_bytes = calculateVariableSize(var_type);
                var->alignment = calculateAlignment(var_type);
                var->scope_level = scope_level;
                var->is_constant = (var_type == TK_CONST);
                var->module = current_module ? strdup(current_module) : NULL;
                var->is_exported = false;
                var->created_at = time(NULL);
                var->modified_at = var->created_at;
                
                if (node->left) {
                    var->is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        var->is_string = true;
                        var->is_float = false;
                        var->is_pointer = false;
                        var->value.str_val = str_copy(node->left->data.str_val);
                        
                        addToDBVar(var->name, var_type, var->size_bytes, var->alignment,
                                  var->value.str_val, true, var->is_constant, var->module);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = \"%s\" (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, var->value.str_val, 
                               var->size_bytes, var->module ? var->module : "global");
                    } 
                    else if (node->left->type == NODE_FLOAT) {
                        var->is_float = true;
                        var->is_string = false;
                        var->is_pointer = false;
                        var->value.float_val = evalFloat(node->left);
                        
                        char value_str[256];
                        char* str_val = evalString(node->left);
                        strncpy(value_str, str_val, 255);
                        value_str[255] = '\0';
                        
                        addToDBVar(var->name, var_type, var->size_bytes, var->alignment,
                                  value_str, true, var->is_constant, var->module);
                        free(str_val);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %s (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, value_str, 
                               var->size_bytes, var->module ? var->module : "global");
                    }
                    else if (node->left->type == NODE_BOOL) {
                        var->is_float = false;
                        var->is_string = false;
                        var->is_pointer = false;
                        var->value.int_val = node->left->data.bool_val ? 1 : 0;
                        
                        addToDBVar(var->name, var_type, var->size_bytes, var->alignment,
                                  node->left->data.bool_val ? "true" : "false", 
                                  true, var->is_constant, var->module);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %s (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, 
                               node->left->data.bool_val ? "true" : "false", 
                               var->size_bytes, var->module ? var->module : "global");
                    }
                    else {
                        var->is_float = false;
                        var->is_string = false;
                        var->is_pointer = false;
                        var->value.int_val = evalInt(node->left);
                        
                        char value_str[64];
                        snprintf(value_str, sizeof(value_str), "%lld", var->value.int_val);
                        
                        addToDBVar(var->name, var_type, var->size_bytes, var->alignment,
                                  value_str, true, var->is_constant, var->module);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %lld (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, var->value.int_val, 
                               var->size_bytes, var->module ? var->module : "global");
                    }
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->is_string = false;
                    var->is_pointer = false;
                    var->value.int_val = 0;
                    
                    addToDBVar(var->name, var_type, var->size_bytes, var->alignment,
                              "uninitialized", false, var->is_constant, var->module);
                    
                    printf(GREEN "[DECL]" RESET " %s %s (uninitialized, %d bytes) [%s]\n", 
                           getTypeName(var_type), var->name, var->size_bytes, 
                           var->module ? var->module : "global");
                }
                
                var_count++;
            } else {
                printf(RED "[ERROR]" RESET " Too many variables (max: 1000)\n");
            }
            break;
        }
            
        case NODE_ASSIGN:
        case NODE_COMPOUND_ASSIGN: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_constant) {
                    printf(RED "[ERROR]" RESET " Cannot assign to constant '%s'\n", node->data.name);
                    return;
                }
                
                if (node->left) {
                    vars[idx].is_initialized = true;
                    vars[idx].modified_at = time(NULL);
                    
                    if (node->left->type == NODE_STRING) {
                        if (vars[idx].value.str_val) free(vars[idx].value.str_val);
                        vars[idx].is_string = true;
                        vars[idx].is_float = false;
                        vars[idx].is_pointer = false;
                        vars[idx].value.str_val = str_copy(node->left->data.str_val);
                        
                        updateDBVar(node->data.name, vars[idx].value.str_val);
                        printf(CYAN "[ASSIGN]" RESET " %s = \"%s\"\n", 
                               node->data.name, vars[idx].value.str_val);
                    }
                    else if (node->left->type == NODE_FLOAT) {
                        vars[idx].is_float = true;
                        vars[idx].is_string = false;
                        vars[idx].is_pointer = false;
                        vars[idx].value.float_val = evalFloat(node->left);
                        
                        char value_str[256];
                        char* str_val = evalString(node->left);
                        strncpy(value_str, str_val, 255);
                        value_str[255] = '\0';
                        
                        updateDBVar(node->data.name, value_str);
                        free(str_val);
                        
                        printf(CYAN "[ASSIGN]" RESET " %s = %s\n", 
                               node->data.name, value_str);
                    }
                    else if (node->left->type == NODE_BOOL) {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].is_pointer = false;
                        vars[idx].value.int_val = node->left->data.bool_val ? 1 : 0;
                        
                        updateDBVar(node->data.name, 
                                   node->left->data.bool_val ? "true" : "false");
                        
                        printf(CYAN "[ASSIGN]" RESET " %s = %s\n", 
                               node->data.name, node->left->data.bool_val ? "true" : "false");
                    }
                    else {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].is_pointer = false;
                        vars[idx].value.int_val = evalInt(node->left);
                        
                        char value_str[64];
                        snprintf(value_str, sizeof(value_str), "%lld", vars[idx].value.int_val);
                        updateDBVar(node->data.name, value_str);
                        
                        printf(CYAN "[ASSIGN]" RESET " %s = %lld\n", 
                               node->data.name, vars[idx].value.int_val);
                    }
                }
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", node->data.name);
            }
            break;
        }
            
        case NODE_PRINT: {
            ASTNode* current = node->left;
            while (current) {
                char* str = evalString(current);
                printf("%s", str);
                free(str);
                
                current = current->right;
                if (current) printf(" ");
            }
            printf("\n");
            break;
        }
            
        case NODE_SIZEOF: {
            int idx = findVar(node->data.size_info.var_name);
            if (idx >= 0) {
                printf("%d bytes (alignment: %d)\n", 
                       vars[idx].size_bytes, vars[idx].alignment);
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", 
                       node->data.size_info.var_name);
            }
            break;
        }
            
        case NODE_DBVAR: {
            printf(CYAN "\n╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
            printf("║                                         TABLE DES VARIABLES (dbvar)                                          ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
            printf("║  Type    │ Nom               │ Taille     │ Align │ Valeur               │ Init  │ Const │ Module          ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < dbvar_count; i++) {
                DBVarEntry* entry = &dbvars[i];
                printf("║ %-8s │ %-17s │ %-10d │ %-5d │ %-20s │ %-5s │ %-5s │ %-15s ║\n",
                       entry->type,
                       entry->name,
                       entry->size_bytes,
                       entry->alignment,
                       entry->value_str,
                       entry->initialized ? "✓" : "✗",
                       entry->constant ? "✓" : "✗",
                       entry->module);
            }
            
            if (dbvar_count == 0) {
                printf("║                                    No variables declared                                               ║\n");
            }
            
            printf("╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n" RESET);
            break;
        }
            
        case NODE_IMPORT:
            executeImport(node);
            break;
            
        case NODE_EXPORT: {
            int idx = findVar(node->data.export.symbol);
            if (idx >= 0) {
                vars[idx].is_exported = true;
                printf(CYAN "[EXPORT]" RESET " Exporting variable: %s as %s%s\n", 
                       node->data.export.symbol, node->data.export.alias,
                       node->data.export.is_default ? " (default)" : "");
                
                if (current_package) {
                    addExportToPackage(current_package, 
                                      node->data.export.alias,
                                      node->data.export.symbol,
                                      NULL,
                                      node->data.export.is_default);
                }
            } else {
                printf(RED "[ERROR]" RESET " Cannot export undefined variable: %s\n", 
                       node->data.export.symbol);
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
            if (node->data.loop.init) {
                execute(node->data.loop.init);
            }
            
            while (evalFloat(node->data.loop.condition) != 0) {
                execute(node->data.loop.body);
                
                if (node->data.loop.update) {
                    execute(node->data.loop.update);
                }
            }
            break;
        }
            
        case NODE_BLOCK: {
            scope_level++;
            
            ASTNode* current_stmt = node->left;
            while (current_stmt) {
                execute(current_stmt);
                current_stmt = current_stmt->right;
            }
            
            scope_level--;
            break;
        }
            
        case NODE_MAIN: {
            printf(CYAN "\n[EXEC]" RESET " Starting main()...\n");
            for (int i = 0; i < 80; i++) printf("═");
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
                printf(CYAN "[RETURN]" RESET " (void)\n");
            }
            break;
        }
            
        case NODE_FUNC_DECL: {
            if (func_count < 500) {
                Function* func = &functions[func_count++];
                strncpy(func->name, node->data.func.func_name, 255);
                func->name[255] = '\0';
                func->params = node->data.func.params;
                func->body = node->data.func.body;
                func->return_type = node->data.func.return_type ? 
                    str_copy(node->data.func.return_type->data.name) : NULL;
                func->is_async = node->data.func.is_async;
                func->is_extern = node->data.func.is_extern;
                func->module = current_module ? strdup(current_module) : NULL;
                func->call_count = 0;
                func->created_at = time(NULL);
                
                printf(GREEN "[FUNC]" RESET " Declared function: %s%s%s\n",
                       func->name,
                       func->is_async ? " (async)" : "",
                       func->is_extern ? " (extern)" : "");
            }
            break;
        }
            
        case NODE_FUNC_CALL: {
            if (node->left && node->left->type == NODE_IDENT) {
                int func_idx = findFunction(node->left->data.name);
                if (func_idx >= 0) {
                    functions[func_idx].call_count++;
                    printf(CYAN "[CALL]" RESET " %s() [call #%d]\n",
                           node->left->data.name, functions[func_idx].call_count);
                    
                    if (functions[func_idx].body) {
                        execute(functions[func_idx].body);
                    }
                } else {
                    printf(RED "[ERROR]" RESET " Undefined function: %s\n", 
                           node->left->data.name);
                }
            }
            break;
        }
            
        case NODE_BINARY: {
            // Evaluate expression
            char* str = evalString(node);
            if (str && str[0] != '\0') {
                printf("%s\n", str);
            }
            free(str);
            break;
        }
            
        default:
            printf(YELLOW "[WARNING]" RESET " Unhandled node type: %d\n", node->type);
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
        if (vars[i].module) free(vars[i].module);
    }
    var_count = 0;
    scope_level = 0;
}

static void cleanupDBVar() {
    dbvar_count = 0;
}

static void cleanupPackages() {
    for (int i = 0; i < package_count; i++) {
        if (packages[i].manifest_path) free(packages[i].manifest_path);
    }
    package_count = 0;
    
    if (current_module) free(current_module);
    if (current_package) free(current_package);
    if (current_dir) free(current_dir);
    if (current_filename) free(current_filename);
    
    current_module = NULL;
    current_package = NULL;
    current_dir = NULL;
    current_filename = NULL;
}

static void cleanupFunctions() {
    for (int i = 0; i < func_count; i++) {
        if (functions[i].return_type) free(functions[i].return_type);
        if (functions[i].module) free(functions[i].module);
    }
    func_count = 0;
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source, const char* filename) {
    int count = 0;
    ASTNode** nodes = parse(source, filename, &count);
    
    if (!nodes) {
        printf(RED "[ERROR]" RESET " Failed to parse source code\n");
        return;
    }
    
    // Set current module name
    if (current_module) free(current_module);
    current_module = strdup(filename);
    
    // Set current filename
    if (current_filename) free(current_filename);
    current_filename = strdup(filename);
    
    // Set current directory
    if (current_dir) free(current_dir);
    char* file_copy = strdup(filename);
    char* last_slash = strrchr(file_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        current_dir = strdup(file_copy);
    } else {
        current_dir = strdup(".");
    }
    free(file_copy);
    
    // Execute imports first
    for (int i = 0; i < count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_IMPORT) {
            executeImport(nodes[i]);
        }
    }
    
    // Find and execute main() if it exists
    ASTNode* main_node = NULL;
    for (int i = 0; i < count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_MAIN) {
            main_node = nodes[i];
            break;
        }
    }
    
    if (main_node) {
        printf(CYAN "\n[EXEC]" RESET " Starting main()...\n");
        for (int i = 0; i < 80; i++) printf("═");
        printf("\n");
        
        execute(main_node);
        
        printf("\n" CYAN "[EXEC]" RESET " main() finished successfully\n");
    } else {
        // Execute all declarations in order (except imports already done)
        for (int i = 0; i < count; i++) {
            if (nodes[i] && nodes[i]->type != NODE_IMPORT) {
                execute(nodes[i]);
            }
        }
    }
    
    // Cleanup AST nodes
    for (int i = 0; i < count; i++) {
        if (nodes[i]) {
            // Free node-specific data
            if (nodes[i]->type == NODE_IMPORT) {
                for (int j = 0; j < nodes[i]->data.imports.module_count; j++) {
                    free(nodes[i]->data.imports.modules[j]);
                }
                free(nodes[i]->data.imports.modules);
                if (nodes[i]->data.imports.from_module) {
                    free(nodes[i]->data.imports.from_module);
                }
                if (nodes[i]->data.imports.alias) {
                    free(nodes[i]->data.imports.alias);
                }
            }
            else if (nodes[i]->type == NODE_EXPORT) {
                if (nodes[i]->data.export.symbol) free(nodes[i]->data.export.symbol);
                if (nodes[i]->data.export.alias) free(nodes[i]->data.export.alias);
            }
            else if (nodes[i]->type == NODE_STRING && nodes[i]->data.str_val) {
                free(nodes[i]->data.str_val);
            }
            else if (nodes[i]->type == NODE_IDENT && nodes[i]->data.name) {
                free(nodes[i]->data.name);
            }
            else if (nodes[i]->type == NODE_SIZEOF && nodes[i]->data.size_info.var_name) {
                free(nodes[i]->data.size_info.var_name);
            }
            else if (nodes[i]->type == NODE_ARRAY || nodes[i]->type == NODE_MAP) {
                for (int j = 0; j < nodes[i]->data.collection.element_count; j++) {
                    free(nodes[i]->data.collection.elements[j]);
                }
                free(nodes[i]->data.collection.elements);
            }
            else if (nodes[i]->type == NODE_FUNC_DECL) {
                if (nodes[i]->data.func.func_name) free(nodes[i]->data.func.func_name);
            }
            
            free(nodes[i]);
        }
    }
    free(nodes);
}

// ======================================================
// [SECTION] REPL (IMPROVED)
// ======================================================
static void repl() {
    printf(GREEN "╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                    SwiftFlow v2.0 - REPL Mode                                                   ║\n");
    printf("║                                               Types CLAIR & SYM Fusion - Complete Language                                      ║\n");
    printf("║                                               Advanced Package System with Full Support                                         ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n" RESET);
    
    printf("\n");
    printf("Available commands:\n");
    printf("  " CYAN "exit" RESET "          - Quit the REPL\n");
    printf("  " CYAN "dbvar" RESET "         - Show variable table\n");
    printf("  " CYAN "clear" RESET "         - Clear screen\n");
    printf("  " CYAN "reset" RESET "         - Reset all variables and packages\n");
    printf("  " CYAN "packages" RESET "      - Show loaded packages\n");
    printf("  " CYAN "functions" RESET "     - Show declared functions\n");
    printf("  " CYAN "help" RESET "          - Show help information\n");
    printf("  " CYAN "version" RESET "       - Show version information\n");
    
    printf("\n");
    printf("Example statements:\n");
    printf("  " YELLOW "var x = 10;" RESET "\n");
    printf("  " YELLOW "net y = 20.5;" RESET "\n");
    printf("  " YELLOW "print(x + y);" RESET "\n");
    printf("  " YELLOW "if (x > 5) print(\"x is big\");" RESET "\n");
    printf("  " YELLOW "import \"./file.swf\";" RESET "\n");
    printf("  " YELLOW "import \"math\" from \"stdlib\";" RESET "\n");
    printf("  " YELLOW "export myVar as \"publicVar\";" RESET "\n");
    printf("  " YELLOW "func add(a, b) { return a + b; }" RESET "\n");
    printf("  " YELLOW "add(5, 3);" RESET "\n");
    
    printf("\n");
    
    char line[4096];
    int line_number = 1;
    
    while (1) {
        printf(GREEN "%04d swift> " RESET, line_number++);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        // Handle commands
        if (strcmp(line, "exit") == 0) break;
        
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        
        if (strcmp(line, "dbvar") == 0) {
            execute(newNode(NODE_DBVAR));
            continue;
        }
        
        if (strcmp(line, "packages") == 0) {
            printf(CYAN "\n╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
            printf("║                                                  LOADED PACKAGES                                                  ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < package_count; i++) {
                Package* pkg = &packages[i];
                printf("║ %-25s │ v%-8s │ Exports: %-3d │ Loaded: %-5s ║\n",
                       pkg->name,
                       pkg->version,
                       pkg->export_count,
                       pkg->loaded ? "✓" : "✗");
                
                for (int j = 0; j < pkg->export_count; j++) {
                    printf("║   └─ %-20s → %-45s %s ║\n",
                           pkg->exports[j].alias,
                           pkg->exports[j].symbol_name,
                           pkg->exports[j].is_default ? "(default)" : "");
                }
            }
            
            if (package_count == 0) {
                printf("║                                          No packages loaded                                               ║\n");
            }
            
            printf("╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        
        if (strcmp(line, "functions") == 0) {
            printf(CYAN "\n╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
            printf("║                                                DECLARED FUNCTIONS                                                ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < func_count; i++) {
                Function* func = &functions[i];
                printf("║ %-25s │ Calls: %-4d │ Async: %-3s │ Extern: %-3s │ Module: %-15s ║\n",
                       func->name,
                       func->call_count,
                       func->is_async ? "✓" : "✗",
                       func->is_extern ? "✓" : "✗",
                       func->module ? func->module : "global");
            }
            
            if (func_count == 0) {
                printf("║                                          No functions declared                                              ║\n");
            }
            
            printf("╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        
        if (strcmp(line, "reset") == 0) {
            cleanupVariables();
            cleanupDBVar();
            cleanupPackages();
            cleanupFunctions();
            printf(GREEN "[INFO]" RESET " All variables, packages, and functions reset\n");
            continue;
        }
        
        if (strcmp(line, "help") == 0) {
            printf("\nSwiftFlow Help:\n");
            printf("  Type SwiftFlow code to execute it immediately\n");
            printf("  Use ';' to separate multiple statements\n");
            printf("  Use import/export for module management\n");
            printf("  Use func to declare functions\n");
            printf("  Use var, net, clog, dos, sel for variable declarations\n");
            printf("  Use print() for output\n");
            printf("  Use dbvar to debug variables\n");
            printf("\n");
            continue;
        }
        
        if (strcmp(line, "version") == 0) {
            printf("\nSwiftFlow v2.0-Fusion\n");
            printf("GoPU.inc © 2026\n");
            printf("CLAIR & SYM Paradigm Fusion\n");
            printf("Advanced Package System\n");
            printf("Built: %s %s\n", __DATE__, __TIME__);
            printf("\n");
            continue;
        }
        
        if (strlen(line) == 0) continue;
        
        // Execute the line
        run(line, "REPL");
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
// [SECTION] MAIN FUNCTION
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
    printf("         Advanced Package System                \n");
    printf("         Full Language Support                  \n");
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
        for (int i = 0; i < 80; i++) printf("═");
        printf("\n\n");
        
        run(source, argv[1]);
        free(source);
    }
    
    // Nettoyage final
    cleanupVariables();
    cleanupDBVar();
    cleanupPackages();
    cleanupFunctions();
    
    return 0;
}
