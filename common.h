#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>

// ======================================================
// [SECTION] COULEURS POUR LE TERMINAL
// ======================================================
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"
#define RESET   "\033[0m"

// ======================================================
// [SECTION] TOKEN DEFINITIONS
// ======================================================
typedef enum {
    // Literals
    TK_INT, TK_FLOAT, TK_STRING, TK_TRUE, TK_FALSE, TK_NULL,
    // Identifiers
    TK_IDENT,
    // Operators
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_ASSIGN, TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_AND, TK_OR, TK_NOT,
    // Punctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_PERIOD, TK_LBRACKET, TK_RBRACKET,
    // Keywords
    TK_VAR, TK_NIP, TK_SIM, TK_NUUM, TK_CONST, TK_PRINT, TK_IF, TK_ELSE,
    TK_WHILE, TK_FOR, TK_FUNC, TK_RETURN, TK_IMPORT, TK_CLASS, TK_MAIN,
    TK_TYPELOCK, TK_ZIS, TK_SIZEOF, TK_JSON, TK_TYPE,
    // Type keywords
    TK_TYPE_INT, TK_TYPE_FLOAT, TK_TYPE_STR, TK_TYPE_BOOL, TK_TYPE_CHAR,
    // Special
    TK_EOF, TK_ERROR
} TokenKind;

// Token structure
typedef struct {
    TokenKind kind;
    const char* start;
    int length;
    int line;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        char char_val;
    } value;
} Token;

// Keyword mapping
typedef struct {
    const char* keyword;
    TokenKind kind;
} Keyword;

static const Keyword keywords[] = {
    // Variables types
    {"var", TK_VAR},
    {"nip", TK_NIP},      // immutable variable
    {"sim", TK_SIM},      // simple/constant
    {"nuum", TK_NUUM},    // number only
    {"const", TK_CONST},
    
    // Control flow
    {"if", TK_IF},
    {"else", TK_ELSE},
    {"while", TK_WHILE},
    {"for", TK_FOR},
    
    // Functions
    {"func", TK_FUNC},
    {"return", TK_RETURN},
    {"main", TK_MAIN},
    
    // I/O
    {"print", TK_PRINT},
    
    // Modules
    {"import", TK_IMPORT},
    {"json", TK_JSON},
    
    // Literals
    {"true", TK_TRUE},
    {"false", TK_FALSE},
    {"null", TK_NULL},
    
    // Types
    {"int", TK_TYPE_INT},
    {"float", TK_TYPE_FLOAT},
    {"string", TK_TYPE_STR},
    {"bool", TK_TYPE_BOOL},
    {"char", TK_TYPE_CHAR},
    {"type", TK_TYPE},
    
    // OOP
    {"class", TK_CLASS},
    {"typelock", TK_TYPELOCK},
    
    // Size/type operations
    {"zis", TK_ZIS},
    {"sizeof", TK_SIZEOF},
    
    // Mots-clés supplémentaires
    {"from", TK_IDENT},
    
    // End marker
    {NULL, TK_ERROR}
};

// ======================================================
// [SECTION] AST NODE DEFINITIONS
// ======================================================
typedef enum {
    // Expressions
    NODE_INT,
    NODE_FLOAT,
    NODE_STRING,
    NODE_CHAR,
    NODE_IDENT,
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_INDEX,
    NODE_MEMBER,
    NODE_NULL,
    NODE_BOOL,
    
    // Statements
    NODE_VAR,
    NODE_NIP,
    NODE_SIM,
    NODE_NUUM,
    NODE_ASSIGN,
    NODE_PRINT,
    NODE_IMPORT,
    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    
    // Functions & Classes
    NODE_FUNC,
    NODE_CLASS,
    NODE_MAIN,
    
    // Type operations
    NODE_ZIS,
    NODE_SIZEOF,
    
    // JSON
    NODE_JSON_OBJ,
    NODE_JSON_ARR,
    NODE_JSON_PAIR,
    
} NodeType;

// JSON Value types
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_INT,
    JSON_FLOAT,
    JSON_STRING,
    JSON_OBJECT,
    JSON_ARRAY
} JsonType;

typedef struct JsonValue JsonValue;
typedef struct JsonPair JsonPair;

struct JsonValue {
    JsonType type;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
        struct {
            JsonPair* pairs;
            int count;
        } obj;
        struct {
            JsonValue* values;
            int count;
        } arr;
    } data;
};

struct JsonPair {
    char* key;
    JsonValue* value;
    JsonPair* next;
};

typedef struct ASTNode {
    NodeType type;
    struct ASTNode* left;
    struct ASTNode* right;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        char char_val;
        bool bool_val;
        char* name;
        char** imports;
        JsonValue* json_val;
        struct {
            char* name;
            struct ASTNode* params;
            struct ASTNode* body;
        } func;
        struct {
            char* name;
            struct ASTNode* fields;
        } class;
        struct {
            struct ASTNode* expr;
            char* type_name;
        } size_op;
    } data;
    int import_count;
    char* from_module;
} ASTNode;

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static inline char* str_copy(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dest = malloc(len + 1);
    if (dest) strcpy(dest, src);
    return dest;
}

static inline char* str_ncopy(const char* src, int n) {
    if (!src || n <= 0) return NULL;
    char* dest = malloc(n + 1);
    if (dest) {
        strncpy(dest, src, n);
        dest[n] = '\0';
    }
    return dest;
}

#endif
