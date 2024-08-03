#pragma once

#include "colla/str.h"

#if COLLA_WIN
#define CR_EXPORT __declspec(dllexport)
#else
// todo linux support?
#define CR_EXPORT
#endif


// api functions:
// int cr_init(cr_t *ctx)
// int cr_loop(cr_t *ctx)
// int cr_close(cr_t *ctx)

typedef struct cr_t {
    void *p;
    void *userdata;
    int version;
    int last_working_version;
} cr_t;

bool crOpen(cr_t *ctx, strview_t path);
void crClose(cr_t *ctx, bool clean_temp_files);
int crStep(cr_t *ctx);
bool crReload(cr_t *ctx);
