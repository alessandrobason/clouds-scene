#pragma once

#include "collatypes.h"

typedef struct arena_t arena_t;

buffer_t base64Encode(arena_t *arena, buffer_t buffer);
buffer_t base64Decode(arena_t *arena, buffer_t buffer);