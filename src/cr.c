#include "cr.h"

#include "colla/arena.h"
#include "colla/file.h"
#include "colla/tracelog.h"

// todo linux support?
#if COLLA_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
// patch stuff up for cross platform for now, the actual program should not really call anything for now
#define HMODULE void*
#endif

typedef int (*cr_f)(cr_t *ctx);

typedef struct {
    arena_t arena;
    str_t path;
    uint64 last_timestamp;
    HMODULE handle;
    cr_f cr_init;
    cr_f cr_loop;
    cr_f cr_close;
} crinternal_t;

static bool cr_file_copy(arena_t scratch, strview_t src, strview_t dst) {
    buffer_t srcbuf = fileReadWhole(&scratch, src);
    if (srcbuf.data == NULL || srcbuf.len == 0) {
        err("fileReadWhole(%v) returned an empty buffer", src);
        return false;
    }
    if (!fileWriteWhole(scratch, dst, srcbuf.data, srcbuf.len)) {
        err("fileWriteWhole failed");
        return false;
    }
    return true;
}

static bool cr_reload(cr_t *ctx) {
#ifndef CR_DISABLE
    crinternal_t *cr = ctx->p;
    arena_t scratch = cr->arena;

    if (!fileExists(cr->path.buf)) {
        err("dll file %v does not exist anymore!", cr->path);
        return false;
    }

    uint64 now = fileGetTime(scratch, strv(cr->path));
    if (now <= cr->last_timestamp) {
        return false;
    }

    ctx->version = ctx->last_working_version + 1;

    // can't copy the dll directly, make a temporary one based on the version
    strview_t dir, name, ext;
    fileSplitPath(strv(cr->path), &dir, &name, &ext);
    str_t dll = strFmt(&scratch, "%v/%v-%d%v", dir, name, ctx->version, ext);

    if (!cr_file_copy(scratch, strv(cr->path), strv(dll))) {
        err("failed to copy %v to %v", cr->path, dll);
        return false;
    }

    info("loading library: %v", dll);

    if (cr->handle) FreeLibrary(cr->handle);

    cr->handle = LoadLibraryA(dll.buf);
    if (!cr->handle) {
        err("couldn't load %v: %u", dll, GetLastError());
        return true;
    }

    cr->cr_init = (cr_f)GetProcAddress(cr->handle, "cr_init");
    DWORD init_err = GetLastError();
    cr->cr_loop = (cr_f)GetProcAddress(cr->handle, "cr_loop");
    DWORD loop_err = GetLastError();
    cr->cr_close = (cr_f)GetProcAddress(cr->handle, "cr_close");
    DWORD close_err = GetLastError();

    if (!cr->cr_init) {
        err("couldn't load address for cr_init: %u", init_err);
        goto error;
    }

    if (!cr->cr_loop) {
        err("couldn't load address for cr_loop: %u", loop_err);
        goto error;
    }

    if (!cr->cr_close) {
        err("couldn't load address for cr_close: %u", close_err);
        goto error;
    }

    info("Reloaded, version: %d", ctx->version);
    cr->last_timestamp = now;
    ctx->last_working_version = ctx->version;

    cr->cr_init(ctx);

    return true;

error:
    if (cr->handle) FreeLibrary(cr->handle);
    cr->handle = NULL;
    cr->cr_init = cr->cr_loop = cr->cr_close = NULL;
    return false;
#endif

    return true;
}

bool crOpen(cr_t *ctx, strview_t path) {
#ifdef CR_DISABLE
    cr_init(ctx);
    return true;
#else
    arena_t arena = arenaMake(ARENA_VIRTUAL, MB(1));

    str_t path_copy = str(&arena, path);

    if (!fileExists(path_copy.buf)) {
        err("dll file: %v does not exist", path);
        arenaCleanup(&arena);
        return false;
    }

    crinternal_t *cr = alloc(&arena, crinternal_t);
    cr->arena = arena;
    cr->path = path_copy;

    ctx->p = cr;
    ctx->last_working_version = 0;

    return cr_reload(ctx);
#endif
}

void crClose(cr_t *ctx, bool clean_temp_files) {
#ifdef CR_DISABLE
    cr_close(ctx);
#else
    crinternal_t *cr = ctx->p;
    if (cr->cr_close) {
        cr->cr_close(ctx);
    }

    if (clean_temp_files) {
        crinternal_t *cr = ctx->p;
        arena_t scratch = cr->arena;

        strview_t dir, name, ext;
        fileSplitPath(strv(cr->path), &dir, &name, &ext);

        for (int i = 0; i < ctx->last_working_version; ++i) {
            str_t fname = strFmt(&scratch, "%v/%v-%d%v", dir, name, i + 1, ext);
            bool r = fileDelete(scratch, strv(fname));
        }
    }

    if (cr->handle) {
        FreeLibrary(cr->handle);
    }

    cr->handle = NULL;
    cr->cr_init = cr->cr_loop = cr->cr_close = NULL;

    arena_t arena = cr->arena;
    arenaCleanup(&arena);

    ctx->p = NULL;
#endif
}

int crStep(cr_t *ctx) {
#ifdef CR_DISABLE
    cr_loop(ctx);
    return 0;
#else
    crinternal_t *cr = ctx->p;

    int result = -1;
    if (cr->cr_loop) {
        result = cr->cr_loop(ctx);
    }
    return result;
#endif
}

bool crReload(cr_t *ctx) {
    return cr_reload(ctx);
}
