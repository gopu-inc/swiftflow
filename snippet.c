#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "common.h"

// ======================================================
// [SECTION] FICHIER ET SYSTÈME DE FICHIERS
// ======================================================

char* read_file(const char* filename, Error* error) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        set_error(error, 0, 0, filename, "Cannot open file: %s", strerror(errno));
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        set_error(error, 0, 0, filename, "Memory allocation failed");
        return NULL;
    }
    
    size_t bytes_read = fread(buffer, 1, size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    return buffer;
}

bool write_file(const char* filename, const char* content, Error* error) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        set_error(error, 0, 0, filename, "Cannot open file for writing: %s", strerror(errno));
        return false;
    }
    
    size_t len = strlen(content);
    size_t bytes_written = fwrite(content, 1, len, file);
    fclose(file);
    
    if (bytes_written != len) {
        set_error(error, 0, 0, filename, "Failed to write file completely");
        return false;
    }
    
    return true;
}

bool file_exists(const char* filename) {
    struct stat st;
    return stat(filename, &st) == 0;
}

bool is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool create_directory(const char* path, Error* error) {
    if (mkdir(path, 0755) != 0) {
        if (errno != EEXIST) {
            set_error(error, 0, 0, path, "Cannot create directory: %s", strerror(errno));
            return false;
        }
    }
    return true;
}

char** list_files(const char* directory, const char* extension, int* count) {
    DIR* dir = opendir(directory);
    if (!dir) {
        *count = 0;
        return NULL;
    }
    
    int capacity = 16;
    int idx = 0;
    char** files = malloc(capacity * sizeof(char*));
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        // Check extension if specified
        if (extension) {
            char* dot = strrchr(entry->d_name, '.');
            if (!dot || strcmp(dot, extension) != 0) continue;
        }
        
        if (idx >= capacity) {
            capacity *= 2;
            files = realloc(files, capacity * sizeof(char*));
        }
        
        files[idx] = str_copy(entry->d_name);
        idx++;
    }
    
    closedir(dir);
    *count = idx;
    
    if (idx == 0) {
        free(files);
        return NULL;
    }
    
    return files;
}

void free_file_list(char** files, int count) {
    for (int i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
}

// ======================================================
// [SECTION] MANIPULATION DE CHEMINS
// ======================================================

char* get_directory(const char* filepath) {
    char* path_copy = strdup(filepath);
    char* last_slash = strrchr(path_copy, '/');
    
    if (last_slash) {
        *last_slash = '\0';
        return path_copy;
    } else {
        free(path_copy);
        return str_copy(".");
    }
}

char* get_filename(const char* filepath) {
    const char* last_slash = strrchr(filepath, '/');
    if (last_slash) {
        return str_copy(last_slash + 1);
    } else {
        return str_copy(filepath);
    }
}

char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (dot) {
        return str_copy(dot);
    }
    return str_copy("");
}

char* change_extension(const char* filename, const char* new_ext) {
    char* result = malloc(strlen(filename) + strlen(new_ext) + 1);
    if (!result) return NULL;
    
    const char* dot = strrchr(filename, '.');
    if (dot) {
        size_t base_len = dot - filename;
        strncpy(result, filename, base_len);
        result[base_len] = '\0';
    } else {
        strcpy(result, filename);
    }
    
    strcat(result, new_ext);
    return result;
}

char* join_path(const char* dir, const char* file) {
    if (!dir || strlen(dir) == 0) return str_copy(file);
    if (!file || strlen(file) == 0) return str_copy(dir);
    
    size_t dir_len = strlen(dir);
    bool needs_slash = (dir[dir_len - 1] != '/');
    
    char* result = malloc(dir_len + strlen(file) + 2);
    if (!result) return NULL;
    
    strcpy(result, dir);
    if (needs_slash) strcat(result, "/");
    strcat(result, file);
    
    return result;
}

char* normalize_path(const char* path) {
    if (!path) return NULL;
    
    char* copy = strdup(path);
    char* components[256];
    int comp_count = 0;
    
    // Tokenize by '/'
    char* token = strtok(copy, "/");
    while (token) {
        if (strcmp(token, ".") == 0) {
            // Ignore current directory
        } else if (strcmp(token, "..") == 0) {
            // Go up one directory
            if (comp_count > 0) {
                comp_count--;
            }
        } else if (strlen(token) > 0) {
            components[comp_count++] = token;
        }
        token = strtok(NULL, "/");
    }
    
    // Reconstruct path
    char* result = malloc(strlen(path) + 2);
    if (!result) {
        free(copy);
        return NULL;
    }
    
    result[0] = '\0';
    
    // Add leading slash for absolute paths
    if (path[0] == '/') {
        strcat(result, "/");
    }
    
    for (int i = 0; i < comp_count; i++) {
        if (i > 0) strcat(result, "/");
        strcat(result, components[i]);
    }
    
    if (comp_count == 0 && result[0] != '/') {
        strcat(result, ".");
    }
    
    free(copy);
    return result;
}

// ======================================================
// [SECTION] TEMPS ET DATE
// ======================================================

char* get_current_time_string() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    char* buffer = malloc(32);
    if (buffer) {
        strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", tm_info);
    }
    return buffer;
}

char* get_timestamp() {
    time_t now = time(NULL);
    char* buffer = malloc(32);
    if (buffer) {
        snprintf(buffer, 32, "%ld", now);
    }
    return buffer;
}

double get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// ======================================================
// [SECTION] FORMATAGE ET AFFICHAGE
// ======================================================

void print_banner() {
    printf(CYAN "\n");
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                       SwiftFlow v%s - GoPU.inc © %d                    ║\n", 
           SWIFTFLOW_VERSION, SWIFTFLOW_YEAR);
    printf("║                 Fusion CLAIR & SYM - Complete Programming Language          ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    printf(RESET "\n");
}

void print_section(const char* title) {
    printf("\n" CYAN "╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ %-72s ║\n", title);
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n" RESET);
}

void print_success(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf(GREEN "[SUCCESS] " RESET);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void print_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf(CYAN "[INFO] " RESET);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void print_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf(YELLOW "[WARNING] " RESET);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void print_error_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, RED "[ERROR] " RESET);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void print_debug(const char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    printf(MAGENTA "[DEBUG] " RESET);
    vprintf(format, args);
    printf("\n");
    va_end(args);
#endif
}

void print_table_header(const char** headers, int column_count, const int* widths) {
    printf(CYAN "╔");
    for (int i = 0; i < column_count; i++) {
        for (int j = 0; j < widths[i] + 2; j++) printf("═");
        if (i < column_count - 1) printf("╦");
    }
    printf("╗\n");
    
    printf("║");
    for (int i = 0; i < column_count; i++) {
        printf(" %-*s ║", widths[i], headers[i]);
    }
    printf("\n");
    
    printf("╠");
    for (int i = 0; i < column_count; i++) {
        for (int j = 0; j < widths[i] + 2; j++) printf("═");
        if (i < column_count - 1) printf("╬");
    }
    printf("╣\n");
}

void print_table_row(const char** cells, int column_count, const int* widths) {
    printf("║");
    for (int i = 0; i < column_count; i++) {
        printf(" %-*s ║", widths[i], cells[i]);
    }
    printf("\n");
}

void print_table_footer(int column_count, const int* widths) {
    printf("╚");
    for (int i = 0; i < column_count; i++) {
        for (int j = 0; j < widths[i] + 2; j++) printf("═");
        if (i < column_count - 1) printf("╩");
    }
    printf("╝\n");
}

// ======================================================
// [SECTION] UTILITAIRES DIVERS
// ======================================================

int random_int(int min, int max) {
    return min + rand() % (max - min + 1);
}

double random_double(double min, double max) {
    return min + ((double)rand() / RAND_MAX) * (max - min);
}

bool random_bool() {
    return rand() % 2 == 0;
}

char* generate_id(int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char* id = malloc(length + 1);
    if (!id) return NULL;
    
    for (int i = 0; i < length; i++) {
        id[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    id[length] = '\0';
    
    return id;
}

char* indent_string(const char* str, int indent_level) {
    if (!str) return NULL;
    
    // Calculate indentation
    char indent[256] = {0};
    for (int i = 0; i < indent_level * 2 && i < 255; i++) {
        indent[i] = ' ';
    }
    
    // Count lines
    int line_count = 1;
    for (const char* p = str; *p; p++) {
        if (*p == '\n') line_count++;
    }
    
    // Allocate result
    size_t len = strlen(str) + (line_count * strlen(indent)) + 1;
    char* result = malloc(len);
    if (!result) return NULL;
    
    // Build indented string
    char* dest = result;
    dest[0] = '\0';
    
    const char* src = str;
    bool at_start_of_line = true;
    
    while (*src) {
        if (at_start_of_line && *src != '\n') {
            strcat(dest, indent);
            dest += strlen(indent);
            at_start_of_line = false;
        }
        
        *dest++ = *src;
        
        if (*src == '\n') {
            at_start_of_line = true;
        }
        
        src++;
    }
    
    *dest = '\0';
    return result;
}

char* trim_string(const char* str) {
    if (!str) return NULL;
    
    // Find start (skip leading whitespace)
    const char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    
    // Find end
    const char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    // Calculate length
    size_t len = end - start + 1;
    
    // Copy trimmed string
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, start, len);
    result[len] = '\0';
    
    return result;
}

char* repeat_string(const char* str, int count) {
    if (!str || count <= 0) return str_copy("");
    
    size_t len = strlen(str);
    char* result = malloc(len * count + 1);
    if (!result) return NULL;
    
    result[0] = '\0';
    
    for (int i = 0; i < count; i++) {
        strcat(result, str);
    }
    
    return result;
}

// ======================================================
// [SECTION] GESTION DE LA MÉMOIRE
// ======================================================

void* safe_malloc(size_t size, Error* error) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        set_error(error, 0, 0, NULL, "Memory allocation failed (requested %zu bytes)", size);
    }
    return ptr;
}

void* safe_calloc(size_t count, size_t size, Error* error) {
    void* ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        set_error(error, 0, 0, NULL, "Memory allocation failed (requested %zu * %zu bytes)", count, size);
    }
    return ptr;
}

void* safe_realloc(void* ptr, size_t size, Error* error) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        set_error(error, 0, 0, NULL, "Memory reallocation failed (requested %zu bytes)", size);
        free(ptr); // Free old pointer to avoid memory leak
        return NULL;
    }
    return new_ptr;
}

// ======================================================
// [SECTION] HACHAGE
// ======================================================

uint32_t hash_string(const char* str) {
    if (!str) return 0;
    
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str;
        hash *= 16777619u;
        str++;
    }
    return hash;
}

uint32_t hash_bytes(const void* data, size_t length) {
    if (!data || length == 0) return 0;
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 2166136261u;
    
    for (size_t i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    
    return hash;
}

// ======================================================
// [SECTION] VALIDATION ET VÉRIFICATION
// ======================================================

bool is_valid_identifier(const char* str) {
    if (!str || strlen(str) == 0) return false;
    
    // First character must be letter or underscore
    if (!isalpha((unsigned char)str[0]) && str[0] != '_' && str[0] != '$') {
        return false;
    }
    
    // Subsequent characters can be alphanumeric or underscore
    for (size_t i = 1; i < strlen(str); i++) {
        if (!isalnum((unsigned char)str[i]) && str[i] != '_' && str[i] != '$') {
            return false;
        }
    }
    
    return true;
}

bool is_numeric_string(const char* str) {
    if (!str || strlen(str) == 0) return false;
    
    // Check for integer
    char* endptr;
    strtol(str, &endptr, 10);
    if (*endptr == '\0') return true;
    
    // Check for float
    strtod(str, &endptr);
    if (*endptr == '\0') return true;
    
    return false;
}

bool is_hex_string(const char* str) {
    if (!str || strlen(str) < 3) return false;
    if (str[0] != '0' || (str[1] != 'x' && str[1] != 'X')) return false;
    
    for (size_t i = 2; i < strlen(str); i++) {
        if (!isxdigit((unsigned char)str[i])) return false;
    }
    
    return true;
}

bool is_binary_string(const char* str) {
    if (!str || strlen(str) < 3) return false;
    if (str[0] != '0' || (str[1] != 'b' && str[1] != 'B')) return false;
    
    for (size_t i = 2; i < strlen(str); i++) {
        if (str[i] != '0' && str[i] != '1') return false;
    }
    
    return true;
}

// ======================================================
// [SECTION] CONVERSIONS
// ======================================================

int string_to_int(const char* str, Error* error) {
    if (!str) {
        set_error(error, 0, 0, NULL, "Null string for conversion to int");
        return 0;
    }
    
    char* endptr;
    long result = strtol(str, &endptr, 10);
    
    if (*endptr != '\0') {
        set_error(error, 0, 0, NULL, "Invalid integer: %s", str);
        return 0;
    }
    
    return (int)result;
}

double string_to_double(const char* str, Error* error) {
    if (!str) {
        set_error(error, 0, 0, NULL, "Null string for conversion to double");
        return 0.0;
    }
    
    char* endptr;
    double result = strtod(str, &endptr);
    
    if (*endptr != '\0') {
        set_error(error, 0, 0, NULL, "Invalid float: %s", str);
        return 0.0;
    }
    
    return result;
}

bool string_to_bool(const char* str, Error* error) {
    if (!str) {
        set_error(error, 0, 0, NULL, "Null string for conversion to bool");
        return false;
    }
    
    if (strcmp(str, "true") == 0 || strcmp(str, "TRUE") == 0 || 
        strcmp(str, "True") == 0 || strcmp(str, "1") == 0) {
        return true;
    }
    
    if (strcmp(str, "false") == 0 || strcmp(str, "FALSE") == 0 || 
        strcmp(str, "False") == 0 || strcmp(str, "0") == 0) {
        return false;
    }
    
    set_error(error, 0, 0, NULL, "Invalid boolean: %s (expected true/false)", str);
    return false;
}

char* int_to_string(int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return str_copy(buffer);
}

char* double_to_string(double value) {
    char buffer[64];
    
    // Check if it's an integer
    if (value == (int64_t)value) {
        snprintf(buffer, sizeof(buffer), "%lld", (int64_t)value);
    } else {
        // Use scientific notation for very small/large numbers
        if (fabs(value) < 1e-6 || fabs(value) > 1e9) {
            snprintf(buffer, sizeof(buffer), "%e", value);
        } else {
            snprintf(buffer, sizeof(buffer), "%.10g", value);
        }
    }
    
    return str_copy(buffer);
}

char* bool_to_string(bool value) {
    return str_copy(value ? "true" : "false");
}

// ======================================================
// [SECTION] COMPARAISONS AVANCÉES
// ======================================================

int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int compare_ints(const void* a, const void* b) {
    return *(const int*)a - *(const int*)b;
}

int compare_doubles(const void* a, const void* b) {
    double diff = *(const double*)a - *(const double*)b;
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
}

void sort_strings(char** strings, int count) {
    qsort(strings, count, sizeof(char*), compare_strings);
}

void sort_ints(int* array, int count) {
    qsort(array, count, sizeof(int), compare_ints);
}

void sort_doubles(double* array, int count) {
    qsort(array, count, sizeof(double), compare_doubles);
}

// ======================================================
// [SECTION] UTILITAIRES MATHÉMATIQUES
// ======================================================

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int lcm(int a, int b) {
    return (a / gcd(a, b)) * b;
}

bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    
    return true;
}

int factorial(int n) {
    if (n < 0) return 0;
    if (n <= 1) return 1;
    
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

double clamp(double value, double min, double max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

double lerp(double a, double b, double t) {
    return a + t * (b - a);
}

double map_range(double value, double in_min, double in_max, double out_min, double out_max) {
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ======================================================
// [SECTION] EXÉCUTION DE COMMANDES SYSTÈME
// ======================================================

int execute_command(const char* command, Error* error) {
    printf(CYAN "[CMD]" RESET " %s\n", command);
    return system(command);
}

char* execute_command_capture(const char* command, Error* error) {
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        set_error(error, 0, 0, NULL, "Failed to execute command: %s", command);
        return NULL;
    }
    
    char buffer[4096];
    size_t total_size = 0;
    char* result = malloc(1);
    result[0] = '\0';
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t chunk_size = strlen(buffer);
        result = realloc(result, total_size + chunk_size + 1);
        strcpy(result + total_size, buffer);
        total_size += chunk_size;
    }
    
    pclose(pipe);
    return result;
}

// ======================================================
// [SECTION] GESTION DES ERREURS AVANCÉE
// ======================================================

void print_stack_trace() {
#ifdef DEBUG
    printf(RED "\n[STACK TRACE]\n" RESET);
    printf("(Stack trace not implemented in this version)\n");
#endif
}

void assert_condition(bool condition, const char* message, const char* file, int line) {
    if (!condition) {
        fprintf(stderr, RED "[ASSERT FAILED]" RESET " %s:%d: %s\n", file, line, message);
#ifdef DEBUG
        print_stack_trace();
#endif
        exit(EXIT_FAILURE);
    }
}

#define ASSERT(cond, msg) assert_condition(cond, msg, __FILE__, __LINE__)

// ======================================================
// [SECTION] PROFILING ET BENCHMARK
// ======================================================

typedef struct {
    char name[256];
    double start_time;
    double total_time;
    int call_count;
} ProfilerEntry;

static ProfilerEntry profiler_entries[100];
static int profiler_count = 0;

void profiler_start(const char* name) {
    for (int i = 0; i < profiler_count; i++) {
        if (strcmp(profiler_entries[i].name, name) == 0) {
            profiler_entries[i].start_time = get_current_time_ms();
            return;
        }
    }
    
    if (profiler_count < 100) {
        strncpy(profiler_entries[profiler_count].name, name, 255);
        profiler_entries[profiler_count].name[255] = '\0';
        profiler_entries[profiler_count].start_time = get_current_time_ms();
        profiler_entries[profiler_count].total_time = 0.0;
        profiler_entries[profiler_count].call_count = 0;
        profiler_count++;
    }
}

void profiler_stop(const char* name) {
    double end_time = get_current_time_ms();
    
    for (int i = 0; i < profiler_count; i++) {
        if (strcmp(profiler_entries[i].name, name) == 0) {
            double elapsed = end_time - profiler_entries[i].start_time;
            profiler_entries[i].total_time += elapsed;
            profiler_entries[i].call_count++;
            return;
        }
    }
}

void profiler_print_results() {
    printf(CYAN "\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                          PROFILING RESULTS                                  ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ Name                          │ Calls │ Total Time (ms) │ Avg Time (ms)     ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < profiler_count; i++) {
        double avg_time = profiler_entries[i].call_count > 0 ? 
                         profiler_entries[i].total_time / profiler_entries[i].call_count : 0.0;
        
        printf("║ %-30s │ %5d │ %15.3f │ %15.3f ║\n",
               profiler_entries[i].name,
               profiler_entries[i].call_count,
               profiler_entries[i].total_time,
               avg_time);
    }
    
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n" RESET);
}

void profiler_reset() {
    profiler_count = 0;
}
