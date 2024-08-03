#include "arena.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vmem.h"
#include "tracelog.h"

static uintptr_t arena__align(uintptr_t ptr, usize align) {
    return (ptr + (align - 1)) & ~(align - 1);
}

static arena_t arena__make_virtual(usize size);
static arena_t arena__make_malloc(usize size);
static arena_t arena__make_static(byte *buf, usize len);

static void arena__free_virtual(arena_t *arena);
static void arena__free_malloc(arena_t *arena);

arena_t arenaInit(const arena_desc_t *desc) {
    if (desc) {
        switch (desc->type) {
            case ARENA_VIRTUAL: return arena__make_virtual(desc->allocation);
            case ARENA_MALLOC:  return arena__make_malloc(desc->allocation);
            case ARENA_STATIC:  return arena__make_static(desc->static_buffer, desc->allocation);
        }
    }

    debug("couldn't init arena: %p %d\n", desc, desc ? desc->type : 0);
    return (arena_t){0};
}

void arenaCleanup(arena_t *arena) {
    if (!arena) {
        return;
    }

    switch (arena->type) {
        case ARENA_VIRTUAL: arena__free_virtual(arena); break;
        case ARENA_MALLOC:  arena__free_malloc(arena);  break;
        // ARENA_STATIC does not need to be freed
        case ARENA_STATIC:  break;
    }

    arena->start = NULL;
    arena->current = NULL;
    arena->end = NULL;
    arena->type = 0;
}

arena_t arenaScratch(arena_t *arena) {
    if (!arena) {
        return (arena_t){0};
    }
    
    return (arena_t) {
        .start   = arena->current,
        .current = arena->current,
        .end     = arena->end,
        .type    = arena->type,
    };
}

void *arenaAlloc(const arena_alloc_desc_t *desc) {
    if (!desc || !desc->arena) {
        return NULL;
    }

    usize total = desc->size * desc->count;
    arena_t *arena = desc->arena;

    arena->current = (byte *)arena__align((uintptr_t)arena->current, desc->align);

    if (total > arenaRemaining(arena)) {
        if (desc->flags & ALLOC_SOFT_FAIL) {
            return NULL;
        }
        printf("finished space in arena, tried to allocate %zu bytes out of %zu\n", total, arenaRemaining(arena));
        abort();
    }

    if (arena->type == ARENA_VIRTUAL) {
        usize allocated = arenaTell(arena);
        usize page_end = vmemPadToPage(allocated);
        usize new_cur = allocated + total;

        if (new_cur > page_end) {
            usize extra_mem = vmemPadToPage(new_cur - page_end);
            usize page_size = vmemGetPageSize();
            // TODO is this really correct?
            usize num_of_pages = (extra_mem / page_size) + 1;

            assert(num_of_pages > 0);

            if (!vmemCommit(arena->current, num_of_pages + 1)) {
                if (desc->flags & ALLOC_SOFT_FAIL) {
                    return NULL;
                }
                printf("failed to commit memory for virtual arena, tried to commit %zu pages\n", num_of_pages);
                exit(1);
            }
        }
    }

    byte *ptr = arena->current;
    arena->current += total;

    return desc->flags & ALLOC_NOZERO ? ptr : memset(ptr, 0, total);
}

usize arenaTell(arena_t *arena) {
    return arena ? arena->current - arena->start : 0;
}

usize arenaRemaining(arena_t *arena) {
    return arena && (arena->current < arena->end) ? arena->end - arena->current : 0;
}

void arenaRewind(arena_t *arena, usize from_start) {
    if (!arena) {
        return;
    }

    assert(arenaTell(arena) >= from_start);

    arena->current = arena->start + from_start;
}

void arenaPop(arena_t *arena, usize amount) {
    if (!arena) {
        return;
    }
    usize position = arenaTell(arena);
    if (!position) {
        return;
    }
    arenaRewind(arena, position - amount);
}

// == VIRTUAL ARENA ====================================================================================================

static arena_t arena__make_virtual(usize size) {
    usize alloc_size = 0;
    byte *ptr = vmemInit(size, &alloc_size);
    if (!vmemCommit(ptr, 1)) {
        vmemRelease(ptr);
        ptr = NULL;
    }

    return (arena_t){
        .start = ptr,
        .current = ptr,
        .end = ptr ? ptr + alloc_size : NULL,
        .type = ARENA_VIRTUAL,
    };
}

static void arena__free_virtual(arena_t *arena) {
    if (!arena->start) {
        return;
    }

    bool success = vmemRelease(arena->start);
    assert(success && "Failed arena free");
}

// == MALLOC ARENA =====================================================================================================

static arena_t arena__make_malloc(usize size) {
    byte *ptr = malloc(size);
    assert(ptr);
    return (arena_t) {
        .start = ptr,
        .current = ptr,
        .end = ptr ? ptr + size : NULL,
        .type = ARENA_MALLOC,
    };
}

static void arena__free_malloc(arena_t *arena) {
    free(arena->start);
}

// == STATIC ARENA =====================================================================================================

static arena_t arena__make_static(byte *buf, usize len) {
    return (arena_t) {
        .start = buf,
        .current = buf,
        .end = buf ? buf + len : NULL,
        .type = ARENA_STATIC,
    };
}
