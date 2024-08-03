#pragma once

#include "collatypes.h"
#include "str.h"
#include "file.h"

typedef struct arena_t arena_t;

typedef struct inivalue_t {
    strview_t key;
    strview_t value;
    struct inivalue_t *next;
} inivalue_t;

typedef struct initable_t {
    strview_t name;
    inivalue_t *values;
    inivalue_t *tail;
    struct initable_t *next;
} initable_t;

typedef struct {
    strview_t text;
    initable_t *tables;
    initable_t *tail;
} ini_t;

typedef struct {
    bool merge_duplicate_tables; // default false
    bool merge_duplicate_keys;   // default false
    char key_value_divider;      // default =
} iniopts_t;

ini_t iniParse(arena_t *arena, strview_t filename, const iniopts_t *options);
ini_t iniParseFile(arena_t *arena, file_t file, const iniopts_t *options);
ini_t iniParseStr(arena_t *arena, strview_t str, const iniopts_t *options);

bool iniIsValid(ini_t *ctx);

#define INI_ROOT strv("__ROOT__")

initable_t *iniGetTable(ini_t *ctx, strview_t name);
inivalue_t *iniGet(initable_t *ctx, strview_t key);

typedef struct {
    strview_t *values;
    usize count;
} iniarray_t;

iniarray_t iniAsArr(arena_t *arena, inivalue_t *value, char delim);
uint64 iniAsUInt(inivalue_t *value);
int64 iniAsInt(inivalue_t *value);
double iniAsNum(inivalue_t *value);
bool iniAsBool(inivalue_t *value);