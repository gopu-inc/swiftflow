#include "native.h"
#include <stdio.h>
#include <stdlib.h>

Value native_print(Value* args, int count, Environment* env) {
    for (int i = 0; i < count; i++) {
        Value v = args[i];
        switch (v.type) {
            case VAL_STRING: printf("%s", v.string); break;
            case VAL_INT: printf("%lld", v.integer); break;
            case VAL_FLOAT: printf("%g", v.number); break;
            case VAL_BOOL: printf("%s", v.boolean ? "true" : "false"); break;
            case VAL_NIL: printf("nil"); break;
            default: printf("[type:%d]", v.type); break;
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return make_nil();
}

Value native_http_run(Value* args, int count, Environment* env) {
    printf("🚀 Serveur HTTP (en développement)\n");
    printf("Port configuré: %lld\n", count > 0 ? args[0].integer : 8080);
    printf("⚠️  Fonctionnalité en cours d'implémentation\n");
    return make_nil();
}

void register_natives(Environment* env) {
    Value native_val;
    
    native_val.type = VAL_NATIVE;
    native_val.native.fn = native_print;
    env_define(env, "print", native_val);
    
    native_val.native.fn = native_http_run;
    env_define(env, "http.run", native_val);
}
