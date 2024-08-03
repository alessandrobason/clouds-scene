#pragma once

#include "collatypes.h"

#ifdef __TINYC__
#define alignof __alignof__
#else
#define alignof _Alignof
#endif

typedef enum {
    ARENA_VIRTUAL,
    ARENA_MALLOC,
    ARENA_STATIC,
} arena_type_e;

typedef enum {
    ALLOC_FLAGS_NONE = 0,
    ALLOC_NOZERO     = 1 << 0,
    ALLOC_SOFT_FAIL  = 1 << 1,
} alloc_flags_e;

typedef struct arena_t {
    uint8 *start;
    uint8 *current;
    uint8 *end;
    arena_type_e type;
} arena_t;

typedef struct {
    arena_type_e type;
    usize allocation;
    byte *static_buffer;
} arena_desc_t;

typedef struct {
    arena_t *arena;
    usize count;
    alloc_flags_e flags;
    usize size;
    usize align;
} arena_alloc_desc_t;

#define KB(count) (  (count) * 1024)
#define MB(count) (KB(count) * 1024)
#define GB(count) (MB(count) * 1024)

// arena_type_e type, usize allocation, [ byte *static_buffer ]
#define arenaMake(...) arenaInit(&(arena_desc_t){ __VA_ARGS__ })

// arena_t *arena, T type, [ usize count, alloc_flags_e flags, usize size, usize align ]
#define alloc(arenaptr, type, ...) arenaAlloc(&(arena_alloc_desc_t){ .size = sizeof(type), .count = 1, .align = alignof(type), .arena = arenaptr, __VA_ARGS__ })

arena_t arenaInit(const arena_desc_t *desc);
void arenaCleanup(arena_t *arena);

void *arenaAlloc(const arena_alloc_desc_t *desc);
usize arenaTell(arena_t *arena);
usize arenaRemaining(arena_t *arena);
void arenaRewind(arena_t *arena, usize from_start);
void arenaPop(arena_t *arena, usize amount);
