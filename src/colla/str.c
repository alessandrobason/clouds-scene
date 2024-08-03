#include "str.h"

#include "warnings/colla_warn_beg.h"

#include "arena.h"
#include "format.h"
#include "tracelog.h"

#if COLLA_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#include <wchar.h>

#endif

// == STR_T ========================================================

str_t strInit(arena_t *arena, const char *buf) {
    return buf ? strInitLen(arena, buf, strlen(buf)) : (str_t){0};
}

str_t strInitLen(arena_t *arena, const char *buf, usize len) {
    if (!buf || !len) return (str_t){0};

    str_t out = {
        .buf = alloc(arena, char, len + 1),
        .len = len
    };

    memcpy(out.buf, buf, len);

    return out;
}

str_t strInitView(arena_t *arena, strview_t view) {
    return strInitLen(arena, view.buf, view.len);
}

str_t strFmt(arena_t *arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str_t out = strFmtv(arena, fmt, args);
    va_end(args);
    return out;
}

str_t strFmtv(arena_t *arena, const char *fmt, va_list args) {
    va_list vcopy;
    va_copy(vcopy, args);
    // stb_vsnprintf returns the length + null_term
    int len = fmtBufferv(NULL, 0, fmt, vcopy);
    va_end(vcopy);

    char *buffer = alloc(arena, char, len + 1);
    fmtBufferv(buffer, len + 1, fmt, args);

    return (str_t) { .buf = buffer, .len = (usize)len };
}

str_t strFromWChar(arena_t *arena, const wchar_t *src, usize srclen) {
    if (!src)    return (str_t){0};
    if (!srclen) srclen = wcslen(src);

    str_t out = {0};

#if COLLA_WIN
    int outlen = WideCharToMultiByte(
        CP_UTF8, 0,
        src, (int)srclen,
        NULL, 0,
        NULL, NULL
    );

    if (outlen == 0) {
        unsigned long error = GetLastError();
        if (error == ERROR_NO_UNICODE_TRANSLATION) {
            err("couldn't translate wide string (%S) to utf8, no unicode translation", src);
        }
        else {
            err("couldn't translate wide string (%S) to utf8, %u", error);
        }

        return (str_t){0};
    }

    out.buf = alloc(arena, char, outlen + 1);
    WideCharToMultiByte(
        CP_UTF8, 0,
        src, (int)srclen,
        out.buf, outlen,
        NULL, NULL
    );

    out.len = outlen;
    
#elif COLLA_LIN
    fatal("strFromWChar not implemented yet!");
#endif

    return out;
}

bool strEquals(str_t a, str_t b) {
    return strCompare(a, b) == 0;
}

int strCompare(str_t a, str_t b) {
    return a.len == b.len ?
        memcmp(a.buf, b.buf, a.len) :
        (int)(a.len - b.len);
}

str_t strDup(arena_t *arena, str_t src) {
    return strInitLen(arena, src.buf, src.len);
}

bool strIsEmpty(str_t ctx) {
    return ctx.len == 0 || ctx.buf == NULL;
}

void strReplace(str_t *ctx, char from, char to) {
    if (!ctx) return;
    char *buf = ctx->buf;
    for (usize i = 0; i < ctx->len; ++i) {
        buf[i] = buf[i] == from ? to : buf[i];
    }
}

strview_t strSub(str_t ctx, usize from, usize to) {
    if (to > ctx.len) to = ctx.len;
    if (from > to)    from = to;
    return (strview_t){ ctx.buf + from, to - from };
}

void strLower(str_t *ctx) {
    char *buf = ctx->buf;
    for (usize i = 0; i < ctx->len; ++i) {
        buf[i] = (buf[i] >= 'A' && buf[i] <= 'Z') ?
                    buf[i] += 'a' - 'A' :
                    buf[i];
    }
}

void strUpper(str_t *ctx) {
    char *buf = ctx->buf;
    for (usize i = 0; i < ctx->len; ++i) {
        buf[i] = (buf[i] >= 'a' && buf[i] <= 'z') ?
                    buf[i] -= 'a' - 'A' :
                    buf[i];
    }
}

str_t strToLower(arena_t *arena, str_t ctx) {
    strLower(&ctx);
    return strDup(arena, ctx);
}

str_t strToUpper(arena_t *arena, str_t ctx) {
    strUpper(&ctx);
    return strDup(arena, ctx);
}


// == STRVIEW_T ====================================================

strview_t strvInit(const char *cstr) {
    return (strview_t){
        .buf = cstr,
        .len = cstr ? strlen(cstr) : 0,
    };
}

strview_t strvInitLen(const char *buf, usize size) {
    return (strview_t){
        .buf = buf,
        .len = size,
    };
}

strview_t strvInitStr(str_t str) {
    return (strview_t){
        .buf = str.buf,
        .len = str.len
    };
}


bool strvIsEmpty(strview_t ctx) {
    return ctx.len == 0 || !ctx.buf;
}

bool strvEquals(strview_t a, strview_t b) {
    return strvCompare(a, b) == 0;
}

int strvCompare(strview_t a, strview_t b) {
    return a.len == b.len ?
        memcmp(a.buf, b.buf, a.len) :
        (int)(a.len - b.len);
}

wchar_t *strvToWChar(arena_t *arena, strview_t ctx, usize *outlen) {
    wchar_t *out = NULL;
    int len = 0;

    if (strvIsEmpty(ctx)) {
        goto error;
    }

#if COLLA_WIN
    len = MultiByteToWideChar(
        CP_UTF8, 0,
        ctx.buf, (int)ctx.len,
        NULL, 0
    );

    if (len == 0) {
        unsigned long error = GetLastError();
        if (error == ERROR_NO_UNICODE_TRANSLATION) {
            err("couldn't translate string (%v) to a wide string, no unicode translation", ctx);
        }
        else {
            err("couldn't translate string (%v) to a wide string, %u", ctx, error);
        }

        goto error;
    }

    out = alloc(arena, wchar_t, len + 1);

    MultiByteToWideChar(
        CP_UTF8, 0,
        ctx.buf, (int)ctx.len,
        out, len
    );

#elif COLLA_LIN
    fatal("strFromWChar not implemented yet!");
#endif

error:
    if (outlen) {
        *outlen = (usize)len;
    }
    return out;
}

TCHAR *strvToTChar(arena_t *arena, strview_t str) {
#if UNICODE
    return strvToWChar(arena, str, NULL);
#else
    char *cstr = alloc(arena, char, str.len + 1);
    memcpy(cstr, str.buf, str.len);
    return cstr;
#endif
}

strview_t strvRemovePrefix(strview_t ctx, usize n) {
    if (n > ctx.len) n = ctx.len;
    return (strview_t){
        .buf = ctx.buf + n,
        .len = ctx.len - n,
    };
}

strview_t strvRemoveSuffix(strview_t ctx, usize n) {
    if (n > ctx.len) n = ctx.len;
    return (strview_t){
        .buf = ctx.buf,
        .len = ctx.len - n,
    };
}

strview_t strvTrim(strview_t ctx) {
    return strvTrimLeft(strvTrimRight(ctx));
}

strview_t strvTrimLeft(strview_t ctx) {
    strview_t out = ctx;
    for (usize i = 0; i < ctx.len; ++i) {
        char c = ctx.buf[i];
        if (c != ' ' && (c < '\t' || c > '\r')) {
            break;
        }
        out.buf++;
        out.len--;
    }
    return out;
}

strview_t strvTrimRight(strview_t ctx) {
    strview_t out = ctx;
    for (isize i = ctx.len - 1; i >= 0; --i) {
        char c = ctx.buf[i];
        if (c != ' ' && (c < '\t' || c > '\r')) {
            break;
        }
        out.len--;
    }
    return out;
}

strview_t strvSub(strview_t ctx, usize from, usize to) {
    if (to > ctx.len) to = ctx.len;
    if (from > to) from = to;
    return (strview_t){ ctx.buf + from, to - from };
}

bool strvStartsWith(strview_t ctx, char c) {
    return ctx.len > 0 && ctx.buf[0] == c;
}

bool strvStartsWithView(strview_t ctx, strview_t view) {
    return ctx.len >= view.len && memcmp(ctx.buf, view.buf, view.len) == 0;
}

bool strvEndsWith(strview_t ctx, char c) {
    return ctx.len > 0 && ctx.buf[ctx.len - 1] == c;
}

bool strvEndsWithView(strview_t ctx, strview_t view) {
    return ctx.len >= view.len && memcmp(ctx.buf + ctx.len, view.buf, view.len) == 0;
}

bool strvContains(strview_t ctx, char c) {
    for(usize i = 0; i < ctx.len; ++i) {
        if(ctx.buf[i] == c) {
            return true;
        }
    }
    return false;
}

bool strvContainsView(strview_t ctx, strview_t view) {
    if (ctx.len < view.len) return false;
    usize end = ctx.len - view.len;
    for (usize i = 0; i < end; ++i) {
        if (memcmp(ctx.buf + i, view.buf, view.len) == 0) {
            return true;
        }
    }
    return false;
}

usize strvFind(strview_t ctx, char c, usize from) {
    for (usize i = from; i < ctx.len; ++i) {
        if (ctx.buf[i] == c) {
            return i;
        }
    }
    return STR_NONE;
}

usize strvFindView(strview_t ctx, strview_t view, usize from) {
    if (ctx.len < view.len) return STR_NONE;
    usize end = ctx.len - view.len;
    for (usize i = from; i < end; ++i) {
        if (memcmp(ctx.buf + i, view.buf, view.len) == 0) {
            return i;
        }
    }
    return STR_NONE;
}

usize strvRFind(strview_t ctx, char c, usize from_right) {
    if (from_right > ctx.len) from_right = ctx.len;
    isize end = (isize)(ctx.len - from_right);
    for (isize i = end; i >= 0; --i) {
        if (ctx.buf[i] == c) {
            return (usize)i;
        }
    }
    return STR_NONE;
}

usize strvRFindView(strview_t ctx, strview_t view, usize from_right) {
    if (from_right > ctx.len) from_right = ctx.len;
    isize end = (isize)(ctx.len - from_right);
    if (end < (isize)view.len) return STR_NONE;
    for (isize i = end - view.len; i >= 0; --i) {
        if (memcmp(ctx.buf + i, view.buf, view.len) == 0) {
            return (usize)i;
        }
    }
    return STR_NONE;
}

#include "warnings/colla_warn_beg.h"

#undef CP_UTF8
#undef ERROR_NO_UNICODE_TRANSLATION