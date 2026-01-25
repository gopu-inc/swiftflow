#ifndef SYS_H
#define SYS_H

void init_sys_module(int argc, char** argv);
char* sys_get_argv(int index);
int sys_exec_int(const char* cmd);

#endif
