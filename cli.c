// cli.c - Interpréteur/VM SwiftVelox
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

// ============ CONFIGURATION ============
#define STACK_SIZE 1024
#define HEAP_SIZE 8192
#define MAX_SYMBOLS 256
#define MAX_FUNCTIONS 64
#define MAX_CALL_STACK 128
#define MAX_STRING_LEN 256

// ============ TYPES DE VALEURS ============
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,
    VAL_NULL,
    VAL_FUNCTION,
    VAL_ARRAY,
    VAL_OBJECT
} ValueType;

typedef struct Value Value;
typedef struct Function Function;

struct Value {
    ValueType type;
    union {
        int intVal;
        float floatVal;
        bool boolVal;
        char* stringVal;
        Function* function;
        struct {
            Value** items;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            Value** values;
            int count;
            int capacity;
        } object;
    } as;
};

// ============ FONCTIONS ============
struct Function {
    char* name;
    char** params;
    int paramCount;
    ASTNode* body;  // À définir plus tard
    bool isNative;
    Value (*nativeFunc)(Value*, int);
};

// ============ VM ETAT ============
typedef struct {
    Value stack[STACK_SIZE];
    int stackTop;
    
    Value heap[HEAP_SIZE];
    int heapPtr;
    
    Value* globals[MAX_SYMBOLS];
    char* globalNames[MAX_SYMBOLS];
    int globalCount;
    
    Function* functions[MAX_FUNCTIONS];
    int functionCount;
    
    // Call stack pour les fonctions
    struct {
        int returnAddr;
        Value* locals;
        int localCount;
    } callStack[MAX_CALL_STACK];
    int callStackTop;
    
    // Pour la gestion de la mémoire
    Value** allocated;
    int allocCount;
    int allocCapacity;
    
    bool hadError;
    bool debugMode;
} VM;

// ============ TOKENS ET AST ============
// (Les définitions de tokens restent les mêmes que précédemment)
typedef enum {
    // ... (tous les tokens comme avant)
    // On garde la même énumération
} TokenType;

typedef struct ASTNode ASTNode;
struct ASTNode {
    TokenType type;
    char* name;
    Value value;
    int isConstant;
    ASTNode* left;
    ASTNode* right;
    ASTNode* condition;
    ASTNode* body;
    ASTNode* next;  // Pour les listes de statements
};

// ============ FONCTIONS UTILITAIRES ============
Value createInt(int val) {
    Value v;
    v.type = VAL_INT;
    v.as.intVal = val;
    return v;
}

Value createFloat(float val) {
    Value v;
    v.type = VAL_FLOAT;
    v.as.floatVal = val;
    return v;
}

Value createBool(bool val) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolVal = val;
    return v;
}

Value createString(const char* str) {
    Value v;
    v.type = VAL_STRING;
    v.as.stringVal = malloc(strlen(str) + 1);
    strcpy(v.as.stringVal, str);
    return v;
}

Value createNull() {
    Value v;
    v.type = VAL_NULL;
    return v;
}

void freeValue(Value* v) {
    if (v->type == VAL_STRING && v->as.stringVal) {
        free(v->as.stringVal);
    } else if (v->type == VAL_ARRAY) {
        for (int i = 0; i < v->as.array.count; i++) {
            freeValue(v->as.array.items[i]);
        }
        free(v->as.array.items);
    } else if (v->type == VAL_OBJECT) {
        for (int i = 0; i < v->as.object.count; i++) {
            free(v->as.object.keys[i]);
            freeValue(v->as.object.values[i]);
        }
        free(v->as.object.keys);
        free(v->as.object.values);
    }
}

// ============ VM OPERATIONS ============
VM* createVM() {
    VM* vm = malloc(sizeof(VM));
    vm->stackTop = 0;
    vm->heapPtr = 0;
    vm->globalCount = 0;
    vm->functionCount = 0;
    vm->callStackTop = 0;
    vm->hadError = false;
    vm->debugMode = false;
    
    vm->allocated = malloc(sizeof(Value*) * 100);
    vm->allocCount = 0;
    vm->allocCapacity = 100;
    
    // Initialiser les built-ins
    initBuiltins(vm);
    
    return vm;
}

void freeVM(VM* vm) {
    for (int i = 0; i < vm->allocCount; i++) {
        freeValue(vm->allocated[i]);
    }
    free(vm->allocated);
    
    for (int i = 0; i < vm->functionCount; i++) {
        free(vm->functions[i]->name);
        if (vm->functions[i]->params) {
            for (int j = 0; j < vm->functions[i]->paramCount; j++) {
                free(vm->functions[i]->params[j]);
            }
            free(vm->functions[i]->params);
        }
        free(vm->functions[i]);
    }
    
    free(vm);
}

void push(VM* vm, Value value) {
    if (vm->stackTop >= STACK_SIZE) {
        printf("Stack overflow!\n");
        vm->hadError = true;
        return;
    }
    vm->stack[vm->stackTop++] = value;
}

Value pop(VM* vm) {
    if (vm->stackTop <= 0) {
        printf("Stack underflow!\n");
        vm->hadError = true;
        return createNull();
    }
    return vm->stack[--vm->stackTop];
}

Value peek(VM* vm, int distance) {
    if (vm->stackTop - 1 - distance < 0) {
        return createNull();
    }
    return vm->stack[vm->stackTop - 1 - distance];
}

// ============ GESTION DES SYMBOLES ============
int findGlobal(VM* vm, const char* name) {
    for (int i = 0; i < vm->globalCount; i++) {
        if (strcmp(vm->globalNames[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

void defineGlobal(VM* vm, const char* name, Value value) {
    int idx = findGlobal(vm, name);
    if (idx >= 0) {
        // Redéfinition
        freeValue(vm->globals[idx]);
        vm->globals[idx] = malloc(sizeof(Value));
        *vm->globals[idx] = value;
    } else {
        // Nouvelle définition
        vm->globalNames[vm->globalCount] = malloc(strlen(name) + 1);
        strcpy(vm->globalNames[vm->globalCount], name);
        vm->globals[vm->globalCount] = malloc(sizeof(Value));
        *vm->globals[vm->globalCount] = value;
        vm->globalCount++;
    }
}

Value getGlobal(VM* vm, const char* name) {
    int idx = findGlobal(vm, name);
    if (idx >= 0) {
        return *vm->globals[idx];
    }
    printf("Undefined variable: %s\n", name);
    vm->hadError = true;
    return createNull();
}

// ============ BUILT-IN FUNCTIONS ============
Value printNative(Value* args, int count) {
    for (int i = 0; i < count; i++) {
        switch (args[i].type) {
            case VAL_INT:
                printf("%d", args[i].as.intVal);
                break;
            case VAL_FLOAT:
                printf("%.2f", args[i].as.floatVal);
                break;
            case VAL_BOOL:
                printf("%s", args[i].as.boolVal ? "true" : "false");
                break;
            case VAL_STRING:
                printf("%s", args[i].as.stringVal);
                break;
            case VAL_NULL:
                printf("null");
                break;
            default:
                printf("[object]");
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return createNull();
}

Value inputNative(Value* args, int count) {
    if (count > 0) {
        printNative(args, count);
    }
    
    char buffer[MAX_STRING_LEN];
    if (fgets(buffer, MAX_STRING_LEN, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0; // Retirer newline
        return createString(buffer);
    }
    return createString("");
}

Value lenNative(Value* args, int count) {
    if (count != 1) {
        printf("len() expects 1 argument\n");
        return createNull();
    }
    
    if (args[0].type == VAL_STRING) {
        return createInt(strlen(args[0].as.stringVal));
    } else if (args[0].type == VAL_ARRAY) {
        return createInt(args[0].as.array.count);
    }
    
    printf("len() expects string or array\n");
    return createNull();
}

Value timeNative(Value* args, int count) {
    return createInt((int)time(NULL));
}

Value randomNative(Value* args, int count) {
    if (count == 0) {
        return createFloat((float)rand() / RAND_MAX);
    } else if (count == 1 && args[0].type == VAL_INT) {
        return createInt(rand() % args[0].as.intVal);
    } else if (count == 2 && args[0].type == VAL_INT && args[1].type == VAL_INT) {
        int min = args[0].as.intVal;
        int max = args[1].as.intVal;
        return createInt(min + rand() % (max - min + 1));
    }
    
    printf("random() invalid arguments\n");
    return createNull();
}

void initBuiltins(VM* vm) {
    // Seed random
    srand(time(NULL));
    
    // Définir Print comme fonction native
    Function* printFunc = malloc(sizeof(Function));
    printFunc->name = malloc(strlen("Print") + 1);
    strcpy(printFunc->name, "Print");
    printFunc->paramCount = -1; // Varargs
    printFunc->params = NULL;
    printFunc->body = NULL;
    printFunc->isNative = true;
    printFunc->nativeFunc = printNative;
    
    vm->functions[vm->functionCount++] = printFunc;
    
    // Définir d'autres built-ins
    defineGlobal(vm, "true", createBool(true));
    defineGlobal(vm, "false", createBool(false));
    defineGlobal(vm, "null", createNull());
    
    // Math functions
    Function* mathFuncs[] = {
        createNativeFunc("input", inputNative),
        createNativeFunc("len", lenNative),
        createNativeFunc("time", timeNative),
        createNativeFunc("random", randomNative),
    };
    
    for (int i = 0; i < sizeof(mathFuncs)/sizeof(mathFuncs[0]); i++) {
        vm->functions[vm->functionCount++] = mathFuncs[i];
    }
}

Function* createNativeFunc(const char* name, Value (*func)(Value*, int)) {
    Function* f = malloc(sizeof(Function));
    f->name = malloc(strlen(name) + 1);
    strcpy(f->name, name);
    f->paramCount = -1; // Varargs par défaut
    f->params = NULL;
    f->body = NULL;
    f->isNative = true;
    f->nativeFunc = func;
    return f;
}

// ============ INTERPRÉTEUR ============
Value interpretExpression(VM* vm, ASTNode* node) {
    if (!node || vm->hadError) return createNull();
    
    switch (node->type) {
        case TOKEN_INT_LIT:
            return createInt(atoi(node->value.as.stringVal));
            
        case TOKEN_FLOAT_LIT:
            return createFloat(atof(node->value.as.stringVal));
            
        case TOKEN_STR_LIT:
            return createString(node->value.as.stringVal);
            
        case TOKEN_TRUE_LIT:
            return createBool(true);
            
        case TOKEN_FALSE_LIT:
            return createBool(false);
            
        case TOKEN_IDENTIFIER:
            return getGlobal(vm, node->name);
            
        case TOKEN_PLUS: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return createInt(left.as.intVal + right.as.intVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
                return createFloat(left.as.floatVal + right.as.floatVal);
            } else if (left.type == VAL_INT && right.type == VAL_FLOAT) {
                return createFloat(left.as.intVal + right.as.floatVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_INT) {
                return createFloat(left.as.floatVal + right.as.intVal);
            } else if (left.type == VAL_STRING || right.type == VAL_STRING) {
                // Convertir en string et concaténer
                char buffer1[256], buffer2[256];
                
                if (left.type == VAL_STRING) strcpy(buffer1, left.as.stringVal);
                else if (left.type == VAL_INT) sprintf(buffer1, "%d", left.as.intVal);
                else if (left.type == VAL_FLOAT) sprintf(buffer1, "%.2f", left.as.floatVal);
                else if (left.type == VAL_BOOL) strcpy(buffer1, left.as.boolVal ? "true" : "false");
                else strcpy(buffer1, "null");
                
                if (right.type == VAL_STRING) strcpy(buffer2, right.as.stringVal);
                else if (right.type == VAL_INT) sprintf(buffer2, "%d", right.as.intVal);
                else if (right.type == VAL_FLOAT) sprintf(buffer2, "%.2f", right.as.floatVal);
                else if (right.type == VAL_BOOL) strcpy(buffer2, right.as.boolVal ? "true" : "false");
                else strcpy(buffer2, "null");
                
                char* result = malloc(strlen(buffer1) + strlen(buffer2) + 1);
                strcpy(result, buffer1);
                strcat(result, buffer2);
                Value v = createString(result);
                free(result);
                return v;
            }
            break;
        }
            
        case TOKEN_MINUS: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return createInt(left.as.intVal - right.as.intVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
                return createFloat(left.as.floatVal - right.as.floatVal);
            } else if (left.type == VAL_INT && right.type == VAL_FLOAT) {
                return createFloat(left.as.intVal - right.as.floatVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_INT) {
                return createFloat(left.as.floatVal - right.as.intVal);
            }
            break;
        }
            
        case TOKEN_MULT: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return createInt(left.as.intVal * right.as.intVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
                return createFloat(left.as.floatVal * right.as.floatVal);
            } else if (left.type == VAL_INT && right.type == VAL_FLOAT) {
                return createFloat(left.as.intVal * right.as.floatVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_INT) {
                return createFloat(left.as.floatVal * right.as.intVal);
            }
            break;
        }
            
        case TOKEN_DIV: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (right.type == VAL_INT && right.as.intVal == 0) {
                printf("Division by zero!\n");
                vm->hadError = true;
                return createNull();
            }
            if (right.type == VAL_FLOAT && right.as.floatVal == 0.0) {
                printf("Division by zero!\n");
                vm->hadError = true;
                return createNull();
            }
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return createInt(left.as.intVal / right.as.intVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
                return createFloat(left.as.floatVal / right.as.floatVal);
            } else if (left.type == VAL_INT && right.type == VAL_FLOAT) {
                return createFloat(left.as.intVal / right.as.floatVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_INT) {
                return createFloat(left.as.floatVal / right.as.intVal);
            }
            break;
        }
            
        case TOKEN_MOD: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                if (right.as.intVal == 0) {
                    printf("Modulo by zero!\n");
                    vm->hadError = true;
                    return createNull();
                }
                return createInt(left.as.intVal % right.as.intVal);
            }
            break;
        }
        
        case TOKEN_EQ: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type != right.type) return createBool(false);
            
            switch (left.type) {
                case VAL_INT:
                    return createBool(left.as.intVal == right.as.intVal);
                case VAL_FLOAT:
                    return createBool(left.as.floatVal == right.as.floatVal);
                case VAL_BOOL:
                    return createBool(left.as.boolVal == right.as.boolVal);
                case VAL_STRING:
                    return createBool(strcmp(left.as.stringVal, right.as.stringVal) == 0);
                case VAL_NULL:
                    return createBool(true);
                default:
                    return createBool(false);
            }
        }
        
        case TOKEN_NEQ: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type != right.type) return createBool(true);
            
            switch (left.type) {
                case VAL_INT:
                    return createBool(left.as.intVal != right.as.intVal);
                case VAL_FLOAT:
                    return createBool(left.as.floatVal != right.as.floatVal);
                case VAL_BOOL:
                    return createBool(left.as.boolVal != right.as.boolVal);
                case VAL_STRING:
                    return createBool(strcmp(left.as.stringVal, right.as.stringVal) != 0);
                case VAL_NULL:
                    return createBool(false);
                default:
                    return createBool(true);
            }
        }
        
        case TOKEN_GT: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return createBool(left.as.intVal > right.as.intVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
                return createBool(left.as.floatVal > right.as.floatVal);
            } else if (left.type == VAL_INT && right.type == VAL_FLOAT) {
                return createBool(left.as.intVal > right.as.floatVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_INT) {
                return createBool(left.as.floatVal > right.as.intVal);
            }
            break;
        }
        
        case TOKEN_LT: {
            Value left = interpretExpression(vm, node->left);
            Value right = interpretExpression(vm, node->right);
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                return createBool(left.as.intVal < right.as.intVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
                return createBool(left.as.floatVal < right.as.floatVal);
            } else if (left.type == VAL_INT && right.type == VAL_FLOAT) {
                return createBool(left.as.intVal < right.as.floatVal);
            } else if (left.type == VAL_FLOAT && right.type == VAL_INT) {
                return createBool(left.as.floatVal < right.as.intVal);
            }
            break;
        }
        
        case TOKEN_AND: {
            Value left = interpretExpression(vm, node->left);
            bool leftBool = false;
            if (left.type == VAL_BOOL) leftBool = left.as.boolVal;
            else if (left.type == VAL_INT) leftBool = left.as.intVal != 0;
            else if (left.type == VAL_FLOAT) leftBool = left.as.floatVal != 0.0;
            else if (left.type == VAL_STRING) leftBool = strlen(left.as.stringVal) > 0;
            else if (left.type == VAL_NULL) leftBool = false;
            
            if (!leftBool) return createBool(false);
            
            Value right = interpretExpression(vm, node->right);
            bool rightBool = false;
            if (right.type == VAL_BOOL) rightBool = right.as.boolVal;
            else if (right.type == VAL_INT) rightBool = right.as.intVal != 0;
            else if (right.type == VAL_FLOAT) rightBool = right.as.floatVal != 0.0;
            else if (right.type == VAL_STRING) rightBool = strlen(right.as.stringVal) > 0;
            else if (right.type == VAL_NULL) rightBool = false;
            
            return createBool(rightBool);
        }
        
        case TOKEN_OR: {
            Value left = interpretExpression(vm, node->left);
            bool leftBool = false;
            if (left.type == VAL_BOOL) leftBool = left.as.boolVal;
            else if (left.type == VAL_INT) leftBool = left.as.intVal != 0;
            else if (left.type == VAL_FLOAT) leftBool = left.as.floatVal != 0.0;
            else if (left.type == VAL_STRING) leftBool = strlen(left.as.stringVal) > 0;
            else if (left.type == VAL_NULL) leftBool = false;
            
            if (leftBool) return createBool(true);
            
            Value right = interpretExpression(vm, node->right);
            bool rightBool = false;
            if (right.type == VAL_BOOL) rightBool = right.as.boolVal;
            else if (right.type == VAL_INT) rightBool = right.as.intVal != 0;
            else if (right.type == VAL_FLOAT) rightBool = right.as.floatVal != 0.0;
            else if (right.type == VAL_STRING) rightBool = strlen(right.as.stringVal) > 0;
            else if (right.type == VAL_NULL) rightBool = false;
            
            return createBool(rightBool);
        }
        
        case TOKEN_NOT: {
            Value right = interpretExpression(vm, node->right);
            bool rightBool = false;
            if (right.type == VAL_BOOL) rightBool = right.as.boolVal;
            else if (right.type == VAL_INT) rightBool = right.as.intVal != 0;
            else if (right.type == VAL_FLOAT) rightBool = right.as.floatVal != 0.0;
            else if (right.type == VAL_STRING) rightBool = strlen(right.as.stringVal) > 0;
            else if (right.type == VAL_NULL) rightBool = false;
            
            return createBool(!rightBool);
        }
        
        default:
            printf("Unsupported expression type: %d\n", node->type);
            vm->hadError = true;
    }
    
    return createNull();
}

void interpretStatement(VM* vm, ASTNode* node) {
    if (!node || vm->hadError) return;
    
    switch (node->type) {
        case TOKEN_VAR: {
            Value value = createNull();
            if (node->left) {
                value = interpretExpression(vm, node->left);
            }
            defineGlobal(vm, node->name, value);
            
            if (vm->debugMode) {
                printf("[DEBUG] Définition: %s = ", node->name);
                switch (value.type) {
                    case VAL_INT: printf("%d\n", value.as.intVal); break;
                    case VAL_FLOAT: printf("%.2f\n", value.as.floatVal); break;
                    case VAL_BOOL: printf("%s\n", value.as.boolVal ? "true" : "false"); break;
                    case VAL_STRING: printf("\"%s\"\n", value.as.stringVal); break;
                    case VAL_NULL: printf("null\n"); break;
                    default: printf("[object]\n");
                }
            }
            break;
        }
        
        case TOKEN_PRINT: {
            if (node->left) {
                Value value = interpretExpression(vm, node->left);
                printNative(&value, 1);
            } else {
                printf("\n");
            }
            break;
        }
        
        case TOKEN_IF: {
            if (!node->condition) {
                printf("If without condition\n");
                vm->hadError = true;
                return;
            }
            
            Value cond = interpretExpression(vm, node->condition);
            bool condBool = false;
            
            if (cond.type == VAL_BOOL) condBool = cond.as.boolVal;
            else if (cond.type == VAL_INT) condBool = cond.as.intVal != 0;
            else if (cond.type == VAL_FLOAT) condBool = cond.as.floatVal != 0.0;
            else if (cond.type == VAL_STRING) condBool = strlen(cond.as.stringVal) > 0;
            else if (cond.type == VAL_NULL) condBool = false;
            else condBool = true;
            
            if (condBool) {
                interpretStatement(vm, node->body);
            } else if (node->right && node->right->type == TOKEN_ELSE) {
                interpretStatement(vm, node->right->body);
            }
            break;
        }
        
        case TOKEN_WHILE: {
            if (!node->condition) {
                printf("While without condition\n");
                vm->hadError = true;
                return;
            }
            
            int iterations = 0;
            while (1) {
                if (iterations++ > 1000000) { // Safety limit
                    printf("Loop iteration limit exceeded\n");
                    vm->hadError = true;
                    break;
                }
                
                Value cond = interpretExpression(vm, node->condition);
                bool condBool = false;
                
                if (cond.type == VAL_BOOL) condBool = cond.as.boolVal;
                else if (cond.type == VAL_INT) condBool = cond.as.intVal != 0;
                else if (cond.type == VAL_FLOAT) condBool = cond.as.floatVal != 0.0;
                else if (cond.type == VAL_STRING) condBool = strlen(cond.as.stringVal) > 0;
                else if (cond.type == VAL_NULL) condBool = false;
                else condBool = true;
                
                if (!condBool) break;
                
                interpretStatement(vm, node->body);
                
                if (vm->hadError) break;
            }
            break;
        }
        
        case TOKEN_ASSIGN: {
            if (!node->name) {
                printf("Assignment without variable name\n");
                vm->hadError = true;
                return;
            }
            
            Value value = interpretExpression(vm, node->left);
            defineGlobal(vm, node->name, value);
            
            if (vm->debugMode) {
                printf("[DEBUG] Assignation: %s = ", node->name);
                switch (value.type) {
                    case VAL_INT: printf("%d\n", value.as.intVal); break;
                    case VAL_FLOAT: printf("%.2f\n", value.as.floatVal); break;
                    case VAL_BOOL: printf("%s\n", value.as.boolVal ? "true" : "false"); break;
                    case VAL_STRING: printf("\"%s\"\n", value.as.stringVal); break;
                    case VAL_NULL: printf("null\n"); break;
                    default: printf("[object]\n");
                }
            }
            break;
        }
        
        case TOKEN_LBRACE: {
            // Block: exécuter tous les statements
            ASTNode* stmt = node->left;
            while (stmt && !vm->hadError) {
                interpretStatement(vm, stmt);
                stmt = stmt->next ? stmt->next : stmt->right;
            }
            break;
        }
        
        case TOKEN_RETURN: {
            // Pour l'instant, on ne gère pas les fonctions
            printf("return statement not yet supported\n");
            break;
        }
        
        default:
            // Traiter comme une expression statement
            interpretExpression(vm, node);
            break;
    }
}

void interpret(VM* vm, ASTNode* program) {
    if (!program || vm->hadError) return;
    
    ASTNode* stmt = program->left;
    while (stmt && !vm->hadError) {
        interpretStatement(vm, stmt);
        stmt = stmt->next ? stmt->next : stmt->right;
    }
}

// ============ PARSER (adapté de swf.c) ============
// On garde le parser mais on retourne un AST au lieu de générer du C
ASTNode* parseProgram(CompilerState* state) {
    ASTNode* program = createASTNode(TOKEN_LBRACE);
    ASTNode* current = NULL;
    
    while (state->errorCount < 10) {
        ASTNode* stmt = parseStatement(state);
        if (!stmt) break;
        
        if (!program->left) {
            program->left = stmt;
            current = stmt;
        } else {
            // Créer une liste chaînée
            current->next = stmt;
            current = stmt;
        }
        
        // Vérifier EOF
        Token* test = getNextToken(state);
        if (test->type == TOKEN_EOF) {
            free(test->value);
            free(test);
            break;
        }
        state->pos -= strlen(test->value);
        free(test->value);
        free(test);
    }
    
    return program;
}

// ============ CLI INTERFACE ============
void printHelp() {
    printf("SwiftVelox Interpreter/VM v1.0\n");
    printf("Usage:\n");
    printf("  swiftvelox run <file.swf>    - Exécuter un fichier\n");
    printf("  swiftvelox repl              - Lancer le REPL\n");
    printf("  swiftvelox debug <file.swf>  - Exécuter en mode debug\n");
    printf("  swiftvelox --help            - Afficher cette aide\n");
    printf("  swiftvelox --version         - Afficher la version\n");
    printf("\nExemples:\n");
    printf("  swiftvelox run example.swf\n");
    printf("  swiftvelox repl\n");
}

void printVersion() {
    printf("SwiftVelox v1.0.0 - Interpréteur/VM\n");
    printf("Compilé le %s %s\n", __DATE__, __TIME__);
}

void runFile(const char* filename, bool debug) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Erreur: impossible d'ouvrir %s\n", filename);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, file);
    source[size] = '\0';
    fclose(file);
    
    printf("⚡ Exécution de %s...\n", filename);
    
    // Parser le code
    CompilerState state;
    state.source = source;
    state.pos = 0;
    state.line = 1;
    state.column = 0;
    state.length = strlen(source);
    state.errorCount = 0;
    state.warningCount = 0;
    state.symbolTable = NULL;
    
    ASTNode* program = parseProgram(&state);
    
    if (state.errorCount == 0) {
        // Créer et exécuter la VM
        VM* vm = createVM();
        vm->debugMode = debug;
        
        interpret(vm, program);
        
        if (vm->hadError) {
            printf("❌ Erreur d'exécution\n");
        } else {
            printf("✅ Exécution terminée avec succès\n");
        }
        
        freeVM(vm);
    } else {
        printf("❌ Erreurs de parsing: %d\n", state.errorCount);
    }
    
    freeAST(program);
    free(source);
}

void repl() {
    printf("SwiftVelox REPL v1.0\n");
    printf("Tapez 'exit' pour quitter, 'help' pour l'aide\n");
    printf(">>> ");
    
    VM* vm = createVM();
    char line[1024];
    
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        if (strcmp(line, "help") == 0) {
            printf("Commandes REPL:\n");
            printf("  exit, quit - Quitter\n");
            printf("  help - Afficher cette aide\n");
            printf("  debug on/off - Activer/désactiver le mode debug\n");
            printf("  clear - Effacer les variables\n");
            printf("  list - Lister les variables définies\n");
            printf(">>> ");
            continue;
        }
        if (strcmp(line, "debug on") == 0) {
            vm->debugMode = true;
            printf("Mode debug activé\n");
            printf(">>> ");
            continue;
        }
        if (strcmp(line, "debug off") == 0) {
            vm->debugMode = false;
            printf("Mode debug désactivé\n");
            printf(">>> ");
            continue;
        }
        if (strcmp(line, "clear") == 0) {
            freeVM(vm);
            vm = createVM();
            printf("Variables effacées\n");
            printf(">>> ");
            continue;
        }
        if (strcmp(line, "list") == 0) {
            printf("Variables globales:\n");
            for (int i = 0; i < vm->globalCount; i++) {
                Value v = *vm->globals[i];
                printf("  %s = ", vm->globalNames[i]);
                switch (v.type) {
                    case VAL_INT: printf("%d (int)\n", v.as.intVal); break;
                    case VAL_FLOAT: printf("%.2f (float)\n", v.as.floatVal); break;
                    case VAL_BOOL: printf("%s (bool)\n", v.as.boolVal ? "true" : "false"); break;
                    case VAL_STRING: printf("\"%s\" (string)\n", v.as.stringVal); break;
                    case VAL_NULL: printf("null\n"); break;
                    default: printf("[object]\n");
                }
            }
            printf(">>> ");
            continue;
        }
        
        if (strlen(line) == 0) {
            printf(">>> ");
            continue;
        }
        
        // Ajouter un point-virgule si nécessaire
        char code[1024];
        if (line[strlen(line)-1] != ';' && !strstr(line, "if") && !strstr(line, "while")) {
            snprintf(code, sizeof(code), "%s;", line);
        } else {
            strcpy(code, line);
        }
        
        // Parser et exécuter
        CompilerState state;
        state.source = code;
        state.pos = 0;
        state.line = 1;
        state.column = 0;
        state.length = strlen(code);
        state.errorCount = 0;
        state.warningCount = 0;
        state.symbolTable = NULL;
        
        ASTNode* program = parseProgram(&state);
        
        if (state.errorCount == 0) {
            interpret(vm, program);
        } else {
            printf("Erreur de syntaxe\n");
        }
        
        freeAST(program);
        printf(">>> ");
    }
    
    freeVM(vm);
    printf("Au revoir!\n");
}

// ============ MAIN ============
int main(int argc, char* argv[]) {
    printf("=========================================\n");
    printf("    SwiftVelox VM/Interpreter v1.0\n");
    printf("=========================================\n\n");
    
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printHelp();
    } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printVersion();
    } else if (strcmp(argv[1], "run") == 0 && argc > 2) {
        runFile(argv[2], false);
    } else if (strcmp(argv[1], "debug") == 0 && argc > 2) {
        runFile(argv[2], true);
    } else if (strcmp(argv[1], "repl") == 0) {
        repl();
    } else if (argc == 2 && strstr(argv[1], ".swf")) {
        // Si on passe juste un fichier .swf
        runFile(argv[1], false);
    } else {
        printf("Commande non reconnue\n");
        printHelp();
        return 1;
    }
    
    return 0;
}