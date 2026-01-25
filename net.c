// net.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include "common.h"
#include "net.h"

// Gestion simplifiée des valeurs AST (copié de io.c pour éviter les dépendances circulaires)
static double extract_number(ASTNode* node) {
    if (!node) return 0.0;
    if (node->type == NODE_INT) return (double)node->data.int_val;
    if (node->type == NODE_FLOAT) return node->data.float_val;
    return 0.0;
}

static char* extract_string(ASTNode* node) {
    if (!node) return NULL;
    if (node->type == NODE_STRING) return str_copy(node->data.str_val);
    if (node->type == NODE_IDENT && node->data.name) return str_copy(node->data.name);
    return NULL;
}

// === IMPLEMENTATION ===

void init_net_module(void) {
    printf("%s[NET MODULE]%s Initializing BSD Sockets...\n", COLOR_CYAN, COLOR_RESET);
}

// net.socket() -> retourne un fd (int)
void net_socket(ASTNode* node) {
    // Par défaut TCP (SOCK_STREAM)
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("%s[NET ERROR]%s Failed to create socket: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        return;
    }

    // On permet de réutiliser l'adresse rapidement
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Stockage du résultat (simulé ici par affichage, mais devra être assigné via swf.c)
    // Dans swf.c, l'exécution stockera le résultat dans la variable si elle est assignée
    // Pour l'instant, on imprime juste pour le debug, le vrai retour se fait via le mécanisme d'evalFloat
    printf("%s[NET]%s Socket created (fd=%d)\n", COLOR_GREEN, COLOR_RESET, fd);
    
    // Astuce: On stocke le FD dans le nœud pour que swf.c puisse le récupérer
    // C'est un hack temporaire, idéalement swf.c devrait gérer le retour
    node->data.int_val = fd; 
}

// net.connect(fd, ip, port)
void net_connect(ASTNode* node) {
    if (!node->left || !node->right || !node->third) return;

    int fd = (int)extract_number(node->left);
    char* ip = extract_string(node->right);
    int port = (int)extract_number(node->third);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("%s[NET ERROR]%s Invalid address: %s\n", COLOR_RED, COLOR_RESET, ip);
        free(ip);
        return;
    }

    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("%s[NET ERROR]%s Connection failed to %s:%d (%s)\n", 
               COLOR_RED, COLOR_RESET, ip, port, strerror(errno));
    } else {
        printf("%s[NET]%s Connected to %s:%d\n", COLOR_GREEN, COLOR_RESET, ip, port);
    }
    free(ip);
}

// net.listen(port) -> retourne server_fd
void net_listen(ASTNode* node) {
    if (!node->left) return;
    int port = (int)extract_number(node->left);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("%s[NET ERROR]%s Socket creation failed\n", COLOR_RED, COLOR_RESET);
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("%s[NET ERROR]%s Bind failed on port %d: %s\n", 
               COLOR_RED, COLOR_RESET, port, strerror(errno));
        close(server_fd);
        return;
    }

    if (listen(server_fd, 3) < 0) {
        printf("%s[NET ERROR]%s Listen failed\n", COLOR_RED, COLOR_RESET);
        close(server_fd);
        return;
    }

    printf("%s[NET]%s Server listening on port %d (fd=%d)\n", 
           COLOR_GREEN, COLOR_RESET, port, server_fd);
    
    // Hack pour retourner la valeur
    node->data.int_val = server_fd;
}

// net.accept(server_fd) -> retourne client_fd
void net_accept(ASTNode* node) {
    if (!node->left) return;
    int server_fd = (int)extract_number(node->left);

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    printf("%s[NET]%s Waiting for connection on fd=%d...\n", COLOR_CYAN, COLOR_RESET, server_fd);
    
    int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (new_socket < 0) {
        printf("%s[NET ERROR]%s Accept failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        return;
    }

    printf("%s[NET]%s Accepted connection from %s:%d (fd=%d)\n", 
           COLOR_GREEN, COLOR_RESET, 
           inet_ntoa(address.sin_addr), ntohs(address.sin_port), new_socket);

    node->data.int_val = new_socket;
}

// net.send(fd, string)
void net_send(ASTNode* node) {
    if (!node->left || !node->right) return;
    int fd = (int)extract_number(node->left);
    char* data = extract_string(node->right);

    if (!data) return;

    ssize_t sent = send(fd, data, strlen(data), 0);
    if (sent < 0) {
        printf("%s[NET ERROR]%s Send failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
    } else {
        printf("%s[NET]%s Sent %zd bytes\n", COLOR_GREEN, COLOR_RESET, sent);
    }
    free(data);
}

// net.recv(fd, size) -> retourne string
void net_recv(ASTNode* node) {
    if (!node->left) return;
    int fd = (int)extract_number(node->left);
    int size = 1024;
    if (node->right) size = (int)extract_number(node->right);
    
    if (size > 65535) size = 65535; // Limite de sécurité
    
    char* buffer = malloc(size + 1);
    ssize_t valread = recv(fd, buffer, size, 0);
    
    if (valread > 0) {
        buffer[valread] = '\0';
        // Pour retourner la string, on l'attache au noeud (hack temporaire)
        // Dans swf.c il faudra adapter evalString pour lire ça si c'est un noeud réseau
        node->data.str_val = buffer; 
    } else {
        free(buffer);
        node->data.str_val = NULL;
    }
}

// net.close(fd)
void net_close(ASTNode* node) {
    if (!node->left) return;
    int fd = (int)extract_number(node->left);
    close(fd);
    printf("%s[NET]%s Closed socket fd=%d\n", COLOR_GREEN, COLOR_RESET, fd);
}
