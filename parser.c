#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

extern Token scanToken();
extern void initLexer(const char* source);

// ======================================================
// [SECTION] PARSER STATE
// ======================================================
static Token current;
static Token previous;

static void advance() {
    previous = current;
    current = scanToken();
}

static bool match(TokenKind kind) {
    if (current.kind == kind) {
        advance();
        return true;
    }
    return false;
}

static ASTNode* newNode(NodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (node) {
        node->type = type;
        node->import_count = 0;
    }
    return node;
}

// ======================================================
// [SECTION] DEBUG HELPER
// ======================================================
static const char* tokenKindToStr(TokenKind kind) {
    switch (kind) {
        case TK_IMPORT: return "import";
        case TK_LPAREN: return "(";
        case TK_RPAREN: return ")";
        case TK_LBRACE: return "{";
        case TK_RBRACE: return "}";
        case TK_STRING: return "string";
        case TK_COMMA: return ",";
        case TK_SEMICOLON: return ";";
        case TK_VAR: return "var";
        case TK_PRINT: return "print";
        case TK_INT: return "int";
        case TK_FLOAT: return "float";
        case TK_IDENT: return "identifier";
        case TK_PLUS: return "+";
        case TK_MINUS: return "-";
        case TK_MULT: return "*";
        case TK_DIV: return "/";
        case TK_ASSIGN: return "=";
        case TK_EOF: return "EOF";
        default: return "unknown";
    }
}

static void debugToken(const char* msg, Token token) {
    printf("[PARSER DEBUG] %s: %s", msg, tokenKindToStr(token.kind));
    if (token.kind == TK_STRING && token.value.str_val) {
        printf(" \"%s\"", token.value.str_val);
    } else if (token.kind == TK_IDENT && token.value.str_val) {
        printf(" %s", token.value.str_val);
    } else if (token.kind == TK_INT) {
        printf(" %lld", token.value.int_val);
    }
    printf("\n");
}

// ======================================================
// [SECTION] IMPORT PARSING
// ======================================================
static ASTNode* parseImportStatement() {
    debugToken("Starting import parse, current token", current);
    
    // Syntaxe: import() {"module1", "module2"};
    
    if (!match(TK_LPAREN)) {
        printf("[PARSER ERROR] Expected '(' after import\n");
        debugToken("Got token instead", current);
        return NULL;
    }
    
    debugToken("Found '(', next token", current);
    
    if (!match(TK_LBRACE)) {
        printf("[PARSER ERROR] Expected '{' for import list\n");
        debugToken("Got token instead", current);
        return NULL;
    }
    
    debugToken("Found '{', parsing import list", current);
    
    ASTNode* node = newNode(NODE_IMPORT);
    if (!node) return NULL;
    
    // Parse import list
    char** imports = NULL;
    int count = 0;
    int capacity = 4;
    
    imports = malloc(capacity * sizeof(char*));
    if (!imports) {
        free(node);
        return NULL;
    }
    
    // Parse first import
    if (match(TK_STRING)) {
        imports[count++] = str_copy(previous.value.str_val);
        printf("[PARSER] Added import: %s\n", previous.value.str_val);
    } else {
        printf("[PARSER ERROR] Expected string in import list\n");
        debugToken("Got token instead", current);
        free(imports);
        free(node);
        return NULL;
    }
    
    // Parse additional imports
    while (match(TK_COMMA)) {
        debugToken("Found comma, expecting next import", current);
        
        if (count >= capacity) {
            capacity *= 2;
            char** new_imports = realloc(imports, capacity * sizeof(char*));
            if (!new_imports) {
                for (int i = 0; i < count; i++) free(imports[i]);
                free(imports);
                free(node);
                return NULL;
            }
            imports = new_imports;
        }
        
        if (match(TK_STRING)) {
            imports[count++] = str_copy(previous.value.str_val);
            printf("[PARSER] Added import: %s\n", previous.value.str_val);
        } else {
            printf("[PARSER ERROR] Expected string after comma\n");
            debugToken("Got token instead", current);
            break;
        }
    }
    
    if (!match(TK_RBRACE)) {
        printf("[PARSER ERROR] Expected '}' after import list\n");
        debugToken("Got token instead", current);
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node);
        return NULL;
    }
    
    debugToken("Found '}', expecting ')'", current);
    
    if (!match(TK_RPAREN)) {
        printf("[PARSER ERROR] Expected ')' after import list\n");
        debugToken("Got token instead", current);
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node);
        return NULL;
    }
    
    debugToken("Found ')', expecting ';'", current);
    
    if (!match(TK_SEMICOLON)) {
        printf("[PARSER ERROR] Expected ';' after import statement\n");
        debugToken("Got token instead", current);
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node);
        return NULL;
    }
    
    node->data.imports = imports;
    node->import_count = count;
    printf("[PARSER] Import statement parsed successfully with %d imports\n", count);
    return node;
}

// ======================================================
// [SECTION] EXPRESSION PARSING
// ======================================================
static ASTNode* parsePrimary();
static ASTNode* parseExpression();

static ASTNode* parsePrimary() {
    debugToken("parsePrimary - current token", current);
    
    if (match(TK_INT)) {
        ASTNode* node = newNode(NODE_INT);
        if (node) {
            node->data.int_val = previous.value.int_val;
            printf("[PARSER] Integer literal: %lld\n", previous.value.int_val);
        }
        return node;
    }
    
    if (match(TK_FLOAT)) {
        ASTNode* node = newNode(NODE_FLOAT);
        if (node) {
            node->data.float_val = previous.value.float_val;
            printf("[PARSER] Float literal: %f\n", previous.value.float_val);
        }
        return node;
    }
    
    if (match(TK_STRING)) {
        ASTNode* node = newNode(NODE_STRING);
        if (node) {
            node->data.str_val = str_copy(previous.value.str_val);
            printf("[PARSER] String literal: %s\n", previous.value.str_val);
        }
        return node;
    }
    
    if (match(TK_TRUE) || match(TK_FALSE)) {
        ASTNode* node = newNode(NODE_INT);
        if (node) {
            node->data.int_val = (previous.kind == TK_TRUE) ? 1 : 0;
            printf("[PARSER] Boolean literal: %d\n", node->data.int_val);
        }
        return node;
    }
    
    if (match(TK_IDENT)) {
        ASTNode* node = newNode(NODE_IDENT);
        if (node) {
            node->data.name = str_copy(previous.value.str_val);
            printf("[PARSER] Identifier: %s\n", previous.value.str_val);
        }
        return node;
    }
    
    if (match(TK_LPAREN)) {
        printf("[PARSER] Found '(' starting subexpression\n");
        ASTNode* expr = parseExpression();
        if (expr) {
            if (match(TK_RPAREN)) {
                printf("[PARSER] Found ')' ending subexpression\n");
                return expr;
            }
            printf("[PARSER ERROR] Expected ')' after expression\n");
            free(expr);
        }
        return NULL;
    }
    
    printf("[PARSER ERROR] Expected expression\n");
    debugToken("Got token instead", current);
    return NULL;
}

static int getPrecedence(TokenKind op) {
    switch (op) {
        case TK_EQ:
        case TK_NEQ:
        case TK_LT:
        case TK_GT:
        case TK_LTE:
        case TK_GTE:
            return 1;
        case TK_PLUS:
        case TK_MINUS:
            return 2;
        case TK_MULT:
        case TK_DIV:
        case TK_MOD:
            return 3;
        default:
            return 0;
    }
}

static ASTNode* parseBinary(int min_prec) {
    ASTNode* left = parsePrimary();
    if (!left) return NULL;
    
    while (1) {
        TokenKind op = current.kind;
        int prec = getPrecedence(op);
        
        if (prec == 0 || prec < min_prec) break;
        
        debugToken("Binary operator found", current);
        advance(); // Consume operator
        
        ASTNode* node = newNode(NODE_BINARY);
        if (!node) {
            free(left);
            return NULL;
        }
        
        node->data.int_val = op; // Store operator
        node->left = left;
        node->right = parseBinary(prec + 1);
        
        if (!node->right) {
            free(node);
            free(left);
            return NULL;
        }
        
        left = node;
    }
    
    return left;
}

static ASTNode* parseExpression() {
    debugToken("parseExpression - starting", current);
    ASTNode* result = parseBinary(0);
    if (result) {
        printf("[PARSER] Expression parsed successfully\n");
    }
    return result;
}

// ======================================================
// [SECTION] STATEMENT PARSING
// ======================================================
static ASTNode* parseStatement();

static ASTNode* parseVarDecl() {
    debugToken("parseVarDecl - expecting identifier", current);
    
    if (!match(TK_IDENT)) {
        printf("[PARSER ERROR] Expected variable name\n");
        return NULL;
    }
    
    char* var_name = str_copy(previous.value.str_val);
    printf("[PARSER] Variable name: %s\n", var_name);
    
    ASTNode* node = newNode(NODE_VAR);
    if (!node) {
        free(var_name);
        return NULL;
    }
    
    node->data.name = var_name;
    
    if (match(TK_ASSIGN)) {
        printf("[PARSER] Found '=', parsing initializer\n");
        node->left = parseExpression();
    } else {
        printf("[PARSER] No initializer, defaulting to 0\n");
    }
    
    if (!match(TK_SEMICOLON)) {
        printf("[PARSER ERROR] Expected ';' after variable declaration\n");
        debugToken("Got token instead", current);
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    printf("[PARSER] Variable declaration parsed successfully\n");
    return node;
}

static ASTNode* parsePrint() {
    debugToken("parsePrint - expecting '('", current);
    
    if (!match(TK_LPAREN)) {
        printf("[PARSER ERROR] Expected '(' after print\n");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_PRINT);
    if (!node) return NULL;
    
    printf("[PARSER] Found '(', parsing expression\n");
    node->left = parseExpression();
    
    if (!match(TK_RPAREN)) {
        printf("[PARSER ERROR] Expected ')' after expression\n");
        debugToken("Got token instead", current);
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        printf("[PARSER ERROR] Expected ';' after print statement\n");
        debugToken("Got token instead", current);
        free(node);
        return NULL;
    }
    
    printf("[PARSER] Print statement parsed successfully\n");
    return node;
}

static ASTNode* parseBlock() {
    debugToken("parseBlock - expecting '{'", current);
    
    if (!match(TK_LBRACE)) {
        printf("[PARSER ERROR] Expected '{' for block\n");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_BLOCK);
    if (!node) return NULL;
    
    printf("[PARSER] Found '{', parsing block statements\n");
    
    // Simple implementation: parse one statement for now
    // In a full implementation, you'd parse multiple statements
    node->left = parseStatement();
    
    if (!match(TK_RBRACE)) {
        printf("[PARSER ERROR] Expected '}' after block\n");
        debugToken("Got token instead", current);
        free(node);
        return NULL;
    }
    
    printf("[PARSER] Block parsed successfully\n");
    return node;
}

static ASTNode* parseAssignment(char* name) {
    printf("[PARSER] Parsing assignment to %s\n", name);
    
    if (!match(TK_ASSIGN)) {
        printf("[PARSER ERROR] Expected '=' after identifier\n");
        free(name);
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_ASSIGN);
    if (!node) {
        free(name);
        return NULL;
    }
    
    node->data.name = name;
    node->left = parseExpression();
    
    if (!match(TK_SEMICOLON)) {
        printf("[PARSER ERROR] Expected ';' after assignment\n");
        debugToken("Got token instead", current);
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    printf("[PARSER] Assignment parsed successfully\n");
    return node;
}

static ASTNode* parseStatement() {
    debugToken("parseStatement - starting", current);
    
    if (match(TK_VAR)) {
        printf("[PARSER] Found 'var' keyword\n");
        return parseVarDecl();
    }
    
    if (match(TK_PRINT)) {
        printf("[PARSER] Found 'print' keyword\n");
        return parsePrint();
    }
    
    if (match(TK_IMPORT)) {
        printf("[PARSER] Found 'import' keyword\n");
        return parseImportStatement();
    }
    
    if (match(TK_LBRACE)) {
        printf("[PARSER] Found '{' for block\n");
        // We already consumed the '{', so we need to backtrack
        // For simplicity, we'll handle blocks differently
        // Return a simple block node for now
        ASTNode* node = newNode(NODE_BLOCK);
        if (node) {
            printf("[PARSER] Created block node\n");
        }
        return node;
    }
    
    // Assignment or expression statement
    if (current.kind == TK_IDENT) {
        char* name = str_copy(current.value.str_val);
        debugToken("Found identifier for possible assignment", current);
        advance(); // Consume identifier
        
        if (current.kind == TK_ASSIGN) {
            return parseAssignment(name);
        }
        
        // Not an assignment, free the name and error
        free(name);
        printf("[PARSER ERROR] Identifier without assignment not supported\n");
        return NULL;
    }
    
    printf("[PARSER ERROR] Expected statement\n");
    debugToken("Got token instead", current);
    return NULL;
}

// ======================================================
// [SECTION] MAIN PARSER FUNCTION
// ======================================================
ASTNode** parse(const char* source, int* count) {
    printf("\n[PARSER] ===== Starting parse =====\n");
    initLexer(source);
    advance(); // Load first token
    
    int capacity = 10;
    ASTNode** nodes = malloc(capacity * sizeof(ASTNode*));
    *count = 0;
    
    if (!nodes) {
        printf("[PARSER ERROR] Memory allocation failed\n");
        return NULL;
    }
    
    while (current.kind != TK_EOF) {
        debugToken("Top of parse loop", current);
        
        if (*count >= capacity) {
            capacity *= 2;
            ASTNode** new_nodes = realloc(nodes, capacity * sizeof(ASTNode*));
            if (!new_nodes) {
                printf("[PARSER ERROR] Memory reallocation failed\n");
                break;
            }
            nodes = new_nodes;
        }
        
        ASTNode* node = parseStatement();
        if (node) {
            nodes[(*count)++] = node;
            printf("[PARSER] Added statement %d, type: %d\n", *count, node->type);
        } else {
            printf("[PARSER WARNING] Failed to parse statement, skipping\n");
            // Skip to next potential statement boundary
            while (current.kind != TK_SEMICOLON && current.kind != TK_EOF) {
                advance();
            }
            if (current.kind == TK_SEMICOLON) {
                advance(); // Skip the semicolon
            }
        }
    }
    
    printf("[PARSER] ===== Parse complete: %d statements =====\n\n", *count);
    return nodes;
}