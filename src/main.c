#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_TOKENS 10000
#define CODE_SIZE 4096

// Types
typedef enum {
    TK_FN, TK_LET, TK_IF, TK_ELSE, TK_RETURN,
    TK_IMPORT, TK_FOR, TK_WHILE, TK_TRUE, TK_FALSE,
    TK_I32, TK_I64, TK_STRING, TK_BOOL, TK_VOID,
    TK_IDENT, TK_INTLIT, TK_STRLIT,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_AND, TK_OR, TK_NOT, TK_ASSIGN,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_SEMICOLON, TK_COLON, TK_COMMA, TK_EOF, TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[256];
    int line;
    int col;
    long int_val;
} Token;

// Buffer de code machine
uint8_t machine_code[CODE_SIZE];
int code_pos = 0;

// Émettre un byte dans le code machine
void emit_byte(uint8_t b) {
    if (code_pos < CODE_SIZE) {
        machine_code[code_pos++] = b;
    }
}

// Émettre plusieurs bytes
void emit_bytes(const uint8_t* bytes, int count) {
    for (int i = 0; i < count && code_pos < CODE_SIZE; i++) {
        machine_code[code_pos++] = bytes[i];
    }
}

// Instructions x86-64
void emit_push_rbp() {
    emit_byte(0x55); // push rbp
}

void emit_pop_rbp() {
    emit_byte(0x5D); // pop rbp
}

void emit_mov_rbp_rsp() {
    emit_byte(0x48); emit_byte(0x89); emit_byte(0xE5); // mov rbp, rsp
}

void emit_ret() {
    emit_byte(0xC3); // ret
}

void emit_mov_rax_imm64(uint64_t value) {
    emit_byte(0x48); emit_byte(0xB8); // mov rax, imm64
    for (int i = 0; i < 8; i++) {
        emit_byte((value >> (i * 8)) & 0xFF);
    }
}

void emit_mov_rdi_imm64(uint64_t value) {
    emit_byte(0x48); emit_byte(0xBF); // mov rdi, imm64
    for (int i = 0; i < 8; i++) {
        emit_byte((value >> (i * 8)) & 0xFF);
    }
}

void emit_syscall() {
    emit_byte(0x0F); emit_byte(0x05); // syscall
}

// Lexer
Token* lexer(const char* source) {
    static Token tokens[MAX_TOKENS];
    int count = 0, i = 0, line = 1, col = 1;
    int len = source ? strlen(source) : 0;
    
    if (!source) {
        tokens[0].type = TK_EOF;
        return tokens;
    }
    
    while (i < len && count < MAX_TOKENS-1) {
        char ch = source[i];
        
        if (isspace(ch)) {
            if (ch == '\n') { line++; col = 1; }
            i++; col++;
            continue;
        }
        
        if (ch == '/' && i+1 < len && source[i+1] == '/') {
            i += 2;
            while (i < len && source[i] != '\n') i++;
            col = 1;
            continue;
        }
        
        Token* tok = &tokens[count];
        tok->line = line;
        tok->col = col;
        tok->lexeme[0] = '\0';
        
        // Identifiants
        if (isalpha(ch) || ch == '_') {
            int j = 0;
            while (i < len && (isalnum(source[i]) || source[i] == '_')) {
                if (j < 255) tok->lexeme[j++] = source[i];
                i++; col++;
            }
            tok->lexeme[j] = '\0';
            
            if (strcmp(tok->lexeme, "fn") == 0) tok->type = TK_FN;
            else if (strcmp(tok->lexeme, "let") == 0) tok->type = TK_LET;
            else if (strcmp(tok->lexeme, "if") == 0) tok->type = TK_IF;
            else if (strcmp(tok->lexeme, "else") == 0) tok->type = TK_ELSE;
            else if (strcmp(tok->lexeme, "return") == 0) tok->type = TK_RETURN;
            else if (strcmp(tok->lexeme, "sw") == 0) tok->type = TK_IMPORT;
            else if (strcmp(tok->lexeme, "swget") == 0) tok->type = TK_IDENT;
            else tok->type = TK_IDENT;
            
            count++;
            continue;
        }
        
        // Nombres
        if (isdigit(ch)) {
            int j = 0;
            while (i < len && isdigit(source[i])) {
                if (j < 255) tok->lexeme[j++] = source[i];
                i++; col++;
            }
            tok->lexeme[j] = '\0';
            tok->type = TK_INTLIT;
            tok->int_val = atol(tok->lexeme);
            count++;
            continue;
        }
        
        // Chaînes
        if (ch == '"') {
            int j = 0;
            i++; col++;
            while (i < len && source[i] != '"') {
                if (j < 255) tok->lexeme[j++] = source[i];
                i++; col++;
            }
            if (i < len && source[i] == '"') { i++; col++; }
            tok->lexeme[j] = '\0';
            tok->type = TK_STRLIT;
            count++;
            continue;
        }
        
        // Opérateurs
        switch (ch) {
            case '+': tok->type = TK_PLUS; strcpy(tok->lexeme, "+"); break;
            case '-': tok->type = TK_MINUS; strcpy(tok->lexeme, "-"); break;
            case '*': tok->type = TK_MUL; strcpy(tok->lexeme, "*"); break;
            case '/': tok->type = TK_DIV; strcpy(tok->lexeme, "/"); break;
            case '(': tok->type = TK_LPAREN; strcpy(tok->lexeme, "("); break;
            case ')': tok->type = TK_RPAREN; strcpy(tok->lexeme, ")"); break;
            case '{': tok->type = TK_LBRACE; strcpy(tok->lexeme, "{"); break;
            case '}': tok->type = TK_RBRACE; strcpy(tok->lexeme, "}"); break;
            case ';': tok->type = TK_SEMICOLON; strcpy(tok->lexeme, ";"); break;
            case ':': tok->type = TK_COLON; strcpy(tok->lexeme, ":"); break;
            case ',': tok->type = TK_COMMA; strcpy(tok->lexeme, ","); break;
            case '=': 
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_EQ;
                    strcpy(tok->lexeme, "==");
                    i++; col++;
                } else {
                    tok->type = TK_ASSIGN;
                    strcpy(tok->lexeme, "=");
                }
                break;
            case '!':
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_NEQ;
                    strcpy(tok->lexeme, "!=");
                    i++; col++;
                } else {
                    tok->type = TK_NOT;
                    strcpy(tok->lexeme, "!");
                }
                break;
            default: 
                tok->type = TK_ERROR;
                tok->lexeme[0] = ch;
                tok->lexeme[1] = '\0';
                break;
        }
        
        i++; col++;
        count++;
    }
    
    tokens[count].type = TK_EOF;
    tokens[count].lexeme[0] = '\0';
    
    return tokens;
}

// Générer un exécutable ELF64 simple
void write_elf_executable(const char* filename, uint8_t* code, int code_len) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    
    // En-tête ELF64 minimal
    uint8_t elf_header[] = {
        // e_ident
        0x7F, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // e_type, e_machine
        0x02, 0x00, 0x3E, 0x00,
        // e_version
        0x01, 0x00, 0x00, 0x00,
        // e_entry (0x400078 = début du code)
        0x78, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        // e_phoff (après l'en-tête)
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // e_shoff (pas de sections)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // e_flags
        0x00, 0x00, 0x00, 0x00,
        // e_ehsize
        0x40, 0x00,
        // e_phentsize
        0x38, 0x00,
        // e_phnum
        0x01, 0x00,
        // e_shentsize, e_shnum, e_shstrndx
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    // En-tête de programme
    uint8_t prog_header[] = {
        // p_type (PT_LOAD)
        0x01, 0x00, 0x00, 0x00,
        // p_flags (Read + Execute)
        0x05, 0x00, 0x00, 0x00,
        // p_offset
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_vaddr (0x400000)
        0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_paddr
        0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_filesz
        0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_memsz
        0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_align
        0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    // Code machine: syscall write + syscall exit
    uint8_t syscall_code[] = {
        // write(1, "Hello SwiftVelox!\n", 18)
        0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, // mov rax, 1 (syscall write)
        0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00, // mov rdi, 1 (stdout)
        0x48, 0x8D, 0x35, 0x00, 0x00, 0x00, 0x00, // lea rsi, [rel msg]
        0x48, 0xC7, 0xC2, 0x12, 0x00, 0x00, 0x00, // mov rdx, 18
        0x0F, 0x05,                               // syscall
        
        // exit(0)
        0x48, 0xC7, 0xC0, 0x3C, 0x00, 0x00, 0x00, // mov rax, 60 (syscall exit)
        0x48, 0xC7, 0xC7, 0x00, 0x00, 0x00, 0x00, // mov rdi, 0
        0x0F, 0x05,                               // syscall
        
        // Message
        'H', 'e', 'l', 'l', 'o', ' ', 'S', 'w', 'i', 'f', 't', 'V', 'e', 'l', 'o', 'x', '!', '\n'
    };
    
    // Écrire l'en-tête ELF
    fwrite(elf_header, 1, sizeof(elf_header), f);
    
    // Écrire l'en-tête de programme
    fwrite(prog_header, 1, sizeof(prog_header), f);
    
    // Écrire le code machine
    if (code_len > 0) {
        fwrite(code, 1, code_len, f);
    } else {
        fwrite(syscall_code, 1, sizeof(syscall_code), f);
    }
    
    // Remplir avec des zéros pour alignement
    uint8_t zero = 0;
    for (int i = 0; i < 100; i++) {
        fwrite(&zero, 1, 1, f);
    }
    
    fclose(f);
    
    // Rendre exécutable
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "chmod +x %s", filename);
    system(cmd);
}

// Générer du code machine direct
void generate_machine_code(Token* tokens) {
    code_pos = 0;
    
    // Prologue
    emit_push_rbp();
    emit_mov_rbp_rsp();
    
    // Analyser les tokens
    int i = 0;
    while (tokens[i].type != TK_EOF) {
        Token tok = tokens[i];
        
        if (tok.type == TK_IDENT && strcmp(tok.lexeme, "swget") == 0) {
            i += 2; // skip '('
            
            if (tokens[i].type == TK_STRLIT) {
                // Générer un appel système write pour afficher la chaîne
                char* msg = tokens[i].lexeme;
                int len = strlen(msg);
                
                // syscall write(1, msg, len)
                emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC0); // mov rax, 1
                emit_byte(0x01); emit_byte(0x00); emit_byte(0x00); emit_byte(0x00);
                
                emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC7); // mov rdi, 1
                emit_byte(0x01); emit_byte(0x00); emit_byte(0x00); emit_byte(0x00);
                
                // On stockera le message plus tard (pour simplifier)
                // Pour l'instant, message fixe
                uint8_t fixed_write[] = {
                    0x48, 0x8D, 0x35, 0x13, 0x00, 0x00, 0x00, // lea rsi, [rip + 0x13]
                    0x48, 0xC7, 0xC2, 0x06, 0x00, 0x00, 0x00, // mov rdx, 6
                    0x0F, 0x05                                // syscall
                };
                emit_bytes(fixed_write, sizeof(fixed_write));
            }
        }
        
        i++;
    }
    
    // syscall exit(0)
    emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC0); // mov rax, 60
    emit_byte(0x3C); emit_byte(0x00); emit_byte(0x00); emit_byte(0x00);
    
    emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC7); // mov rdi, 0
    emit_byte(0x00); emit_byte(0x00); emit_byte(0x00); emit_byte(0x00);
    
    emit_syscall();
    
    // Épilogue
    emit_pop_rbp();
    emit_ret();
    
    // Message "Hello\n"
    uint8_t hello_msg[] = {'H', 'e', 'l', 'l', 'o', '\n', 0};
    emit_bytes(hello_msg, sizeof(hello_msg));
}

int compile_direct(const char* input, const char* output) {
    printf("🔨 Compilation directe vers code machine: %s\n", input);
    
    FILE* f = fopen(input, "r");
    if (!f) {
        printf("❌ Fichier non trouvé\n");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    Token* tokens = lexer(source);
    
    // Générer le code machine
    generate_machine_code(tokens);
    
    // Écrire l'exécutable ELF
    if (!output) {
        output = "a.out";
    }
    
    write_elf_executable(output, machine_code, code_pos);
    
    free(source);
    
    printf("✅ Exécutable ELF64 créé: %s\n", output);
    printf("📏 Taille du code: %d bytes\n", code_pos);
    
    return 0;
}

void compile_and_run_direct(const char* filename) {
    char exe_name[256];
    snprintf(exe_name, sizeof(exe_name), "%s.bin", filename);
    
    if (compile_direct(filename, exe_name) == 0) {
        printf("🚀 Exécution...\n\n");
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "./%s", exe_name);
        system(cmd);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("SwiftVelox Native Compiler v1.0\n");
        printf("Compile directement vers code machine x86-64\n");
        printf("\nUsage: %s <file.svx>\n", argv[0]);
        printf("       %s run <file.svx>\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            printf("❌ Fichier requis\n");
            return 1;
        }
        compile_and_run_direct(argv[2]);
    } else {
        compile_and_run_direct(argv[1]);
    }
    
    return 0;
}
