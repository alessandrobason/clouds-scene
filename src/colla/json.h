#pragma once

#include "str.h"
#include "arena.h"

typedef enum {
    JSON_NULL,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_OBJECT,
} jsontype_e;

typedef enum {
    JSON_DEFAULT            = 0,
    JSON_NO_TRAILING_COMMAS = 1 << 0,
    JSON_NO_COMMENTS        = 1 << 1,
} jsonflags_e;

typedef struct jsonval_t jsonval_t;

typedef struct jsonval_t {
    jsonval_t *next;
    jsonval_t *prev;

    str_t key;

    union {
        jsonval_t *array;
        str_t string;
        double number;
        bool boolean;
        jsonval_t *object;
    };
    jsontype_e type;
} jsonval_t;

typedef jsonval_t *json_t;

json_t jsonParse(arena_t *arena, arena_t scratch, strview_t filename, jsonflags_e flags);
json_t jsonParseStr(arena_t *arena, strview_t jsonstr, jsonflags_e flags);

jsonval_t *jsonGet(jsonval_t *node, strview_t key);

#define json_check(val, js_type) ((val) && (val)->type == js_type)
#define json_for(name, arr) for (jsonval_t *name = json_check(arr, JSON_ARRAY) ? arr->array : NULL; name; name = name->next)
