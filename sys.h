#ifndef SYS_H
#define SYS_H
#include "common.h"
void init_sys_module(int argc, char** argv);
void sys_exec(ASTNode* node);   // ex: sys.exec("ls -la")
void sys_argv(ASTNode* node);   // ex: sys.argv(1) -> retourne l'argument 1
void sys_exit(ASTNode* node);   // ex: sys.exit(0)
#endif
