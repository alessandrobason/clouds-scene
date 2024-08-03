#pragma once

#include <stdarg.h> // va_list
#include <string.h> // strlen

#include "collatypes.h"

typedef struct arena_t arena_t;

#define STR_NONE SIZE_MAX

typedef struct {
    char *buf;
    usize len;
} str_t;

typedef struct {
    const char *buf;
    usize len;
} strview_t;

// == STR_T ========================================================

#define str__1(arena, x)            \
    _Generic((x),                   \
        const char *: strInit,      \
        char *:       strInit,      \
        strview_t:    strInitView   \
    )(arena, x)

#define str__2(arena, cstr, clen) strInitLen(arena, cstr, clen)
#define str__impl(_1, _2, n, ...) str__##n

// either:
//     arena_t arena, [const] char *cstr, [usize len]
//     arena_t arena, strview_t view
#define str(arena, ...) str__impl(__VA_ARGS__, 2, 1, 0)(arena, __VA_ARGS__)

str_t strInit(arena_t *arena, const char *buf);
str_t strInitLen(arena_t *arena, const char *buf, usize len);
str_t strInitView(arena_t *arena, strview_t view);
str_t strFmt(arena_t *arena, const char *fmt, ...);
str_t strFmtv(arena_t *arena, const char *fmt, va_list args);

str_t strFromWChar(arena_t *arena, const wchar_t *src, usize srclen);

bool strEquals(str_t a, str_t b);
int strCompare(str_t a, str_t b);

str_t strDup(arena_t *arena, str_t src);
bool strIsEmpty(str_t ctx);

void strReplace(str_t *ctx, char from, char to);
// if len == SIZE_MAX, copies until end
strview_t strSub(str_t ctx, usize from, usize to);

void strLower(str_t *ctx);
void strUpper(str_t *ctx);

str_t strToLower(arena_t *arena, str_t ctx);
str_t strToUpper(arena_t *arena, str_t ctx);

// == STRVIEW_T ====================================================

#define strv__1(x)                  \
    _Generic((x),                   \
        char *:       strvInit,     \
        const char *: strvInit,     \
        str_t:        strvInitStr   \
    )(x)

#define strv__2(cstr, clen) strvInitLen(cstr, clen)
#define strv__impl(_1, _2, n, ...) strv__##n

#define strv(...) strv__impl(__VA_ARGS__, 2, 1, 0)(__VA_ARGS__)

strview_t strvInit(const char *cstr);
strview_t strvInitLen(const char *buf, usize size);
strview_t strvInitStr(str_t str);

bool strvIsEmpty(strview_t ctx);
bool strvEquals(strview_t a, strview_t b);
int strvCompare(strview_t a, strview_t b);

wchar_t *strvToWChar(arena_t *arena, strview_t ctx, usize *outlen);
TCHAR *strvToTChar(arena_t *arena, strview_t str);

strview_t strvRemovePrefix(strview_t ctx, usize n);
strview_t strvRemoveSuffix(strview_t ctx, usize n);
strview_t strvTrim(strview_t ctx);
strview_t strvTrimLeft(strview_t ctx);
strview_t strvTrimRight(strview_t ctx);

strview_t strvSub(strview_t ctx, usize from, usize to);

bool strvStartsWith(strview_t ctx, char c);
bool strvStartsWithView(strview_t ctx, strview_t view);

bool strvEndsWith(strview_t ctx, char c);
bool strvEndsWithView(strview_t ctx, strview_t view);

bool strvContains(strview_t ctx, char c);
bool strvContainsView(strview_t ctx, strview_t view);

usize strvFind(strview_t ctx, char c, usize from);
usize strvFindView(strview_t ctx, strview_t view, usize from);

usize strvRFind(strview_t ctx, char c, usize from_right);
usize strvRFindView(strview_t ctx, strview_t view, usize from_right);
