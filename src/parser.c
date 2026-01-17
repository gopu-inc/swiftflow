#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Variables globales
ASTNode* current_function = NULL;

ASTNode* new_node(NodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type;
    node->token = current_token;
    node->left = node->right = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->inferred_type = VAL_NIL;
    return node;
}

// Forward declarations
ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();

// Primary expressions
ASTNode* primary() {
    if (match(TK_NUMBER) || match(TK_STRING_LIT) || 
        match(TK_TRUE) || match(TK_FALSE) || match(TK_NIL)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_IDENTIFIER)) {
        ASTNode* n = new_node(NODE_IDENTIFIER);
        n->token = previous_token;
        return n;
    }
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        consume(TK_RPAREN, "')' attendu");
        return expr;
    }
    
    syntax_error(current_token, "Expression attendue");
    return NULL;
}

// Call expressions
ASTNode* call() {
    ASTNode* expr = primary();
    
    if (match(TK_LPAREN)) {
        ASTNode* n = new_node(NODE_CALL);
        n->left = expr;
        
        if (!match(TK_RPAREN)) {
            do {
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = expression();
            } while (match(TK_COMMA));
            consume(TK_RPAREN, "')' attendu");
        }
        expr = n;
    }
    
    return expr;
}

// Unary operators
ASTNode* unary() {
    if (match(TK_MINUS) || match(TK_BANG)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = unary();
        return n;
    }
    
    return call();
}

// Multiplication and division
ASTNode* factor() {
    ASTNode* expr = unary();
    
    while (match(TK_STAR) || match(TK_SLASH) || match(TK_PERCENT)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = unary();
        expr = n;
    }
    
    return expr;
}

// Addition and subtraction
ASTNode* term() {
    ASTNode* expr = factor();
    
    while (match(TK_PLUS) || match(TK_MINUS)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = factor();
        expr = n;
    }
    
    return expr;
}

// Comparison operators
ASTNode* comparison() {
    ASTNode* expr = term();
    
    while (match(TK_EQEQ) || match(TK_BANGEQ) || 
           match(TK_LT) || match(TK_GT) || 
           match(TK_LTEQ) || match(TK_GTEQ)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = term();
        expr = n;
    }
    
    return expr;
}

// Assignment
ASTNode* assignment() {
    ASTNode* expr = comparison();
    
    if (match(TK_EQ)) {
        ASTNode* n = new_node(NODE_ASSIGN);
        n->token = previous_token;
        n->left = expr;
        n->right = assignment();
        return n;
    }
    
    return expr;
}

// Expression
ASTNode* expression() {
    return assignment();
}

// Expression statement
ASTNode* expression_statement() {
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "';' attendu");
    
    ASTNode* n = new_node(NODE_EXPR_STMT);
    n->left = expr;
    return n;
}

// Variable declaration
ASTNode* var_declaration() {
    TokenType decl_type = current_token.type;
    next_token(); // Skip let/const/var
    
    ASTNode* n = new_node(NODE_VAR_DECL);
    
    consume(TK_IDENTIFIER, "Nom de variable attendu");
    n->token = previous_token;
    
    if (match(TK_EQ)) {
        n->right = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Block statement
ASTNode* block_statement() {
    ASTNode* n = new_node(NODE_BLOCK);
    
    consume(TK_LBRACE, "'{' attendu");
    
    while (current_token.type != TK_RBRACE && !is_at_end()) {
        n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
        n->children[n->child_count++] = declaration();
    }
    
    consume(TK_RBRACE, "'}' attendu");
    return n;
}

// If statement
ASTNode* if_statement() {
    ASTNode* n = new_node(NODE_IF);
    
    consume(TK_LPAREN, "'(' attendu après 'if'");
    n->left = expression(); // Condition
    consume(TK_RPAREN, "')' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 2);
    n->child_count = 0;
    
    // Then branch
    n->children[n->child_count++] = statement();
    
    // Else branch
    if (match(TK_ELSE)) {
        n->children[n->child_count++] = statement();
    }
    
    return n;
}

// While statement
ASTNode* while_statement() {
    ASTNode* n = new_node(NODE_WHILE);
    
    consume(TK_LPAREN, "'(' attendu après 'while'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    
    n->right = statement();
    return n;
}

// Return statement
ASTNode* return_statement() {
    ASTNode* n = new_node(NODE_RETURN);
    
    if (current_token.type != TK_SEMICOLON) {
        n->left = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Function declaration
ASTNode* function_declaration() {
    ASTNode* n = new_node(NODE_FUNCTION);
    
    consume(TK_IDENTIFIER, "Nom de fonction attendu");
    n->token = previous_token;
    
    consume(TK_LPAREN, "'(' attendu");
    
    // Parameters
    if (!match(TK_RPAREN)) {
        do {
            ASTNode* param = new_node(NODE_IDENTIFIER);
            consume(TK_IDENTIFIER, "Nom de paramètre attendu");
            param->token = previous_token;
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = param;
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    // Save current function
    ASTNode* enclosing = current_function;
    current_function = n;
    
    n->left = block_statement(); // Function body
    
    // Restore
    current_function = enclosing;
    
    return n;
}

// Statement
ASTNode* statement() {
    if (match(TK_IF)) return if_statement();
    if (match(TK_WHILE)) return while_statement();
    if (match(TK_RETURN)) return return_statement();
    if (match(TK_LBRACE)) return block_statement();
    
    return expression_statement();
}

// Declaration (top-level)
ASTNode* declaration() {
    if (match(TK_LET) || match(TK_CONST) || match(TK_VAR)) {
        return var_declaration();
    }
    if (match(TK_FN)) {
        return function_declaration();
    }
    
    return statement();
}

// Parse program
ASTNode* parse(const char* source) {
    init_scanner(source);
    next_token();
    
    ASTNode* program = new_node(NODE_PROGRAM);
    
    while (!is_at_end() && current_token.type != TK_EOF) {
        program->children = realloc(program->children, sizeof(ASTNode*) * (program->child_count + 1));
        program->children[program->child_count++] = declaration();
    }
    
    return program;
}
