#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "value.h"

// Types de nœuds AST
typedef enum {
    NODE_PROGRAM, NODE_BLOCK, NODE_VAR_DECL,
    NODE_FUNCTION, NODE_IF, NODE_WHILE, NODE_RETURN,
    NODE_BINARY, NODE_UNARY, NODE_ASSIGN,
    NODE_LITERAL, NODE_IDENTIFIER,
    NODE_CALL, NODE_EXPR_STMT
} NodeType;

typedef struct ASTNode {
    NodeType type;
    Token token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int child_count;
    ValueType inferred_type;
} ASTNode;

// Fonctions du parser
ASTNode* parse(const char* source);
ASTNode* new_node(NodeType type);
ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();

// Variables globales
extern ASTNode* current_function;

#endif
