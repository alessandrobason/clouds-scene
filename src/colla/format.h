#pragma once

#include <stdarg.h>

#include "collatypes.h"

typedef struct arena_t arena_t;

int fmtPrint(const char *fmt, ...);
int fmtPrintv(const char *fmt, va_list args);

int fmtBuffer(char *buffer, usize buflen, const char *fmt, ...);
int fmtBufferv(char *buffer, usize buflen, const char *fmt, va_list args);
