#include "base64.h"

#include "warnings/colla_warn_beg.h"

#include "arena.h"

static char encoding_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',           
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

static uint8 decoding_table[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 63, 
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 
    0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 
    24, 25, 0, 0, 0, 0, 0, 0, 26, 27, 28, 29, 30, 31, 
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 
    44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

buffer_t base64Encode(arena_t *arena, buffer_t buffer) {
    usize outlen = ((buffer.len + 2) / 3) * 4;
    uint8 *out = alloc(arena, uint8, outlen);

    for (usize i = 0, j = 0; i < buffer.len;) {
        uint32 a = i < buffer.len ? buffer.data[i++] : 0;
        uint32 b = i < buffer.len ? buffer.data[i++] : 0;
        uint32 c = i < buffer.len ? buffer.data[i++] : 0;

        uint32 triple = (a << 16) | (b << 8) | c;

        out[j++] = encoding_table[(triple >> 18) & 0x3F];
        out[j++] = encoding_table[(triple >> 12) & 0x3F];
        out[j++] = encoding_table[(triple >>  6) & 0x3F];
        out[j++] = encoding_table[(triple >>  0) & 0x3F];
    }

    usize mod = buffer.len % 3;
    if (mod) {
        mod = 3 - mod;
        for (usize i = 0; i < mod; ++i) {
            out[outlen - 1 - i] = '=';
        }
    }

    return (buffer_t){
        .data = out,
        .len = outlen
    };
}

buffer_t base64Decode(arena_t *arena, buffer_t buffer) {
    uint8 *out = arena->current;
    usize start = arenaTell(arena);

    for (usize i = 0; i < buffer.len; i += 4) {
        uint8 a = decoding_table[buffer.data[i + 0]];
        uint8 b = decoding_table[buffer.data[i + 1]];
        uint8 c = decoding_table[buffer.data[i + 2]];
        uint8 d = decoding_table[buffer.data[i + 3]];

        uint32 triple =
            ((uint32)a << 18) |
            ((uint32)b << 12) |
            ((uint32)c << 6)  |
            ((uint32)d);

        uint8 *bytes = alloc(arena, uint8, 3);

        bytes[0] = (triple >> 16) & 0xFF;
        bytes[1] = (triple >>  8) & 0xFF;
        bytes[2] = (triple >>  0) & 0xFF;
    }

    usize outlen = arenaTell(arena) - start;

    return (buffer_t){
        .data = out,
        .len = outlen,
    };
}

#include "warnings/colla_warn_end.h"
