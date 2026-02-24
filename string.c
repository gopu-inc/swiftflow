// string.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "string.h"

void init_string_module(void) {
    // Module chargé
}

int str_native_len(const char* str) {
    if (!str) return 0;
    return (int)strlen(str);
}

char* str_native_upper(const char* str) {
    if (!str) return NULL;
    char* res = strdup(str);
    for (int i = 0; res[i]; i++) {
        res[i] = toupper((unsigned char)res[i]);
    }
    return res;
}

char* str_native_lower(const char* str) {
    if (!str) return NULL;
    char* res = strdup(str);
    for (int i = 0; res[i]; i++) {
        res[i] = tolower((unsigned char)res[i]);
    }
    return res;
}

char* str_native_sub(const char* str, int start, int end) {
    if (!str) return NULL;
    int len = strlen(str);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return strdup("");

    int sub_len = end - start;
    char* res = malloc(sub_len + 1);
    strncpy(res, str + start, sub_len);
    res[sub_len] = '\0';
    return res;
}

int str_native_find(const char* str, const char* search) {
    if (!str || !search) return -1;
    char* pos = strstr(str, search);
    if (pos) return (int)(pos - str);
    return -1;
}

char* str_native_replace(const char* str, const char* old, const char* new_str) {
    if (!str || !old || !new_str) return str ? strdup(str) : NULL;
    
    char *result;
    int i, count = 0;
    size_t oldlen = strlen(old);
    size_t newlen = strlen(new_str);

    for (i = 0; str[i] != '\0'; i++) {
        if (strstr(&str[i], old) == &str[i]) {
            count++;
            i += oldlen - 1;
        }
    }

    result = (char *)malloc(i + count * (newlen - oldlen) + 1);
    if (!result) return NULL;

    i = 0;
    while (*str) {
        if (strstr(str, old) == str) {
            strcpy(&result[i], new_str);
            i += newlen;
            str += oldlen;
        } else {
            result[i++] = *str++;
        }
    }
    result[i] = '\0';
    return result;
}

