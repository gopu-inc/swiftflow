#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// ======================================================
// [SECTION] COULEURS POUR LE TERMINAL
// ======================================================
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"

// ======================================================
// [SECTION] TOKEN DEFINITIONS - COMPLET
// ======================================================
typedef enum {
    // Literals
    TK_INT, TK_FLOAT, TK_STRING, TK_TRUE, TK_FALSE,
    TK_NULL, TK_UNDEFINED,
    
    // Identifiers
    TK_IDENT,
    TK_AS,
    
    
    // Operators
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_POW, TK_CONCAT,
    
    // Assignment operators
    TK_ASSIGN, TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_PLUS_ASSIGN, TK_MINUS_ASSIGN, TK_MULT_ASSIGN, 
    TK_DIV_ASSIGN, TK_MOD_ASSIGN,
    
    // Logical operators
    TK_AND, TK_OR, TK_NOT,
    
    // Bitwise operators
    TK_BIT_AND, TK_BIT_OR, TK_BIT_XOR, TK_BIT_NOT,
    TK_SHL, TK_SHR,
    
    // Special operators
    TK_RARROW, TK_DARROW, TK_LDARROW, TK_RDARROW,
    TK_SPACESHIP, TK_ELLIPSIS, TK_RANGE,
    TK_QUESTION, TK_SCOPE,
    
    // New operators
    TK_IN, TK_IS, TK_ISNOT,
    
    // Punctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_PERIOD,
    TK_AT, TK_HASH, TK_DOLLAR,
    
    // Keywords
    TK_VAR, TK_LET, TK_CONST,
    TK_NET, TK_CLOG, TK_DOS, TK_SEL,
    TK_THEN,
    
    // Control flow
    TK_IF, TK_ELSE, TK_ELIF,
    TK_WHILE, TK_FOR, TK_DO,
    TK_SWITCH, TK_CASE, TK_DEFAULT,
    TK_BREAK, TK_CONTINUE, TK_RETURN,
    
    // Functions & Modules
    TK_FUNC, TK_IMPORT, TK_EXPORT, TK_FROM,
    TK_CLASS, TK_STRUCT, TK_ENUM, TK_TYPEDEF,
    TK_TYPELOCK, TK_NAMESPACE,
    
    // Types
    TK_TYPE_INT, TK_TYPE_FLOAT, TK_TYPE_STR,
    TK_TYPE_BOOL, TK_TYPE_CHAR, TK_TYPE_VOID,
    TK_TYPE_ANY, TK_TYPE_AUTO,
    TK_TYPE_NET, TK_TYPE_CLOG, TK_TYPE_DOS,
    TK_TYPE_SEL,
    
    // Memory & Size
    TK_SIZEOF, TK_SIZE, TK_SIZ,
    TK_NEW, TK_DELETE, TK_FREE,
    
    // Debug & DB
    TK_DB, TK_DBVAR, TK_PRINT_DB,
    
    // I/O
    TK_PRINT, TK_WELD,  // weld remplace input
    
    // New keywords (tous)
    TK_PASS, TK_GLOBAL, TK_TRY, TK_LAMBDA,
    TK_BDD, TK_DEF, TK_TYPE, TK_RAISE,
    TK_YIELD, TK_WITH, TK_WRITE, TK_READ, 
    TK_LEARN, TK_NONLOCAL, TK_LOCK, TK_APPEND,
    
    // Special
    TK_MAIN, TK_THIS, TK_SELF,
    
    // End markers
    TK_EOF, TK_ERROR
} TokenKind;

// Token structure
typedef struct {
    TokenKind kind;
    const char* start;
    int length;
    int line;
    int column;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
    } value;
} Token;

// Keyword mapping
typedef struct {
    const char* keyword;
    TokenKind kind;
} Keyword;

static const Keyword keywords[] = {
    // Variables
    {"var", TK_VAR}, {"let", TK_LET}, {"const", TK_CONST},
    {"net", TK_NET}, {"clog", TK_CLOG}, {"dos", TK_DOS}, {"sel", TK_SEL},
    
    // Control flow
    {"if", TK_IF}, {"else", TK_ELSE}, {"elif", TK_ELIF},
    {"while", TK_WHILE}, {"for", TK_FOR}, {"do", TK_DO},
    {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    {"break", TK_BREAK}, {"continue", TK_CONTINUE}, {"return", TK_RETURN},
    {"then", TK_THEN},
    
    // Functions
    {"func", TK_FUNC}, {"import", TK_IMPORT}, {"export", TK_EXPORT},
    {"from", TK_FROM}, {"class", TK_CLASS}, {"struct", TK_STRUCT},
    {"enum", TK_ENUM}, {"typedef", TK_TYPEDEF}, {"typelock", TK_TYPELOCK},
    {"namespace", TK_NAMESPACE},
    
    // Types
    {"int", TK_TYPE_INT}, {"float", TK_TYPE_FLOAT}, {"string", TK_TYPE_STR},
    {"bool", TK_TYPE_BOOL}, {"char", TK_TYPE_CHAR}, {"void", TK_TYPE_VOID},
    {"any", TK_TYPE_ANY}, {"auto", TK_TYPE_AUTO},
    {"netvar", TK_TYPE_NET}, {"clogvar", TK_TYPE_CLOG}, 
    {"dosvar", TK_TYPE_DOS}, {"selvar", TK_TYPE_SEL},
    
    // Memory
    {"sizeof", TK_SIZEOF}, {"size", TK_SIZE}, {"siz", TK_SIZ},
    {"new", TK_NEW}, {"delete", TK_DELETE}, {"free", TK_FREE},
    
    // Debug
    {"db", TK_DB}, {"dbvar", TK_DBVAR}, {"printdb", TK_PRINT_DB},
    
    // I/O
    {"print", TK_PRINT}, {"weld", TK_WELD},
    
    // New keywords (tous)
    {"pass", TK_PASS}, {"global", TK_GLOBAL}, {"try", TK_TRY},
    {"lambda", TK_LAMBDA}, {"bdd", TK_BDD}, {"def", TK_DEF},
    {"type", TK_TYPE}, {"raise", TK_RAISE}, {"yield", TK_YIELD},
    {"with", TK_WITH}, {"write", TK_WRITE}, {"read", TK_READ},
    {"learn", TK_LEARN}, {"nonlocal", TK_NONLOCAL}, {"lock", TK_LOCK},
    {"append", TK_APPEND},
    
    // Operators as keywords
    {"in", TK_IN}, {"is", TK_IS}, {"isnot", TK_ISNOT},
    
    // Special
    {"main", TK_MAIN}, {"this", TK_THIS}, {"self", TK_SELF},
    
    // Literals
    {"true", TK_TRUE}, {"false", TK_FALSE}, {"null", TK_NULL},
    {"undefined", TK_UNDEFINED},
    
    // End marker
    {NULL, TK_ERROR}
};

// ======================================================
// [SECTION] AST NODE DEFINITIONS - COMPLET
// ======================================================
typedef enum {
    // Expressions
    NODE_INT,
    NODE_FLOAT,
    NODE_STRING,
    NODE_BOOL,
    NODE_IDENT,
    NODE_NULL,
    NODE_UNDEFINED,
    NODE_LIST,
    NODE_FUNC,
    NODE_FUNC_CALL,

    
    // Operations
    NODE_BINARY,
    NODE_UNARY,
    NODE_ASSIGN,
    
    // Control flow
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    
    // Variables
    NODE_VAR_DECL,
    NODE_NET_DECL,
    NODE_CLOG_DECL,
    NODE_DOS_DECL,
    NODE_SEL_DECL,
    NODE_CONST_DECL,
    
    // Memory
    NODE_SIZEOF,
    
    // Modules
    NODE_IMPORT,
    NODE_EXPORT,
    
    // Debug
    NODE_DBVAR,
    
    // I/O
    NODE_PRINT,
    NODE_WELD,
    
    // New nodes (tous)
    NODE_PASS,
    NODE_GLOBAL,
    NODE_TRY,
    NODE_LAMBDA,
    NODE_WITH,
    NODE_YIELD,
    NODE_APPEND,
    NODE_WRITE,
    NODE_READ,
    NODE_LEARN,
    NODE_LOCK,
    
    // Blocks
    NODE_BLOCK,
    
    // Special
    NODE_MAIN,
    NODE_EMPTY
} NodeType;

// AST Node structure
typedef struct ASTNode {
    NodeType type;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* third;
    struct ASTNode* fourth;
    
    union {
        // Basic values
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
        
        // Identifiers
        char* name;
        
        // Types
        char* type_name;
        
        // Import/Export
        struct {
            char** modules;
            char* from_module;
            int module_count;
        } imports;
        
        // Export
        struct {
            char* symbol;
            char* alias;
        } export;
        
        // Size info
        struct {
            char* var_name;
            int size_bytes;
        } size_info;
        
        // For loop
        struct {
            struct ASTNode* init;
            struct ASTNode* condition;
            struct ASTNode* update;
            struct ASTNode* body;
        } loop;
        
        // Append operation
        struct {
            struct ASTNode* list;
            struct ASTNode* value;
        } append_op;
    } data;
    
    // Additional info
    int line;
    int column;
    TokenKind op_type;
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
