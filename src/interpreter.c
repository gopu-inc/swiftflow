#include "interpreter.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

// Variables globales
Environment* global_env = NULL;

void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;31m❌ ERREUR FATALE: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");
    va_end(args);
    exit(1);
}

// ===== ENVIRONNEMENT =====
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

// ===== ÉVALUATION =====
Value eval_block(ASTNode* node, Environment* env) {
    Environment* scope = new_environment(env);
    Value result = make_nil();
    
    for (int i = 0; i < node->child_count; i++) {
        result = eval(node->children[i], scope);
        
        // Check for control flow
        if (result.type == VAL_RETURN_SIG || 
            result.type == VAL_ERROR ||
            (node->type == NODE_LOOP_BODY && 
             (result.type == VAL_BREAK || result.type == VAL_CONTINUE))) {
            return result;
        }
    }
    
    return result;
}

Value eval_binary(ASTNode* node, Environment* env) {
    Value left = eval(node->left, env);
    Value right = eval(node->right, env);
    Value result = make_nil();
    
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
                const char* s1, *s2;
                
                if (left.type == VAL_STRING) s1 = left.string;
                else if (left.type == VAL_INT) { sprintf(buf1, "%lld", left.integer); s1 = buf1; }
                else if (left.type == VAL_FLOAT) { sprintf(buf1, "%g", left.number); s1 = buf1; }
                else if (left.type == VAL_BOOL) { s1 = left.boolean ? "true" : "false"; }
                else { s1 = "nil"; }
                
                if (right.type == VAL_STRING) s2 = right.string;
                else if (right.type == VAL_INT) { sprintf(buf2, "%lld", right.integer); s2 = buf2; }
                else if (right.type == VAL_FLOAT) { sprintf(buf2, "%g", right.number); s2 = buf2; }
                else if (right.type == VAL_BOOL) { s2 = right.boolean ? "true" : "false"; }
                else { s2 = "nil"; }
                
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
            
        case TK_PERCENT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                if (right.integer == 0) fatal_error("Modulo par zéro");
                result = make_number(left.integer % right.integer);
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
            } else if (left.type == VAL_NIL && right.type == VAL_NIL) {
                result = make_bool(1);
            }
            break;
            
        case TK_BANGEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer != right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(fabs(l - r) >= 1e-9);
            }
            break;
            
        case TK_LT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer < right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l < r);
            }
            break;
            
        case TK_GT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer > right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l > r);
            }
            break;
            
        case TK_LTEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer <= right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l <= r);
            }
            break;
            
        case TK_GTEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer >= right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l >= r);
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
    Value result = make_nil();
    
    switch (node->token.type) {
        case TK_BANG:
            if (right.type == VAL_BOOL) {
                result = make_bool(!right.boolean);
            }
            break;
            
        case TK_MINUS:
            if (right.type == VAL_INT) {
                result = make_number(-right.integer);
            } else if (right.type == VAL_FLOAT) {
                result = make_number(-right.number);
            }
            break;
            
        case TK_PLUS:
            // Convert to number
            if (right.type == VAL_INT || right.type == VAL_FLOAT) {
                result = right;
            } else if (right.type == VAL_STRING) {
                // Try to parse as number
                char* end;
                double d = strtod(right.string, &end);
                if (end != right.string) {
                    result = make_number(d);
                }
            }
            break;
    }
    
    return result;
}

Value eval_ternary(ASTNode* node, Environment* env) {
    Value condition = eval(node->left, env);
    
    if (condition.type == VAL_BOOL && condition.boolean) {
        return eval(node->right, env);
    } else {
        return eval(node->children[0]->left, env);
    }
}

Value eval_assignment(ASTNode* node, Environment* env) {
    if (node->left->type != NODE_IDENTIFIER) {
        fatal_error("Cible d'assignation invalide");
    }
    
    Value value = eval(node->right, env);
    env_define(env, node->left->token.s, value);
    
    return value;
}

Value eval_call(ASTNode* node, Environment* env) {
    Value callee = eval(node->left, env);
    
    // Prepare arguments
    Value* args = malloc(sizeof(Value) * node->child_count);
    for (int i = 0; i < node->child_count; i++) {
        args[i] = eval(node->children[i], env);
    }
    
    // Native function
    if (callee.type == VAL_NATIVE) {
        Value result = callee.native.fn(args, node->child_count, env);
        free(args);
        return result;
    }
    
    // User-defined function
    if (callee.type == VAL_FUNCTION) {
        ASTNode* decl = callee.function.declaration;
        Environment* func_env = new_environment(callee.function.closure);
        
        // Bind parameters
        for (int i = 0; i < decl->child_count; i++) {
            ASTNode* param = decl->children[i];
            if (i < node->child_count) {
                env_define(func_env, param->token.s, args[i]);
            } else if (param->right) {
                // Default value
                Value default_val = eval(param->right, env);
                env_define(func_env, param->token.s, default_val);
            } else {
                fatal_error("Argument manquant pour le paramètre '%s'", param->token.s);
            }
        }
        
        free(args);
        
        // Execute function body
        Value result = eval(decl->left, func_env);
        
        // Handle return
        if (result.type == VAL_RETURN_SIG) {
            return result;
        }
        
        return make_nil();
    }
    
    fatal_error("Tentative d'appel sur une valeur non-fonction");
    free(args);
    return make_nil();
}

Value eval_array(ASTNode* node, Environment* env) {
    Value array = make_array();
    
    for (int i = 0; i < node->child_count; i++) {
        Value element = eval(node->children[i], env);
        array_push(&array, element);
    }
    
    return array;
}

Value eval_object(ASTNode* node, Environment* env) {
    Value obj = make_object();
    
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* prop = node->children[i];
        Value value = eval(prop->left, env);
        object_set(&obj, prop->token.s, value);
    }
    
    return obj;
}

Value eval_identifier(ASTNode* node, Environment* env) {
    Value value;
    if (env_get(env, node->token.s, &value)) {
        return value;
    }
    
    fatal_error("Variable non définie: '%s'", node->token.s);
    return make_nil();
}

Value eval_if(ASTNode* node, Environment* env) {
    Value condition = eval(node->left, env);
    
    if (condition.type == VAL_BOOL && condition.boolean) {
        return eval(node->children[0], env);
    }
    
    // Check elif branches
    for (int i = 1; i < node->child_count - 1; i++) {
        ASTNode* elif = node->children[i];
        Value elif_cond = eval(elif->left, env);
        if (elif_cond.type == VAL_BOOL && elif_cond.boolean) {
            return eval(elif->right, env);
        }
    }
    
    // Else branch
    if (node->child_count > 0 && 
        node->children[node->child_count - 1] != NULL) {
        return eval(node->children[node->child_count - 1], env);
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
        
        // Handle break/continue
        if (result.type == VAL_BREAK) {
            result = make_nil();
            break;
        }
        if (result.type == VAL_CONTINUE) {
            result = make_nil();
            continue;
        }
        if (result.type == VAL_RETURN_SIG || result.type == VAL_ERROR) {
            return result;
        }
    }
    
    return result;
}

Value eval_function(ASTNode* node, Environment* env) {
    Value func;
    func.type = VAL_FUNCTION;
    func.function.declaration = node;
    func.function.closure = env;
    
    // Register function name if it has one
    if (node->token.s) {
        env_define(env, node->token.s, func);
    }
    
    return func;
}

// Main evaluation function
Value eval(ASTNode* node, Environment* env) {
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
            } else if (node->token.type == TK_STRING_LIT || 
                      node->token.type == TK_TEMPLATE_LIT) {
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
            
        case NODE_TERNARY:
            return eval_ternary(node, env);
            
        case NODE_ASSIGN:
        case NODE_COMPOUND_ASSIGN:
            return eval_assignment(node, env);
            
        case NODE_CALL:
            return eval_call(node, env);
            
        case NODE_ARRAY:
            return eval_array(node, env);
            
        case NODE_OBJECT:
            return eval_object(node, env);
            
        case NODE_VAR_DECL:
        case NODE_CONST_DECL: {
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
            
        case NODE_BREAK: {
            Value v;
            v.type = VAL_BREAK;
            return v;
        }
            
        case NODE_CONTINUE: {
            Value v;
            v.type = VAL_CONTINUE;
            return v;
        }
            
        case NODE_BLOCK:
            return eval_block(node, env);
            
        case NODE_EXPR_STMT:
            return eval(node->left, env);
            
        default:
            printf("Node type non implémenté: %d\n", node->type);
            break;
    }
    
    return make_nil();
}
