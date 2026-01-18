#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

// ======================================================
// [SECTION] LEXER STATE
// ======================================================
typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

static Lexer lexer;

// ======================================================
// [SECTION] LEXER UTILITIES
// ======================================================
void initLexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
}

static bool isAtEnd() { 
    return *lexer.current == '\0'; 
}

static char advance() { 
    lexer.current++;
    return lexer.current[-1];
}

static char peek() { 
    return *lexer.current; 
}

static char peekNext() { 
    if (isAtEnd()) return '\0';
    return lexer.current[1]; 
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*lexer.current != expected) return false;
    lexer.current++;
    return true;
}

// ======================================================
// [SECTION] TOKEN CREATION
// ======================================================
static Token makeToken(TokenKind kind) {
    Token token;
    token.kind = kind;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.kind = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    return token;
}

// ======================================================
// [SECTION] SKIP WHITESPACE & COMMENTS
// ======================================================
static void skipWhitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                lexer.line++;
                advance();
                break;
            case '#':
                while (peek() != '\n' && !isAtEnd()) advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if (peekNext() == '*') {
                    advance(); advance();
                    while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                        if (peek() == '\n') lexer.line++;
                        advance();
                    }
                    if (!isAtEnd()) {
                        advance(); advance();
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

// ======================================================
// [SECTION] CHAR LEXING
// ======================================================
static Token character() {
    advance();
    
    char c = advance();
    char value = c;
    
    if (c == '\\') {
        c = advance();
        switch (c) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '"': value = '"'; break;
            case '0': value = '\0'; break;
            default: value = c; break;
        }
    }
    
    if (peek() != '\'') {
        return errorToken("Unterminated character literal");
    }
    advance();
    
    Token token = makeToken(TK_CHAR);
    token.value.char_val = value;
    return token;
}

// ======================================================
// [SECTION] STRING LEXING
// ======================================================
static Token string(char quote_char) {
    while (peek() != quote_char && !isAtEnd()) {
        if (peek() == '\n') lexer.line++;
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case 'n': case 't': case 'r': case '\\': 
                case '"': case '\'': case '0':
                    advance();
                    break;
                case 'x':
                    advance();
                    if (isxdigit(peek()) && isxdigit(peekNext())) {
                        advance(); advance();
                    }
                    break;
                case 'u':
                    advance();
                    for (int i = 0; i < 4 && isxdigit(peek()); i++) {
                        advance();
                    }
                    break;
                default:
                    advance();
                    break;
            }
        } else {
            advance();
        }
    }
    
    if (isAtEnd()) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Unterminated string (started with '%c')", quote_char);
        return errorToken(error_msg);
    }
    
    advance();
    
    int length = (int)(lexer.current - lexer.start - 2);
    char* str = malloc(length + 1);
    if (str) {
        const char* src = lexer.start + 1;
        int dest_idx = 0;
        
        for (int i = 0; i < length; i++) {
            if (src[i] == '\\') {
                i++;
                if (i < length) {
                    switch (src[i]) {
                        case 'n': str[dest_idx++] = '\n'; break;
                        case 't': str[dest_idx++] = '\t'; break;
                        case 'r': str[dest_idx++] = '\r'; break;
                        case '\\': str[dest_idx++] = '\\'; break;
                        case '"': str[dest_idx++] = '"'; break;
                        case '\'': str[dest_idx++] = '\''; break;
                        case '0': str[dest_idx++] = '\0'; break;
                        case 'x':
                            if (i + 2 < length) {
                                char hex[3] = {src[i+1], src[i+2], '\0'};
                                int val;
                                sscanf(hex, "%x", &val);
                                str[dest_idx++] = (char)val;
                                i += 2;
                            }
                            break;
                        default: str[dest_idx++] = src[i]; break;
                    }
                }
            } else {
                str[dest_idx++] = src[i];
            }
        }
        str[dest_idx] = '\0';
    }
    
    Token token = makeToken(TK_STRING);
    token.value.str_val = str;
    return token;
}

// ======================================================
// [SECTION] NUMBER LEXING
// ======================================================
static Token number() {
    bool is_float = false;
    bool is_hex = false;
    
    if (peek() == '0' && (peekNext() == 'x' || peekNext() == 'X')) {
        is_hex = true;
        advance();
        advance();
        
        while (isxdigit(peek())) advance();
    } else {
        while (isdigit(peek())) advance();
        
        if (peek() == '.' && isdigit(peekNext())) {
            is_float = true;
            advance();
            while (isdigit(peek())) advance();
        }
        
        if (peek() == 'e' || peek() == 'E') {
            is_float = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (isdigit(peek())) advance();
        }
    }
    
    int length = (int)(lexer.current - lexer.start);
    char* num_str = malloc(length + 1);
    if (!num_str) return errorToken("Failed to allocate memory");
    
    strncpy(num_str, lexer.start, length);
    num_str[length] = '\0';
    
    Token token;
    if (is_float) {
        token = makeToken(TK_FLOAT);
        token.value.float_val = atof(num_str);
    } else if (is_hex) {
        token = makeToken(TK_INT);
        token.value.int_val = strtoll(num_str, NULL, 16);
    } else {
        token = makeToken(TK_INT);
        token.value.int_val = atoll(num_str);
    }
    
    free(num_str);
    return token;
}

// ======================================================
// [SECTION] IDENTIFIER & KEYWORD LEXING
// ======================================================
static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isAlphaNumeric(char c) {
    return isAlpha(c) || isdigit(c);
}

static Token identifier() {
    while (isAlphaNumeric(peek())) advance();
    
    int length = (int)(lexer.current - lexer.start);
    char* text = malloc(length + 1);
    if (text) {
        strncpy(text, lexer.start, length);
        text[length] = '\0';
    }
    
    if (text) {
        for (int i = 0; keywords[i].keyword != NULL; i++) {
            if (strcmp(text, keywords[i].keyword) == 0) {
                free(text);
                return makeToken(keywords[i].kind);
            }
        }
        
        if (strcmp(text, "null") == 0) {
            free(text);
            Token token = makeToken(TK_NULL);
            return token;
        }
        
        Token token = makeToken(TK_IDENT);
        token.value.str_val = text;
        return token;
    }
    
    free(text);
    return errorToken("Failed to allocate memory for identifier");
}

// ======================================================
// [SECTION] MAIN LEXER FUNCTION
// ======================================================
Token scanToken() {
    skipWhitespace();
    lexer.start = lexer.current;
    
    if (isAtEnd()) return makeToken(TK_EOF);
    
    char c = advance();
    
    switch (c) {
        case '(': return makeToken(TK_LPAREN);
        case ')': return makeToken(TK_RPAREN);
        case '{': return makeToken(TK_LBRACE);
        case '}': return makeToken(TK_RBRACE);
        case '[': return makeToken(TK_LBRACKET);
        case ']': return makeToken(TK_RBRACKET);
        case ',': return makeToken(TK_COMMA);
        case ';': return makeToken(TK_SEMICOLON);
        case ':': return makeToken(TK_COLON);
        case '.': return makeToken(TK_PERIOD);
        
        case '+': return makeToken(TK_PLUS);
        case '-': return makeToken(TK_MINUS);
        case '*': return makeToken(TK_MULT);
        case '/': return makeToken(TK_DIV);
        case '%': return makeToken(TK_MOD);
        
        case '=':
            if (match('=')) return makeToken(TK_EQ);
            return makeToken(TK_ASSIGN);
        case '!':
            if (match('=')) return makeToken(TK_NEQ);
            return makeToken(TK_NOT);
        case '<':
            if (match('=')) return makeToken(TK_LTE);
            return makeToken(TK_LT);
        case '>':
            if (match('=')) return makeToken(TK_GTE);
            return makeToken(TK_GT);
        
        case '&':
            if (match('&')) return makeToken(TK_AND);
            break;
        case '|':
            if (match('|')) return makeToken(TK_OR);
            break;
        
        case '"': return string('"');
        case '\'': return character();
    }
    
    if (isdigit(c)) return number();
    
    if (isAlpha(c)) return identifier();
    
    char error_msg[32];
    snprintf(error_msg, sizeof(error_msg), "Unexpected character: '%c'", c);
    return errorToken(error_msg);
}

// ======================================================
// [SECTION] HELPER FUNCTIONS FOR DEBUGGING
// ======================================================
const char* tokenKindToString(TokenKind kind) {
    switch (kind) {
        case TK_INT: return "TK_INT";
        case TK_FLOAT: return "TK_FLOAT";
        case TK_STRING: return "TK_STRING";
        case TK_CHAR: return "TK_CHAR";
        case TK_IDENT: return "TK_IDENT";
        case TK_NULL: return "TK_NULL";
        case TK_TRUE: return "TK_TRUE";
        case TK_FALSE: return "TK_FALSE";
        
        case TK_PLUS: return "TK_PLUS";
        case TK_MINUS: return "TK_MINUS";
        case TK_MULT: return "TK_MULT";
        case TK_DIV: return "TK_DIV";
        case TK_MOD: return "TK_MOD";
        
        case TK_EQ: return "TK_EQ";
        case TK_NEQ: return "TK_NEQ";
        case TK_ASSIGN: return "TK_ASSIGN";
        
        case TK_VAR: return "TK_VAR";
        case TK_NIP: return "TK_NIP";
        case TK_SIM: return "TK_SIM";
        case TK_NUUM: return "TK_NUUM";
        
        case TK_PRINT: return "TK_PRINT";
        case TK_IF: return "TK_IF";
        case TK_ELSE: return "TK_ELSE";
        case TK_WHILE: return "TK_WHILE";
        case TK_FOR: return "TK_FOR";
        case TK_FUNC: return "TK_FUNC";
        case TK_RETURN: return "TK_RETURN";
        case TK_MAIN: return "TK_MAIN";
        case TK_IMPORT: return "TK_IMPORT";
        case TK_JSON: return "TK_JSON";
        
        case TK_CLASS: return "TK_CLASS";
        case TK_TYPELOCK: return "TK_TYPELOCK";
        case TK_ZIS: return "TK_ZIS";
        case TK_SIZEOF: return "TK_SIZEOF";
        
        case TK_TYPE_INT: return "TK_TYPE_INT";
        case TK_TYPE_FLOAT: return "TK_TYPE_FLOAT";
        case TK_TYPE_STR: return "TK_TYPE_STR";
        case TK_TYPE_BOOL: return "TK_TYPE_BOOL";
        case TK_TYPE_CHAR: return "TK_TYPE_CHAR";
        
        case TK_LPAREN: return "TK_LPAREN";
        case TK_RPAREN: return "TK_RPAREN";
        case TK_LBRACE: return "TK_LBRACE";
        case TK_RBRACE: return "TK_RBRACE";
        case TK_LBRACKET: return "TK_LBRACKET";
        case TK_RBRACKET: return "TK_RBRACKET";
        case TK_COMMA: return "TK_COMMA";
        case TK_SEMICOLON: return "TK_SEMICOLON";
        case TK_COLON: return "TK_COLON";
        case TK_PERIOD: return "TK_PERIOD";
        
        case TK_AND: return "TK_AND";
        case TK_OR: return "TK_OR";
        case TK_NOT: return "TK_NOT";
        case TK_GT: return "TK_GT";
        case TK_LT: return "TK_LT";
        case TK_GTE: return "TK_GTE";
        case TK_LTE: return "TK_LTE";
        
        case TK_EOF: return "TK_EOF";
        case TK_ERROR: return "TK_ERROR";
        
        default: return "TK_UNKNOWN";
    }
}

void printToken(Token token) {
    printf("[TOKEN] Line %d: %s '", token.line, tokenKindToString(token.kind));
    
    if (token.kind == TK_STRING && token.value.str_val) {
        printf("%s", token.value.str_val);
    } else if (token.kind == TK_INT) {
        printf("%lld", token.value.int_val);
    } else if (token.kind == TK_FLOAT) {
        printf("%f", token.value.float_val);
    } else if (token.kind == TK_CHAR) {
        printf("%c", token.value.char_val);
    } else {
        for (int i = 0; i < token.length; i++) {
            putchar(token.start[i]);
        }
    }
    
    printf("'\n");
}
