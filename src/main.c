#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Déclarations du compilateur
extern Token* tokenize(const char* source);
extern void generate_c_code(Token* tokens, const char* output_filename);
extern int compile_file(const char* input_file, const char* output_file);

void print_help() {
    printf("SwiftVelox Compiler v0.1.0\n");
    printf("Usage:\n");
    printf("  swiftvelox build <file.svx>     Compile un fichier\n");
    printf("  swiftvelox run <file.svx>       Compile et exécute\n");
    printf("  swiftvelox version              Affiche la version\n");
    printf("  swiftvelox help                 Affiche cette aide\n");
}

void compile_and_run(const char* filename) {
    char c_file[256];
    char exe_file[256];
    
    // Générer le nom des fichiers
    snprintf(c_file, sizeof(c_file), "%s.c", filename);
    snprintf(exe_file, sizeof(exe_file), "%s.out", filename);
    
    // Compiler
    if (!compile_file(filename, c_file)) {
        return;
    }
    
    // Compiler le C généré
    printf("Compilation du code C...\n");
    char command[512];
    snprintf(command, sizeof(command), 
             "cc -Os -s -o %s %s src/runtime.c 2>&1", 
             exe_file, c_file);
    
    int result = system(command);
    if (result != 0) {
        printf("❌ Erreur lors de la compilation C\n");
        return;
    }
    
    printf("✅ Exécutable créé: %s\n", exe_file);
    
    // Exécuter
    printf("🚀 Exécution...\n");
    snprintf(command, sizeof(command), "./%s", exe_file);
    system(command);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    const char* command = argv[1];
    
    if (strcmp(command, "version") == 0) {
        printf("SwiftVelox v0.1.0\n");
        printf("Compilateur écrit en C pur\n");
        printf("Sans dépendances externes\n");
    }
    else if (strcmp(command, "help") == 0) {
        print_help();
    }
    else if (strcmp(command, "build") == 0) {
        if (argc < 3) {
            printf("Erreur: Fichier .svx requis\n");
            return 1;
        }
        compile_file(argv[2], NULL);
    }
    else if (strcmp(command, "run") == 0) {
        if (argc < 3) {
            printf("Erreur: Fichier .svx requis\n");
            return 1;
        }
        compile_and_run(argv[2]);
    }
    else {
        // Si pas de commande, essayer de compiler le fichier directement
        compile_and_run(argv[1]);
    }
    
    return 0;
}
