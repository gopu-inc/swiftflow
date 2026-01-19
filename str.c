#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>
#include "common.h"

// ======================================================
// [SECTION] STRUCTURES DE DONNÉES POUR CHAÎNES
// ======================================================

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} StringBuilder;

typedef struct {
    char** items;
    size_t count;
    size_t capacity;
} StringList;

// ======================================================
// [SECTION] STRINGBUILDER - CONSTRUCTION DE CHAÎNES
// ======================================================

StringBuilder* sb_create(size_t initial_capacity) {
    StringBuilder* sb = malloc(sizeof(StringBuilder));
    if (!sb) return NULL;
    
    sb->capacity = initial_capacity > 0 ? initial_capacity : 32;
    sb->data = malloc(sb->capacity);
    if (!sb->data) {
        free(sb);
        return NULL;
    }
    
    sb->data[0] = '\0';
    sb->length = 0;
    return sb;
}

void sb_free(StringBuilder* sb) {
    if (sb) {
        if (sb->data) free(sb->data);
        free(sb);
    }
}

void sb_clear(StringBuilder* sb) {
    if (sb) {
        sb->data[0] = '\0';
        sb->length = 0;
    }
}

static void sb_ensure_capacity(StringBuilder* sb, size_t needed) {
    if (sb->length + needed + 1 > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < sb->length + needed + 1) {
            new_capacity *= 2;
        }
        
        char* new_data = realloc(sb->data, new_capacity);
        if (new_data) {
            sb->data = new_data;
            sb->capacity = new_capacity;
        }
    }
}

void sb_append_char(StringBuilder* sb, char c) {
    if (!sb) return;
    sb_ensure_capacity(sb, 1);
    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
}

void sb_append_string(StringBuilder* sb, const char* str) {
    if (!sb || !str) return;
    
    size_t len = strlen(str);
    sb_ensure_capacity(sb, len);
    
    memcpy(sb->data + sb->length, str, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
}

void sb_append_format(StringBuilder* sb, const char* format, ...) {
    if (!sb || !format) return;
    
    va_list args;
    va_start(args, format);
    
    // Get required size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed > 0) {
        sb_ensure_capacity(sb, needed);
        
        // Actually format
        vsnprintf(sb->data + sb->length, sb->capacity - sb->length, format, args);
        sb->length += needed;
    }
    
    va_end(args);
}

char* sb_to_string(const StringBuilder* sb) {
    if (!sb || !sb->data) return NULL;
    return str_copy(sb->data);
}

const char* sb_c_str(const StringBuilder* sb) {
    return sb ? sb->data : NULL;
}

size_t sb_length(const StringBuilder* sb) {
    return sb ? sb->length : 0;
}

// ======================================================
// [SECTION] STRINGLIST - LISTE DE CHAÎNES
// ======================================================

StringList* sl_create(size_t initial_capacity) {
    StringList* sl = malloc(sizeof(StringList));
    if (!sl) return NULL;
    
    sl->capacity = initial_capacity > 0 ? initial_capacity : 16;
    sl->items = malloc(sl->capacity * sizeof(char*));
    if (!sl->items) {
        free(sl);
        return NULL;
    }
    
    sl->count = 0;
    return sl;
}

void sl_free(StringList* sl) {
    if (sl) {
        for (size_t i = 0; i < sl->count; i++) {
            free(sl->items[i]);
        }
        free(sl->items);
        free(sl);
    }
}

void sl_add(StringList* sl, const char* str) {
    if (!sl || !str) return;
    
    if (sl->count >= sl->capacity) {
        size_t new_capacity = sl->capacity * 2;
        char** new_items = realloc(sl->items, new_capacity * sizeof(char*));
        if (!new_items) return;
        
        sl->items = new_items;
        sl->capacity = new_capacity;
    }
    
    sl->items[sl->count++] = str_copy(str);
}

void sl_add_format(StringList* sl, const char* format, ...) {
    if (!sl || !format) return;
    
    va_list args;
    va_start(args, format);
    
    // Format string
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed > 0) {
        char* buffer = malloc(needed + 1);
        if (buffer) {
            vsnprintf(buffer, needed + 1, format, args);
            sl_add(sl, buffer);
            free(buffer);
        }
    }
    
    va_end(args);
}

void sl_remove(StringList* sl, size_t index) {
    if (!sl || index >= sl->count) return;
    
    free(sl->items[index]);
    
    // Shift remaining items
    for (size_t i = index; i < sl->count - 1; i++) {
        sl->items[i] = sl->items[i + 1];
    }
    
    sl->count--;
}

void sl_clear(StringList* sl) {
    if (!sl) return;
    
    for (size_t i = 0; i < sl->count; i++) {
        free(sl->items[i]);
    }
    sl->count = 0;
}

char* sl_join(const StringList* sl, const char* delimiter) {
    if (!sl || sl->count == 0) return str_copy("");
    
    // Calculate total length
    size_t total_len = 0;
    size_t delim_len = delimiter ? strlen(delimiter) : 0;
    
    for (size_t i = 0; i < sl->count; i++) {
        total_len += strlen(sl->items[i]);
        if (i < sl->count - 1 && delimiter) {
            total_len += delim_len;
        }
    }
    
    // Allocate and build string
    char* result = malloc(total_len + 1);
    if (!result) return NULL;
    
    char* ptr = result;
    for (size_t i = 0; i < sl->count; i++) {
        size_t item_len = strlen(sl->items[i]);
        memcpy(ptr, sl->items[i], item_len);
        ptr += item_len;
        
        if (i < sl->count - 1 && delimiter) {
            memcpy(ptr, delimiter, delim_len);
            ptr += delim_len;
        }
    }
    
    *ptr = '\0';
    return result;
}

StringList* sl_split(const char* str, const char* delimiter) {
    if (!str || !delimiter) return NULL;
    
    StringList* sl = sl_create(16);
    if (!sl) return NULL;
    
    size_t delim_len = strlen(delimiter);
    const char* start = str;
    const char* end;
    
    while ((end = strstr(start, delimiter)) != NULL) {
        size_t token_len = end - start;
        char* token = malloc(token_len + 1);
        if (token) {
            memcpy(token, start, token_len);
            token[token_len] = '\0';
            sl_add(sl, token);
            free(token);
        }
        start = end + delim_len;
    }
    
    // Add last token
    if (*start) {
        sl_add(sl, start);
    }
    
    return sl;
}

// ======================================================
// [SECTION] MANIPULATION DE CHAÎNES AVANCÉE
// ======================================================

char* str_replace(const char* str, const char* old, const char* new) {
    if (!str || !old || !new) return NULL;
    
    size_t old_len = strlen(old);
    if (old_len == 0) return str_copy(str);
    
    // Count occurrences
    size_t count = 0;
    const char* pos = str;
    while ((pos = strstr(pos, old)) != NULL) {
        count++;
        pos += old_len;
    }
    
    // Allocate result
    size_t new_len = strlen(new);
    size_t result_len = strlen(str) + count * (new_len - old_len);
    char* result = malloc(result_len + 1);
    if (!result) return NULL;
    
    // Perform replacement
    char* dest = result;
    const char* src = str;
    
    while (*src) {
        if (strstr(src, old) == src) {
            memcpy(dest, new, new_len);
            dest += new_len;
            src += old_len;
        } else {
            *dest++ = *src++;
        }
    }
    
    *dest = '\0';
    return result;
}

char* str_substring(const char* str, size_t start, size_t length) {
    if (!str) return NULL;
    
    size_t str_len = strlen(str);
    if (start >= str_len) return str_copy("");
    
    if (start + length > str_len) {
        length = str_len - start;
    }
    
    char* result = malloc(length + 1);
    if (!result) return NULL;
    
    memcpy(result, str + start, length);
    result[length] = '\0';
    return result;
}

char* str_trim_left(const char* str) {
    if (!str) return NULL;
    
    // Skip leading whitespace
    while (*str && isspace((unsigned char)*str)) str++;
    
    return str_copy(str);
}

char* str_trim_right(const char* str) {
    if (!str) return NULL;
    
    const char* end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) end--;
    
    size_t len = end - str + 1;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

char* str_trim(const char* str) {
    if (!str) return NULL;
    
    // Skip leading whitespace
    while (*str && isspace((unsigned char)*str)) str++;
    
    // Skip trailing whitespace
    const char* end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) end--;
    
    size_t len = end - str + 1;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

char* str_pad_left(const char* str, size_t length, char pad_char) {
    if (!str) return NULL;
    
    size_t str_len = strlen(str);
    if (str_len >= length) return str_copy(str);
    
    size_t pad_len = length - str_len;
    char* result = malloc(length + 1);
    if (!result) return NULL;
    
    memset(result, pad_char, pad_len);
    memcpy(result + pad_len, str, str_len);
    result[length] = '\0';
    
    return result;
}

char* str_pad_right(const char* str, size_t length, char pad_char) {
    if (!str) return NULL;
    
    size_t str_len = strlen(str);
    if (str_len >= length) return str_copy(str);
    
    size_t pad_len = length - str_len;
    char* result = malloc(length + 1);
    if (!result) return NULL;
    
    memcpy(result, str, str_len);
    memset(result + str_len, pad_char, pad_len);
    result[length] = '\0';
    
    return result;
}

char* str_reverse(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        result[i] = str[len - 1 - i];
    }
    result[len] = '\0';
    
    return result;
}

char* str_to_upper(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        result[i] = toupper((unsigned char)str[i]);
    }
    result[len] = '\0';
    
    return result;
}

char* str_to_lower(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        result[i] = tolower((unsigned char)str[i]);
    }
    result[len] = '\0';
    
    return result;
}

char* str_to_title(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    bool new_word = true;
    for (size_t i = 0; i < len; i++) {
        if (isspace((unsigned char)str[i])) {
            result[i] = str[i];
            new_word = true;
        } else if (new_word) {
            result[i] = toupper((unsigned char)str[i]);
            new_word = false;
        } else {
            result[i] = tolower((unsigned char)str[i]);
        }
    }
    result[len] = '\0';
    
    return result;
}

char* str_capitalize(const char* str) {
    if (!str || strlen(str) == 0) return str_copy("");
    
    char* result = str_to_lower(str);
    if (result) {
        result[0] = toupper((unsigned char)result[0]);
    }
    
    return result;
}

// ======================================================
// [SECTION] COMPARAISON ET RECHERCHE
// ======================================================

bool str_equals(const char* a, const char* b) {
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

bool str_equals_ignore_case(const char* a, const char* b) {
    if (!a || !b) return a == b;
    
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    
    return *a == *b;
}

bool str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    
    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);
    
    if (prefix_len > str_len) return false;
    
    return strncmp(str, prefix, prefix_len) == 0;
}

bool str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return false;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

bool str_contains(const char* str, const char* substring) {
    if (!str || !substring) return false;
    return strstr(str, substring) != NULL;
}

int str_count(const char* str, const char* substring) {
    if (!str || !substring || strlen(substring) == 0) return 0;
    
    int count = 0;
    const char* pos = str;
    size_t sub_len = strlen(substring);
    
    while ((pos = strstr(pos, substring)) != NULL) {
        count++;
        pos += sub_len;
    }
    
    return count;
}

int str_count_char(const char* str, char ch) {
    if (!str) return 0;
    
    int count = 0;
    while (*str) {
        if (*str == ch) count++;
        str++;
    }
    
    return count;
}

// ======================================================
// [SECTION] ENCODAGE ET DÉCODAGE
// ======================================================

char* str_encode_html(const char* str) {
    if (!str) return NULL;
    
    StringBuilder* sb = sb_create(strlen(str) * 2);
    if (!sb) return NULL;
    
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '&': sb_append_string(sb, "&amp;"); break;
            case '<': sb_append_string(sb, "&lt;"); break;
            case '>': sb_append_string(sb, "&gt;"); break;
            case '"': sb_append_string(sb, "&quot;"); break;
            case '\'': sb_append_string(sb, "&#39;"); break;
            default: sb_append_char(sb, *p); break;
        }
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}

char* str_decode_html(const char* str) {
    if (!str) return NULL;
    
    StringBuilder* sb = sb_create(strlen(str));
    if (!sb) return NULL;
    
    for (const char* p = str; *p; p++) {
        if (*p == '&') {
            if (strncmp(p, "&amp;", 5) == 0) {
                sb_append_char(sb, '&');
                p += 4;
            } else if (strncmp(p, "&lt;", 4) == 0) {
                sb_append_char(sb, '<');
                p += 3;
            } else if (strncmp(p, "&gt;", 4) == 0) {
                sb_append_char(sb, '>');
                p += 3;
            } else if (strncmp(p, "&quot;", 6) == 0) {
                sb_append_char(sb, '"');
                p += 5;
            } else if (strncmp(p, "&#39;", 5) == 0) {
                sb_append_char(sb, '\'');
                p += 4;
            } else {
                sb_append_char(sb, *p);
            }
        } else {
            sb_append_char(sb, *p);
        }
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}

char* str_encode_url(const char* str) {
    if (!str) return NULL;
    
    StringBuilder* sb = sb_create(strlen(str) * 3);
    if (!sb) return NULL;
    
    for (const char* p = str; *p; p++) {
        if (isalnum((unsigned char)*p) || *p == '-' || *p == '_' || 
            *p == '.' || *p == '~') {
            sb_append_char(sb, *p);
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)*p);
            sb_append_string(sb, hex);
        }
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}

char* str_decode_url(const char* str) {
    if (!str) return NULL;
    
    StringBuilder* sb = sb_create(strlen(str));
    if (!sb) return NULL;
    
    for (const char* p = str; *p; p++) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hex[3] = {p[1], p[2], '\0'};
            int value;
            sscanf(hex, "%02x", &value);
            sb_append_char(sb, (char)value);
            p += 2;
        } else if (*p == '+') {
            sb_append_char(sb, ' ');
        } else {
            sb_append_char(sb, *p);
        }
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}

char* str_encode_base64(const char* str) {
    if (!str) return NULL;
    
    const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    size_t len = strlen(str);
    size_t out_len = 4 * ((len + 2) / 3);
    char* result = malloc(out_len + 1);
    if (!result) return NULL;
    
    size_t i = 0, j = 0;
    unsigned char a, b, c;
    
    while (i < len) {
        a = (unsigned char)str[i++];
        b = (i < len) ? (unsigned char)str[i++] : 0;
        c = (i < len) ? (unsigned char)str[i++] : 0;
        
        unsigned int triple = (a << 16) + (b << 8) + c;
        
        result[j++] = base64_chars[(triple >> 18) & 0x3F];
        result[j++] = base64_chars[(triple >> 12) & 0x3F];
        result[j++] = base64_chars[(triple >> 6) & 0x3F];
        result[j++] = base64_chars[triple & 0x3F];
    }
    
    // Add padding
    if (len % 3 == 1) {
        result[j - 2] = '=';
        result[j - 1] = '=';
    } else if (len % 3 == 2) {
        result[j - 1] = '=';
    }
    
    result[out_len] = '\0';
    return result;
}

char* str_decode_base64(const char* str) {
    if (!str) return NULL;
    
    // Build decoding table
    unsigned char decoding_table[256];
    memset(decoding_table, 0xFF, 256);
    for (int i = 0; i < 64; i++) {
        decoding_table[(unsigned char)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    }
    
    size_t len = strlen(str);
    if (len % 4 != 0) return NULL;
    
    size_t out_len = len / 4 * 3;
    if (str[len - 1] == '=') out_len--;
    if (str[len - 2] == '=') out_len--;
    
    char* result = malloc(out_len + 1);
    if (!result) return NULL;
    
    size_t i = 0, j = 0;
    unsigned char a, b, c, d;
    
    while (i < len) {
        a = decoding_table[(unsigned char)str[i++]];
        b = decoding_table[(unsigned char)str[i++]];
        c = decoding_table[(unsigned char)str[i++]];
        d = decoding_table[(unsigned char)str[i++]];
        
        if (a == 0xFF || b == 0xFF || c == 0xFF || d == 0xFF) {
            free(result);
            return NULL;
        }
        
        unsigned int triple = (a << 18) + (b << 12) + (c << 6) + d;
        
        if (j < out_len) result[j++] = (triple >> 16) & 0xFF;
        if (j < out_len) result[j++] = (triple >> 8) & 0xFF;
        if (j < out_len) result[j++] = triple & 0xFF;
    }
    
    result[out_len] = '\0';
    return result;
}

// ======================================================
// [SECTION] FORMATAGE DE CHAÎNES
// ======================================================

char* str_format_size(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    size_t unit_index = 0;
    double size = (double)bytes;
    
    while (size >= 1024.0 && unit_index < 6) {
        size /= 1024.0;
        unit_index++;
    }
    
    char buffer[32];
    if (unit_index == 0) {
        snprintf(buffer, sizeof(buffer), "%zu %s", bytes, units[unit_index]);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
    }
    
    return str_copy(buffer);
}

char* str_format_time(double seconds) {
    if (seconds < 0.001) {
        // Microseconds
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f µs", seconds * 1000000.0);
        return str_copy(buffer);
    } else if (seconds < 1.0) {
        // Milliseconds
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f ms", seconds * 1000.0);
        return str_copy(buffer);
    } else if (seconds < 60.0) {
        // Seconds
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f s", seconds);
        return str_copy(buffer);
    } else if (seconds < 3600.0) {
        // Minutes and seconds
        int minutes = (int)(seconds / 60.0);
        double remaining = seconds - minutes * 60.0;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d m %.1f s", minutes, remaining);
        return str_copy(buffer);
    } else {
        // Hours, minutes, seconds
        int hours = (int)(seconds / 3600.0);
        int minutes = (int)((seconds - hours * 3600.0) / 60.0);
        double remaining = seconds - hours * 3600.0 - minutes * 60.0;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d h %d m %.1f s", hours, minutes, remaining);
        return str_copy(buffer);
    }
}

char* str_format_number(double number) {
    char buffer[64];
    
    if (number == (int64_t)number) {
        // Integer
        snprintf(buffer, sizeof(buffer), "%lld", (int64_t)number);
    } else if (fabs(number) < 1e-4 || fabs(number) > 1e9) {
        // Scientific notation for very small/large numbers
        snprintf(buffer, sizeof(buffer), "%e", number);
    } else {
        // Regular decimal
        snprintf(buffer, sizeof(buffer), "%.6g", number);
    }
    
    return str_copy(buffer);
}

char* str_wrap(const char* str, size_t line_width) {
    if (!str || line_width < 10) return str_copy(str);
    
    StringBuilder* sb = sb_create(strlen(str) + 100);
    if (!sb) return NULL;
    
    size_t current_line_length = 0;
    const char* word_start = str;
    const char* p = str;
    
    while (*p) {
        // Find next word
        while (*p && !isspace((unsigned char)*p)) p++;
        
        size_t word_length = p - word_start;
        
        if (current_line_length + word_length + 1 > line_width && current_line_length > 0) {
            // Need new line
            sb_append_char(sb, '\n');
            current_line_length = 0;
        } else if (current_line_length > 0) {
            // Add space between words
            sb_append_char(sb, ' ');
            current_line_length++;
        }
        
        // Add word
        for (const char* w = word_start; w < p; w++) {
            sb_append_char(sb, *w);
        }
        current_line_length += word_length;
        
        // Skip whitespace
        while (*p && isspace((unsigned char)*p)) p++;
        word_start = p;
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}

// ======================================================
// [SECTION] CHAÎNES UNICODE/UTF-8
// ======================================================

size_t str_utf8_length(const char* str) {
    if (!str) return 0;
    
    size_t len = 0;
    while (*str) {
        if ((*str & 0xC0) != 0x80) len++;
        str++;
    }
    return len;
}

char* str_utf8_substring(const char* str, size_t start, size_t length) {
    if (!str) return NULL;
    
    StringBuilder* sb = sb_create(length * 4);
    if (!sb) return NULL;
    
    size_t current = 0;
    const char* p = str;
    
    // Skip to start
    while (*p && current < start) {
        if ((*p & 0xC0) != 0x80) current++;
        p++;
    }
    
    // Copy length characters
    current = 0;
    while (*p && current < length) {
        // Get UTF-8 character length
        size_t char_len = 1;
        if ((*p & 0xE0) == 0xC0) char_len = 2;
        else if ((*p & 0xF0) == 0xE0) char_len = 3;
        else if ((*p & 0xF8) == 0xF0) char_len = 4;
        
        // Copy character
        for (size_t i = 0; i < char_len && p[i]; i++) {
            sb_append_char(sb, p[i]);
        }
        
        p += char_len;
        current++;
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}

bool str_is_valid_utf8(const char* str) {
    if (!str) return true;
    
    while (*str) {
        unsigned char c = (unsigned char)*str;
        
        if (c <= 0x7F) {
            // ASCII character
            str++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if ((str[1] & 0xC0) != 0x80) return false;
            str += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80) return false;
            str += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80 || 
                (str[3] & 0xC0) != 0x80) return false;
            str += 4;
        } else {
            return false;
        }
    }
    
    return true;
}

// ======================================================
// [SECTION] EXPRESSIONS RÉGULIÈRES SIMPLES
// ======================================================

typedef struct {
    char* pattern;
    bool is_wildcard;
} RegexPattern;

bool str_match_pattern(const char* str, const char* pattern) {
    if (!str || !pattern) return false;
    
    // Simple pattern matching with * and ?
    const char* s = str;
    const char* p = pattern;
    const char* star = NULL;
    const char* ss = s;
    
    while (*s) {
        if (*p == '?' || *p == *s) {
            s++;
            p++;
        } else if (*p == '*') {
            star = p++;
            ss = s;
        } else if (star) {
            p = star + 1;
            s = ++ss;
        } else {
            return false;
        }
    }
    
    // Skip remaining stars
    while (*p == '*') p++;
    
    return *p == '\0';
}

StringList* str_extract_matches(const char* str, const char* pattern) {
    // Simple extraction: find all occurrences of pattern
    StringList* sl = sl_create(16);
    if (!sl) return NULL;
    
    size_t pattern_len = strlen(pattern);
    if (pattern_len == 0) return sl;
    
    const char* pos = str;
    while ((pos = strstr(pos, pattern)) != NULL) {
        sl_add(sl, pattern);
        pos += pattern_len;
    }
    
    return sl;
}

// ======================================================
// [SECTION] HACHAGE DE CHAÎNES
// ======================================================

uint32_t str_hash_fnv1a(const char* str) {
    if (!str) return 0;
    
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str;
        hash *= 16777619u;
        str++;
    }
    return hash;
}

uint64_t str_hash_fnv1a_64(const char* str) {
    if (!str) return 0;
    
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint8_t)*str;
        hash *= 1099511628211ULL;
        str++;
    }
    return hash;
}

uint32_t str_hash_djb2(const char* str) {
    if (!str) return 0;
    
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (uint8_t)*str;
        str++;
    }
    return hash;
}

uint32_t str_hash_sdbm(const char* str) {
    if (!str) return 0;
    
    uint32_t hash = 0;
    while (*str) {
        hash = (uint8_t)*str + (hash << 6) + (hash << 16) - hash;
        str++;
    }
    return hash;
}

// ======================================================
// [SECTION] DIFFÉRENCE DE CHAÎNES (LEVENSHTEIN)
// ======================================================

int str_levenshtein_distance(const char* s1, const char* s2) {
    if (!s1 || !s2) return -1;
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    // Create matrix
    int** matrix = malloc((len1 + 1) * sizeof(int*));
    for (size_t i = 0; i <= len1; i++) {
        matrix[i] = malloc((len2 + 1) * sizeof(int));
        matrix[i][0] = i;
    }
    
    for (size_t j = 0; j <= len2; j++) {
        matrix[0][j] = j;
    }
    
    // Calculate distance
    for (size_t i = 1; i <= len1; i++) {
        for (size_t j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            matrix[i][j] = MIN(
                matrix[i - 1][j] + 1,      // deletion
                MIN(
                    matrix[i][j - 1] + 1,  // insertion
                    matrix[i - 1][j - 1] + cost  // substitution
                )
            );
        }
    }
    
    int result = matrix[len1][len2];
    
    // Free matrix
    for (size_t i = 0; i <= len1; i++) {
        free(matrix[i]);
    }
    free(matrix);
    
    return result;
}

double str_similarity(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0.0;
    
    int distance = str_levenshtein_distance(s1, s2);
    if (distance < 0) return 0.0;
    
    size_t max_len = MAX(strlen(s1), strlen(s2));
    if (max_len == 0) return 1.0;
    
    return 1.0 - ((double)distance / max_len);
}

// ======================================================
// [SECTION] COMPRESSION SIMPLE (RLE)
// ======================================================

char* str_compress_rle(const char* str) {
    if (!str) return NULL;
    
    StringBuilder* sb = sb_create(strlen(str));
    if (!sb) return NULL;
    
    const char* p = str;
    while (*p) {
        char current = *p;
        int count = 1;
        
        while (p[count] == current) {
            count++;
        }
        
        if (count > 3 || current == '\\' || current == ':' || current == ';') {
            // Compress
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "\\%c%d:", current, count);
            sb_append_string(sb, buffer);
        } else {
            // Don't compress
            for (int i = 0; i < count; i++) {
                sb_append_char(sb, current);
            }
        }
        
        p += count;
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}

char* str_decompress_rle(const char* str) {
    if (!str) return NULL;
    
    StringBuilder* sb = sb_create(strlen(str) * 2);
    if (!sb) return NULL;
    
    const char* p = str;
    while (*p) {
        if (*p == '\\' && p[2] == ':') {
            // Compressed sequence
            char ch = p[1];
            char* end;
            int count = strtol(p + 2, &end, 10);
            
            if (count > 0) {
                for (int i = 0; i < count; i++) {
                    sb_append_char(sb, ch);
                }
                p = end + 1; // Skip ':'
            } else {
                sb_append_char(sb, *p++);
            }
        } else {
            sb_append_char(sb, *p++);
        }
    }
    
    char* result = sb_to_string(sb);
    sb_free(sb);
    return result;
}
