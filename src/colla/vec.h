#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define vec(T)                  T *

#define vecFree(vec)            ((vec) ? free(_vecheader(vec)), NULL : NULL)
#define vecCopy(src, dest)      (vecFree(dest), vecAdd(dest, vecCount(src)), memcpy(dest, src, vecCount(src)))

#define vecAppend(vec, ...)     (_vecmaygrow(vec, 1), (vec)[_veclen(vec)] = (__VA_ARGS__), _veclen(vec)++)
#define vecRemove(vec, ind)     ((vec) ? (vec)[(ind)] = (vec)[--_veclen(vec)], NULL : 0)
#define vecRemoveIt(vec, it)    (vecRemove((vec), (it) - (vec)))
#define vecLen(vec)             ((vec) ? _veclen(vec) : 0)
#define vecCap(vec)             ((vec) ? _veccap(vec) : 0)

#define vecBeg(vec)             (vec)
#define vecEnd(vec)             ((vec) ? (vec) + _veclen(vec) : NULL)
#define vecBack(vec)            ((vec)[_veclen(vec) - 1])

#define vecAdd(vec, n)          (_vecmaygrow(vec, (n)), _veclen(vec) += (size_type)(n), &(vec)[_veclen(vec)-(n)])
#define vecReserve(vec, n)      (_vecmaygrow(vec, (n)))
#define vecShrink(vec)          (_vecshrink((void **)&(vec), _veclen(vec), sizeof(*(vec))))

#define vecClear(vec)           ((vec) ? _veclen(vec) = 0 : 0)
#define vecPop(vec)             ((vec)[--_veclen(vec)])

// == IMPLEMENTATION ==========================================================================================

#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#ifndef size_type
    #define size_type uint32_t
#endif

#define _vecheader(vec)         ((size_type *)(vec) - 2)
#define _veccap(vec)            _vecheader(vec)[0]
#define _veclen(vec)            _vecheader(vec)[1]

#define _vecneedgrow(vec, n)    ((vec) == NULL || _veclen(vec) + n >= _veccap(vec))
#define _vecmaygrow(vec, n)     (_vecneedgrow(vec, (n)) ? _vecgrow(vec, (size_type)(n)) : (void)0)
#define _vecgrow(vec, n)        _vecgrowimpl((void **)&(vec), (n), sizeof(*(vec)))

inline static void _vecgrowimpl(void **arr, size_type increment, size_type itemsize) {
    int newcap = *arr ? 2 * _veccap(*arr) + increment : increment + 1;
    void *ptr = realloc(*arr ? _vecheader(*arr) : 0, itemsize * newcap + sizeof(size_type) * 2);
    assert(ptr);
    if (ptr) {
        if (!*arr) ((size_type *)ptr)[1] = 0;
        *arr = (void *) ((size_type *)ptr + 2);
        _veccap(*arr) = newcap;
    }
}

inline static void _vecshrink(void **arr, size_type newcap, size_t itemsize) {
    if (newcap == _veccap(*arr) || !*arr) return;
    void *ptr = realloc(_vecheader(*arr), itemsize * newcap + sizeof(size_type) * 2);
    assert(ptr);
    if (ptr) {
        *arr = (void *) ((size_type *)ptr + 2);
        if (_veclen(*arr) < newcap) _veclen(*arr) = newcap;
        _veccap(*arr) = newcap;
    }
}

#ifdef __cplusplus
} // extern "C"
#endif
