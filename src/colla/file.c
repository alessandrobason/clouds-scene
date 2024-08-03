#include "file.h"

#include "warnings/colla_warn_beg.h"

#include "tracelog.h"
#include "format.h"

#if COLLA_WIN

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef FILE_BEGIN
#undef FILE_CURRENT
#undef FILE_END

#define FILE_BEGIN           0
#define FILE_CURRENT         1
#define FILE_END             2

static DWORD file__mode_to_access(filemode_e mode) {
    if (mode & FILE_APPEND) return FILE_APPEND_DATA;

    DWORD out = 0;
    if (mode & FILE_READ) out |= GENERIC_READ;
    if (mode & FILE_WRITE) out |= GENERIC_WRITE;
    return out;
}

static DWORD file__mode_to_creation(filemode_e mode) {
    if (mode == FILE_READ)  return OPEN_EXISTING;
    if (mode == FILE_WRITE) return CREATE_ALWAYS;
    return OPEN_ALWAYS;
}

bool fileExists(const char *name) {
    return GetFileAttributesA(name) != INVALID_FILE_ATTRIBUTES;
}

TCHAR *fileGetFullPath(arena_t *arena, strview_t filename) {
    TCHAR long_path_prefix[] = TEXT("\\\\?\\");
    const usize prefix_len = arrlen(long_path_prefix) - 1;

    uint8 tempbuf[4096];
    arena_t scratch = arenaMake(ARENA_STATIC, sizeof(tempbuf), tempbuf);

    TCHAR *rel_path = strvToTChar(&scratch, filename);
    DWORD pathlen = GetFullPathName(rel_path, 0, NULL, NULL);

    TCHAR *full_path = alloc(arena, TCHAR, pathlen + prefix_len + 1);
    memcpy(full_path, long_path_prefix, prefix_len * sizeof(TCHAR));

    GetFullPathName(rel_path, pathlen + 1, full_path + prefix_len, NULL);

    return full_path;
}

strview_t fileGetFilename(strview_t path) {
    usize last_lin = strvRFind(path, '/', 0);
    usize last_win = strvRFind(path, '\\', 0);
    last_lin = last_lin != SIZE_MAX ? last_lin : 0;
    last_win = last_win != SIZE_MAX ? last_win : 0;
    usize last = max(last_lin, last_win);
    return strvSub(path, last ? last + 1 : last, SIZE_MAX);
}

strview_t fileGetExtension(strview_t path) {
    usize ext_pos = strvRFind(path, '.', 0);
    return strvSub(path, ext_pos, SIZE_MAX);
}

void fileSplitPath(strview_t path, strview_t *dir, strview_t *name, strview_t *ext) {
    usize dir_lin = strvRFind(path, '/', 0);
    usize dir_win = strvRFind(path, '\\', 0);
    dir_lin = dir_lin != STR_NONE ? dir_lin : 0;
    dir_win = dir_win != STR_NONE ? dir_win : 0;
    usize dir_pos = max(dir_lin, dir_win);

    usize ext_pos = strvRFind(path, '.', 0);

    if (dir) {
        *dir = strvSub(path, 0, dir_pos);
    }
    if (name) {
        *name = strvSub(path, dir_pos ? dir_pos + 1 : dir_pos, ext_pos);
    }
    if (ext) {
        *ext = strvSub(path, ext_pos, SIZE_MAX);
    }
}

bool fileDelete(arena_t scratch, strview_t filename) {
    wchar_t *wfname = strvToWChar(&scratch, filename, NULL);
    return DeleteFileW(wfname);
}

file_t fileOpen(arena_t scratch, strview_t name, filemode_e mode) {
    TCHAR *full_path = fileGetFullPath(&scratch, name);

    HANDLE handle = CreateFile(
        full_path,
        file__mode_to_access(mode),
        FILE_SHARE_READ,
        NULL,
        file__mode_to_creation(mode),
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    return (file_t){
        .handle = (uintptr_t)handle,
    };
}

void fileClose(file_t ctx) {
    if (!fileIsValid(ctx)) return;
    CloseHandle((HANDLE)ctx.handle);
}

bool fileIsValid(file_t ctx) {
    return (HANDLE)ctx.handle != 0 && 
           (HANDLE)ctx.handle != INVALID_HANDLE_VALUE;
}

usize fileRead(file_t ctx, void *buf, usize len) {
    if (!fileIsValid(ctx)) return 0;
    DWORD read = 0;
    ReadFile((HANDLE)ctx.handle, buf, (DWORD)len, &read, NULL);
    return (usize)read;
}

usize fileWrite(file_t ctx, const void *buf, usize len) {
    if (!fileIsValid(ctx)) return 0;
    DWORD written = 0;
    WriteFile((HANDLE)ctx.handle, buf, (DWORD)len, &written, NULL);
    return (usize)written;
}

bool fileSeekEnd(file_t ctx) {
    if (!fileIsValid(ctx)) return false;
    DWORD result = SetFilePointer((HANDLE)ctx.handle, 0, NULL, FILE_END);
    return result != INVALID_SET_FILE_POINTER;
}

void fileRewind(file_t ctx) {
    if (!fileIsValid(ctx)) return;
    SetFilePointer((HANDLE)ctx.handle, 0, NULL, FILE_BEGIN);
}

usize fileTell(file_t ctx) {
    if (!fileIsValid(ctx)) return 0;
    LARGE_INTEGER tell = {0};
    BOOL result = SetFilePointerEx((HANDLE)ctx.handle, (LARGE_INTEGER){0}, &tell, FILE_CURRENT);
    return result == TRUE ? (usize)tell.QuadPart : 0;
}

usize fileSize(file_t ctx) {
    if (!fileIsValid(ctx)) return 0;
    LARGE_INTEGER size = {0};
    BOOL result = GetFileSizeEx((HANDLE)ctx.handle, &size);
    return result == TRUE ? (usize)size.QuadPart : 0;
}

uint64 fileGetTimeFP(file_t ctx) {
    if (!fileIsValid(ctx)) return 0;
    FILETIME time = {0};
    GetFileTime((HANDLE)ctx.handle, NULL, NULL, &time);
    ULARGE_INTEGER utime = {
        .HighPart = time.dwHighDateTime,
        .LowPart = time.dwLowDateTime,
    };
    return (uint64)utime.QuadPart;
}

#else

#include <stdio.h>

static const char *file__mode_to_stdio(filemode_e mode) {
    if (mode == FILE_READ)   return "rb";
    if (mode == FILE_WRITE)  return "wb";
    if (mode == FILE_APPEND) return "ab";
    if (mode == (FILE_READ | FILE_WRITE)) return "rb+";

    return "ab+";
}

bool fileExists(const char *name) {
    FILE *fp = fopen(name, "rb");
    bool exists = fp != NULL;
    fclose(fp);
    return exists;
}

file_t fileOpen(arena_t scratch, strview_t name, filemode_e mode) {
    str_t filename = str(&scratch, name);
    return (file_t) {
        .handle = (uintptr_t)fopen(filename.buf, file__mode_to_stdio(mode))
    };
}

void fileClose(file_t ctx) {
    FILE *fp = (FILE *)ctx.handle;
    if (fp) {
        fclose(fp);
    }
}

bool fileIsValid(file_t ctx) {
    return (FILE *)ctx.handle != NULL;
}

usize fileRead(file_t ctx, void *buf, usize len) {
    if (!fileIsValid(ctx)) return 0;
    return fread(buf, 1, len, (FILE *)ctx.handle);
}

usize fileWrite(file_t ctx, const void *buf, usize len) {
    if (!fileIsValid(ctx)) return 0;
    return fwrite(buf, 1, len, (FILE *)ctx.handle);
}

bool fileSeekEnd(file_t ctx) {
    if (!fileIsValid(ctx)) return false;
    return fseek((FILE *)ctx.handle, 0, SEEK_END) == 0;
}

void fileRewind(file_t ctx) {
    if (!fileIsValid(ctx)) return;
    fseek((FILE *)ctx.handle, 0, SEEK_SET);
}

usize fileTell(file_t ctx) {
    if (!fileIsValid(ctx)) return 0;
    return ftell((FILE *)ctx.handle);
}

usize fileSize(file_t ctx) {
    if (!fileIsValid(ctx)) return 0;
    FILE *fp = (FILE *)ctx.handle;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return (usize)len;
}

uint64 fileGetTimeFP(file_t ctx) {
#if COLLA_LIN
    return 0;
#else
    fatal("fileGetTime not implemented yet outside of linux and windows");
    return 0;
#endif
}

#endif

bool filePutc(file_t ctx, char c) {
    return fileWrite(ctx, &c, 1) == 1;
}

bool filePuts(file_t ctx, strview_t v) {
    return fileWrite(ctx, v.buf, v.len) == v.len;
}

bool filePrintf(arena_t scratch, file_t ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    bool result = filePrintfv(scratch, ctx, fmt, args);
    va_end(args);
    return result;
}

bool filePrintfv(arena_t scratch, file_t ctx, const char *fmt, va_list args) {
    str_t string = strFmtv(&scratch, fmt, args);
    return fileWrite(ctx, string.buf, string.len) == string.len;
}

buffer_t fileReadWhole(arena_t *arena, strview_t name) {
    file_t fp = fileOpen(*arena, name, FILE_READ);
    buffer_t out = fileReadWholeFP(arena, fp);
    fileClose(fp);
    return out;
}

buffer_t fileReadWholeFP(arena_t *arena, file_t ctx) {
    if (!fileIsValid(ctx)) return (buffer_t){0};
    buffer_t out = {0};

    out.len = fileSize(ctx);
    out.data = alloc(arena, uint8, out.len);
    usize read = fileRead(ctx, out.data, out.len);

    if (read != out.len) {
        err("fileReadWholeFP: fileRead failed, should be %zu but is %zu", out.len, read);
        arenaPop(arena, out.len);
        return (buffer_t){0};
    }

    return out;
}

str_t fileReadWholeStr(arena_t *arena, strview_t name) {
    file_t fp = fileOpen(*arena, name, FILE_READ);
    str_t out = fileReadWholeStrFP(arena, fp);
    fileClose(fp);
    return out;
}

str_t fileReadWholeStrFP(arena_t *arena, file_t ctx) {
    if (!fileIsValid(ctx)) return (str_t){0};

    str_t out = {0};

    out.len = fileSize(ctx);
    out.buf = alloc(arena, uint8, out.len + 1);
    usize read = fileRead(ctx, out.buf, out.len);

    if (read != out.len) {
        err("fileReadWholeFP: fileRead failed, should be %zu but is %zu", out.len, read);
        arenaPop(arena, out.len + 1);
        return (str_t){0};
    }

    return out;
}

bool fileWriteWhole(arena_t scratch, strview_t name, const void *buf, usize len) {
    file_t fp = fileOpen(scratch, name, FILE_WRITE);
    if (!fileIsValid(fp)) {
        return false;
    }
    usize written = fileWrite(fp, buf, len);
    fileClose(fp);
    return written == len;
}

uint64 fileGetTime(arena_t scratch, strview_t name) {
    file_t fp = fileOpen(scratch, name, FILE_READ);
    uint64 result = fileGetTimeFP(fp);
    fileClose(fp);
    return result;
}

bool fileHasChanged(arena_t scratch, strview_t name, uint64 last_timestamp) {
    uint64 timestamp = fileGetTime(scratch, name);
    return timestamp > last_timestamp;
}

#include "warnings/colla_warn_end.h"
