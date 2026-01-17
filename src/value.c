#include "value.h"
#include <stdlib.h>
#include <string.h>

// Helper pour strdup si non disponible
#ifndef strdup
char* my_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
#define strdup my_strdup
#endif

Value make_number(double value) {
    Value v;
    if (floor(value) == value) {
        v.type = VAL_INT;
        v.integer = (int64_t)value;
    } else {
        v.type = VAL_FLOAT;
        v.number = value;
    }
    return v;
}

Value make_string(const char* str) {
    Value v;
    v.type = VAL_STRING;
    v.string = strdup(str ? str : "");
    return v;
}

Value make_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.boolean = b;
    return v;
}

Value make_nil() {
    Value v;
    v.type = VAL_NIL;
    return v;
}

Value make_array() {
    Value v;
    v.type = VAL_ARRAY;
    v.array.items = NULL;
    v.array.count = 0;
    v.array.capacity = 0;
    return v;
}

void array_push(Value* array, Value item) {
    if (array->type != VAL_ARRAY) return;
    
    if (array->array.count >= array->array.capacity) {
        array->array.capacity = array->array.capacity == 0 ? 8 : array->array.capacity * 2;
        array->array.items = realloc(array->array.items, sizeof(Value) * array->array.capacity);
    }
    
    array->array.items[array->array.count++] = item;
}

Value make_object() {
    Value v;
    v.type = VAL_OBJECT;
    v.object.keys = NULL;
    v.object.values = NULL;
    v.object.count = 0;
    v.object.capacity = 0;
    return v;
}

void object_set(Value* obj, const char* key, Value value) {
    if (obj->type != VAL_OBJECT) return;
    
    for (int i = 0; i < obj->object.count; i++) {
        if (strcmp(obj->object.keys[i], key) == 0) {
            obj->object.values[i] = value;
            return;
        }
    }
    
    if (obj->object.count >= obj->object.capacity) {
        obj->object.capacity = obj->object.capacity == 0 ? 8 : obj->object.capacity * 2;
        obj->object.keys = realloc(obj->object.keys, sizeof(char*) * obj->object.capacity);
        obj->object.values = realloc(obj->object.values, sizeof(Value) * obj->object.capacity);
    }
    
    obj->object.keys[obj->object.count] = strdup(key);
    obj->object.values[obj->object.count] = value;
    obj->object.count++;
}
