/*
 * SwiftVelox 4.0.0 - Langage de programmation moderne
 * Fichier unique complet avec corrections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

// ===== CONFIGURATION =====
#define VERSION "4.0.0"
#define LIB_PATH "/usr/local/lib/swiftvelox/"
#define MAX_LINE_LENGTH 4096

// ===== COULEURS TERMINAL =====
#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[0;33m"
#define BLUE    "\033[0;34m"
#define CYAN    "\033[0;36m"
#define NC      "\033[0m"

// ===== TYPES DE BASE =====
typedef enum {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, 
    VAL_STRING, VAL_FUNCTION, VAL_NATIVE, VAL_RETURN_SIG,
    VAL_ARRAY, VAL_OBJECT
} ValueType;

// ===== TOKENS SIMPLIFIÉS =====
typedef enum {
    // Keywords de base
    TK_FN, TK_LET, TK_VAR, TK_CONST,
    TK_IF, TK_ELSE, 
    TK_WHILE, TK_FOR,
    TK_RETURN,
    TK_TRUE, TK_FALSE, TK_NIL,
    
    // Literals
    TK_IDENTIFIER, TK_NUMBER, TK_STRING_LIT,
    
    // Operators
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH,
    TK_EQ, TK_EQEQ, TK_BANGEQ,
    TK_LT, TK_GT, TK_LTEQ, TK_GTEQ,
    TK_AND, TK_OR, TK_BANG,
    
    // Punctuation
    TK_LPAREN, TK_RPAREN,
    TK_LBRACE, TK_RBRACE,
    TK_SEMICOLON, TK_COMMA, TK_DOT,
    
    TK_EOF, TK_ERROR
} TokenType;

// ===== STRUCTURES =====
typedef struct Token {
    TokenType type;
    const char* start;
    int length;
    int line;
    union { 
        int64_t i; 
        double d; 
        char* s;
    };
} Token;

typedef struct Scanner {
    const char* start;
    const char* current;
    int line;
    int col;
} Scanner;

typedef struct ASTNode ASTNode;
typedef struct Environment Environment;
typedef struct Value Value;

// Types de nœuds AST
typedef enum {
    NODE_PROGRAM, NODE_BLOCK, NODE_VAR_DECL,
    NODE_FUNCTION, NODE_IF, NODE_WHILE, NODE_RETURN,
    NODE_BINARY, NODE_UNARY, NODE_ASSIGN,
    NODE_LITERAL, NODE_IDENTIFIER,
    NODE_CALL, NODE_EXPR_STMT
} NodeType;

// Structure Value
struct Value {
    ValueType type;
    union {
        int boolean;
        int64_t integer;
        double number;
        char* string;
        struct {
            Value* items;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            Value* values;
            int count;
            int capacity;
        } object;
        struct {
            Value (*fn)(Value*, int, Environment*);
        } native;
        struct {
            ASTNode* declaration;
            Environment* closure;
        } function;
    };
};

// Structure ASTNode
struct ASTNode {
    NodeType type;
    Token token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int child_count;
    ValueType inferred_type;
};

// Environnement
struct Environment {
    struct Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
};

// ===== VARIABLES GLOBALES =====
Scanner scanner;
Token current_token;
Token previous_token;
int had_error = 0;
int panic_mode = 0;
Environment* global_env = NULL;

// ===== FONCTIONS UTILITAIRES =====
void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, RED "[ERROR]" NC " ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, RED "[ERROR]" NC " ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void log_success(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, GREEN "[SUCCESS]" NC " ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void log_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, YELLOW "[WARNING]" NC " ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, BLUE "[INFO]" NC " ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

// Helper pour strdup
char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

// ===== FONCTIONS VALUE =====
Value make_number(double value) {
    Value v;
    if (floor(value) == value && value <= 9223372036854775807.0 && value >= -9223372036854775808.0) {
        v.type = VAL_INT;
        v.integer = (int64_t)value;
    } else {
        v.type = VAL_FLOAT;
        v.number = value;
    }
    return v;
}

Value make_string(const char* str) {
    Value v;
    v.type = VAL_STRING;
    v.string = my_strdup(str ? str : "");
    return v;
}

Value make_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.boolean = b ? 1 : 0;
    return v;
}

Value make_nil() {
    Value v;
    v.type = VAL_NIL;
    return v;
}

// ===== LEXER =====
void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.col = 1;
    had_error = 0;
    panic_mode = 0;
}

int is_at_end(void) {
    return *scanner.current == '\0';
}

char advance() {
    scanner.current++;
    scanner.col++;
    return scanner.current[-1];
}

char peek() {
    return *scanner.current;
}

char peek_next() {
    if (is_at_end()) return '\0';
    return scanner.current[1];
}

int match_char(char expected) {
    if (is_at_end()) return 0;
    if (*scanner.current != expected) return 0;
    scanner.current++;
    scanner.col++;
    return 1;
}

Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    token.col = scanner.col - token.length;
    return token;
}

Token error_token(const char* message) {
    Token token;
    token.type = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    token.col = scanner.col;
    return token;
}

void skip_whitespace() {
    while (1) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t') {
            advance();
        } else if (c == '\n') {
            scanner.line++;
            scanner.col = 1;
            advance();
        } else if (c == '/') {
            if (peek_next() == '/') {
                // Commentaire ligne
                while (peek() != '\n' && !is_at_end()) advance();
            } else if (peek_next() == '*') {
                // Commentaire multi-ligne
                advance(); // /
                advance(); // *
                while (!(peek() == '*' && peek_next() == '/') && !is_at_end()) {
                    if (peek() == '\n') {
                        scanner.line++;
                        scanner.col = 1;
                    }
                    advance();
                }
                if (!is_at_end()) {
                    advance(); // *
                    advance(); // /
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

Token string_literal() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            scanner.line++;
            scanner.col = 1;
        }
        advance();
    }
    
    if (is_at_end()) {
        return error_token("Chaîne non terminée");
    }
    
    advance(); // closing "
    
    Token token = make_token(TK_STRING_LIT);
    int length = token.length - 2;
    char* str = malloc(length + 1);
    if (length > 0) {
        strncpy(str, token.start + 1, length);
    }
    str[length] = '\0';
    token.s = str;
    
    return token;
}

Token number_literal() {
    while (isdigit(peek())) advance();
    
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek())) advance();
    }
    
    Token token = make_token(TK_NUMBER);
    
    char* num_str = malloc(token.length + 1);
    strncpy(num_str, token.start, token.length);
    num_str[token.length] = '\0';
    
    if (strchr(num_str, '.')) {
        token.d = strtod(num_str, NULL);
    } else {
        token.i = strtoll(num_str, NULL, 10);
    }
    
    free(num_str);
    return token;
}

int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int is_alphanumeric(char c) {
    return is_alpha(c) || isdigit(c);
}

Token identifier() {
    while (is_alphanumeric(peek())) advance();
    
    Token token = make_token(TK_IDENTIFIER);
    
    // Vérifier les mots-clés
    char* ident = malloc(token.length + 1);
    strncpy(ident, token.start, token.length);
    ident[token.length] = '\0';
    
    // Mots-clés
    if (strcmp(ident, "fn") == 0) token.type = TK_FN;
    else if (strcmp(ident, "let") == 0) token.type = TK_LET;
    else if (strcmp(ident, "var") == 0) token.type = TK_VAR;
    else if (strcmp(ident, "const") == 0) token.type = TK_CONST;
    else if (strcmp(ident, "if") == 0) token.type = TK_IF;
    else if (strcmp(ident, "else") == 0) token.type = TK_ELSE;
    else if (strcmp(ident, "while") == 0) token.type = TK_WHILE;
    else if (strcmp(ident, "for") == 0) token.type = TK_FOR;
    else if (strcmp(ident, "return") == 0) token.type = TK_RETURN;
    else if (strcmp(ident, "true") == 0) token.type = TK_TRUE;
    else if (strcmp(ident, "false") == 0) token.type = TK_FALSE;
    else if (strcmp(ident, "nil") == 0) token.type = TK_NIL;
    else {
        token.s = ident;
    }
    
    if (token.type == TK_IDENTIFIER) {
        token.s = ident;
    } else {
        free(ident);
    }
    
    return token;
}

Token scan_token() {
    skip_whitespace();
    
    scanner.start = scanner.current;
    
    if (is_at_end()) return make_token(TK_EOF);
    
    char c = advance();
    
    if (is_alpha(c)) return identifier();
    if (isdigit(c)) return number_literal();
    
    switch (c) {
        case '(': return make_token(TK_LPAREN);
        case ')': return make_token(TK_RPAREN);
        case '{': return make_token(TK_LBRACE);
        case '}': return make_token(TK_RBRACE);
        case ';': return make_token(TK_SEMICOLON);
        case ',': return make_token(TK_COMMA);
        case '.': return make_token(TK_DOT);
        case '-': return make_token(TK_MINUS);
        case '+': return make_token(TK_PLUS);
        case '/': return make_token(TK_SLASH);
        case '*': return make_token(TK_STAR);
        case '!':
            if (match_char('=')) return make_token(TK_BANGEQ);
            return make_token(TK_BANG);
        case '=':
            if (match_char('=')) return make_token(TK_EQEQ);
            return make_token(TK_EQ);
        case '<':
            if (match_char('=')) return make_token(TK_LTEQ);
            return make_token(TK_LT);
        case '>':
            if (match_char('=')) return make_token(TK_GTEQ);
            return make_token(TK_GT);
        case '&':
            if (match_char('&')) return make_token(TK_AND);
            break;
        case '|':
            if (match_char('|')) return make_token(TK_OR);
            break;
        case '"': return string_literal();
    }
    
    return error_token("Caractère inattendu");
}

void next_token() {
    previous_token = current_token;
    
    if (panic_mode) {
        panic_mode = 0;
        // Skip tokens until we find a statement boundary
        while (current_token.type != TK_SEMICOLON && 
               current_token.type != TK_RBRACE && 
               current_token.type != TK_EOF) {
            current_token = scan_token();
            if (current_token.type == TK_ERROR) break;
        }
    }
    
    current_token = scan_token();
    
    if (current_token.type == TK_ERROR) {
        had_error = 1;
        panic_mode = 1;
    }
}

int match(TokenType type) {
    if (current_token.type == type) {
        next_token();
        return 1;
    }
    return 0;
}

void syntax_error(Token token, const char* message) {
    if (panic_mode) return;
    panic_mode = 1;
    
    fprintf(stderr, RED "[SYNTAX ERROR]" NC " Ligne %d:%d: %s\n", 
            token.line, token.col, message);
    had_error = 1;
}

void consume(TokenType type, const char* message) {
    if (current_token.type == type) {
        next_token();
        return;
    }
    
    syntax_error(current_token, message);
}

// ===== PARSER =====
ASTNode* new_node(NodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type;
    node->token = current_token;  // Stocke le token courant
    node->left = node->right = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->inferred_type = VAL_NIL;
    return node;
}

// Déclarations anticipées
ASTNode* expression(void);
ASTNode* statement(void);
ASTNode* declaration(void);

// Primary expressions
ASTNode* primary(void) {
    if (match(TK_NUMBER) || match(TK_STRING_LIT) || 
        match(TK_TRUE) || match(TK_FALSE) || match(TK_NIL)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;  // Utilise le token précédent (celui qui a matché)
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
ASTNode* finish_call(ASTNode* callee) {
    ASTNode* n = new_node(NODE_CALL);
    n->left = callee;
    n->child_count = 0;
    n->children = NULL;
    
    if (!match(TK_RPAREN)) {
        do {
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = expression();
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    return n;
}

ASTNode* call(void) {
    ASTNode* expr = primary();
    
    if (match(TK_LPAREN)) {
        expr = finish_call(expr);
    }
    
    return expr;
}

// Unary operators
ASTNode* unary(void) {
    if (match(TK_MINUS) || match(TK_BANG)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = unary();
        return n;
    }
    
    return call();
}

// Multiplication and division
ASTNode* factor(void) {
    ASTNode* expr = unary();
    
    while (match(TK_STAR) || match(TK_SLASH)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = unary();
        expr = n;
    }
    
    return expr;
}

// Addition and subtraction
ASTNode* term(void) {
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
ASTNode* comparison(void) {
    ASTNode* expr = term();
    
    while (match(TK_LT) || match(TK_GT) || match(TK_LTEQ) || match(TK_GTEQ)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = term();
        expr = n;
    }
    
    return expr;
}

// Equality
ASTNode* equality(void) {
    ASTNode* expr = comparison();
    
    while (match(TK_EQEQ) || match(TK_BANGEQ)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = comparison();
        expr = n;
    }
    
    return expr;
}

// Assignment
ASTNode* assignment(void) {
    ASTNode* expr = equality();
    
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
ASTNode* expression(void) {
    return assignment();
}

// Expression statement
ASTNode* expression_statement(void) {
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "';' attendu");
    
    ASTNode* n = new_node(NODE_EXPR_STMT);
    n->left = expr;
    return n;
}

// Variable declaration - CORRECTION IMPORTANTE ICI
ASTNode* var_declaration(void) {
    // On a déjà consommé TK_LET/TK_VAR/TK_CONST
    TokenType decl_type = previous_token.type;
    
    ASTNode* n = new_node(NODE_VAR_DECL);
    n->token = previous_token;  // Stocke le token let/var/const
    
    // Maintenant on s'attend à un identifiant
    if (current_token.type != TK_IDENTIFIER) {
        syntax_error(current_token, "Nom de variable attendu");
        return n;
    }
    
    // Consommer l'identifiant
    n->token = current_token;  // Maintenant stocke le nom de la variable
    next_token();
    
    // Vérifier s'il y a une initialisation
    if (match(TK_EQ)) {
        n->right = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Block statement
ASTNode* block_statement(void) {
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
ASTNode* if_statement(void) {
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
ASTNode* while_statement(void) {
    ASTNode* n = new_node(NODE_WHILE);
    
    consume(TK_LPAREN, "'(' attendu après 'while'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    
    n->right = statement();
    return n;
}

// Return statement
ASTNode* return_statement(void) {
    ASTNode* n = new_node(NODE_RETURN);
    
    if (current_token.type != TK_SEMICOLON) {
        n->left = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Statement
ASTNode* statement(void) {
    if (match(TK_IF)) return if_statement();
    if (match(TK_WHILE)) return while_statement();
    if (match(TK_RETURN)) return return_statement();
    if (match(TK_LBRACE)) return block_statement();
    
    return expression_statement();
}

// Declaration (top-level)
ASTNode* declaration(void) {
    if (match(TK_LET) || match(TK_CONST) || match(TK_VAR)) {
        return var_declaration();
    }
    if (match(TK_FN)) {
        // Simple function stub for now
        ASTNode* n = new_node(NODE_FUNCTION);
        consume(TK_IDENTIFIER, "Nom de fonction attendu");
        n->token = previous_token;
        consume(TK_LPAREN, "'(' attendu");
        consume(TK_RPAREN, "')' attendu");
        n->left = block_statement();
        return n;
    }
    
    return statement();
}

// Parse program
ASTNode* parse(const char* source) {
    init_scanner(source);
    next_token();  // Récupère le premier token
    
    ASTNode* program = new_node(NODE_PROGRAM);
    
    while (!is_at_end() && current_token.type != TK_EOF) {
        program->children = realloc(program->children, sizeof(ASTNode*) * (program->child_count + 1));
        program->children[program->child_count++] = declaration();
    }
    
    return program;
}

// ===== ENVIRONMENT FUNCTIONS =====
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
    env->names[env->count] = my_strdup(name);
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

// ===== EVALUATION FUNCTIONS =====
Value eval(void* node_ptr, Environment* env);

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

Value eval_var_decl(ASTNode* node, Environment* env) {
    Value value = make_nil();
    if (node->right) {
        value = eval(node->right, env);
    }
    env_define(env, node->token.s, value);
    return value;
}

// Main evaluation function
Value eval(void* node_ptr, Environment* env) {
    if (!node_ptr) return make_nil();
    
    ASTNode* node = (ASTNode*)node_ptr;
    
    switch (node->type) {
        case NODE_LITERAL:
            if (node->token.type == TK_NUMBER) {
                if (strchr(node->token.start, '.')) {
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
            
        case NODE_VAR_DECL:
            return eval_var_decl(node, env);
            
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

// ===== NATIVE FUNCTIONS =====
Value native_print(Value* args, int count, Environment* env) {
    (void)env;
    for (int i = 0; i < count; i++) {
        Value v = args[i];
        switch (v.type) {
            case VAL_STRING: printf("%s", v.string); break;
            case VAL_INT: printf("%lld", v.integer); break;
            case VAL_FLOAT: printf("%g", v.number); break;
            case VAL_BOOL: printf("%s", v.boolean ? "true" : "false"); break;
            case VAL_NIL: printf("nil"); break;
            default: printf("[object]"); break;
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return make_nil();
}

Value native_http_run(Value* args, int count, Environment* env) {
    (void)env;
    printf(BLUE "[INFO]" NC " Serveur HTTP (en développement)\n");
    printf(BLUE "[INFO]" NC " Port configuré: %lld\n", count > 0 ? args[0].integer : 8080);
    printf(YELLOW "[WARNING]" NC " Fonctionnalité en cours d'implémentation\n");
    return make_nil();
}

Value native_time(Value* args, int count, Environment* env) {
    (void)args; (void)count; (void)env;
    return make_number((double)time(NULL));
}

Value native_random(Value* args, int count, Environment* env) {
    (void)args; (void)count; (void)env;
    srand(time(NULL));
    return make_number((double)rand() / RAND_MAX);
}

void register_natives(Environment* env) {
    Value native_val;
    
    native_val.type = VAL_NATIVE;
    native_val.native.fn = native_print;
    env_define(env, "print", native_val);
    
    native_val.native.fn = native_http_run;
    env_define(env, "http.run", native_val);
    
    native_val.native.fn = native_time;
    env_define(env, "time", native_val);
    
    native_val.native.fn = native_random;
    env_define(env, "random", native_val);
}

// ===== MAIN FUNCTION =====
int main(int argc, char** argv) {
    // Initialize global environment
    global_env = new_environment(NULL);
    register_natives(global_env);
    
    // Command line interface
    if (argc < 2) {
        printf(CYAN "⚡ SwiftVelox v%s - Langage Moderne\n" NC, VERSION);
        printf("\nUsage:\n");
        printf("  swiftvelox run <fichier.svx>     Exécuter un script\n");
        printf("  swiftvelox http --port <port>    Démarrer un serveur HTTP\n");
        printf("  swiftvelox repl                  Mode interactif REPL\n");
        printf("  swiftvelox test [fichier]        Exécuter les tests\n");
        printf("  swiftvelox fmt <fichier>         Formatter le code\n");
        printf("\nExemples:\n");
        printf("  swiftvelox run mon_script.svx\n");
        printf("  swiftvelox http --port 3000\n");
        printf("  swiftvelox repl\n");
        return 0;
    }
    
    if (strcmp(argv[1], "run") == 0 && argc >= 3) {
        // Run script
        FILE* f = fopen(argv[2], "rb");
        if (!f) {
            log_error("Fichier non trouvé: %s", argv[2]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        fclose(f);
        source[size] = '\0';
        
        log_info("Exécution de %s...", argv[2]);
        
        ASTNode* program = parse(source);
        if (!had_error) {
            eval((void*)program, global_env);
        }
        
        free(source);
        log_success("Exécution terminée");
        return 0;
    }
    else if (strcmp(argv[1], "repl") == 0) {
        // REPL mode
        printf(CYAN "💻 SwiftVelox REPL v%s\n" NC, VERSION);
        printf("Tapez 'exit' pour quitter\n\n");
        
        char line[4096];
        while (1) {
            printf(CYAN ">>> " NC);
            if (!fgets(line, sizeof(line), stdin)) break;
            
            line[strcspn(line, "\n")] = 0;
            
            if (strcmp(line, "exit") == 0) break;
            if (strcmp(line, "help") == 0) {
                printf("Commandes REPL:\n");
                printf("  exit     - Quitter le REPL\n");
                printf("  help     - Afficher cette aide\n");
                printf("  clear    - Effacer l'écran\n");
                continue;
            }
            if (strcmp(line, "clear") == 0) {
                printf("\033[2J\033[1;1H");
                continue;
            }
            
            if (strlen(line) == 0) continue;
            
            ASTNode* program = parse(line);
            if (!had_error) {
                Value result = eval((void*)program, global_env);
                
                // Print result if not nil
                if (result.type != VAL_NIL && result.type != VAL_RETURN_SIG) {
                    printf(GREEN "=> " NC);
                    Value* print_args = &result;
                    native_print(print_args, 1, global_env);
                }
            }
            had_error = 0; // Reset pour la prochaine commande
        }
        
        printf("\n👋 Au revoir !\n");
        return 0;
    }
    else {
        // Try to run as file
        FILE* f = fopen(argv[1], "rb");
        if (f) {
            fclose(f);
            // It's a file, run it
            char* new_argv[3] = {argv[0], "run", argv[1]};
            return main(3, new_argv);
        }
        
        log_error("Commande inconnue: %s", argv[1]);
        printf("Utilisez 'swiftvelox' sans arguments pour l'aide\n");
        return 1;
    }
    
    return 0;
}
