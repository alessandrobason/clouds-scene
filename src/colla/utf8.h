#pragma once

#include "collatypes.h"

typedef uint32 rune;

enum {
    UTF8_MAX_SIZE = 4,
    UTF8_INVALID = 0x80
};

// grabs the next UTF-8 codepoint and advances string ptr
rune utf8Decode(const char **str);
// encodes a codepoint as UTF-8 and returns the length
usize utf8Encode(char *str, rune ch);
// returns the size of the next UTF-8 codepoint
int utf8Size(const char *str);
// returns the size of a UTF-8 codepoint
usize utf8CpSize(rune ch);
