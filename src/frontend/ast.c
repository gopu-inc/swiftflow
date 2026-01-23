#include "ast.h"
#include "common.h"
#include <stdio.h>

// Create a new AST node
ASTNode* ast_new_node(NodeType type, int line, int column) {
    ASTNode* node = ALLOC(ASTNode);
    CHECK_ALLOC(node, "AST node allocation");
    
    node->type = type;
    node->left = node->right = node->third = node->fourth = NULL;
    node->line = line;
    node->column = column;
    node->op_type = TK_ERROR;
    
    return node;
}

// Create literal nodes
ASTNode* ast_new_int(int64_t value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_INT, line, column);
    node->data.int_val = value;
    return node;
}

ASTNode* ast_new_float(double value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FLOAT, line, column);
    node->data.float_val = value;
    return node;
}

ASTNode* ast_new_string(const char* value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_STRING, line, column);
    node->data.str_val = str_copy(value);
    return node;
}

ASTNode* ast_new_bool(bool value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_BOOL, line, column);
    node->data.bool_val = value;
    return node;
}

ASTNode* ast_new_identifier(const char* name, int line, int column) {
    ASTNode* node = ast_new_node(NODE_IDENT, line, column);
    node->data.name = str_copy(name);
    return node;
}

// Create operation nodes
ASTNode* ast_new_binary(NodeType type, ASTNode* left, ASTNode* right, int line, int column) {
    ASTNode* node = ast_new_node(type, line, column);
    node->left = left;
    node->right = right;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    return node;
}

ASTNode* ast_new_unary(NodeType type, ASTNode* operand, int line, int column) {
    ASTNode* node = ast_new_node(type, line, column);
    node->left = operand;
    return node;
}

ASTNode* ast_new_assignment(ASTNode* left, ASTNode* right, int line, int column) {
    ASTNode* node = ast_new_node(NODE_ASSIGN, line, column);
    node->left = left;
    node->right = right;
    return node;
}

// Create variable declaration
ASTNode* ast_new_var_decl(const char* name, ASTNode* value, TokenKind var_type, int line, int column) {
    ASTNode* node = NULL;
    
    switch (var_type) {
        case TK_VAR: node = ast_new_node(NODE_VAR_DECL, line, column); break;
        case TK_NET: node = ast_new_node(NODE_NET_DECL, line, column); break;
        case TK_CLOG: node = ast_new_node(NODE_CLOG_DECL, line, column); break;
        case TK_DOS: node = ast_new_node(NODE_DOS_DECL, line, column); break;
        case TK_SEL: node = ast_new_node(NODE_SEL_DECL, line, column); break;
        case TK_CONST: node = ast_new_node(NODE_CONST_DECL, line, column); break;
        case TK_GLOBAL: node = ast_new_node(NODE_GLOBAL_DECL, line, column); break;
        default: node = ast_new_node(NODE_VAR_DECL, line, column); break;
    }
    
    node->data.name = str_copy(name);
    node->left = value; // Initial value
    node->op_type = var_type;
    
    return node;
}

// Create control flow nodes
ASTNode* ast_new_if(ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch, int line, int column) {
    ASTNode* node = ast_new_node(NODE_IF, line, column);
    node->left = condition;
    node->right = then_branch;
    node->third = else_branch;
    return node;
}

ASTNode* ast_new_while(ASTNode* condition, ASTNode* body, int line, int column) {
    ASTNode* node = ast_new_node(NODE_WHILE, line, column);
    node->left = condition;
    node->right = body;
    return node;
}

ASTNode* ast_new_for(ASTNode* init, ASTNode* condition, ASTNode* update, ASTNode* body, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FOR, line, column);
    node->data.loop.init = init;
    node->data.loop.condition = condition;
    node->data.loop.update = update;
    node->data.loop.body = body;
    return node;
}

// Create function node
ASTNode* ast_new_function(const char* name, ASTNode* params, ASTNode* body, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FUNC, line, column);
    node->data.func_def.name = str_copy(name);
    node->data.func_def.params = params;
    node->data.func_def.body = body;
    node->data.func_def.return_type = NULL;
    node->data.func_def.is_async = false;
    node->data.func_def.is_generator = false;
    return node;
}

// Create function call node
ASTNode* ast_new_function_call(ASTNode* function, ASTNode* args, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FUNC_CALL, line, column);
    node->data.func_call.function = function;
    node->data.func_call.arguments = args;
    // Count arguments
    int count = 0;
    ASTNode* arg = args;
    while (arg) {
        count++;
        arg = arg->right; // Arguments are stored as a linked list
    }
    node->data.func_call.arg_count = count;
    return node;
}

// Create return node
ASTNode* ast_new_return(ASTNode* value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_RETURN, line, column);
    node->left = value;
    return node;
}

// Create import node
ASTNode* ast_new_import(char** modules, int count, const char* from_module, int line, int column) {
    ASTNode* node = ast_new_node(NODE_IMPORT, line, column);
    node->data.imports.modules = modules;
    node->data.imports.module_count = count;
    node->data.imports.from_module = str_copy(from_module);
    node->data.imports.is_selective = (count > 0);
    return node;
}

// Create print node
ASTNode* ast_new_print(ASTNode* value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_PRINT, line, column);
    node->left = value;
    return node;
}

// Create input node
ASTNode* ast_new_input(const char* prompt, int line, int column) {
    ASTNode* node = ast_new_node(NODE_INPUT, line, column);
    node->data.input_op.prompt = str_copy(prompt);
    return node;
}

// Free AST node recursively
void ast_free(ASTNode* node) {
    if (!node) return;
    
    // Free children
    ast_free(node->left);
    ast_free(node->right);
    ast_free(node->third);
    ast_free(node->fourth);
    
    // Free node-specific data
    switch (node->type) {
        case NODE_STRING:
        case NODE_IDENT:
            FREE(node->data.str_val);
            break;
        case NODE_FUNC:
            FREE(node->data.func_def.name);
            ast_free(node->data.func_def.params);
            ast_free(node->data.func_def.body);
            ast_free(node->data.func_def.return_type);
            break;
        case NODE_IMPORT:
            if (node->data.imports.modules) {
                for (int i = 0; i < node->data.imports.module_count; i++) {
                    FREE(node->data.imports.modules[i]);
                }
                FREE(node->data.imports.modules);
            }
            FREE(node->data.imports.from_module);
            break;
        case NODE_INPUT:
            FREE(node->data.input_op.prompt);
            break;
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL:
        case NODE_GLOBAL_DECL:
            FREE(node->data.name);
            break;
        default:
            // No special cleanup needed
            break;
    }
    
    FREE(node);
}

// Print AST for debugging
void ast_print(ASTNode* node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    printf("%s (%d:%d)", node_type_to_string(node->type), node->line, node->column);
    
    switch (node->type) {
        case NODE_INT:
            printf(": %ld\n", node->data.int_val);
            break;
        case NODE_FLOAT:
            printf(": %f\n", node->data.float_val);
            break;
        case NODE_STRING:
            printf(": \"%s\"\n", node->data.str_val);
            break;
        case NODE_BOOL:
            printf(": %s\n", node->data.bool_val ? "true" : "false");
            break;
        case NODE_IDENT:
            printf(": %s\n", node->data.name);
            break;
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL:
        case NODE_GLOBAL_DECL:
            printf(": %s\n", node->data.name);
            ast_print(node->left, indent + 1);
            break;
        case NODE_FUNC:
            printf(": %s\n", node->data.func_def.name);
            ast_print(node->data.func_def.params, indent + 1);
            ast_print(node->data.func_def.body, indent + 1);
            break;
        case NODE_IF:
            printf("\n");
            ast_print(node->left, indent + 1);  // Condition
            printf("%*sThen:\n", indent * 2, "");
            ast_print(node->right, indent + 1); // Then branch
            if (node->third) {
                printf("%*sElse:\n", indent * 2, "");
                ast_print(node->third, indent + 1); // Else branch
            }
            break;
        default:
            printf("\n");
            ast_print(node->left, indent + 1);
            ast_print(node->right, indent + 1);
            ast_print(node->third, indent + 1);
            ast_print(node->fourth, indent + 1);
            break;
    }
}

// Convert node type to string
const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NODE_INT: return "INT";
        case NODE_FLOAT: return "FLOAT";
        case NODE_STRING: return "STRING";
        case NODE_BOOL: return "BOOL";
        case NODE_IDENT: return "IDENT";
        case NODE_NULL: return "NULL";
        case NODE_UNDEFINED: return "UNDEFINED";
        case NODE_NAN: return "NAN";
        case NODE_INF: return "INF";
        case NODE_LIST: return "LIST";
        case NODE_MAP: return "MAP";
        case NODE_FUNC: return "FUNC";
        case NODE_FUNC_CALL: return "FUNC_CALL";
        case NODE_LAMBDA: return "LAMBDA";
        case NODE_ARRAY_ACCESS: return "ARRAY_ACCESS";
        case NODE_MEMBER_ACCESS: return "MEMBER_ACCESS";
        case NODE_BINARY: return "BINARY";
        case NODE_UNARY: return "UNARY";
        case NODE_TERNARY: return "TERNARY";
        case NODE_ASSIGN: return "ASSIGN";
        case NODE_COMPOUND_ASSIGN: return "COMPOUND_ASSIGN";
        case NODE_IF: return "IF";
        case NODE_WHILE: return "WHILE";
        case NODE_FOR: return "FOR";
        case NODE_FOR_IN: return "FOR_IN";
        case NODE_SWITCH: return "SWITCH";
        case NODE_CASE: return "CASE";
        case NODE_RETURN: return "RETURN";
        case NODE_YIELD: return "YIELD";
        case NODE_BREAK: return "BREAK";
        case NODE_CONTINUE: return "CONTINUE";
        case NODE_THROW: return "THROW";
        case NODE_TRY: return "TRY";
        case NODE_CATCH: return "CATCH";
        case NODE_VAR_DECL: return "VAR_DECL";
        case NODE_NET_DECL: return "NET_DECL";
        case NODE_CLOG_DECL: return "CLOG_DECL";
        case NODE_DOS_DECL: return "DOS_DECL";
        case NODE_SEL_DECL: return "SEL_DECL";
        case NODE_CONST_DECL: return "CONST_DECL";
        case NODE_GLOBAL_DECL: return "GLOBAL_DECL";
        case NODE_SIZEOF: return "SIZEOF";
        case NODE_NEW: return "NEW";
        case NODE_DELETE: return "DELETE";
        case NODE_FREE: return "FREE";
        case NODE_IMPORT: return "IMPORT";
        case NODE_EXPORT: return "EXPORT";
        case NODE_MODULE: return "MODULE";
        case NODE_DBVAR: return "DBVAR";
        case NODE_ASSERT: return "ASSERT";
        case NODE_PRINT: return "PRINT";
        case NODE_WELD: return "WELD";
        case NODE_READ: return "READ";
        case NODE_WRITE: return "WRITE";
        case NODE_INPUT: return "INPUT";
        case NODE_PASS: return "PASS";
        case NODE_WITH: return "WITH";
        case NODE_LEARN: return "LEARN";
        case NODE_LOCK: return "LOCK";
        case NODE_APPEND: return "APPEND";
        case NODE_PUSH: return "PUSH";
        case NODE_POP: return "POP";
        case NODE_CLASS: return "CLASS";
        case NODE_STRUCT: return "STRUCT";
        case NODE_ENUM: return "ENUM";
        case NODE_INTERFACE: return "INTERFACE";
        case NODE_TYPEDEF: return "TYPEDEF";
        case NODE_NAMESPACE: return "NAMESPACE";
        case NODE_NEW_INSTANCE: return "NEW_INSTANCE";
        case NODE_METHOD_CALL: return "METHOD_CALL";
        case NODE_PROPERTY_ACCESS: return "PROPERTY_ACCESS";
        case NODE_JSON: return "JSON";
        case NODE_YAML: return "YAML";
        case NODE_XML: return "XML";
        case NODE_ASYNC: return "ASYNC";
        case NODE_AWAIT: return "AWAIT";
        case NODE_BLOCK: return "BLOCK";
        case NODE_SCOPE: return "SCOPE";
        case NODE_MAIN: return "MAIN";
        case NODE_PROGRAM: return "PROGRAM";
        case NODE_EMPTY: return "EMPTY";
        default: return "UNKNOWN";
    }
}

// Convert token kind to string
const char* token_kind_to_string(TokenKind kind) {
    switch (kind) {
        case TK_INT: return "INT";
        case TK_FLOAT: return "FLOAT";
        case TK_STRING: return "STRING";
        case TK_TRUE: return "TRUE";
        case TK_FALSE: return "FALSE";
        case TK_NULL: return "NULL";
        case TK_UNDEFINED: return "UNDEFINED";
        case TK_NAN: return "NAN";
        case TK_INF: return "INF";
        case TK_IDENT: return "IDENT";
        case TK_AS: return "AS";
        case TK_OF: return "OF";
        case TK_PLUS: return "PLUS";
        case TK_MINUS: return "MINUS";
        case TK_MULT: return "MULT";
        case TK_DIV: return "DIV";
        case TK_MOD: return "MOD";
        case TK_POW: return "POW";
        case TK_CONCAT: return "CONCAT";
        case TK_SPREAD: return "SPREAD";
        case TK_NULLISH: return "NULLISH";
        case TK_ASSIGN: return "ASSIGN";
        case TK_EQ: return "EQ";
        case TK_NEQ: return "NEQ";
        case TK_GT: return "GT";
        case TK_LT: return "LT";
        case TK_GTE: return "GTE";
        case TK_LTE: return "LTE";
        case TK_PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TK_MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TK_MULT_ASSIGN: return "MULT_ASSIGN";
        case TK_DIV_ASSIGN: return "DIV_ASSIGN";
        case TK_MOD_ASSIGN: return "MOD_ASSIGN";
        case TK_POW_ASSIGN: return "POW_ASSIGN";
        case TK_CONCAT_ASSIGN: return "CONCAT_ASSIGN";
        case TK_AND: return "AND";
        case TK_OR: return "OR";
        case TK_NOT: return "NOT";
        case TK_BIT_AND: return "BIT_AND";
        case TK_BIT_OR: return "BIT_OR";
        case TK_BIT_XOR: return "BIT_XOR";
        case TK_BIT_NOT: return "BIT_NOT";
        case TK_SHL: return "SHL";
        case TK_SHR: return "SHR";
        case TK_USHR: return "USHR";
        case TK_RARROW: return "RARROW";
        case TK_DARROW: return "DARROW";
        case TK_LDARROW: return "LDARROW";
        case TK_RDARROW: return "RDARROW";
        case TK_SPACESHIP: return "SPACESHIP";
        case TK_ELLIPSIS: return "ELLIPSIS";
        case TK_RANGE: return "RANGE";
        case TK_RANGE_INCL: return "RANGE_INCL";
        case TK_QUESTION: return "QUESTION";
        case TK_SCOPE: return "SCOPE";
        case TK_SAFE_NAV: return "SAFE_NAV";
        case TK_IN: return "IN";
        case TK_IS: return "IS";
        case TK_ISNOT: return "ISNOT";
        case TK_AS_OP: return "AS_OP";
        case TK_LPAREN: return "LPAREN";
        case TK_RPAREN: return "RPAREN";
        case TK_LBRACE: return "LBRACE";
        case TK_RBRACE: return "RBRACE";
        case TK_LBRACKET: return "LBRACKET";
        case TK_RBRACKET: return "RBRACKET";
        case TK_LSQUARE: return "LSQUARE";
        case TK_RSQUARE: return "RSQUARE";
        case TK_COMMA: return "COMMA";
        case TK_SEMICOLON: return "SEMICOLON";
        case TK_COLON: return "COLON";
        case TK_PERIOD: return "PERIOD";
        case TK_AT: return "AT";
        case TK_HASH: return "HASH";
        case TK_DOLLAR: return "DOLLAR";
        case TK_BACKTICK: return "BACKTICK";
        case TK_VAR: return "VAR";
        case TK_LET: return "LET";
        case TK_CONST: return "CONST";
        case TK_NET: return "NET";
        case TK_CLOG: return "CLOG";
        case TK_DOS: return "DOS";
        case TK_SEL: return "SEL";
        case TK_THEN: return "THEN";
        case TK_DO: return "DO";
        case TK_IF: return "IF";
        case TK_ELSE: return "ELSE";
        case TK_ELIF: return "ELIF";
        case TK_WHILE: return "WHILE";
        case TK_FOR: return "FOR";
        case TK_SWITCH: return "SWITCH";
        case TK_CASE: return "CASE";
        case TK_DEFAULT: return "DEFAULT";
        case TK_BREAK: return "BREAK";
        case TK_CONTINUE: return "CONTINUE";
        case TK_RETURN: return "RETURN";
        case TK_YIELD: return "YIELD";
        case TK_TRY: return "TRY";
        case TK_CATCH: return "CATCH";
        case TK_FINALLY: return "FINALLY";
        case TK_THROW: return "THROW";
        case TK_FUNC: return "FUNC";
        case TK_IMPORT: return "IMPORT";
        case TK_EXPORT: return "EXPORT";
        case TK_FROM: return "FROM";
        case TK_CLASS: return "CLASS";
        case TK_STRUCT: return "STRUCT";
        case TK_ENUM: return "ENUM";
        case TK_INTERFACE: return "INTERFACE";
        case TK_TYPEDEF: return "TYPEDEF";
        case TK_TYPELOCK: return "TYPELOCK";
        case TK_NAMESPACE: return "NAMESPACE";
        case TK_TYPE_INT: return "TYPE_INT";
        case TK_TYPE_FLOAT: return "TYPE_FLOAT";
        case TK_TYPE_STR: return "TYPE_STR";
        case TK_TYPE_BOOL: return "TYPE_BOOL";
        case TK_TYPE_CHAR: return "TYPE_CHAR";
        case TK_TYPE_VOID: return "TYPE_VOID";
        case TK_TYPE_ANY: return "TYPE_ANY";
        case TK_TYPE_AUTO: return "TYPE_AUTO";
        case TK_TYPE_UNKNOWN: return "TYPE_UNKNOWN";
        case TK_TYPE_NET: return "TYPE_NET";
        case TK_TYPE_CLOG: return "TYPE_CLOG";
        case TK_TYPE_DOS: return "TYPE_DOS";
        case TK_TYPE_SEL: return "TYPE_SEL";
        case TK_TYPE_ARRAY: return "TYPE_ARRAY";
        case TK_TYPE_MAP: return "TYPE_MAP";
        case TK_TYPE_FUNC: return "TYPE_FUNC";
        case TK_DECREMENT: return "DECREMENT";
        case TK_INCREMENT: return "INCREMENT";
        case TK_TYPEOF: return "TYPEOF";
        case TK_SIZEOF: return "SIZEOF";
        case TK_SIZE: return "SIZE";
        case TK_SIZ: return "SIZ";
        case TK_NEW: return "NEW";
        case TK_DELETE: return "DELETE";
        case TK_FREE: return "FREE";
        case TK_DB: return "DB";
        case TK_DBVAR: return "DBVAR";
        case TK_PRINT_DB: return "PRINT_DB";
        case TK_ASSERT: return "ASSERT";
        case TK_PRINT: return "PRINT";
        case TK_WELD: return "WELD";
        case TK_READ: return "READ";
        case TK_WRITE: return "WRITE";
        case TK_INPUT: return "INPUT";
        case TK_PASS: return "PASS";
        case TK_GLOBAL: return "GLOBAL";
        case TK_LAMBDA: return "LAMBDA";
        case TK_BDD: return "BDD";
        case TK_DEF: return "DEF";
        case TK_TYPE: return "TYPE";
        case TK_RAISE: return "RAISE";
        case TK_WITH: return "WITH";
        case TK_LEARN: return "LEARN";
        case TK_NONLOCAL: return "NONLOCAL";
        case TK_LOCK: return "LOCK";
        case TK_APPEND: return "APPEND";
        case TK_PUSH: return "PUSH";
        case TK_POP: return "POP";
        case TK_TO: return "TO";
        case TK_JSON: return "JSON";
        case TK_YAML: return "YAML";
        case TK_XML: return "XML";
        case TK_MAIN: return "MAIN";
        case TK_THIS: return "THIS";
        case TK_SELF: return "SELF";
        case TK_SUPER: return "SUPER";
        case TK_STATIC: return "STATIC";
        case TK_PUBLIC: return "PUBLIC";
        case TK_PRIVATE: return "PRIVATE";
        case TK_PROTECTED: return "PROTECTED";
        case TK_ASYNC: return "ASYNC";
        case TK_AWAIT: return "AWAIT";
        case TK_FILE_OPEN: return "FILE_OPEN";
        case TK_FILE_CLOSE: return "FILE_CLOSE";
        case TK_FILE_READ: return "FILE_READ";
        case TK_FILE_WRITE: return "FILE_WRITE";
        case TK_EOF: return "EOF";
        case TK_ERROR: return "ERROR";
        default: return "UNKNOWN_TOKEN";
    }
}

// AST Optimizer (simple version)
ASTNode* ast_optimize(ASTNode* node) {
    if (!node) return NULL;
    
    // Optimize children first
    node->left = ast_optimize(node->left);
    node->right = ast_optimize(node->right);
    node->third = ast_optimize(node->third);
    node->fourth = ast_optimize(node->fourth);
    
    // Constant folding for binary operations
    if (node->type == NODE_BINARY) {
        if (node->left && node->right) {
            // Check if both operands are constants
            if (node->left->type == NODE_INT && node->right->type == NODE_INT) {
                int64_t left_val = node->left->data.int_val;
                int64_t right_val = node->right->data.int_val;
                int64_t result = 0;
                
                switch (node->op_type) {
                    case TK_PLUS:
                        result = left_val + right_val;
                        ast_free(node->left);
                        ast_free(node->right);
                        node->type = NODE_INT;
                        node->data.int_val = result;
                        node->left = node->right = NULL;
                        break;
                    case TK_MINUS:
                        result = left_val - right_val;
                        ast_free(node->left);
                        ast_free(node->right);
                        node->type = NODE_INT;
                        node->data.int_val = result;
                        node->left = node->right = NULL;
                        break;
                    case TK_MULT:
                        result = left_val * right_val;
                        ast_free(node->left);
                        ast_free(node->right);
                        node->type = NODE_INT;
                        node->data.int_val = result;
                        node->left = node->right = NULL;
                        break;
                    case TK_DIV:
                        if (right_val != 0) {
                            result = left_val / right_val;
                            ast_free(node->left);
                            ast_free(node->right);
                            node->type = NODE_INT;
                            node->data.int_val = result;
                            node->left = node->right = NULL;
                        }
                        break;
                }
            }
        }
    }
    
    return node;
}
