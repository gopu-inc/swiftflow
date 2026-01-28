#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdarg.h>

// Headers Modules
#include "common.h"
#include "io.h"
#include "net.h"
#include "sys.h"
#include "http.h"
#include "json.h"
#include "stdlib.h" // Important pour Crypto/Path/Math

// ======================================================
// [SECTION] GLOBAL STATE & STRUCTS
// ======================================================

static char current_working_dir[PATH_MAX];
static const char* current_exec_filename = "main";

extern ASTNode** parse(const char* source, int* count);

// Gestion des Variables
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
    bool is_locked; // Sécurité
    int scope_level;
} Variable;

static Variable vars[2000];
static int var_count = 0;
static int scope_level = 0;

// Gestion des Fonctions
typedef struct {
    char name[100];
    ASTNode* params;
    ASTNode* body;
    int param_count;
    char** param_names;
    double return_value;
    char* return_string;
    bool has_returned;
} Function;

static Function functions[500];
static int func_count = 0;
static Function* current_function = NULL;

// Gestion OOP (Instances)
typedef struct {
    char id[64];      // "inst_1"
    char class_name[64]; // "Zarch"
} InstanceRegistry;

static InstanceRegistry instances[500];
static int instance_count = 0;
static char* current_this = NULL;

// Gestion Imports (Cache)
typedef enum { MODULE_STATUS_LOADING, MODULE_STATUS_LOADED } ModuleStatus;
typedef struct {
    char* path;
    ModuleStatus status;
} ModuleCache;

static ModuleCache module_registry[200];
static int registry_count = 0;

// Exports
typedef struct {
    char* symbol;
    char* alias;
    char* module;
} ExportEntry;

static ExportEntry exports[200];
static int export_count = 0;

// ======================================================
// [SECTION] PROTOTYPES
// ======================================================
static void execute(ASTNode* node);
static double evalFloat(ASTNode* node);
static char* evalString(ASTNode* node);
static bool evalBool(ASTNode* node);
static char* weldInput(const char* prompt);

// ======================================================
// [SECTION] HELPERS
// ======================================================

void runtime_error(ASTNode* node, const char* fmt, ...) {
    va_list args;
    fprintf(stderr, "%s[RUNTIME ERROR] %s:%d:%d: ", COLOR_RED, current_exec_filename, node ? node->line : 0, node ? node->column : 0);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "%s\n", COLOR_RESET);
    exit(1);
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
    if (func_count < 500) {
        Function* func = &functions[func_count++];
        strncpy(func->name, name, 99);
        func->params = params;
        func->body = body;
        func->param_count = param_count;
        func->return_string = NULL;
        
        if (param_count > 0) {
            func->param_names = malloc(param_count * sizeof(char*));
            ASTNode* param = params;
            int i = 0;
            while (param && i < param_count) {
                if (param->type == NODE_IDENT && param->data.name) {
                    func->param_names[i] = strdup(param->data.name);
                }
                param = param->right;
                i++;
            }
        } else {
            func->param_names = NULL;
        }
    }
}

static Function* findFunction(const char* name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) return &functions[i];
    }
    return NULL;
}

static void registerClass(const char* name, char* parent, ASTNode* members) {
    // Placeholder pour enregistrement de classe
    // La logique des méthodes est gérée dans execute(NODE_CLASS)
}

static void registerInstance(const char* id, const char* class_name) {
    if (instance_count < 500) {
        strcpy(instances[instance_count].id, id);
        strcpy(instances[instance_count].class_name, class_name);
        instance_count++;
    }
}

static char* findClassOf(const char* id) {
    for(int i=0; i<instance_count; i++) {
        if(strcmp(instances[i].id, id) == 0) return instances[i].class_name;
    }
    return NULL;
}

static char* generateLambdaName() {
    static int lambda_id = 0;
    char* name = malloc(32);
    sprintf(name, "__lambda_%d", lambda_id++);
    return name;
}

static char* resolveModulePath(const char* import_path, const char* from_module) {
    char base_path[PATH_MAX];
    char candidate[PATH_MAX];
    char resolved[PATH_MAX];

    if (from_module && from_module[0]) {
        char temp[PATH_MAX]; strcpy(temp, from_module);
        strcpy(base_path, dirname(temp));
    } else {
        strcpy(base_path, current_working_dir);
    }

    const char* search_paths[] = { base_path, "./zarch_modules", "/usr/local/lib/swift", NULL };

    for (int i = 0; search_paths[i]; i++) {
        snprintf(candidate, PATH_MAX, "%s/%s", search_paths[i], import_path);
        
        // Test direct ou dossier/index.swf
        if (access(candidate, F_OK) == 0) {
            struct stat s; stat(candidate, &s);
            if (S_ISDIR(s.st_mode)) {
                snprintf(candidate, PATH_MAX, "%s/%s/index.swf", search_paths[i], import_path); // Package
                if (access(candidate, F_OK) == 0 && realpath(candidate, resolved)) return strdup(resolved);
                snprintf(candidate, PATH_MAX, "%s/%s/main.swf", search_paths[i], import_path); // Package v2
                if (access(candidate, F_OK) == 0 && realpath(candidate, resolved)) return strdup(resolved);
            } else if (realpath(candidate, resolved)) return strdup(resolved);
        }
        
        // Test avec .swf
        snprintf(candidate, PATH_MAX, "%s/%s.swf", search_paths[i], import_path);
        if (access(candidate, F_OK) == 0 && realpath(candidate, resolved)) return strdup(resolved);
    }
    return NULL;
}

// ======================================================
// [SECTION] EVALUATION (FLOAT/STRING/BOOL)
// ======================================================

static bool evalBool(ASTNode* node) {
    if (!node) return false;
    if (node->type == NODE_BOOL) return node->data.bool_val;
    if (node->type == NODE_INT) return node->data.int_val != 0;
    if (node->type == NODE_FLOAT) return fabs(node->data.float_val) > 0.000001;
    // ...
    return false;
}

static double evalFloat(ASTNode* node) {
    if (!node) return 0.0;
    
    switch (node->type) {
        case NODE_INT: return (double)node->data.int_val;
        case NODE_FLOAT: return node->data.float_val;
        case NODE_BOOL: return node->data.bool_val ? 1.0 : 0.0;
        
        case NODE_MATH_FUNC: {
            if (node->op_type == TK_MATH_PI || node->op_type == TK_MATH_E) 
                return std_math_const(node->op_type);
            double v1 = node->left ? evalFloat(node->left) : 0;
            double v2 = node->right ? evalFloat(node->right) : 0;
            return std_math_calc(node->op_type, v1, v2);
        }

        case NODE_PATH_EXISTS: {
            char* path = evalString(node->left);
            bool res = io_exists_bool(path);
            if (path) free(path);
            return res ? 1.0 : 0.0;
        }

        case NODE_STR_FUNC: {
            // Pour contains, starts, etc qui retournent un booléen (1.0/0.0)
            if (node->op_type == TK_STR_CONTAINS) {
                char* h = evalString(node->left);
                char* n = evalString(node->right);
                int r = std_str_contains(h, n);
                free(h); free(n); return (double)r;
            }
            return 0.0;
        }

        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_float) return vars[idx].value.float_val;
                return (double)vars[idx].value.int_val;
            }
            return 0.0;
        }

        case NODE_BINARY: {
            double left = evalFloat(node->left);
            double right = evalFloat(node->right);
            switch (node->op_type) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0 ? left / right : 0;
                case TK_MOD: return fmod(left, right);
                case TK_GT: return left > right;
                case TK_LT: return left < right;
                case TK_GTE: return left >= right;
                case TK_LTE: return left <= right;
                case TK_EQ: return left == right;
                case TK_NEQ: return left != right;
                default: return 0.0;
            }
        }
        
        // Redirection appel fonction float (via execute ou logique commune)
        // Ici simplifié: si c'est un appel, on délègue à evalString et on convertit
        // ou on duplique la logique FUNC_CALL. Pour faire simple et robuste :
        case NODE_FUNC_CALL: {
            char* s = evalString(node); // Utilise la logique complète de evalString
            double d = atof(s);
            free(s);
            return d;
        }

        default: return 0.0;
    }
}

static char* evalString(ASTNode* node) {
    if (!node) return strdup("");
    
    switch (node->type) {
    
    // --- MODULE CRYPTO ---
    case NODE_CRYPTO_FUNC: {
        char* data = evalString(node->left);
        char* res = NULL;
        if (node->op_type == TK_CRYPTO_SHA256) res = std_crypto_sha256(data);
        else if (node->op_type == TK_CRYPTO_MD5) res = std_crypto_md5(data);
        else if (node->op_type == TK_CRYPTO_B64ENC) res = std_crypto_b64enc(data);
        else if (node->op_type == TK_CRYPTO_B64DEC) res = std_crypto_b64dec(data);
        if (data) free(data);
        return res ? res : strdup("");
    }

    // --- MODULE PATH ---
    case NODE_PATH_FUNC: {
        char* p1 = evalString(node->left);
        char* res = NULL;
        if (node->op_type == TK_PATH_BASENAME) res = std_path_basename(p1);
        else if (node->op_type == TK_PATH_DIRNAME) res = std_path_dirname(p1);
        else if (node->op_type == TK_PATH_ABS) res = std_path_abs(p1);
        else if (node->op_type == TK_PATH_JOIN) {
            char* p2 = evalString(node->right);
            res = std_path_join(p1, p2);
            free(p2);
        }
        free(p1);
        return res ? res : strdup("");
    }

    // --- MODULE ENV ---
    case NODE_ENV_FUNC: {
        if (node->op_type == TK_ENV_GET) {
            char* k = evalString(node->left);
            char* v = std_env_get(k);
            free(k); return v ? v : strdup("");
        }
        if (node->op_type == TK_ENV_OS) return std_env_os();
        return strdup("");
    }

    // --- MODULE STR ---
    case NODE_STR_FUNC: {
        char* s = evalString(node->left);
        char* res = NULL;
        if (node->op_type == TK_STR_UPPER) res = std_str_upper(s);
        else if (node->op_type == TK_STR_LOWER) res = std_str_lower(s);
        else if (node->op_type == TK_STR_TRIM) res = std_str_trim(s);
        else if (node->op_type == TK_STR_REPLACE) {
            char* f = evalString(node->right); char* r = evalString(node->third);
            res = std_str_replace(s, f, r); free(f); free(r);
        }
        if (s) free(s);
        return res ? res : strdup("");
    }

    // --- MODULE MATH (Stringify) ---
    case NODE_MATH_FUNC: {
        double val = evalFloat(node);
        char buf[64]; sprintf(buf, "%g", val); return strdup(buf);
    }

    // --- IO ---
    case NODE_FILE_READ: {
        char* p = evalString(node->left);
        char* c = io_read_string(p); free(p); return c ? c : strdup("");
    }
    case NODE_WELD: {
        char* p = node->left ? evalString(node->left) : NULL;
        char* i = weldInput(p); if (p) free(p); return i ? i : strdup("");
    }

    // --- HTTP / SYS / JSON / NET / STD ---
    case NODE_HTTP_GET: { char* u = evalString(node->left); char* r = http_get(u); free(u); return r ? r : strdup(""); }
    case NODE_HTTP_POST: { char* u = evalString(node->left); char* d = evalString(node->right); char* r = http_post(u, d); free(u); free(d); return r ? r : strdup(""); }
    case NODE_HTTP_DOWNLOAD: { char* u = evalString(node->left); char* o = evalString(node->right); char* r = http_download(u, o); free(u); free(o); return r ? r : strdup("failed"); }
    case NODE_SYS_ARGV: { int idx = (int)evalFloat(node->left); char* a = sys_get_argv(idx); return a ? strdup(a) : NULL; }
    case NODE_JSON_GET: { char* j = evalString(node->left); char* k = evalString(node->right); char* r = json_extract(j, k); free(j); free(k); return r ? r : NULL; }
    
    // --- OOP ---
    case NODE_NEW: {
        static int iid = 0; char n[64]; sprintf(n, "inst_%d", ++iid);
        registerInstance(n, node->data.name); return strdup(n);
    }
    case NODE_MEMBER_ACCESS: {
        char* obj = (node->left->type == NODE_THIS) ? (current_this ? strdup(current_this) : strdup("")) : evalString(node->left);
        if (!obj || !*obj) { if(obj) free(obj); return strdup(""); }
        char full[256]; snprintf(full, 256, "%s_%s", obj, node->right->data.name);
        free(obj);
        int idx = findVar(full);
        if (idx >= 0) {
            if (vars[idx].is_string) return strdup(vars[idx].value.str_val);
            char buf[64]; sprintf(buf, "%g", vars[idx].value.float_val); return strdup(buf);
        }
        return strdup("");
    }

    // --- TYPES DE BASE ---
    case NODE_STRING: return strdup(node->data.str_val);
    case NODE_INT: { char b[32]; sprintf(b, "%lld", node->data.int_val); return strdup(b); }
    case NODE_FLOAT: { char b[32]; sprintf(b, "%g", node->data.float_val); return strdup(b); }
    case NODE_BOOL: return strdup(node->data.bool_val ? "true" : "false");
    case NODE_NULL: return strdup("null");
    case NODE_IDENT: {
        int idx = findVar(node->data.name);
        if (idx >= 0) {
            if (vars[idx].is_string) return strdup(vars[idx].value.str_val);
            char b[32]; sprintf(b, "%g", vars[idx].value.float_val); return strdup(b);
        }
        return strdup("undefined");
    }
    case NODE_BINARY: {
        if (node->op_type == TK_CONCAT) {
            char* l = evalString(node->left); char* r = evalString(node->right);
            char* res = malloc(strlen(l) + strlen(r) + 1); strcpy(res, l); strcat(res, r);
            free(l); free(r); return res;
        }
        double v = evalFloat(node); char b[32]; sprintf(b, "%g", v); return strdup(b);
    }

    // --- APPEL FONCTION / METHODE ---
    case NODE_FUNC_CALL: {
        char* fname = node->data.name;
        char* prev_this = current_this;
        bool is_method = false;
        
        // OOP Redirection
        char* dot = strchr(fname, '.');
        char real_name[256];
        if (dot) {
            int len = dot - fname; char vname[128]; strncpy(vname, fname, len); vname[len] = 0;
            int idx = findVar(vname);
            if (idx >= 0 && vars[idx].is_string) {
                char* cls = findClassOf(vars[idx].value.str_val);
                if (cls) {
                    snprintf(real_name, 256, "%s_%s", cls, dot + 1);
                    fname = real_name;
                    current_this = vars[idx].value.str_val;
                    is_method = true;
                }
            }
        }

        Function* f = findFunction(fname);
        if (f) {
            Function* pf = current_function;
            current_function = f;
            int os = scope_level++;
            
            // Args
            ASTNode* arg = node->left;
            for (int i=0; i<f->param_count && arg; i++) {
                if (var_count < 2000) {
                    Variable* v = &vars[var_count++];
                    strncpy(v->name, f->param_names[i], 99);
                    v->scope_level = scope_level;
                    v->is_initialized = true;
                    if (arg->type == NODE_STRING) { v->is_string=true; v->value.str_val=evalString(arg); }
                    else { v->is_float=true; v->value.float_val=evalFloat(arg); }
                }
                arg = arg->right;
            }
            
            f->has_returned = false;
            if (f->body) execute(f->body);
            
            scope_level = os;
            current_function = pf;
            if (is_method) current_this = prev_this;
            
            if (f->return_string) return strdup(f->return_string);
            char b[64]; sprintf(b, "%g", f->return_value); return strdup(b);
        }
        if (is_method) current_this = prev_this;
        return strdup("");
    }

    default: return strdup("");
    }
}

static char* weldInput(const char* prompt) {
    if (prompt) printf("%s", prompt);
    char buf[1024];
    if (fgets(buf, 1024, stdin)) {
        buf[strcspn(buf, "\n")] = 0;
        return strdup(buf);
    }
    return strdup("");
}

// ======================================================
// [SECTION] EXECUTE (STATEMENTS)
// ======================================================

static void execute(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_VAR_DECL: 
        case NODE_CONST_DECL: {
            if (var_count < 2000) {
                Variable* v = &vars[var_count++];
                strncpy(v->name, node->data.name, 99);
                v->type = (node->type == NODE_CONST_DECL) ? TK_CONST : TK_VAR;
                v->scope_level = scope_level;
                v->is_constant = (v->type == TK_CONST);
                v->is_locked = false;
                
                if (node->left) {
                    v->is_initialized = true;
                    if (node->left->type == NODE_STRING) {
                        v->is_string = true; v->value.str_val = evalString(node->left);
                    } else {
                        v->is_float = true; v->value.float_val = evalFloat(node->left);
                    }
                }
            }
            break;
        }

        case NODE_ASSIGN: {
            char* target = NULL;
            bool is_prop = false;
            if (node->data.name) target = strdup(node->data.name);
            else if (node->left && node->left->type == NODE_MEMBER_ACCESS) {
                char* o = evalString(node->left->left);
                if (o && *o) {
                    target = malloc(256);
                    snprintf(target, 256, "%s_%s", o, node->left->right->data.name);
                    is_prop = true;
                }
                free(o);
            }

            if (target) {
                int idx = findVar(target);
                if (idx == -1 && var_count < 2000) { // Auto-create property
                    idx = var_count++;
                    strncpy(vars[idx].name, target, 99);
                    vars[idx].type = TK_VAR;
                    vars[idx].scope_level = is_prop ? 0 : scope_level;
                }

                if (idx >= 0) {
                    if (vars[idx].is_locked) runtime_error(node, "Variable '%s' is locked", target);
                    if (vars[idx].is_constant) runtime_error(node, "Cannot assign constant '%s'", target);
                    
                    if (node->right) {
                        vars[idx].is_initialized = true;
                        if (vars[idx].is_string && vars[idx].value.str_val) free(vars[idx].value.str_val);
                        
                        // Type detection naive
                        char* sval = evalString(node->right);
                        char* end;
                        double dval = strtod(sval, &end);
                        if (*end == 0 && *sval != 0 && !strstr(sval, "inst_")) {
                            vars[idx].is_float = true; vars[idx].is_string = false;
                            vars[idx].value.float_val = dval;
                            free(sval);
                        } else {
                            vars[idx].is_string = true; vars[idx].is_float = false;
                            vars[idx].value.str_val = sval;
                        }
                    }
                }
                free(target);
            }
            break;
        }

        case NODE_PRINT: {
            ASTNode* arg = node->left;
            while (arg) {
                char* s = evalString(arg);
                printf("%s", s); free(s);
                arg = arg->right;
                if (arg) printf(" ");
            }
            printf("\n");
            break;
        }

        case NODE_IF:
            if (evalBool(node->left)) execute(node->right);
            else if (node->third) execute(node->third);
            break;

        case NODE_WHILE:
            while (evalBool(node->left)) {
                execute(node->right);
                if (current_function && current_function->has_returned) break;
            }
            break;

        case NODE_FOR:
            if (node->data.loop.init) execute(node->data.loop.init);
            while (evalBool(node->data.loop.condition)) {
                execute(node->data.loop.body);
                if (current_function && current_function->has_returned) break;
                if (node->data.loop.update) execute(node->data.loop.update);
            }
            break;

        case NODE_RETURN:
            if (current_function) {
                current_function->has_returned = true;
                if (node->left) {
                    char* s = evalString(node->left);
                    if (current_function->return_string) free(current_function->return_string);
                    current_function->return_string = s;
                    // Try parse double
                    char* end;
                    current_function->return_value = strtod(s, &end);
                }
            }
            break;

        case NODE_BLOCK: {
            int os = scope_level++;
            ASTNode* cur = node->left;
            while (cur && !(current_function && current_function->has_returned)) {
                execute(cur);
                cur = cur->right;
            }
            scope_level = os;
            break;
        }

        case NODE_CLASS: {
            char* cname = node->data.class_def.name;
            if (cname) {
                registerClass(cname, NULL, NULL);
                ASTNode* m = node->data.class_def.members;
                while (m) {
                    if (m->type == NODE_FUNC) {
                        char full[256]; snprintf(full, 256, "%s_%s", cname, m->data.name);
                        int pc = 0; ASTNode* p = m->left; while(p) { pc++; p=p->right; }
                        registerFunction(full, m->left, m->right, pc);
                    }
                    m = m->right;
                }
            }
            break;
        }

        case NODE_ENUM: {
            if (node->data.name && node->left) {
                ASTNode* v = node->left;
                int val = 0;
                while (v) {
                    char full[256]; snprintf(full, 256, "%s_%s", node->data.name, v->data.name);
                    if (var_count < 2000) {
                        Variable* var = &vars[var_count++];
                        strncpy(var->name, full, 99);
                        var->type = TK_CONST; var->is_constant = true; var->is_initialized = true;
                        var->is_float = true; var->value.float_val = (double)val++;
                    }
                    v = v->right;
                }
            }
            break;
        }

        // --- COMMANDES VOID ---
        case NODE_ENV_FUNC: if (node->op_type == TK_ENV_SET) { char* k=evalString(node->left); char* v=evalString(node->right); std_env_set(k, v); free(k); free(v); } break;
        case NODE_TIME_SLEEP: std_time_sleep(evalFloat(node->left)); break;
        case NODE_SYS_EXEC: { char* c=evalString(node->left); sys_exec_int(c); free(c); } break;
        case NODE_SYS_EXIT: exit((int)evalFloat(node->left)); break;
        case NODE_NET_CONNECT: { int fd=(int)evalFloat(node->left); char* ip=evalString(node->right); int p=(int)evalFloat(node->third); net_connect_to(fd, ip, p); free(ip); } break;
        case NODE_NET_SEND: { int fd=(int)evalFloat(node->left); char* d=evalString(node->right); net_send_data(fd, d); free(d); } break;
        case NODE_NET_CLOSE: net_close_socket((int)evalFloat(node->left)); break;
        case NODE_IO_WRITE: { char* f=evalString(node->left); char* d=evalString(node->right); FILE* fp=fopen(f,"w"); if(fp){fputs(d,fp); fclose(fp);} free(f); free(d); } break;
        // Ajouter MKDIR, etc. ici si utilisé en instruction

        // --- APPEL FONCTION VOID ---
        case NODE_FUNC_CALL: evalString(node); break; 
        case NODE_METHOD_CALL: {
            // Conversion en NODE_FUNC_CALL pour utiliser la logique unique
            // (Note: Parser a déjà fait le travail de structure, mais ici on réutilise le bloc logique)
            // Pour simplifier, on appelle evalString qui contient TOUTE la logique OOP
            evalString(node); 
            break; 
        }

        default: break;
    }
}

// ======================================================
// [SECTION] MAIN LOADERS
// ======================================================

static bool loadAndExecuteModule(const char* import_path, const char* from_module, bool import_named, char** named_symbols, int symbol_count) {
    if (strcmp(import_path, "sys") == 0 || strcmp(import_path, "http") == 0 || 
        strcmp(import_path, "io") == 0 || strcmp(import_path, "json") == 0 || 
        strcmp(import_path, "net") == 0 || strcmp(import_path, "std") == 0 ||
        strcmp(import_path, "math") == 0 || strcmp(import_path, "str") == 0 ||
        strcmp(import_path, "time") == 0 || strcmp(import_path, "env") == 0 ||
        strcmp(import_path, "path") == 0 || strcmp(import_path, "crypto") == 0) {
        return true; 
    }

    char* full_path = resolveModulePath(import_path, from_module);
    if (!full_path) {
        // fprintf(stderr, "Module not found: %s\n", import_path);
        return false;
    }

    // Cache Check
    for(int i=0; i<registry_count; i++) {
        if(strcmp(module_registry[i].path, full_path) == 0) { free(full_path); return true; }
    }
    
    // Add to cache
    module_registry[registry_count].path = strdup(full_path);
    module_registry[registry_count].status = MODULE_STATUS_LOADING;
    registry_count++;

    // Load & Parse
    FILE* f = fopen(full_path, "r");
    if (!f) { free(full_path); return false; }
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = malloc(s + 1); fread(src, 1, s, f); src[s] = 0; fclose(f);

    int count = 0;
    ASTNode** nodes = parse(src, &count);
    
    // Context Switch
    char old_wd[PATH_MAX]; strcpy(old_wd, current_working_dir);
    const char* old_fn = current_exec_filename;
    char mod_dir[PATH_MAX]; strcpy(mod_dir, full_path);
    strcpy(current_working_dir, dirname(mod_dir));
    current_exec_filename = full_path;

    // Execute
    if (nodes) {
        for(int i=0; i<count; i++) {
            if (nodes[i] && nodes[i]->type != NODE_MAIN) execute(nodes[i]);
        }
        // Cleanup nodes (simplified)
        free(nodes);
    }

    strcpy(current_working_dir, old_wd);
    current_exec_filename = old_fn;
    free(src); 
    // free(full_path); // Keep in cache
    return true;
}

static void run(const char* source, const char* filename) {
    current_exec_filename = filename;
    char abs[PATH_MAX];
    if (realpath(filename, abs)) {
        char d[PATH_MAX]; strcpy(d, abs);
        strcpy(current_working_dir, dirname(d));
    } else {
        getcwd(current_working_dir, PATH_MAX);
    }

    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) return;

    // 1. Enregistrement Fonctions/Classes
    for(int i=0; i<count; i++) {
        if (nodes[i]->type == NODE_FUNC || nodes[i]->type == NODE_CLASS || nodes[i]->type == NODE_ENUM) {
            execute(nodes[i]);
        }
    }

    ASTNode* main_node = NULL;

    // 2. Exécution Globale
    for(int i=0; i<count; i++) {
        if (nodes[i]->type == NODE_MAIN) {
            main_node = nodes[i];
        } else if (nodes[i]->type != NODE_FUNC && nodes[i]->type != NODE_CLASS && nodes[i]->type != NODE_ENUM) {
            execute(nodes[i]);
        }
    }

    // 3. Main
    if (main_node && main_node->left) execute(main_node->left);

    free(nodes);
}

static void repl() {
    printf("SwiftFlow REPL v1.5\n");
    char line[4096];
    while (1) {
        printf(">> ");
        if (!fgets(line, 4096, stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "exit") == 0) break;
        run(line, "REPL");
    }
}

static char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = malloc(s + 1);
    if(src) { fread(src, 1, s, f); src[s] = 0; }
    fclose(f);
    return src;
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    init_io_module();
    init_net_module();
    init_sys_module(argc, argv);
    init_http_module();

    if (argc < 2) { repl(); return 0; }
    
    char* src = loadFile(argv[1]);
    if (!src) { fprintf(stderr, "Cannot open %s\n", argv[1]); return 1; }
    
    run(src, argv[1]);
    free(src);
    return 0;
}
