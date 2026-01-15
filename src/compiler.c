#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 10000
#define MAX_SYMBOLS 1000

typedef enum {
    // Mots-clés
    TK_FN, TK_LET, TK_IF, TK_ELSE, TK_RETURN,
    TK_IMPORT, TK_FOR, TK_WHILE,
    TK_TRUE, TK_FALSE,
    
    // Types
    TK_I32, TK_I64, TK_F32, TK_F64,
    TK_STRING, TK_BOOL, TK_VOID,
    
    // Identifiants et littéraux
    TK_IDENT, TK_INTLIT, TK_FLOATLIT, TK_STRLIT,
    
    // Opérateurs
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_AND, TK_OR, TK_NOT,
    TK_ASSIGN, TK_ARROW,
    
    // Ponctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET, TK_COMMA, TK_COLON,
    TK_SEMICOLON, TK_DOT,
    
    // Spécial
    TK_EOF, TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[256];
    int line;
    int col;
    int int_value;
    double float_value;
} Token;

typedef struct {
    char name[256];
    char type[256];
    int offset;
} Symbol;

typedef struct ASTNode {
    int node_type;
    struct ASTNode* left;
    struct ASTNode* right;
    Token token;
} ASTNode;

// Fonctions de tokenisation
Token* tokenize(const char* source) {
    static Token tokens[MAX_TOKENS];
    int token_count = 0;
    int i = 0, line = 1, col = 1;
    int source_len = strlen(source);
    
    while (i < source_len && token_count < MAX_TOKENS - 1) {
        char ch = source[i];
        
        // Espaces
        if (isspace(ch)) {
            if (ch == '\n') {
                line++;
                col = 1;
            }
            i++;
            col++;
            continue;
        }
        
        // Commentaires
        if (ch == '/' && i + 1 < source_len && source[i + 1] == '/') {
            while (i < source_len && source[i] != '\n') {
                i++;
                col++;
            }
            continue;
        }
        
        Token* tok = &tokens[token_count];
        tok->line = line;
        tok->col = col;
        
        // Identifiants et mots-clés
        if (isalpha(ch) || ch == '_') {
            int j = 0;
            while (i < source_len && (isalnum(source[i]) || source[i] == '_')) {
                if (j < 255) tok->lexeme[j++] = source[i];
                i++;
                col++;
            }
            tok->lexeme[j] = '\0';
            
            // Mots-clés
            if (strcmp(tok->lexeme, "fn") == 0) tok->type = TK_FN;
            else if (strcmp(tok->lexeme, "let") == 0) tok->type = TK_LET;
            else if (strcmp(tok->lexeme, "if") == 0) tok->type = TK_IF;
            else if (strcmp(tok->lexeme, "else") == 0) tok->type = TK_ELSE;
            else if (strcmp(tok->lexeme, "return") == 0) tok->type = TK_RETURN;
            else if (strcmp(tok->lexeme, "import") == 0) tok->type = TK_IMPORT;
            else if (strcmp(tok->lexeme, "sw") == 0) tok->type = TK_IMPORT;
            else if (strcmp(tok->lexeme, "for") == 0) tok->type = TK_FOR;
            else if (strcmp(tok->lexeme, "while") == 0) tok->type = TK_WHILE;
            else if (strcmp(tok->lexeme, "true") == 0) tok->type = TK_TRUE;
            else if (strcmp(tok->lexeme, "false") == 0) tok->type = TK_FALSE;
            else if (strcmp(tok->lexeme, "i32") == 0) tok->type = TK_I32;
            else if (strcmp(tok->lexeme, "i64") == 0) tok->type = TK_I64;
            else if (strcmp(tok->lexeme, "f32") == 0) tok->type = TK_F32;
            else if (strcmp(tok->lexeme, "f64") == 0) tok->type = TK_F64;
            else if (strcmp(tok->lexeme, "string") == 0) tok->type = TK_STRING;
            else if (strcmp(tok->lexeme, "bool") == 0) tok->type = TK_BOOL;
            else if (strcmp(tok->lexeme, "void") == 0) tok->type = TK_VOID;
            else tok->type = TK_IDENT;
            
            token_count++;
            continue;
        }
        
        // Nombres
        if (isdigit(ch) || (ch == '.' && i + 1 < source_len && isdigit(source[i + 1]))) {
            int j = 0;
            int has_dot = 0;
            
            while (i < source_len && (isdigit(source[i]) || (!has_dot && source[i] == '.'))) {
                if (source[i] == '.') has_dot = 1;
                if (j < 255) tok->lexeme[j++] = source[i];
                i++;
                col++;
            }
            tok->lexeme[j] = '\0';
            
            if (has_dot) {
                tok->type = TK_FLOATLIT;
                tok->float_value = atof(tok->lexeme);
            } else {
                tok->type = TK_INTLIT;
                tok->int_value = atoi(tok->lexeme);
            }
            
            token_count++;
            continue;
        }
        
        // Chaînes de caractères
        if (ch == '"') {
            int j = 0;
            i++; col++; // Sauter le "
            
            while (i < source_len && source[i] != '"') {
                if (source[i] == '\\' && i + 1 < source_len) {
                    i++; col++;
                    switch (source[i]) {
                        case 'n': tok->lexeme[j++] = '\n'; break;
                        case 't': tok->lexeme[j++] = '\t'; break;
                        case '"': tok->lexeme[j++] = '"'; break;
                        case '\\': tok->lexeme[j++] = '\\'; break;
                        default: tok->lexeme[j++] = source[i]; break;
                    }
                } else {
                    if (j < 255) tok->lexeme[j++] = source[i];
                }
                i++;
                col++;
            }
            
            if (i < source_len && source[i] == '"') {
                i++; col++; // Sauter le " fermant
            }
            
            tok->lexeme[j] = '\0';
            tok->type = TK_STRLIT;
            token_count++;
            continue;
        }
        
        // Opérateurs et ponctuation
        switch (ch) {
            case '+': tok->type = TK_PLUS; break;
            case '-': 
                if (i + 1 < source_len && source[i + 1] == '>') {
                    tok->type = TK_ARROW;
                    i++; col++;
                    tok->lexeme[0] = '-'; tok->lexeme[1] = '>'; tok->lexeme[2] = '\0';
                } else {
                    tok->type = TK_MINUS;
                }
                break;
            case '*': tok->type = TK_MULT; break;
            case '/': tok->type = TK_DIV; break;
            case '%': tok->type = TK_MOD; break;
            case '=':
                if (i + 1 < source_len && source[i + 1] == '=') {
                    tok->type = TK_EQ;
                    i++; col++;
                    tok->lexeme[0] = '='; tok->lexeme[1] = '='; tok->lexeme[2] = '\0';
                } else {
                    tok->type = TK_ASSIGN;
                }
                break;
            case '!':
                if (i + 1 < source_len && source[i + 1] == '=') {
                    tok->type = TK_NEQ;
                    i++; col++;
                    tok->lexeme[0] = '!'; tok->lexeme[1] = '='; tok->lexeme[2] = '\0';
                } else {
                    tok->type = TK_NOT;
                }
                break;
            case '<':
                if (i + 1 < source_len && source[i + 1] == '=') {
                    tok->type = TK_LE;
                    i++; col++;
                    tok->lexeme[0] = '<'; tok->lexeme[1] = '='; tok->lexeme[2] = '\0';
                } else {
                    tok->type = TK_LT;
                }
                break;
            case '>':
                if (i + 1 < source_len && source[i + 1] == '=') {
                    tok->type = TK_GE;
                    i++; col++;
                    tok->lexeme[0] = '>'; tok->lexeme[1] = '='; tok->lexeme[2] = '\0';
                } else {
                    tok->type = TK_GT;
                }
                break;
            case '&':
                if (i + 1 < source_len && source[i + 1] == '&') {
                    tok->type = TK_AND;
                    i++; col++;
                    tok->lexeme[0] = '&'; tok->lexeme[1] = '&'; tok->lexeme[2] = '\0';
                }
                break;
            case '|':
                if (i + 1 < source_len && source[i + 1] == '|') {
                    tok->type = TK_OR;
                    i++; col++;
                    tok->lexeme[0] = '|'; tok->lexeme[1] = '|'; tok->lexeme[2] = '\0';
                }
                break;
            case '(': tok->type = TK_LPAREN; break;
            case ')': tok->type = TK_RPAREN; break;
            case '{': tok->type = TK_LBRACE; break;
            case '}': tok->type = TK_RBRACE; break;
            case '[': tok->type = TK_LBRACKET; break;
            case ']': tok->type = TK_RBRACKET; break;
            case ',': tok->type = TK_COMMA; break;
            case ':': tok->type = TK_COLON; break;
            case ';': tok->type = TK_SEMICOLON; break;
            case '.': tok->type = TK_DOT; break;
            default: tok->type = TK_ERROR; break;
        }
        
        if (tok->lexeme[0] == '\0') {
            tok->lexeme[0] = ch;
            tok->lexeme[1] = '\0';
        }
        
        i++;
        col++;
        token_count++;
    }
    
    // Token EOF
    tokens[token_count].type = TK_EOF;
    tokens[token_count].lexeme[0] = '\0';
    
    return tokens;
}

// Génération de code C
void generate_c_code(Token* tokens, const char* output_filename) {
    FILE* output = fopen(output_filename, "w");
    if (!output) {
        fprintf(stderr, "Erreur: Impossible d'écrire dans %s\n", output_filename);
        return;
    }
    
    fprintf(output, "/* Code généré par SwiftVelox */\n");
    fprintf(output, "#include <stdio.h>\n");
    fprintf(output, "#include <stdlib.h>\n");
    fprintf(output, "#include <string.h>\n");
    fprintf(output, "#include <math.h>\n\n");
    
    // Déclaration des fonctions du runtime
    fprintf(output, "/* Runtime functions */\n");
    fprintf(output, "void sv_print(const char* msg);\n");
    fprintf(output, "void sv_print_int(int value);\n");
    fprintf(output, "void sv_print_float(double value);\n\n");
    
    fprintf(output, "int main() {\n");
    
    int i = 0;
    int in_function = 0;
    int brace_count = 0;
    
    while (tokens[i].type != TK_EOF) {
        Token tok = tokens[i];
        
        if (tok.type == TK_FN) {
            // Début de fonction
            i++;
            if (tokens[i].type == TK_IDENT) {
                if (strcmp(tokens[i].lexeme, "main") == 0) {
                    fprintf(output, "    /* Fonction main */\n");
                } else {
                    fprintf(output, "void %s() {\n", tokens[i].lexeme);
                    in_function = 1;
                }
            }
        }
        else if (tok.type == TK_STRLIT && !in_function) {
            // swget implicite dans main
            fprintf(output, "    sv_print(\"%s\");\n", tok.lexeme);
        }
        else if (tok.type == TK_LET) {
            // Déclaration de variable
            i++;
            if (tokens[i].type == TK_IDENT) {
                char var_name[256];
                strcpy(var_name, tokens[i].lexeme);
                
                i += 2; // skip : and type or = 
                
                if (tokens[i].type == TK_INTLIT) {
                    fprintf(output, "    int %s = %d;\n", var_name, tokens[i].int_value);
                }
                else if (tokens[i].type == TK_FLOATLIT) {
                    fprintf(output, "    double %s = %f;\n", var_name, tokens[i].float_value);
                }
                else if (tokens[i].type == TK_STRLIT) {
                    fprintf(output, "    const char* %s = \"%s\";\n", var_name, tokens[i].lexeme);
                }
            }
        }
        else if (tok.type == TK_IDENT && strcmp(tok.lexeme, "swget") == 0) {
            // Appel à swget
            i += 2; // skip '('
            if (tokens[i].type == TK_STRLIT) {
                fprintf(output, "    sv_print(\"%s\");\n", tokens[i].lexeme);
            }
            i += 2; // skip string and ')'
        }
        else if (tok.type == TK_LBRACE) {
            brace_count++;
            fprintf(output, "    {\n");
        }
        else if (tok.type == TK_RBRACE) {
            brace_count--;
            fprintf(output, "    }\n");
            if (brace_count == 0 && in_function) {
                in_function = 0;
                if (strcmp(tokens[i-2].lexeme, "main") != 0) {
                    fprintf(output, "}\n\n");
                }
            }
        }
        
        i++;
    }
    
    fprintf(output, "    return 0;\n");
    fprintf(output, "}\n");
    
    fclose(output);
}

// Compile un fichier SwiftVelox vers C
int compile_file(const char* input_file, const char* output_file) {
    printf("Compilation de %s...\n", input_file);
    
    // Lire le fichier source
    FILE* file = fopen(input_file, "r");
    if (!file) {
        fprintf(stderr, "Erreur: Impossible d'ouvrir %s\n", input_file);
        return 0;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, file);
    source[size] = '\0';
    fclose(file);
    
    // Tokeniser
    Token* tokens = tokenize(source);
    
    // Générer le code C
    if (output_file == NULL) {
        char default_output[256];
        snprintf(default_output, sizeof(default_output), "%s.c", input_file);
        output_file = default_output;
    }
    
    generate_c_code(tokens, output_file);
    
    // Nettoyer
    free(source);
    
    printf("✅ Code C généré: %s\n", output_file);
    printf("Pour compiler: cc -Os -o program %s src/runtime.c\n", output_file);
    
    return 1;
}
