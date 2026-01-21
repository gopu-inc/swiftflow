// ======================================================
// EXTENSIONS SWIFTFLOW - Nouvelles fonctionnalités
// ======================================================

// Structure pour les listes
typedef struct {
    char** items;
    int capacity;
    int length;
} List;

// Créer une liste
List* list_create() {
    List* list = malloc(sizeof(List));
    list->capacity = 10;
    list->length = 0;
    list->items = malloc(list->capacity * sizeof(char*));
    return list;
}

// Ajouter à une liste
void list_append(List* list, const char* item) {
    if (list->length >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, list->capacity * sizeof(char*));
    }
    list->items[list->length] = strdup(item);
    list->length++;
}

// Afficher une liste
void list_print(List* list) {
    printf("[");
    for (int i = 0; i < list->length; i++) {
        printf("\"%s\"", list->items[i]);
        if (i < list->length - 1) printf(", ");
    }
    printf("]");
}

// Fonction input() - lecture utilisateur
char* swiftflow_input(const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    
    static char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        // Retirer le newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        return buffer;
    }
    return "";
}

// Opérateur 'in' pour vérifier si un élément est dans une liste
bool swiftflow_in(const char* item, List* list) {
    for (int i = 0; i < list->length; i++) {
        if (strcmp(item, list->items[i]) == 0) {
            return true;
        }
    }
    return false;
}
