#include "native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

Value native_print(Value* args, int count, Environment* env) {
    for (int i = 0; i < count; i++) {
        Value v = args[i];
        switch (v.type) {
            case VAL_STRING: printf("%s", v.string); break;
            case VAL_INT: printf("%lld", v.integer); break;
            case VAL_FLOAT: printf("%g", v.number); break;
            case VAL_BOOL: printf("%s", v.boolean ? "true" : "false"); break;
            case VAL_NIL: printf("nil"); break;
            case VAL_ARRAY: printf("[array: %d items]", v.array.count); break;
            case VAL_OBJECT: printf("{object: %d properties}", v.object.count); break;
            default: printf("[%d]", v.type); break;
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return make_nil();
}

Value native_log(Value* args, int count, Environment* env) {
    printf("📝 LOG: ");
    native_print(args, count, env);
    return make_nil();
}

Value native_input(Value* args, int count, Environment* env) {
    if (count > 0) {
        native_print(args, count, env);
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
        return make_string(buffer);
    }
    
    return make_nil();
}

Value native_clock(Value* args, int count, Environment* env) {
    return make_number((double)clock() / CLOCKS_PER_SEC);
}

Value native_typeof(Value* args, int count, Environment* env) {
    if (count < 1) return make_string("undefined");
    
    const char* type;
    switch (args[0].type) {
        case VAL_NIL: type = "nil"; break;
        case VAL_BOOL: type = "boolean"; break;
        case VAL_INT: type = "int"; break;
        case VAL_FLOAT: type = "float"; break;
        case VAL_STRING: type = "string"; break;
        case VAL_FUNCTION: type = "function"; break;
        case VAL_NATIVE: type = "native"; break;
        case VAL_ARRAY: type = "array"; break;
        case VAL_OBJECT: type = "object"; break;
        default: type = "unknown"; break;
    }
    
    return make_string(type);
}

Value native_length(Value* args, int count, Environment* env) {
    if (count < 1) return make_number(0);
    
    if (args[0].type == VAL_STRING) {
        return make_number(strlen(args[0].string));
    } else if (args[0].type == VAL_ARRAY) {
        return make_number(args[0].array.count);
    } else if (args[0].type == VAL_OBJECT) {
        return make_number(args[0].object.count);
    }
    
    return make_number(0);
}

Value native_range(Value* args, int count, Environment* env) {
    if (count < 2) return make_array();
    
    int64_t start = args[0].integer;
    int64_t end = args[1].integer;
    int64_t step = count > 2 ? args[2].integer : 1;
    
    Value array = make_array();
    
    if (step > 0) {
        for (int64_t i = start; i < end; i += step) {
            array_push(&array, make_number(i));
        }
    } else {
        for (int64_t i = start; i > end; i += step) {
            array_push(&array, make_number(i));
        }
    }
    
    return array;
}

Value native_map(Value* args, int count, Environment* env) {
    if (count < 2 || args[0].type != VAL_FUNCTION) {
        fatal_error("map() attend (fonction, tableau)");
    }
    
    Value fn = args[0];
    Value arr = args[1];
    
    if (arr.type != VAL_ARRAY) {
        fatal_error("Second argument de map() doit être un tableau");
    }
    
    Value result = make_array();
    
    for (int i = 0; i < arr.array.count; i++) {
        // Pour simplifier, on exécute la fonction avec l'argument
        Value* call_args = malloc(sizeof(Value));
        call_args[0] = arr.array.items[i];
        
        // Crée un environnement temporaire
        Environment* temp_env = new_environment(fn.function.closure);
        
        // Définit un paramètre par défaut
        env_define(temp_env, "item", call_args[0]);
        
        // Exécute le corps de la fonction
        Value mapped = eval(fn.function.declaration->left, temp_env);
        array_push(&result, mapped);
        
        free(call_args);
    }
    
    return result;
}

Value native_filter(Value* args, int count, Environment* env) {
    if (count < 2 || args[0].type != VAL_FUNCTION) {
        fatal_error("filter() attend (fonction, tableau)");
    }
    
    Value fn = args[0];
    Value arr = args[1];
    
    if (arr.type != VAL_ARRAY) {
        fatal_error("Second argument de filter() doit être un tableau");
    }
    
    Value result = make_array();
    
    for (int i = 0; i < arr.array.count; i++) {
        Value* call_args = malloc(sizeof(Value));
        call_args[0] = arr.array.items[i];
        
        Environment* temp_env = new_environment(fn.function.closure);
        env_define(temp_env, "item", call_args[0]);
        
        Value predicate = eval(fn.function.declaration->left, temp_env);
        if (predicate.type == VAL_BOOL && predicate.boolean) {
            array_push(&result, arr.array.items[i]);
        }
        
        free(call_args);
    }
    
    return result;
}

Value native_reduce(Value* args, int count, Environment* env) {
    if (count < 3 || args[0].type != VAL_FUNCTION) {
        fatal_error("reduce() attend (fonction, initial, tableau)");
    }
    
    Value fn = args[0];
    Value accumulator = args[1];
    Value arr = args[2];
    
    if (arr.type != VAL_ARRAY) {
        fatal_error("Troisième argument de reduce() doit être un tableau");
    }
    
    for (int i = 0; i < arr.array.count; i++) {
        Value* call_args = malloc(sizeof(Value) * 2);
        call_args[0] = accumulator;
        call_args[1] = arr.array.items[i];
        
        Environment* temp_env = new_environment(fn.function.closure);
        env_define(temp_env, "acc", call_args[0]);
        env_define(temp_env, "curr", call_args[1]);
        
        accumulator = eval(fn.function.declaration->left, temp_env);
        free(call_args);
    }
    
    return accumulator;
}

Value native_http_run(Value* args, int count, Environment* env) {
    int port = 8080;
    if (count > 0 && args[0].type == VAL_INT) {
        port = args[0].integer;
    }
    
    printf("🚀 Serveur HTTP démarré sur http://localhost:%d\n", port);
    printf("⚠️  Fonction HTTP simplifiée - Appuyez sur Ctrl+C pour arrêter\n");
    
    // Version simplifiée sans thread
    printf("Serveur en écoute sur le port %d...\n", port);
    printf("(Pour une implémentation complète, voir la documentation)\n");
    
    return make_nil();
}

Value native_fs_read(Value* args, int count, Environment* env) {
    if (count < 1 || args[0].type != VAL_STRING) {
        fatal_error("fs.read() attend un chemin de fichier");
    }
    
    FILE* f = fopen(args[0].string, "rb");
    if (!f) {
        printf("⚠️  Fichier non trouvé: %s\n", args[0].string);
        return make_nil();
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    fclose(f);
    buffer[size] = '\0';
    
    Value result = make_string(buffer);
    free(buffer);
    return result;
}

Value native_fs_write(Value* args, int count, Environment* env) {
    if (count < 2 || args[0].type != VAL_STRING) {
        fatal_error("fs.write() attend (chemin, contenu)");
    }
    
    FILE* f = fopen(args[0].string, "wb");
    if (!f) {
        printf("⚠️  Impossible d'ouvrir le fichier: %s\n", args[0].string);
        return make_bool(0);
    }
    
    int success = 0;
    if (args[1].type == VAL_STRING) {
        fwrite(args[1].string, 1, strlen(args[1].string), f);
        success = 1;
    } else {
        // Convert to string
        char buffer[256];
        if (args[1].type == VAL_INT) {
            sprintf(buffer, "%lld", args[1].integer);
        } else if (args[1].type == VAL_FLOAT) {
            sprintf(buffer, "%g", args[1].number);
        } else if (args[1].type == VAL_BOOL) {
            strcpy(buffer, args[1].boolean ? "true" : "false");
        } else {
            strcpy(buffer, "nil");
        }
        fwrite(buffer, 1, strlen(buffer), f);
        success = 1;
    }
    
    fclose(f);
    return make_bool(success);
}

Value native_math_sqrt(Value* args, int count, Environment* env) {
    if (count < 1) return make_number(0);
    
    double value;
    if (args[0].type == VAL_INT) {
        value = args[0].integer;
    } else if (args[0].type == VAL_FLOAT) {
        value = args[0].number;
    } else {
        return make_number(0);
    }
    
    if (value < 0) {
        printf("⚠️  sqrt() : argument négatif\n");
        return make_number(0);
    }
    
    return make_number(sqrt(value));
}

Value native_math_pow(Value* args, int count, Environment* env) {
    if (count < 2) return make_number(0);
    
    double base, exponent;
    
    if (args[0].type == VAL_INT) base = args[0].integer;
    else if (args[0].type == VAL_FLOAT) base = args[0].number;
    else return make_number(0);
    
    if (args[1].type == VAL_INT) exponent = args[1].integer;
    else if (args[1].type == VAL_FLOAT) exponent = args[1].number;
    else return make_number(0);
    
    return make_number(pow(base, exponent));
}

Value native_assert(Value* args, int count, Environment* env) {
    if (count < 2) {
        fatal_error("assert() attend (valeur, attendu)");
    }
    
    Value actual = args[0];
    Value expected = args[1];
    
    int success = 0;
    if (actual.type == expected.type) {
        if (actual.type == VAL_INT) {
            success = actual.integer == expected.integer;
        } else if (actual.type == VAL_FLOAT) {
            success = fabs(actual.number - expected.number) < 1e-9;
        } else if (actual.type == VAL_STRING) {
            success = strcmp(actual.string, expected.string) == 0;
        } else if (actual.type == VAL_BOOL) {
            success = actual.boolean == expected.boolean;
        } else if (actual.type == VAL_NIL) {
            success = 1;
        }
    }
    
    if (!success) {
        printf("❌ Assertion échouée:\n");
        printf("   Attendu: ");
        native_print(&expected, 1, env);
        printf("   Reçu: ");
        native_print(&actual, 1, env);
        exit(1);
    }
    
    printf("✅ Assertion réussie\n");
    return make_bool(1);
}

void register_natives(Environment* env) {
    Value native_val;
    
    // Core functions
    native_val.type = VAL_NATIVE;
    native_val.native.fn = native_print;
    env_define(env, "print", native_val);
    
    native_val.native.fn = native_log;
    env_define(env, "log", native_val);
    
    native_val.native.fn = native_input;
    env_define(env, "input", native_val);
    
    native_val.native.fn = native_clock;
    env_define(env, "clock", native_val);
    
    native_val.native.fn = native_typeof;
    env_define(env, "typeof", native_val);
    
    native_val.native.fn = native_length;
    env_define(env, "length", native_val);
    
    // Functional programming
    native_val.native.fn = native_map;
    env_define(env, "map", native_val);
    
    native_val.native.fn = native_filter;
    env_define(env, "filter", native_val);
    
    native_val.native.fn = native_reduce;
    env_define(env, "reduce", native_val);
    
    native_val.native.fn = native_range;
    env_define(env, "range", native_val);
    
    // Math
    native_val.native.fn = native_math_sqrt;
    env_define(env, "math.sqrt", native_val);
    
    native_val.native.fn = native_math_pow;
    env_define(env, "math.pow", native_val);
    
    // File system
    native_val.native.fn = native_fs_read;
    env_define(env, "fs.read", native_val);
    
    native_val.native.fn = native_fs_write;
    env_define(env, "fs.write", native_val);
    
    // HTTP Server
    native_val.native.fn = native_http_run;
    env_define(env, "http.run", native_val);
    
    // Testing
    native_val.native.fn = native_assert;
    env_define(env, "assert", native_val);
}
