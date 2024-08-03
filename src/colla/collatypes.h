#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "colladefines.h"

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef size_t    usize;
typedef ptrdiff_t isize;

typedef uint8 byte;

typedef struct {
    uint8 *data;
    usize len;
} buffer_t;

#if COLLA_WIN && defined(UNICODE)
typedef wchar_t TCHAR;
#else
typedef char TCHAR;
#endif
