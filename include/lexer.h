#ifndef LEXER_H
#define LEXER_H

#include "common.h"

// Structure déjà déclarée dans common.h, on utilise une déclaration forward
struct Lexer;

// Fonctions lexer
void lexer_init(struct Lexer* lexer, const char* source, const char* filename);
Token lexer_next_token(struct Lexer* lexer);
Token lexer_peek_token(struct Lexer* lexer);
void lexer_skip_whitespace(struct Lexer* lexer);
bool lexer_is_at_end(struct Lexer* lexer);
char lexer_advance(struct Lexer* lexer);
char lexer_peek(struct Lexer* lexer);
char lexer_peek_next(struct Lexer* lexer);
bool lexer_match(struct Lexer* lexer, char expected);
Token lexer_make_token(struct Lexer* lexer, TokenKind kind);
Token lexer_error_token(struct Lexer* lexer, const char* message);
Token lexer_string(struct Lexer* lexer);
Token lexer_number(struct Lexer* lexer);
Token lexer_identifier(struct Lexer* lexer);

#endif // LEXER_H
