#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "json.h"

// Parseur JSON "Quick & Dirty" pour le MVP
// Ne gère que le premier niveau et les structures simples
char* json_extract(const char* json, const char* key) {
    if (!json || !key) return NULL;

    char search_pattern[256];
    // Cherche "key": ou "key" :
    snprintf(search_pattern, 256, "\"%s\"", key);
    
    char* pos = strstr(json, search_pattern);
    if (!pos) return NULL;

    // Avance après la clé
    pos += strlen(search_pattern);
    
    // Cherche le séparateur :
    pos = strchr(pos, ':');
    if (!pos) return NULL;
    pos++; // Passe le :

    // Skip whitespace
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;

    if (*pos == '"') {
        // C'est une string
        pos++; // Passe le "
        char* end = pos;
        // Cherche la fin de la string (attention aux échappements \" non gérés ici pour simplifier)
        while (*end && *end != '"') end++;
        
        if (*end == '"') {
            size_t len = end - pos;
            char* val = malloc(len + 1);
            strncpy(val, pos, len);
            val[len] = '\0';
            return val;
        }
    } else {
        // C'est un nombre, null, true ou false
        // Lit jusqu'à , ou } ou ] ou espace
        char* end = pos;
        while (*end && *end != ',' && *end != '}' && *end != ']' && 
               *end != ' ' && *end != '\n' && *end != '\r') {
            end++;
        }
        
        size_t len = end - pos;
        if (len > 0) {
            char* val = malloc(len + 1);
            strncpy(val, pos, len);
            val[len] = '\0';
            
            // Nettoyage si c'est "null" -> retourne NULL pour SwiftFlow
            if (strcmp(val, "null") == 0) {
                free(val);
                return NULL;
            }
            return val;
        }
    }
    return NULL;
}
