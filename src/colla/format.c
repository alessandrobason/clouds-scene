#include "format.h"

#define STB_SPRINTF_DECORATE(name) stb_##name
#define STB_SPRINTF_NOUNALIGNED
#define STB_SPRINTF_IMPLEMENTATION
#include "stb/stb_sprintf.h"

#include "arena.h"

static char *fmt__stb_callback(const char *buf, void *ud, int len) {
    (void)len;
    printf("%s", buf);
    return (char *)ud;
}

int fmtPrint(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int out = fmtPrintv(fmt, args);
    va_end(args);
    return out;
}

int fmtPrintv(const char *fmt, va_list args) {
    char buffer[STB_SPRINTF_MIN] = {0};
    return stb_vsprintfcb(fmt__stb_callback, buffer, buffer, fmt, args);
}

int fmtBuffer(char *buffer, usize buflen, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int out = fmtBufferv(buffer, buflen, fmt, args);
    va_end(args);
    return out;
}

int fmtBufferv(char *buffer, usize buflen, const char *fmt, va_list args) {
    return stb_vsnprintf(buffer, (int)buflen, fmt, args);
}

#if 0
str_t fmtStr(Arena *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str_t out = fmtStrv(arena, fmt, args);
    va_end(args);
    return out;
}

str_t fmtStrv(Arena *arena, const char *fmt, va_list args) {
    va_list vcopy;
    va_copy(vcopy, args);
    int len = stb_vsnprintf(NULL, 0, fmt, vcopy);
    va_end(vcopy);

    char *buffer = alloc(arena, char, len + 1);
    stb_vsnprintf(buffer, len + 1, fmt, args);

    return (str_t){ .buf = buffer, .len = (usize)len };
}
#endif