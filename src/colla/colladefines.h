#pragma once

#define arrlen(a) (sizeof(a) / sizeof((a)[0]))
#define for_each(it, list) for (typeof(list) it = list; it; it = it->next)

#if defined(_DEBUG) || !defined(NDEBUG)
#define COLLA_DEBUG   1
#define COLLA_RELEASE 0
#else
#define COLLA_DEBUG   0
#define COLLA_RELEASE 1
#endif

#if defined(_WIN32)

#define COLLA_WIN 1
#define COLLA_OSX 0
#define COLLA_LIN 0
#define COLLA_EMC 0

#elif defined(__EMSCRIPTEN__)

#define COLLA_WIN 0
#define COLLA_OSX 0
#define COLLA_LIN 0
#define COLLA_EMC 1

#elif defined(__linux__)

#define COLLA_WIN 0
#define COLLA_OSX 0
#define COLLA_LIN 1
#define COLLA_EMC 0

#elif defined(__APPLE__)

#define COLLA_WIN 0
#define COLLA_OSX 1
#define COLLA_LIN 0
#define COLLA_EMC 0

#endif

#if defined(__clang__)

#define COLLA_CLANG 1
#define COLLA_MSVC  0
#define COLLA_TCC   0
#define COLLA_GCC   0

#elif defined(_MSC_VER)

#define COLLA_CLANG 0
#define COLLA_MSVC  1
#define COLLA_TCC   0
#define COLLA_GCC   0

#elif defined(__TINYC__)

#define COLLA_CLANG 0
#define COLLA_MSVC  0
#define COLLA_TCC   1
#define COLLA_GCC   0

#elif defined(__GNUC__)

#define COLLA_CLANG 0
#define COLLA_MSVC  0
#define COLLA_TCC   0
#define COLLA_GCC   1

#endif

#if   COLLA_CLANG

#define COLLA_CMT_LIB 0

#elif COLLA_MSVC

#define COLLA_CMT_LIB 1

#elif COLLA_TCC

#define COLLA_CMT_LIB 1

#elif COLLA_GCC

#define COLLA_CMT_LIB 0

#endif
