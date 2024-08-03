#include "strstream.h"

#include "warnings/colla_warn_beg.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h> // HUGE_VALF

#include "tracelog.h"
#include "arena.h"

#if COLLA_WIN && COLLA_TCC
#define strtoull _strtoui64
#define strtoll _strtoi64
#define strtof strtod
#endif

/* == INPUT STREAM ============================================ */

instream_t istrInit(const char *str) {
    return istrInitLen(str, strlen(str));
}

instream_t istrInitLen(const char *str, usize len) {
    instream_t res;
    res.start = res.cur = str;
    res.size = len;
    return res;
}

char istrGet(instream_t *ctx) {
    return *ctx->cur++;
}

void istrIgnore(instream_t *ctx, char delim) {
    for (; !istrIsFinished(*ctx) && *ctx->cur != delim; ++ctx->cur) {

    }
}

void istrIgnoreAndSkip(instream_t *ctx, char delim) {
    istrIgnore(ctx, delim);
    istrSkip(ctx, 1);
}

char istrPeek(instream_t *ctx) {
    return *ctx->cur;
}

char istrPeekNext(instream_t *ctx) {
    usize offset = (ctx->cur - ctx->start) + 1;
    return offset > ctx->size ? '\0' : *(ctx->cur + 1);
}

void istrSkip(instream_t *ctx, usize n) {
    usize remaining = ctx->size - (ctx->cur - ctx->start);
    if(n > remaining) {
        return;
    }
    ctx->cur += n;
}

void istrSkipWhitespace(instream_t *ctx) {
    while (*ctx->cur && isspace(*ctx->cur)) {
        ++ctx->cur;
    }
}

void istrRead(instream_t *ctx, char *buf, usize len) {
    usize remaining = ctx->size - (ctx->cur - ctx->start);
    if(len > remaining) {
        warn("istrRead: trying to read len %zu from remaining %zu", len, remaining);
        return;
    }
    memcpy(buf, ctx->cur, len);
    ctx->cur += len;
}

usize istrReadMax(instream_t *ctx, char *buf, usize len) {
    usize remaining = ctx->size - (ctx->cur - ctx->start);
    len = remaining < len ? remaining : len;
    memcpy(buf, ctx->cur, len);
    ctx->cur += len;
    return len;
}

void istrRewind(instream_t *ctx) {
    ctx->cur = ctx->start;
}

void istrRewindN(instream_t *ctx, usize amount) {
    usize remaining = ctx->size - (ctx->cur - ctx->start);
    if (amount > remaining) amount = remaining;
    ctx->cur -= amount;
}

usize istrTell(instream_t ctx) {
    return ctx.cur - ctx.start;
}

usize istrRemaining(instream_t ctx) {
    return ctx.size - (ctx.cur - ctx.start);
}

bool istrIsFinished(instream_t ctx) {
    return (usize)(ctx.cur - ctx.start) >= ctx.size;
}

bool istrGetBool(instream_t *ctx, bool *val) {
    usize remaining = ctx->size - (ctx->cur - ctx->start);
    if(strncmp(ctx->cur, "true", remaining) == 0) {
        *val = true;
        return true;
    }
    if(strncmp(ctx->cur, "false", remaining) == 0) {
        *val = false;
        return true;
    }
    return false;
}

bool istrGetU8(instream_t *ctx, uint8 *val) {
    char *end = NULL;
    *val = (uint8) strtoul(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetU8: no valid conversion could be performed");
        return false;
    }
    else if(*val == UINT8_MAX) {
        warn("istrGetU8: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istrGetU16(instream_t *ctx, uint16 *val) {
    char *end = NULL;
    *val = (uint16) strtoul(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetU16: no valid conversion could be performed");
        return false;
    }
    else if(*val == UINT16_MAX) {
        warn("istrGetU16: value read is out of the range of representable values");
        return false;
    }
    
    ctx->cur = end;
    return true;
}

bool istrGetU32(instream_t *ctx, uint32 *val) {
    char *end = NULL;
    *val = (uint32) strtoul(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetU32: no valid conversion could be performed");
        return false;
    }
    else if(*val == UINT32_MAX) {
        warn("istrGetU32: value read is out of the range of representable values");
        return false;
    }
    
    ctx->cur = end;
    return true;
}

bool istrGetU64(instream_t *ctx, uint64 *val) {
    char *end = NULL;
    *val = strtoull(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetU64: no valid conversion could be performed");
        return false;
    }
    else if(*val == ULLONG_MAX) {
        warn("istrGetU64: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istrGetI8(instream_t *ctx, int8 *val) {
    char *end = NULL;
    *val = (int8) strtol(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetI8: no valid conversion could be performed");
        return false;
    }
    else if(*val == INT8_MAX || *val == INT8_MIN) {
        warn("istrGetI8: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istrGetI16(instream_t *ctx, int16 *val) {
    char *end = NULL;
    *val = (int16) strtol(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetI16: no valid conversion could be performed");
        return false;
    }
    else if(*val == INT16_MAX || *val == INT16_MIN) {
        warn("istrGetI16: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istrGetI32(instream_t *ctx, int32 *val) {
    char *end = NULL;
    *val = (int32) strtol(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetI32: no valid conversion could be performed");
        return false;
    }
    else if(*val == INT32_MAX || *val == INT32_MIN) {
        warn("istrGetI32: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istrGetI64(instream_t *ctx, int64 *val) {
    char *end = NULL;
    *val = strtoll(ctx->cur, &end, 0);
    
    if(ctx->cur == end) {
        warn("istrGetI64: no valid conversion could be performed");
        return false;
    }
    else if(*val == INT64_MAX || *val == INT64_MIN) {
        warn("istrGetI64: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istrGetFloat(instream_t *ctx, float *val) {
    char *end = NULL;
    *val = strtof(ctx->cur, &end);
    
    if(ctx->cur == end) {
        warn("istrGetFloat: no valid conversion could be performed");
        return false;
    }
    else if(*val == HUGE_VALF || *val == -HUGE_VALF) {
        warn("istrGetFloat: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

bool istrGetDouble(instream_t *ctx, double *val) {
    char *end = NULL;
    *val = strtod(ctx->cur, &end);
    
    if(ctx->cur == end) {
        warn("istrGetDouble: no valid conversion could be performed (%.5s)", ctx->cur);
        return false;
    }
    else if(*val == HUGE_VAL || *val == -HUGE_VAL) {
        warn("istrGetDouble: value read is out of the range of representable values");
        return false;
    }

    ctx->cur = end;
    return true;
}

str_t istrGetStr(arena_t *arena, instream_t *ctx, char delim) {
    const char *from = ctx->cur;
    istrIgnore(ctx, delim);
    // if it didn't actually find it, it just reached the end of the string
    if(*ctx->cur != delim) {
        return (str_t){0};
    }
    usize len = ctx->cur - from;
    str_t out = {
        .buf = alloc(arena, char, len + 1),
        .len = len
    };
    memcpy(out.buf, from, len);
    return out;
}

usize istrGetBuf(instream_t *ctx, char *buf, usize buflen) {
    usize remaining = ctx->size - (ctx->cur - ctx->start);
    buflen -= 1;
    buflen = remaining < buflen ? remaining : buflen;
    memcpy(buf, ctx->cur, buflen);
    buf[buflen] = '\0';
    ctx->cur += buflen;
    return buflen;
}

strview_t istrGetView(instream_t *ctx, char delim) {
    const char *from = ctx->cur;
    istrIgnore(ctx, delim);
    usize len = ctx->cur - from;
    return strvInitLen(from, len);
}

strview_t istrGetViewEither(instream_t *ctx, strview_t chars) {
    const char *from = ctx->cur;
    for (; !istrIsFinished(*ctx) && !strvContains(chars, *ctx->cur); ++ctx->cur) {

    }
    usize len = ctx->cur - from;
    return strvInitLen(from, len);
}

strview_t istrGetViewLen(instream_t *ctx, usize len) {
    const char *from = ctx->cur;
    istrSkip(ctx, len);
    usize buflen = ctx->cur - from;
    return (strview_t){ from, buflen };
}

/* == OUTPUT STREAM =========================================== */

void ostr__remove_null(outstream_t *o) {
    usize len = ostrTell(o);
    if (len && o->beg[len - 1] == '\0') {
        arenaPop(o->arena, 1);
    }
}

outstream_t ostrInit(arena_t *arena) {
    return (outstream_t){
        .beg = (char *)(arena ? arena->current : NULL),
        .arena = arena,
    };
}

void ostrClear(outstream_t *ctx) {
    arenaPop(ctx->arena, ostrTell(ctx));
}

usize ostrTell(outstream_t *ctx) {
    return ctx->arena ? (char *)ctx->arena->current - ctx->beg : 0;
}

char ostrBack(outstream_t *ctx) {
    usize len = ostrTell(ctx);
    return len ? ctx->beg[len - 1] : '\0';
}

str_t ostrAsStr(outstream_t *ctx) {
    bool is_null_terminated = ostrBack(ctx) == '\0' && false;
    return (str_t){
        .buf = ctx->beg,
        .len = ostrTell(ctx) - is_null_terminated
    };
}

strview_t ostrAsView(outstream_t *ctx) {
    bool is_null_terminated = ostrBack(ctx) == '\0';
    return (strview_t){
        .buf = ctx->beg,
        .len = ostrTell(ctx) - is_null_terminated
    };
}

void ostrPrintf(outstream_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ostrPrintfV(ctx, fmt, args);
    va_end(args);
}

void ostrPrintfV(outstream_t *ctx, const char *fmt, va_list args) {
    if (!ctx->arena) return;
    ostr__remove_null(ctx);
    strFmtv(ctx->arena, fmt, args);
    // remove null termination
    arenaPop(ctx->arena, 1);
}

void ostrPutc(outstream_t *ctx, char c) {
    if (!ctx->arena) return;
    ostr__remove_null(ctx);
    char *newc = alloc(ctx->arena, char);
    *newc = c;
}

void ostrPuts(outstream_t *ctx, strview_t v) {
    if (strvIsEmpty(v)) return;
    ostr__remove_null(ctx);
    str(ctx->arena, v);
}

void ostrAppendBool(outstream_t *ctx, bool val) {
    ostrPuts(ctx, val ? strv("true") : strv("false"));
}

void ostrAppendUInt(outstream_t *ctx, uint64 val) {
    ostrPrintf(ctx, "%I64u", val);
}

void ostrAppendInt(outstream_t *ctx, int64 val) {
    ostrPrintf(ctx, "%I64d", val);
}

void ostrAppendNum(outstream_t *ctx, double val) {
    ostrPrintf(ctx, "%g", val);
}

/* == OUT BYTE STREAM ========================================= */

obytestream_t obstrInit(arena_t *exclusive_arena) {
    return (obytestream_t){
        .beg = exclusive_arena ? exclusive_arena->current : NULL,
        .arena = exclusive_arena,
    };
}

void obstrClear(obytestream_t *ctx) {
    if (ctx->arena) {
        ctx->arena->current = ctx->beg;
    }
}

usize obstrTell(obytestream_t *ctx) {
    return ctx->arena ? ctx->arena->current - ctx->beg : 0;
}

buffer_t obstrAsBuf(obytestream_t *ctx) {
    return (buffer_t){ .data = ctx->beg, .len = obstrTell(ctx) };
}

void obstrWrite(obytestream_t *ctx, const void *buf, usize buflen) {
    uint8 *dst = alloc(ctx->arena, uint8, buflen);
    memcpy(dst, buf, buflen);
}

void obstrPuts(obytestream_t *ctx, strview_t str) {
    obstrWrite(ctx, str.buf, str.len);
}

void obstrAppendU8(obytestream_t *ctx, uint8 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

void obstrAppendU16(obytestream_t *ctx, uint16 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

void obstrAppendU32(obytestream_t *ctx, uint32 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

void obstrAppendU64(obytestream_t *ctx, uint64 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

void obstrAppendI8(obytestream_t *ctx, int8 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

void obstrAppendI16(obytestream_t *ctx, int16 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

void obstrAppendI32(obytestream_t *ctx, int32 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

void obstrAppendI64(obytestream_t *ctx, int64 value) { 
    obstrWrite(ctx, &value, sizeof(value)); 
}

/* == IN BYTE STREAM ========================================== */

ibytestream_t ibstrInit(const void *buf, usize len) {
    return (ibytestream_t) {
        .cur = buf,
        .start = buf,
        .size = len,
    };
}

usize ibstrRemaining(ibytestream_t *ctx) {
    return ctx->size - (ctx->cur - ctx->start);
}

usize ibstrTell(ibytestream_t *ctx) {
    return ctx->cur ? ctx->cur - ctx->start : 0;
}

usize ibstrRead(ibytestream_t *ctx, void *buf, usize buflen) {
    if (!ctx->cur) return 0;
    usize remaining = ibstrRemaining(ctx);
    if (buflen > remaining) buflen = remaining;
    memcpy(buf, ctx->cur, buflen);
    ctx->cur += buflen;
    return buflen;
}

uint8 ibstrGetU8(ibytestream_t *ctx) { 
    uint8 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

uint16 ibstrGetU16(ibytestream_t *ctx) { 
    uint16 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

uint32 ibstrGetU32(ibytestream_t *ctx) { 
    uint32 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

uint64 ibstrGetU64(ibytestream_t *ctx) { 
    uint64 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

int8 ibstrGetI8(ibytestream_t *ctx) { 
    int8 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

int16 ibstrGetI16(ibytestream_t *ctx) { 
    int16 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

int32 ibstrGetI32(ibytestream_t *ctx) { 
    int32 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

int64 ibstrGetI64(ibytestream_t *ctx) { 
    int64 value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

float ibstrGetFloat(ibytestream_t *ctx) { 
    float value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

double ibstrGetDouble(ibytestream_t *ctx) { 
    double value = 0;
    usize read = ibstrRead(ctx, &value, sizeof(value));
    return read == sizeof(value) ? value : 0;
}

strview_t ibstrGetView(ibytestream_t *ctx, usize lensize) {
    uint64 len = 0;
    usize read = ibstrRead(ctx, &len, lensize);
    if (read != lensize) {
        warn("couldn't read %zu bytes, instead read %zu for string", lensize, read);
        return (strview_t){0};
    }
    usize remaining = ibstrRemaining(ctx);
    if (len > remaining) {
        warn("trying to read a string of length %zu, but only %zu bytes remaining", len, remaining);
        len = remaining;
    }
    
    const char *str = (const char *)ctx->cur;
    ctx->cur += len;
    
    return (strview_t){
        .buf = str,
        .len = len,
    };
}

#include "warnings/colla_warn_end.h"