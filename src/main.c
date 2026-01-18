// main.c - Point d'entrée principal
#include "../include/compiler.h"
#include "../include/vm.h"
#include <string.h>

static void printHelp() {
    printf("SwiftVelox Compiler/Interpreter v1.0\n");
    printf("Usage:\n");
    printf("  swiftvelox run <file.swf>    - Exécuter un fichier\n");
    printf("  swiftvelox compile <file.swf> - Compiler en C\n");
    printf("  swiftvelox repl              - Lancer le REPL\n");
    printf("  swiftvelox debug <file.swf>  - Exécuter en mode debug\n");
    printf("  swiftvelox --help            - Afficher cette aide\n");
    printf("  swiftvelox --version         - Afficher la version\n");
}

static void printVersion() {
    printf("SwiftVelox v1.0.0\n");
    printf("Compilé le %s %s\n", __DATE__, __TIME__);
}

int main(int argc, char* argv[]) {
    printf("=========================================\n");
    printf("    SwiftVelox Compiler/VM v1.0\n");
    printf("=========================================\n\n");
    
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printHelp();
    } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printVersion();
    } else if (strcmp(argv[1], "run") == 0 && argc > 2) {
        runFile(argv[2], false);
    } else if (strcmp(argv[1], "debug") == 0 && argc > 2) {
        runFile(argv[2], true);
    } else if (strcmp(argv[1], "compile") == 0 && argc > 2) {
        FILE* file = fopen(argv[2], "r");
        if (!file) {
            printf("Erreur: impossible d'ouvrir %s\n", argv[2]);
            return 1;
        }
        
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, file);
        source[size] = '\0';
        fclose(file);
        
        CompilerState* state = createCompilerState(source);
        ASTNode* program = parseProgram(state);
        
        if (state->errorCount == 0) {
            char outputFile[256];
            snprintf(outputFile, sizeof(outputFile), "%s.c", argv[2]);
            compileToC(program, outputFile);
            printf("✅ Compilation réussie: %s\n", outputFile);
        } else {
            printf("❌ Erreurs de compilation: %d\n", state->errorCount);
        }
        
        freeAST(program);
        freeCompilerState(state);
        free(source);
    } else if (strcmp(argv[1], "repl") == 0) {
        repl();
    } else if (argc == 2 && strstr(argv[1], ".swf")) {
        runFile(argv[1], false);
    } else {
        printf("Commande non reconnue\n");
        printHelp();
        return 1;
    }
    
    return 0;
}