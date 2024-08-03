#pragma once

#include "collatypes.h"

uint32 bitsCtz(uint32 v);

// == INLINE IMPLEMENTATION ==============================================================

#if COLLA_MSVC
#define BITS_WIN 1
#define BITS_LIN 0
#include <intrin.h>
#elif COLLA_GCC || COLLA_CLANG || COLLA_EMC
#define BITS_WIN 0
#define BITS_LIN 1
#else
#error "bits header not supported on this compiler"
#endif

#include "tracelog.h"

inline uint32 bitsCtz(uint32 v) {
#if BITS_LIN
    return v ? __builtin_ctz(v) : 0;
#elif BITS_WIN
    uint32 trailing = 0;
    return _BitScanForward(&trailing, v) ? trailing : 0;
#else
    return 0;
#endif
}

#undef BITS_WIN
#undef BITS_LIN
