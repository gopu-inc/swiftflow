// http.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "common.h"
#include "http.h"

struct string {
  char *ptr;
  size_t len;
};

void init_string(struct string *s) {
  s->len = 0;
  s->ptr = malloc(s->len + 1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
  size_t new_len = s->len + size * nmemb;
  s->ptr = realloc(s->ptr, new_len + 1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr + s->len, ptr, size * nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;
  
  return size * nmemb;
}

// Pour la barre de progression
int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
    if (dltotal <= 0) return 0;
    
    int barWidth = 50;
    double progress = dlnow / dltotal;
    int pos = (int)(barWidth * progress);

    printf("\r[");
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %d%%", (int)(progress * 100.0));
    fflush(stdout);
    return 0;
}

void init_http_module(void) {
    curl_global_init(CURL_GLOBAL_ALL);
    printf("%s[HTTP MODULE]%s Initialized libcurl\n", COLOR_CYAN, COLOR_RESET);
}

char* http_get(const char* url) {
    CURL *curl;
    CURLcode res;
    struct string s;
    init_string(&s);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Zarch-Client/1.0");
        
        // Pour HTTPS (désactive la vérif si besoin, mais mieux de laisser)
        // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            printf("%s[HTTP ERROR]%s GET failed: %s\n", COLOR_RED, COLOR_RESET, curl_easy_strerror(res));
            free(s.ptr);
            curl_easy_cleanup(curl);
            return NULL;
        }
        curl_easy_cleanup(curl);
        return s.ptr;
    }
    return NULL;
}

char* http_post(const char* url, const char* json_data) {
    CURL *curl;
    CURLcode res;
    struct string s;
    init_string(&s);

    curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Zarch-Client/1.0");

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            printf("%s[HTTP ERROR]%s POST failed: %s\n", COLOR_RED, COLOR_RESET, curl_easy_strerror(res));
            free(s.ptr);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return NULL;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return s.ptr;
    }
    return NULL;
}

size_t write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

char* http_download(const char* url, const char* output_filename) {
    CURL *curl;
    FILE *fp;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(output_filename, "wb");
        if (!fp) {
            printf("%s[HTTP ERROR]%s Cannot open file for writing: %s\n", COLOR_RED, COLOR_RESET, output_filename);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Zarch-Client/1.0");

        printf("Downloading %s...\n", output_filename);
        res = curl_easy_perform(curl);
        printf("\n"); // Nouvelle ligne après la barre de progression

        fclose(fp);
        curl_easy_cleanup(curl);

        if(res == CURLE_OK) {
            return str_copy("success");
        } else {
            printf("%s[HTTP ERROR]%s Download failed: %s\n", COLOR_RED, COLOR_RESET, curl_easy_strerror(res));
            remove(output_filename); // Supprimer le fichier incomplet
            return NULL;
        }
    }
    return NULL;
}
