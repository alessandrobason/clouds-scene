#pragma once

#include <stdarg.h>

#include "collatypes.h"
#include "str.h"
#include "arena.h"

typedef enum {
    FILE_READ   = 1 << 0,
    FILE_WRITE  = 1 << 1,
    FILE_APPEND = 1 << 2,
} filemode_e;

typedef struct {
    uintptr_t handle;
} file_t;

bool fileExists(const char *name);
TCHAR *fileGetFullPath(arena_t *arena, strview_t filename);
strview_t fileGetFilename(strview_t path);
strview_t fileGetExtension(strview_t path);
void fileSplitPath(strview_t path, strview_t *dir, strview_t *name, strview_t *ext);
bool fileDelete(arena_t scratch, strview_t filename);

file_t fileOpen(arena_t scratch, strview_t name, filemode_e mode);
void fileClose(file_t ctx);

bool fileIsValid(file_t ctx);

bool filePutc(file_t ctx, char c);
bool filePuts(file_t ctx, strview_t v);
bool filePrintf(arena_t scratch, file_t ctx, const char *fmt, ...);
bool filePrintfv(arena_t scratch, file_t ctx, const char *fmt, va_list args);

usize fileRead(file_t ctx, void *buf, usize len);
usize fileWrite(file_t ctx, const void *buf, usize len);

bool fileSeekEnd(file_t ctx);
void fileRewind(file_t ctx);

usize fileTell(file_t ctx);
usize fileSize(file_t ctx);

buffer_t fileReadWhole(arena_t *arena, strview_t name);
buffer_t fileReadWholeFP(arena_t *arena, file_t ctx);

str_t fileReadWholeStr(arena_t *arena, strview_t name);
str_t fileReadWholeStrFP(arena_t *arena, file_t ctx);

bool fileWriteWhole(arena_t scratch, strview_t name, const void *buf, usize len);

uint64 fileGetTime(arena_t scratch, strview_t name);
uint64 fileGetTimeFP(file_t ctx);
bool fileHasChanged(arena_t scratch, strview_t name, uint64 last_timestamp);