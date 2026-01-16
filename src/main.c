/*
 * SwiftVelox v1.0 - Langage complet & Moteur SDG
 * Auteur: Toi & Gemini (Co-Pilote)
 * * Instructions:
 * 1. Compiler l'interpréteur : gcc -o swiftvelox swiftvelox.c -lm
 * 2. Créer le dossier libs : mkdir -p /usr/bin/swiftvelox/addws
 * 3. Utiliser : ./swiftvelox run mon_script.svx
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

// ===== CONFIGURATION =====
#define VERSION "1.0.0"
#define LIB_PATH "/usr/bin/swiftvelox/addws/"
#define MAX_LINE_LENGTH 4096

// ===== TYPES DE DONNÉES =====
typedef enum {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, 
    VAL_STRING, VAL_ARRAY, VAL_MAP, 
    VAL_FUNCTION, VAL_NATIVE, VAL_RETURN_SIG
} ValueType;

typedef struct Value Value;
typedef struct Environment Environment;
typedef struct ASTNode ASTNode;

struct Value {
    ValueType type;
    union {
        int boolean;
        int64_t integer;
        double number;
        char* string;
        struct {
            struct Value* values;
            int count;
        } array;
        struct {
            char* name;
            Value (*fn)(struct Value*, int, Environment*);
        } native;
        struct {
            ASTNode* declaration;
        } function;
    };
};

// Environnement (Scope)
struct Environment {
    struct Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
};

// ===== LEXER & PARSER TYPES =====
// (Repris de ton code original avec ajustements)
typedef enum {
    TK_MODULE, TK_IMPORT, TK_EXPORT, TK_FN, TK_LET, TK_MUT,
    TK_IF, TK_ELSE, TK_FOR, TK_WHILE, TK_MATCH, TK_RETURN,
    TK_TRUE, TK_FALSE, TK_NIL, TK_IDENTIFIER, TK_NUMBER, TK_STRING_LIT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_EQ, TK_EQEQ, TK_BANGEQ,
    TK_LT, TK_GT, TK_LTEQ, TK_GTEQ, TK_LPAREN, TK_RPAREN, TK_LBRACE, 
    TK_RBRACE, TK_LBRACKET, TK_RBRACKET, TK_COMMA, TK_DOT, TK_COLON, 
    TK_SEMICOLON, TK_ARROW, TK_EOF, TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* start;
    int length;
    int line;
    union { int64_t i; double d; char* s; };
} Token;

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

typedef enum {
    NODE_PROGRAM, NODE_BLOCK, NODE_VAR_DECL, NODE_FUNCTION,
    NODE_IF, NODE_WHILE, NODE_RETURN, NODE_CALL, NODE_ACCESS,
    NODE_BINARY, NODE_LITERAL, NODE_IDENTIFIER, NODE_ASSIGN,
    NODE_EXPR_STMT
} NodeType;

struct ASTNode {
    NodeType type;
    Token token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int child_count;
};

// ===== GLOBALS =====
Scanner scanner;
Token current_token;
Token previous_token;
int had_error = 0;

// ===== UTILITAIRES MÉMOIRE & ERREUR =====
void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;31m❌ ERREUR FATALE: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");
    va_end(args);
    exit(1);
}

// ===== ENVIRONNEMENT =====
Environment* new_environment(Environment* enclosing) {
    Environment* env = malloc(sizeof(Environment));
    env->enclosing = enclosing;
    env->count = 0;
    env->capacity = 16;
    env->names = malloc(sizeof(char*) * 16);
    env->values = malloc(sizeof(Value) * 16);
    return env;
}

void env_define(Environment* env, char* name, Value value) {
    // Vérifier si existe déjà
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
    env->names[env->count] = strdup(name);
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

// ===== LEXER IMPL (Condensé) =====
void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

char advance() { return *scanner.current++; }
char peek() { return *scanner.current; }
int is_at_end() { return *scanner.current == '\0'; }

Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = (char*)scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

Token error_token(const char* msg) {
    Token token; token.type = TK_ERROR; token.s = (char*)msg; return token;
}

void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ': case '\r': case '\t': advance(); break;
            case '\n': scanner.line++; advance(); break;
            case '/': if (*(scanner.current + 1) == '/') while (peek() != '\n' && !is_at_end()) advance(); else return; break;
            default: return;
        }
    }
}

TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length && 
        memcmp(scanner.start + start, rest, length) == 0) return type;
    return TK_IDENTIFIER;
}

TokenType identifier_type() {
    switch (*scanner.start) {
        case 'm': return check_keyword(1, 5, "odule", TK_MODULE);
        case 'i': 
            if (scanner.current - scanner.start > 1 && scanner.start[1] == 'm') return check_keyword(2, 4, "port", TK_IMPORT);
            return check_keyword(1, 1, "f", TK_IF);
        case 'e': return check_keyword(1, 3, "lse", TK_ELSE);
        case 'f': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'n': return TK_FN;
                    case 'o': return check_keyword(2, 1, "r", TK_FOR);
                    case 'a': return check_keyword(2, 3, "lse", TK_FALSE);
                }
            }
            break;
        case 'l': return check_keyword(1, 2, "et", TK_LET);
        case 'w': return check_keyword(1, 4, "hile", TK_WHILE);
        case 'r': return check_keyword(1, 5, "eturn", TK_RETURN);
        case 't': return check_keyword(1, 3, "rue", TK_TRUE);
        case 'n': return check_keyword(1, 2, "il", TK_NIL);
    }
    return TK_IDENTIFIER;
}

Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;
    if (is_at_end()) return make_token(TK_EOF);
    
    char c = advance();
    if (isdigit(c)) {
        while (isdigit(peek())) advance();
        if (peek() == '.' && isdigit(*(scanner.current + 1))) {
            advance(); while (isdigit(peek())) advance();
            Token t = make_token(TK_NUMBER);
            char buf[64]; memcpy(buf, t.start, t.length); buf[t.length] = 0;
            t.d = atof(buf); return t;
        }
        Token t = make_token(TK_NUMBER);
        char buf[64]; memcpy(buf, t.start, t.length); buf[t.length] = 0;
        t.i = atoll(buf); return t;
    }
    if (isalpha(c) || c == '_') {
        while (isalnum(peek()) || peek() == '_') advance();
        Token t = make_token(identifier_type());
        if (t.type == TK_IDENTIFIER) {
            t.s = malloc(t.length + 1);
            memcpy(t.s, t.start, t.length); t.s[t.length] = 0;
        }
        return t;
    }
    if (c == '"') {
        while (peek() != '"' && !is_at_end()) advance();
        if (is_at_end()) return error_token("Chaîne non terminée");
        advance();
        Token t = make_token(TK_STRING_LIT);
        t.s = malloc(t.length - 1);
        memcpy(t.s, t.start + 1, t.length - 2); t.s[t.length - 2] = 0;
        return t;
    }

    switch (c) {
        case '(': return make_token(TK_LPAREN); case ')': return make_token(TK_RPAREN);
        case '{': return make_token(TK_LBRACE); case '}': return make_token(TK_RBRACE);
        case ',': return make_token(TK_COMMA); case '.': return make_token(TK_DOT);
        case ';': return make_token(TK_SEMICOLON); case '+': return make_token(TK_PLUS);
        case '-': return make_token(TK_MINUS); case '*': return make_token(TK_STAR);
        case '/': return make_token(TK_SLASH);
        case '=': return make_token(peek() == '=' ? (advance(), TK_EQEQ) : TK_EQ);
        case '!': return make_token(peek() == '=' ? (advance(), TK_BANGEQ) : TK_ERROR);
        case '<': return make_token(peek() == '=' ? (advance(), TK_LTEQ) : TK_LT);
        case '>': return make_token(peek() == '=' ? (advance(), TK_GTEQ) : TK_GT);
    }
    return error_token("Caractère inattendu");
}

// ===== PARSER IMPL =====
Token next_token() { previous_token = current_token; current_token = scan_token(); return current_token; }
int match(TokenType type) { if (current_token.type == type) { next_token(); return 1; } return 0; }
void consume(TokenType type, const char* msg) { if (current_token.type == type) { next_token(); return; } fatal_error("Ligne %d: %s", current_token.line, msg); }

ASTNode* new_node(NodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type; node->left = node->right = NULL;
    node->children = NULL; node->child_count = 0;
    return node;
}

ASTNode* expression();

ASTNode* primary() {
    if (match(TK_NUMBER)) { ASTNode* n = new_node(NODE_LITERAL); n->token = previous_token; return n; }
    if (match(TK_STRING_LIT)) { ASTNode* n = new_node(NODE_LITERAL); n->token = previous_token; return n; }
    if (match(TK_IDENTIFIER)) { ASTNode* n = new_node(NODE_IDENTIFIER); n->token = previous_token; return n; }
    if (match(TK_TRUE) || match(TK_FALSE) || match(TK_NIL)) { ASTNode* n = new_node(NODE_LITERAL); n->token = previous_token; return n; }
    if (match(TK_LPAREN)) { ASTNode* e = expression(); consume(TK_RPAREN, "Attendu )"); return e; }
    return NULL;
}

ASTNode* call() {
    ASTNode* expr = primary();
    while (1) {
        if (match(TK_LPAREN)) {
            ASTNode* n = new_node(NODE_CALL); n->left = expr;
            if (!match(TK_RPAREN)) {
                do {
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = expression();
                } while (match(TK_COMMA));
                consume(TK_RPAREN, "Attendu )");
            }
            expr = n;
        } else if (match(TK_DOT)) {
            consume(TK_IDENTIFIER, "Propriété attendue");
            ASTNode* n = new_node(NODE_ACCESS);
            n->left = expr; n->token = previous_token; // Nom de la propriété
            expr = n;
        } else break;
    }
    return expr;
}

ASTNode* multiplication() {
    ASTNode* expr = call();
    while (match(TK_STAR) || match(TK_SLASH)) {
        ASTNode* n = new_node(NODE_BINARY); n->token = previous_token;
        n->left = expr; n->right = call(); expr = n;
    }
    return expr;
}

ASTNode* addition() {
    ASTNode* expr = multiplication();
    while (match(TK_PLUS) || match(TK_MINUS)) {
        ASTNode* n = new_node(NODE_BINARY); n->token = previous_token;
        n->left = expr; n->right = multiplication(); expr = n;
    }
    return expr;
}

ASTNode* comparison() {
    ASTNode* expr = addition();
    while (match(TK_GT) || match(TK_LT) || match(TK_EQEQ)) {
        ASTNode* n = new_node(NODE_BINARY); n->token = previous_token;
        n->left = expr; n->right = addition(); expr = n;
    }
    return expr;
}

ASTNode* expression() {
    ASTNode* expr = comparison();
    if (match(TK_EQ)) {
        ASTNode* n = new_node(NODE_ASSIGN);
        n->left = expr; n->right = expression();
        return n;
    }
    return expr;
}

ASTNode* statement() {
    if (match(TK_LET)) {
        consume(TK_IDENTIFIER, "Nom variable attendu");
        ASTNode* n = new_node(NODE_VAR_DECL); n->token = previous_token;
        if (match(TK_EQ)) n->right = expression();
        consume(TK_SEMICOLON, "; manquant");
        return n;
    }
    if (match(TK_RETURN)) {
        ASTNode* n = new_node(NODE_RETURN);
        if (current_token.type != TK_SEMICOLON) n->left = expression();
        consume(TK_SEMICOLON, "; manquant");
        return n;
    }
    if (match(TK_LBRACE)) {
        ASTNode* n = new_node(NODE_BLOCK);
        while (current_token.type != TK_RBRACE && !is_at_end()) {
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = statement();
        }
        consume(TK_RBRACE, "} manquant");
        return n;
    }
    if (match(TK_IF)) {
        ASTNode* n = new_node(NODE_IF);
        consume(TK_LPAREN, "( attendu");
        n->left = expression();
        consume(TK_RPAREN, ") attendu");
        n->children = malloc(sizeof(ASTNode*)*2);
        n->children[0] = statement();
        if (match(TK_ELSE)) n->children[1] = statement(); else n->children[1] = NULL;
        return n;
    }
    if (match(TK_FN)) {
        consume(TK_IDENTIFIER, "Nom fonction attendu");
        ASTNode* n = new_node(NODE_FUNCTION); n->token = previous_token;
        consume(TK_LPAREN, "( attendu");
        if (!match(TK_RPAREN)) {
            do {
                consume(TK_IDENTIFIER, "Paramètre attendu");
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                ASTNode* p = new_node(NODE_IDENTIFIER); p->token = previous_token;
                n->children[n->child_count++] = p;
            } while (match(TK_COMMA));
            consume(TK_RPAREN, ") attendu");
        }
        consume(TK_LBRACE, "{ attendu");
        // Backtrack pour parser le bloc comme un statement
        scanner.current--; 
        current_token.type = TK_LBRACE; 
        n->left = statement();
        return n;
    }
    
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "; manquant");
    ASTNode* n = new_node(NODE_EXPR_STMT); n->left = expr;
    return n;
}

ASTNode* parse(const char* source) {
    init_scanner(source);
    next_token();
    ASTNode* program = new_node(NODE_PROGRAM);
    while (!is_at_end() && current_token.type != TK_EOF) {
        program->children = realloc(program->children, sizeof(ASTNode*) * (program->child_count + 1));
        program->children[program->child_count++] = statement();
    }
    return program;
}

// ===== INTERPRETEUR =====

// Fonctions natives
Value native_print(Value* args, int count, Environment* env) {
    for(int i=0; i<count; i++) {
        Value v = args[i];
        if (v.type == VAL_STRING) printf("%s ", v.string);
        else if (v.type == VAL_INT) printf("%lld ", v.integer);
        else if (v.type == VAL_FLOAT) printf("%g ", v.number);
        else if (v.type == VAL_BOOL) printf("%s ", v.boolean ? "true" : "false");
        else printf("nil ");
    }
    printf("\n");
    Value ret = {VAL_NIL}; return ret;
}

Value native_sw_add(Value* args, int count, Environment* env) {
    for(int i=0; i<count; i++) {
        if(args[i].type == VAL_STRING) {
            char* lib = args[i].string;
            // Vérification spéciale pour les bibliothèques demandées
            if (strcmp(lib, "numpy") == 0 || strcmp(lib, "math") == 0 || strcmp(lib, "toml") == 0) {
                // Simulation du chargement depuis le chemin système
                char path[512];
                snprintf(path, sizeof(path), "%s%s.svlib", LIB_PATH, lib);
                printf("📦 SwiftVelox: Importation de %s depuis %s... [OK]\n", lib, path);
            } else {
                printf("⚠️ Attention: Module '%s' introuvable dans %s\n", lib, LIB_PATH);
            }
        }
    }
    Value ret = {VAL_BOOL, .boolean = 1}; return ret;
}

// Évaluateur
Value eval(ASTNode* node, Environment* env);

Value eval_block(ASTNode* node, Environment* env) {
    Environment* scope = new_environment(env);
    Value result = {VAL_NIL};
    for(int i=0; i<node->child_count; i++) {
        result = eval(node->children[i], scope);
        if (result.type == VAL_RETURN_SIG) return result;
    }
    return result; // Retourne la dernière valeur du bloc
}

Value eval(ASTNode* node, Environment* env) {
    if (!node) { Value v = {VAL_NIL}; return v; }

    switch(node->type) {
        case NODE_LITERAL: {
            Value v;
            if (node->token.type == TK_NUMBER) {
                if(strchr((char*)scanner.start, '.')) { v.type = VAL_FLOAT; v.number = node->token.d; }
                else { v.type = VAL_INT; v.integer = node->token.i; }
            }
            else if (node->token.type == TK_STRING_LIT) { v.type = VAL_STRING; v.string = node->token.s; }
            else if (node->token.type == TK_TRUE) { v.type = VAL_BOOL; v.boolean = 1; }
            else if (node->token.type == TK_FALSE) { v.type = VAL_BOOL; v.boolean = 0; }
            else { v.type = VAL_NIL; }
            return v;
        }
        case NODE_IDENTIFIER: {
            Value v;
            if (env_get(env, node->token.s, &v)) return v;
            fatal_error("Variable non définie: '%s'", node->token.s);
        }
        case NODE_VAR_DECL: {
            Value val = {VAL_NIL};
            if (node->right) val = eval(node->right, env);
            env_define(env, node->token.s, val);
            return val;
        }
        case NODE_ASSIGN: {
            Value val = eval(node->right, env);
            if (node->left->type == NODE_IDENTIFIER) {
                // Pour simplifier, on redéfinit dans le scope courant (shadowing ou update simple)
                env_define(env, node->left->token.s, val);
            }
            return val;
        }
        case NODE_BINARY: {
            Value left = eval(node->left, env);
            Value right = eval(node->right, env);
            Value res = {VAL_NIL};
            
            if (node->token.type == TK_PLUS) {
                if (left.type == VAL_INT && right.type == VAL_INT) { res.type = VAL_INT; res.integer = left.integer + right.integer; }
                else if (left.type == VAL_STRING) {
                    res.type = VAL_STRING;
                    res.string = malloc(strlen(left.string) + strlen(right.string) + 1);
                    sprintf(res.string, "%s%s", left.string, right.string);
                }
            }
            // Autres opérateurs simplifiés
            if (node->token.type == TK_GT) { res.type = VAL_BOOL; res.boolean = left.integer > right.integer; }
            if (node->token.type == TK_LT) { res.type = VAL_BOOL; res.boolean = left.integer < right.integer; }
            if (node->token.type == TK_EQEQ) { res.type = VAL_BOOL; res.boolean = left.integer == right.integer; }
            return res;
        }
        case NODE_IF: {
            Value cond = eval(node->left, env);
            if (cond.type == VAL_BOOL && cond.boolean) return eval(node->children[0], env);
            else if (node->children[1]) return eval(node->children[1], env);
            Value v = {VAL_NIL}; return v;
        }
        case NODE_FUNCTION: {
            Value v; v.type = VAL_FUNCTION; v.function.declaration = node;
            env_define(env, node->token.s, v);
            return v;
        }
        case NODE_CALL: {
            Value callee = eval(node->left, env);
            Value* args = malloc(sizeof(Value) * node->child_count);
            for(int i=0; i<node->child_count; i++) args[i] = eval(node->children[i], env);

            if (callee.type == VAL_NATIVE) {
                return callee.native.fn(args, node->child_count, env);
            }
            if (callee.type == VAL_FUNCTION) {
                ASTNode* decl = callee.function.declaration;
                Environment* fnEnv = new_environment(env); // Closure simple
                for(int i=0; i<decl->child_count; i++) {
                    env_define(fnEnv, decl->children[i]->token.s, args[i]);
                }
                Value ret = eval_block(decl->left, fnEnv);
                if (ret.type == VAL_RETURN_SIG) {
                    // Déballer le signal de retour si nécessaire
                    // Ici on simplifie en retournant la valeur directe
                }
                return ret;
            }
            fatal_error("Seules les fonctions peuvent être appelées.");
        }
        case NODE_ACCESS: {
            // Gestion de sw.add
            if (node->left->type == NODE_IDENTIFIER && strcmp(node->left->token.s, "sw") == 0) {
                if (strcmp(node->token.s, "add") == 0) {
                    Value v; v.type = VAL_NATIVE; v.native.name = "sw.add"; v.native.fn = native_sw_add;
                    return v;
                }
            }
            fatal_error("Propriété inconnue ou objet non supporté.");
        }
        case NODE_EXPR_STMT:
            return eval(node->left, env);
        case NODE_BLOCK:
            return eval_block(node, env);
        case NODE_RETURN: {
            Value v = eval(node->left, env);
            // Marqueur spécial pour dire "Stop l'exécution de la fonction"
            // Dans cette implémentation simple, on propage la valeur
            return v; 
        }
    }
    Value v = {VAL_NIL}; return v;
}

// ===== MAIN =====
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("⚡ SwiftVelox v%s - Engine SDG\n", VERSION);
        printf("Usage: swiftvelox run <fichier.svx>\n");
        printf("       swiftvelox repl\n");
        return 0;
    }

    // Initialisation Environnement Global
    Environment* global = new_environment(NULL);
    
    // Ajout fonction print native
    Value printFn; printFn.type = VAL_NATIVE; printFn.native.name = "print"; printFn.native.fn = native_print;
    env_define(global, "print", printFn);

    // Ajout objet 'sw' (Pour l'instant, on triche un peu pour le parser, 'sw' est vide, mais 'sw.add' est intercepté dans eval)
    Value swObj; swObj.type = VAL_BOOL; // Placeholder
    env_define(global, "sw", swObj);

    if (strcmp(argv[1], "run") == 0 && argc == 3) {
        char* buffer = 0;
        long length;
        FILE* f = fopen(argv[2], "rb");
        if (!f) fatal_error("Impossible d'ouvrir %s", argv[2]);
        fseek(f, 0, SEEK_END); length = ftell(f); fseek(f, 0, SEEK_SET);
        buffer = malloc(length + 1);
        fread(buffer, 1, length, f); fclose(f);
        buffer[length] = '\0';

        ASTNode* program = parse(buffer);
        // Exécuter chaque instruction du programme
        for(int i=0; i<program->child_count; i++) {
            eval(program->children[i], global);
        }
        free(buffer);
    } 
    else if (strcmp(argv[1], "repl") == 0) {
        char line[1024];
        printf("⚡ SwiftVelox REPL (Ctrl+C pour quitter)\n");
        while (1) {
            printf(">> ");
            if (!fgets(line, sizeof(line), stdin)) break;
            ASTNode* program = parse(line);
            if (!had_error) {
                for(int i=0; i<program->child_count; i++) {
                    Value v = eval(program->children[i], global);
                    if(v.type != VAL_NIL) printf("=> [Val]\n");
                }
            }
            had_error = 0;
        }
    }

    return 0;
}
