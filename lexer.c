#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include "common.h"

// ======================================================
// [SECTION] LEXER STATE
// ======================================================
typedef struct {
    const char* start;
    const char* current;
    const char* filename;
    int line;
    int column;
    int start_column;
    bool in_string;
    bool in_comment;
    bool in_doc_comment;
    int brace_depth;
    int paren_depth;
    int bracket_depth;
} Lexer;

static Lexer lexer;

// ======================================================
// [SECTION] LEXER UTILITIES
// ======================================================
void initLexer(const char* source, const char* filename) {
    lexer.start = source;
    lexer.current = source;
    lexer.filename = filename;
    lexer.line = 1;
    lexer.column = 1;
    lexer.start_column = 1;
    lexer.in_string = false;
    lexer.in_comment = false;
    lexer.in_doc_comment = false;
    lexer.brace_depth = 0;
    lexer.paren_depth = 0;
    lexer.bracket_depth = 0;
}

static bool isAtEnd() { 
    return *lexer.current == '\0'; 
}

static char advance() {
    char c = *lexer.current;
    if (c == '\n') {
        lexer.line++;
        lexer.column = 1;
    } else {
        lexer.column++;
    }
    lexer.current++;
    return c;
}

static char peek() { 
    return *lexer.current; 
}

static char peekNext() { 
    if (isAtEnd()) return '\0';
    return lexer.current[1]; 
}

static char peekPrev() {
    if (lexer.current == lexer.start) return '\0';
    return lexer.current[-1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*lexer.current != expected) return false;
    advance();
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
    token.column = lexer.start_column;
    
    // Initialize value
    token.value.int_val = 0;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.kind = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    token.column = lexer.column;
    token.value.str_val = NULL;
    
    log_error(lexer.filename, token.line, token.column, "%s", message);
    return token;
}

static Token warningToken(const char* message) {
    Token token;
    token.kind = TK_WARNING;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    token.column = lexer.column;
    token.value.str_val = NULL;
    
    log_warning(lexer.filename, token.line, token.column, "%s", message);
    return token;
}

static Token makeStringToken(TokenKind kind, char* str) {
    Token token = makeToken(kind);
    token.value.str_val = str;
    return token;
}

static Token makeIntToken(int64_t value) {
    Token token = makeToken(TK_INT);
    token.value.int_val = value;
    return token;
}

static Token makeFloatToken(double value) {
    Token token = makeToken(TK_FLOAT);
    token.value.float_val = value;
    return token;
}

static Token makeBoolToken(bool value) {
    Token token = makeToken(value ? TK_TRUE : TK_FALSE);
    token.value.bool_val = value;
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
            case '\v':
            case '\f':
                advance();
                break;
            case '\n':
                lexer.line++;
                lexer.column = 1;
                advance();
                if (lexer.in_comment) {
                    lexer.in_comment = false;
                }
                break;
            case '#': // Commentaire avec #
                if (peekNext() == '#') { // Documentation comment ##
                    lexer.in_doc_comment = true;
                    advance(); // Skip first #
                    advance(); // Skip second #
                    while (peek() != '\n' && !isAtEnd()) advance();
                    lexer.in_doc_comment = false;
                } else { // Regular comment #
                    lexer.in_comment = true;
                    while (peek() != '\n' && !isAtEnd()) advance();
                    lexer.in_comment = false;
                }
                break;
            case '/':
                if (peekNext() == '/') { // Commentaire //
                    lexer.in_comment = true;
                    advance(); // Skip '/'
                    advance(); // Skip '/'
                    while (peek() != '\n' && !isAtEnd()) advance();
                    lexer.in_comment = false;
                } else if (peekNext() == '*') { // Commentaire /* */
                    lexer.in_comment = true;
                    advance(); // Skip '/'
                    advance(); // Skip '*'
                    while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                        if (peek() == '\n') {
                            lexer.line++;
                            lexer.column = 1;
                        }
                        advance();
                    }
                    if (!isAtEnd()) {
                        advance(); // Skip '*'
                        advance(); // Skip '/'
                    }
                    lexer.in_comment = false;
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
// [SECTION] STRING LEXING (IMPROVED)
// ======================================================
static Token string(char quote_char) {
    // Skip opening quote (already consumed)
    bool is_raw = (peekPrev() == 'r' || peekPrev() == 'R');
    bool is_multiline = false;
    
    // Check for multiline string (triple quotes)
    if (peek() == quote_char && peekNext() == quote_char) {
        is_multiline = true;
        advance(); // Skip second quote
        advance(); // Skip third quote
    }
    
    while (!isAtEnd()) {
        if (is_multiline) {
            if (peek() == quote_char && peekNext() == quote_char && 
                lexer.current[2] == quote_char) {
                // End of multiline string
                advance(); // Skip first quote
                advance(); // Skip second quote
                advance(); // Skip third quote
                break;
            }
        } else {
            if (peek() == quote_char) {
                // End of regular string
                advance(); // Skip closing quote
                break;
            }
        }
        
        if (peek() == '\n') {
            if (!is_multiline) {
                char error_msg[64];
                snprintf(error_msg, sizeof(error_msg), 
                        "Unterminated string (started with '%c')", quote_char);
                return errorToken(error_msg);
            }
            lexer.line++;
            lexer.column = 1;
        }
        
        if (!is_raw && peek() == '\\') { // Handle escape sequences
            advance();
            switch (peek()) {
                case 'n': case 't': case 'r': case '\\': 
                case '"': case '\'': case '0': case 'b': case 'f':
                case 'v': case 'x': case 'u': case 'U':
                case 'a': case 'e': case '?':
                    advance();
                    break;
                case '\n': // Continue on next line
                    advance();
                    break;
                default:
                    // Just skip unknown escape or keep backslash
                    break;
            }
        } else {
            advance();
        }
    }
    
    if (isAtEnd()) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), 
                "Unterminated string (started with '%c')", quote_char);
        return errorToken(error_msg);
    }
    
    // Extract string content
    const char* content_start = lexer.start;
    if (is_raw && (content_start[0] == 'r' || content_start[0] == 'R')) {
        content_start++; // Skip 'r' or 'R'
    }
    if (is_multiline) {
        content_start += 3; // Skip opening triple quotes
    } else {
        content_start++; // Skip opening quote
    }
    
    int content_length = (int)(lexer.current - content_start - 
                              (is_multiline ? 3 : 1)); // Subtract closing quotes
    
    char* str = malloc(content_length + 1);
    if (str) {
        if (!is_raw) {
            // Process escape sequences
            const char* src = content_start;
            int dest_idx = 0;
            
            for (int i = 0; i < content_length; i++) {
                if (src[i] == '\\') {
                    i++; // Skip backslash
                    if (i < content_length) {
                        switch (src[i]) {
                            case 'n': str[dest_idx++] = '\n'; break;
                            case 't': str[dest_idx++] = '\t'; break;
                            case 'r': str[dest_idx++] = '\r'; break;
                            case '\\': str[dest_idx++] = '\\'; break;
                            case '"': str[dest_idx++] = '"'; break;
                            case '\'': str[dest_idx++] = '\''; break;
                            case '0': str[dest_idx++] = '\0'; break;
                            case 'b': str[dest_idx++] = '\b'; break;
                            case 'f': str[dest_idx++] = '\f'; break;
                            case 'v': str[dest_idx++] = '\v'; break;
                            case 'a': str[dest_idx++] = '\a'; break;
                            case 'e': str[dest_idx++] = '\033'; break;
                            case 'x': // Hexadecimal escape
                                if (i + 2 < content_length && 
                                    isxdigit(src[i+1]) && isxdigit(src[i+2])) {
                                    char hex[3] = {src[i+1], src[i+2], '\0'};
                                    str[dest_idx++] = (char)strtol(hex, NULL, 16);
                                    i += 2;
                                } else {
                                    str[dest_idx++] = src[i];
                                }
                                break;
                            case 'u': // Unicode escape (4 hex digits)
                            case 'U': // Unicode escape (8 hex digits)
                                // Simplified - just copy as is for now
                                str[dest_idx++] = src[i];
                                break;
                            default:
                                str[dest_idx++] = src[i];
                                break;
                        }
                    }
                } else {
                    str[dest_idx++] = src[i];
                }
            }
            str[dest_idx] = '\0';
        } else {
            // Raw string - copy as is
            strncpy(str, content_start, content_length);
            str[content_length] = '\0';
        }
    }
    
    return makeStringToken(TK_STRING, str);
}

// ======================================================
// [SECTION] NUMBER LEXING (IMPROVED WITH SCIENTIFIC NOTATION)
// ======================================================
static Token number() {
    bool is_float = false;
    bool is_hex = false;
    bool is_binary = false;
    bool is_octal = false;
    bool has_exponent = false;
    bool has_sign = false;
    
    // Check for sign
    if (peek() == '+' || peek() == '-') {
        has_sign = true;
        advance();
    }
    
    // Check for hex (0x) or binary (0b) or octal (0o)
    if (peek() == '0') {
        char next = peekNext();
        if (next == 'x' || next == 'X') {
            is_hex = true;
            advance(); // Skip '0'
            advance(); // Skip 'x' or 'X'
        } else if (next == 'b' || next == 'B') {
            is_binary = true;
            advance(); // Skip '0'
            advance(); // Skip 'b' or 'B'
        } else if (next == 'o' || next == 'O') {
            is_octal = true;
            advance(); // Skip '0'
            advance(); // Skip 'o' or 'O'
        }
    }
    
    if (is_hex) {
        // Parse hexadecimal
        while (isxdigit(peek())) advance();
        
        // Optional fractional part for hex floats
        if (peek() == '.' && isxdigit(peekNext())) {
            is_float = true;
            advance(); // Consume '.'
            while (isxdigit(peek())) advance();
        }
        
        // Optional binary exponent for hex floats
        if ((peek() == 'p' || peek() == 'P') && 
            (peekNext() == '+' || peekNext() == '-' || isdigit(peekNext()))) {
            is_float = true;
            advance(); // Consume 'p' or 'P'
            if (peek() == '+' || peek() == '-') advance();
            while (isdigit(peek())) advance();
        }
    } else if (is_binary) {
        // Parse binary
        while (peek() == '0' || peek() == '1') advance();
    } else if (is_octal) {
        // Parse octal
        while (peek() >= '0' && peek() <= '7') advance();
    } else {
        // Parse decimal
        while (isdigit(peek())) advance();
        
        // Decimal part
        if (peek() == '.' && isdigit(peekNext())) {
            is_float = true;
            advance(); // Consume '.'
            while (isdigit(peek())) advance();
        }
        
        // Exponent part
        if (peek() == 'e' || peek() == 'E') {
            has_exponent = true;
            is_float = true;
            advance(); // Consume 'e' or 'E'
            if (peek() == '+' || peek() == '-') advance();
            while (isdigit(peek())) advance();
        }
    }
    
    // Parse suffix
    if (peek() == 'f' || peek() == 'F') {
        is_float = true;
        advance();
    } else if (peek() == 'l' || peek() == 'L') {
        advance(); // Long suffix
        if (peek() == 'l' || peek() == 'L') advance(); // Long long
    } else if (peek() == 'u' || peek() == 'U') {
        advance(); // Unsigned suffix
        if (peek() == 'l' || peek() == 'L') {
            advance(); // Unsigned long
            if (peek() == 'l' || peek() == 'L') advance(); // Unsigned long long
        }
    }
    
    // Parse the number
    int length = (int)(lexer.current - lexer.start);
    char* num_str = malloc(length + 1);
    if (num_str) {
        strncpy(num_str, lexer.start, length);
        num_str[length] = '\0';
        
        if (is_float || has_exponent) {
            double value = strtod(num_str, NULL);
            free(num_str);
            
            // Check for special values
            if (isnan(value)) return makeToken(TK_NAN);
            if (isinf(value)) return makeToken(TK_INF);
            
            return makeFloatToken(value);
        } else {
            // Determine base
            int base = 10;
            if (is_hex) base = 16;
            else if (is_binary) base = 2;
            else if (is_octal) base = 8;
            
            // Check for overflow
            char* endptr;
            errno = 0;
            int64_t value = strtoll(num_str, &endptr, base);
            
            if (errno == ERANGE) {
                // Number too large, treat as float
                double float_val = strtod(num_str, NULL);
                free(num_str);
                return makeFloatToken(float_val);
            }
            
            free(num_str);
            return makeIntToken(value);
        }
    }
    
    return errorToken("Failed to parse number");
}

// ======================================================
// [SECTION] IDENTIFIER & KEYWORD LEXING (IMPROVED)
// ======================================================
static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isAlphaNumeric(char c) {
    return isAlpha(c) || isdigit(c);
}

static Token identifier() {
    while (isAlphaNumeric(peek()) || peek() == '.' || peek() == '$' || 
           peek() == '@' || peek() == '?' || peek() == '!') {
        advance();
    }
    
    int length = (int)(lexer.current - lexer.start);
    char* text = malloc(length + 1);
    if (text) {
        strncpy(text, lexer.start, length);
        text[length] = '\0';
    }
    
    // Check for keywords
    if (text) {
        // Special case for compound operators
        if (strcmp(text, "and") == 0) return makeToken(TK_AND);
        if (strcmp(text, "or") == 0) return makeToken(TK_OR);
        if (strcmp(text, "xor") == 0) return makeToken(TK_XOR);
        if (strcmp(text, "not") == 0) return makeToken(TK_NOT);
        if (strcmp(text, "is") == 0) return makeToken(TK_IS);
        if (strcmp(text, "in") == 0) return makeToken(TK_IN);
        if (strcmp(text, "of") == 0) return makeToken(TK_OF);
        if (strcmp(text, "as") == 0) return makeToken(TK_AS);
        
        for (int i = 0; keywords[i].keyword != NULL; i++) {
            if (strcmp(text, keywords[i].keyword) == 0) {
                free(text);
                return makeToken(keywords[i].kind);
            }
        }
        
        // If not a keyword, it's an identifier
        Token token = makeToken(TK_IDENT);
        token.value.str_val = text;
        return token;
    }
    
    free(text);
    return errorToken("Failed to allocate memory for identifier");
}

// ======================================================
// [SECTION] OPERATOR LEXING (EXTENDED)
// ======================================================
static Token operatorLexer() {
    char c = peek();
    char error_msg[32];
    
    // Update depth counters
    if (c == '(') lexer.paren_depth++;
    else if (c == ')') lexer.paren_depth--;
    else if (c == '{') lexer.brace_depth++;
    else if (c == '}') lexer.brace_depth--;
    else if (c == '[') lexer.bracket_depth++;
    else if (c == ']') lexer.bracket_depth--;
    
    // Multi-character operators
    switch (c) {
        case '=':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_SPACESHIP); // ===
                if (match('>')) return makeToken(TK_RDARROW);   // ==>
                return makeToken(TK_EQ); // ==
            }
            if (match('>')) return makeToken(TK_DARROW); // =>
            return makeToken(TK_ASSIGN); // =
            
        case '!':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_NEQ); // !==
                return makeToken(TK_NEQ); // !=
            }
            return makeToken(TK_NOT); // !
            
        case '<':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_LDARROW); // <==
                if (match('>')) return makeToken(TK_SPACESHIP); // <=>
                return makeToken(TK_LTE); // <=
            }
            if (match('<')) {
                if (match('=')) return makeToken(TK_SHL_ASSIGN); // <<=
                return makeToken(TK_SHL); // <<
            }
            return makeToken(TK_LT); // <
            
        case '>':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_RDARROW); // ==>
                return makeToken(TK_GTE); // >=
            }
            if (match('>')) {
                if (match('=')) return makeToken(TK_SHR_ASSIGN); // >>=
                return makeToken(TK_SHR); // >>
            }
            return makeToken(TK_GT); // >
            
        case '&':
            advance();
            if (match('&')) {
                if (match('=')) return makeToken(TK_AND_ASSIGN); // &&=
                return makeToken(TK_AND); // &&
            }
            if (match('=')) return makeToken(TK_BIT_AND); // &=
            return makeToken(TK_BIT_AND); // &
            
        case '|':
            advance();
            if (match('|')) {
                if (match('=')) return makeToken(TK_OR_ASSIGN); // ||=
                return makeToken(TK_OR); // ||
            }
            if (match('=')) return makeToken(TK_BIT_OR); // |=
            return makeToken(TK_BIT_OR); // |
            
        case '^':
            advance();
            if (match('=')) return makeToken(TK_XOR_ASSIGN); // ^=
            return makeToken(TK_BIT_XOR); // ^
            
        case '~':
            advance();
            return makeToken(TK_BIT_NOT); // ~
            
        case '+':
            advance();
            if (match('=')) return makeToken(TK_PLUS_ASSIGN); // +=
            if (match('+')) return makeToken(TK_INC); // ++
            return makeToken(TK_PLUS); // +
            
        case '-':
            advance();
            if (match('=')) return makeToken(TK_MINUS_ASSIGN); // -=
            if (match('-')) return makeToken(TK_DEC); // --
            if (match('>')) return makeToken(TK_RARROW); // ->
            return makeToken(TK_MINUS); // -
            
        case '*':
            advance();
            if (match('=')) return makeToken(TK_MULT_ASSIGN); // *=
            if (match('*')) {
                if (match('=')) return makeToken(TK_POW_ASSIGN); // **=
                return makeToken(TK_POW); // **
            }
            return makeToken(TK_MULT); // *
            
        case '/':
            advance();
            if (match('=')) return makeToken(TK_DIV_ASSIGN); // /=
            return makeToken(TK_DIV); // /
            
        case '%':
            advance();
            if (match('=')) return makeToken(TK_MOD_ASSIGN); // %=
            return makeToken(TK_MOD); // %
            
        case '.':
            advance();
            if (match('.')) {
                if (match('.')) return makeToken(TK_ELLIPSIS); // ...
                if (match('=')) return makeToken(TK_RANGE); // ..=
                return makeToken(TK_RANGE); // ..
            }
            if (match('?')) return makeToken(TK_SAFE_NAV); // .?
            return makeToken(TK_PERIOD); // .
            
        case '?':
            advance();
            if (match('.')) return makeToken(TK_SAFE_NAV); // ?.
            if (match('?')) {
                if (match('=')) return makeToken(TK_OR_ASSIGN); // ??=
                return makeToken(TK_OR); // ??
            }
            if (match(':')) return makeToken(TK_TERNARY); // ?:
            return makeToken(TK_QUESTION); // ?
            
        case ':':
            advance();
            if (match(':')) return makeToken(TK_SCOPE); // ::
            return makeToken(TK_COLON); // :
            
        case '@':
            advance();
            return makeToken(TK_AT); // @
            
        case '$':
            advance();
            return makeToken(TK_DOLLAR); // $
            
        case '#':
            advance();
            return makeToken(TK_HASH); // #
            
        case '`':
            advance();
            return makeToken(TK_BACKTICK); // `
            
        case ';':
            advance();
            return makeToken(TK_SEMICOLON); // ;
            
        case ',':
            advance();
            return makeToken(TK_COMMA); // ,
            
        case '(':
            advance();
            return makeToken(TK_LPAREN); // (
            
        case ')':
            advance();
            return makeToken(TK_RPAREN); // )
            
        case '{':
            advance();
            return makeToken(TK_LBRACE); // {
            
        case '}':
            advance();
            return makeToken(TK_RBRACE); // }
            
        case '[':
            advance();
            return makeToken(TK_LBRACKET); // [
            
        case ']':
            advance();
            return makeToken(TK_RBRACKET); // ]
            
        case '"':
        case '\'':
            advance();
            return string(c);
            
        case '\\':
            advance();
            return makeToken(TK_BACKSLASH);
            
        default:
            // Unknown character
            advance();
            snprintf(error_msg, sizeof(error_msg), 
                    "Unexpected character: '%c' (0x%02x)", c, c);
            return warningToken(error_msg);
    }
}

// ======================================================
// [SECTION] MAIN LEXER FUNCTION
// ======================================================
Token scanToken() {
    skipWhitespace();
    lexer.start = lexer.current;
    lexer.start_column = lexer.column;
    
    if (isAtEnd()) return makeToken(TK_EOF);
    
    char c = peek();
    
    // Identifiers and keywords
    if (isAlpha(c) || c == '_' || c == '$' || c == '@') {
        return identifier();
    }
    
    // Numbers
    if (isdigit(c) || ((c == '+' || c == '-') && isdigit(peekNext()))) {
        return number();
    }
    
    // Strings and characters
    if (c == '"' || c == '\'') {
        return operatorLexer(); // Will handle string
    }
    
    // Raw string literals
    if ((c == 'r' || c == 'R') && (peekNext() == '"' || peekNext() == '\'')) {
        advance(); // Skip 'r' or 'R'
        return string(peek()); // Process string with current char as quote
    }
    
    // Operators and punctuation
    return operatorLexer();
}
