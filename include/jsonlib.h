#ifndef JSONLIB_H
#define JSONLIB_H

#include "common.h"

// JSON value types
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_INT,
    JSON_FLOAT,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

// JSON value structure
typedef struct JsonValue JsonValue;
struct JsonValue {
    JsonType type;
    union {
        bool bool_val;
        int64_t int_val;
        double float_val;
        char* str_val;
        struct {
            JsonValue** elements;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            JsonValue** values;
            int count;
            int capacity;
        } object;
    } as;
};

// SwiftFlow JSON API
Value swiftflow_json_parse(const char* json_str);
char* swiftflow_json_stringify(Value value);
Value swiftflow_json_read_file(const char* filename);
bool swiftflow_json_write_file(const char* filename, Value value);

// Utility functions
JsonValue* json_parse_string(const char* str);
char* json_stringify(JsonValue* json);
void json_free(JsonValue* json);
JsonValue* swiftflow_value_to_json(Value value);
Value json_to_swiftflow_value(JsonValue* json);

// Package registration
void jsonlib_register(SwiftFlowInterpreter* interpreter);

#endif // JSONLIB_H
