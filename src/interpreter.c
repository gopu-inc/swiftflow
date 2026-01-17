#include "interpreter.h"
#include "parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

// Variables globales
Environment* global_env = NULL;

void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "❌ ERREUR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

// Déclaration explicite de eval
Value eval(void* node_ptr, Environment* env);

// ===== FONCTIONS D'ENVIRONNEMENT =====
Environment* new_environment(Environment* enclosing) {
    Environment* env = malloc(sizeof(Environment));
    env->enclosing = enclosing;
    env->count = 0;
    env->capacity = 8;
    env->names = malloc(sizeof(char*) * 8);
    env->values = malloc(sizeof(Value) * 8);
    return env;
}

void env_define(Environment* env, char* name, Value value) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            env->values[i] = value;
            return;
        }
    }
    if (env->count >= env->capacity) {
        env->capacity *= 2;
        env->names = realloc(env->names, sizeof(char*) * env->capacity);
        env->values = realloc(env->values, sizeof(Value) * env->capacity);
    }
    env->names[env->count] = strdup(name);
    env->values[env->count] = value;
    env->count++;
}

int env_get(Environment* env, char* name, Value* out) {
    Environment* current = env;
    while (current != NULL) {
        for (int i = 0; i < current->count; i++) {
            if (strcmp(current->names[i], name) == 0) {
                *out = current->values[i];
                return 1;
            }
        }
        current = current->enclosing;
    }
    return 0;
}

// ===== FONCTIONS D'ÉVALUATION =====
Value eval_binary(ASTNode* node, Environment* env) {
    Value left = eval(node->left, env);
    Value right = eval(node->right, env);
    Value result = make_nil();
    
    // On ne gère que les tokens essentiels pour éviter les warnings
    switch (node->token.type) {
        case TK_PLUS:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number(left.integer + right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l + r);
            } else if (left.type == VAL_STRING || right.type == VAL_STRING) {
                char buf1[256], buf2[256];
                const char* s1 = NULL, *s2 = NULL;
                
                if (left.type == VAL_STRING) s1 = left.string;
                else if (left.type == VAL_INT) { 
                    sprintf(buf1, "%lld", left.integer); 
                    s1 = buf1; 
                } else if (left.type == VAL_FLOAT) { 
                    sprintf(buf1, "%g", left.number); 
                    s1 = buf1; 
                } else if (left.type == VAL_BOOL) { 
                    s1 = left.boolean ? "true" : "false"; 
                } else { 
                    s1 = "nil"; 
                }
                
                if (right.type == VAL_STRING) s2 = right.string;
                else if (right.type == VAL_INT) { 
                    sprintf(buf2, "%lld", right.integer); 
                    s2 = buf2; 
                } else if (right.type == VAL_FLOAT) { 
                    sprintf(buf2, "%g", right.number); 
                    s2 = buf2; 
                } else if (right.type == VAL_BOOL) { 
                    s2 = right.boolean ? "true" : "false"; 
                } else { 
                    s2 = "nil"; 
                }
                
                char* combined = malloc(strlen(s1) + strlen(s2) + 1);
                strcpy(combined, s1);
                strcat(combined, s2);
                result = make_string(combined);
                free(combined);
            }
            break;
            
        case TK_MINUS:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number(left.integer - right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l - r);
            }
            break;
            
        case TK_STAR:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number(left.integer * right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l * r);
            }
            break;
            
        case TK_SLASH:
            if ((right.type == VAL_INT && right.integer == 0) ||
                (right.type == VAL_FLOAT && fabs(right.number) < 1e-9)) {
                fatal_error("Division par zéro");
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number((double)left.integer / right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l / r);
            }
            break;
            
        case TK_EQEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer == right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(fabs(l - r) < 1e-9);
            } else if (left.type == VAL_STRING && right.type == VAL_STRING) {
                result = make_bool(strcmp(left.string, right.string) == 0);
            } else if (left.type == VAL_BOOL && right.type == VAL_BOOL) {
                result = make_bool(left.boolean == right.boolean);
            }
            break;
            
        case TK_BANGEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer != right.integer);
            }
            break;
            
        case TK_LT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer < right.integer);
            }
            break;
            
        case TK_GT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer > right.integer);
            }
            break;
            
        case TK_AND:
            if (left.type == VAL_BOOL && right.type == VAL_BOOL) {
                result = make_bool(left.boolean && right.boolean);
            }
            break;
            
        case TK_OR:
            if (left.type == VAL_BOOL && right.type == VAL_BOOL) {
                result = make_bool(left.boolean || right.boolean);
            }
            break;
    }
    
    return result;
}

Value eval_unary(ASTNode* node, Environment* env) {
    Value right = eval(node->right, env);
    
    if (node->token.type == TK_MINUS) {
        if (right.type == VAL_INT) {
            return make_number(-right.integer);
        } else if (right.type == VAL_FLOAT) {
            return make_number(-right.number);
        }
    }
    else if (node->token.type == TK_BANG) {
        if (right.type == VAL_BOOL) {
            return make_bool(!right.boolean);
        }
    }
    
    return right;
}

Value eval_assignment(ASTNode* node, Environment* env) {
    if (node->left->type != NODE_IDENTIFIER) {
        fatal_error("Cible d'assignation invalide");
    }
    
    Value value = eval(node->right, env);
    env_define(env, node->left->token.s, value);
    
    return value;
}

Value eval_identifier(ASTNode* node, Environment* env) {
    Value value;
    if (env_get(env, node->token.s, &value)) {
        return value;
    }
    
    fatal_error("Variable non définie: '%s'", node->token.s);
    return make_nil();
}

Value eval_call(ASTNode* node, Environment* env) {
    Value callee = eval(node->left, env);
    
    if (callee.type == VAL_NATIVE) {
        Value* args = malloc(sizeof(Value) * node->child_count);
        for (int i = 0; i < node->child_count; i++) {
            args[i] = eval(node->children[i], env);
        }
        Value result = callee.native.fn(args, node->child_count, env);
        free(args);
        return result;
    }
    
    fatal_error("Tentative d'appel sur une valeur non-fonction");
    return make_nil();
}

Value eval_if(ASTNode* node, Environment* env) {
    Value condition = eval(node->left, env);
    
    if (condition.type == VAL_BOOL && condition.boolean) {
        return eval(node->children[0], env);
    } else if (node->child_count > 1) {
        return eval(node->children[1], env);
    }
    
    return make_nil();
}

Value eval_while(ASTNode* node, Environment* env) {
    Value result = make_nil();
    
    while (1) {
        Value condition = eval(node->left, env);
        if (condition.type != VAL_BOOL || !condition.boolean) {
            break;
        }
        
        result = eval(node->right, env);
    }
    
    return result;
}

Value eval_block(ASTNode* node, Environment* env) {
    Environment* scope = new_environment(env);
    Value result = make_nil();
    
    for (int i = 0; i < node->child_count; i++) {
        result = eval(node->children[i], scope);
    }
    
    return result;
}

Value eval_function(ASTNode* node, Environment* env) {
    Value func;
    func.type = VAL_FUNCTION;
    func.function.declaration = node;
    func.function.closure = env;
    
    // Register function name
    env_define(env, node->token.s, func);
    
    return func;
}

// Fonction d'évaluation principale
Value eval(void* node_ptr, Environment* env) {
    ASTNode* node = (ASTNode*)node_ptr;
    if (!node) return make_nil();
    
    switch (node->type) {
        case NODE_LITERAL:
            if (node->token.type == TK_NUMBER) {
                if (strchr(node->token.start, '.') || 
                    strchr(node->token.start, 'e') || 
                    strchr(node->token.start, 'E')) {
                    return make_number(node->token.d);
                } else {
                    return make_number(node->token.i);
                }
            } else if (node->token.type == TK_STRING_LIT) {
                return make_string(node->token.s);
            } else if (node->token.type == TK_TRUE) {
                return make_bool(1);
            } else if (node->token.type == TK_FALSE) {
                return make_bool(0);
            } else if (node->token.type == TK_NIL) {
                return make_nil();
            }
            break;
            
        case NODE_IDENTIFIER:
            return eval_identifier(node, env);
            
        case NODE_BINARY:
            return eval_binary(node, env);
            
        case NODE_UNARY:
            return eval_unary(node, env);
            
        case NODE_ASSIGN:
            return eval_assignment(node, env);
            
        case NODE_CALL:
            return eval_call(node, env);
            
        case NODE_VAR_DECL: {
            Value value = make_nil();
            if (node->right) {
                value = eval(node->right, env);
            }
            env_define(env, node->token.s, value);
            return value;
        }
            
        case NODE_FUNCTION:
            return eval_function(node, env);
            
        case NODE_IF:
            return eval_if(node, env);
            
        case NODE_WHILE:
            return eval_while(node, env);
            
        case NODE_RETURN: {
            Value value = make_nil();
            if (node->left) {
                value = eval(node->left, env);
            }
            value.type = VAL_RETURN_SIG;
            return value;
        }
            
        case NODE_BLOCK:
            return eval_block(node, env);
            
        case NODE_EXPR_STMT:
            return eval(node->left, env);
            
        case NODE_PROGRAM:
            return eval_block(node, env);
    }
    
    return make_nil();
}
