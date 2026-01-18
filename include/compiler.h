// compiler.h - Interface du compilateur
#ifndef COMPILER_H
#define COMPILER_H

#include "common.h"

// Fonctions du compilateur
CompilerState* createCompilerState(const char* source);
void freeCompilerState(CompilerState* state);
ASTNode* parseProgram(CompilerState* state);
void freeAST(ASTNode* node);
void printAST(ASTNode* node, int depth);
int compileToC(ASTNode* ast, const char* outputFile);
Token* getNextToken(CompilerState* state);

#endif