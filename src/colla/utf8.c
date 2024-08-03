#include "utf8.h"

static const uint8 masks[] = {
    0x7f, // 0111-1111
    0x1f, // 0001-1111
    0x0f, // 0000-1111
    0x07, // 0000-0111
    0x03, // 0000-0011
    0x01  // 0000-0001
};

static struct {
    uint8 mask;
    uint8 result;
    int octets;
} sizes[] = {
    { 0x80, 0x00, 1 },  // 1000-0000, 0000-0000
    { 0xE0, 0xC0, 2 },  // 1110-0000, 1100-0000
    { 0xF0, 0xE0, 3 },  // 1111-0000, 1110-0000
    { 0xF8, 0xF0, 4 },  // 1111-1000, 1111-0000
    { 0xFC, 0xF8, 5 },  // 1111-1100, 1111-1000
    { 0xFE, 0xF8, 6 },  // 1111-1110, 1111-1000
    { 0x80, 0x80, -1 }, // 1000-0000, 1000-0000
};


/*
UTF-8 codepoints are encoded using the first bits of the first character

byte 1    | byte 2    | byte 3    | byte 4   
0xxx xxxx |           |           | 
110x xxxx | 10xx xxxx |           | 
1110 xxxx | 10xx xxxx | 10xx xxxx | 
1111 0xxx | 10xx xxxx | 10xx xxxx | 10xx xxxx  

so when we decode it we first find the size of the codepoint (from 1 to 4)
then we apply the mask to the first byte to get the first character
then we keep shifting the rune left 6 and applying the next byte to the mask
until the codepoint is finished (size is 0)

## EXAMPLE

utf8 string (€) = 1110-0010 1000-0010 1010-1100

cp = 0000-0000 0000-0000 0000-0000 0000-0000
size = 3
mask = 0x0f -> 0000-1111
cp = *s & mask = 1110-0010 & 0000-1111 = 0000-0000 0000-0000 0000-0000 0000-0010
++s = 1000-0010

--size = 2
cp <<= 6 = 0000-0000 0000-0000 0000-0000 1000-0000
cp |= *s & 0x3f = 1000-0010 & 0011-1111 = 0000-0000 0000-0000 0000-0000 1000-0010
++s = 1010-1100

--size = 1
cp <<= 6 = 0000-0000 0000-0000 0010-0000 1000-0000
cp |= *s & 0x3f = 1010-1100 & 0011-1111 = 0000-0000 0000-0000 0010-0000 1010-1100
++s = ----------

final codepoint = 0010-0000 1010-1100
€ codepoint     = 0010-0000 1010-1100
*/

rune utf8Decode(const char **char_str) {
    uint8 **s = (uint8 **)char_str;

    rune ch = 0;
    // if is ascii
    if (**s < 128) {
        ch = **s;
        ++*s;
        return ch;
    }
    int size = utf8Size((char *)*s);
    if (size == -1) {
        ++*s;
        return UTF8_INVALID;
    }
    uint8 mask = masks[size - 1];
    ch = **s & mask;
    ++*s;
    while(--size) {
        ch <<= 6;
        ch |= **s & 0x3f; // 0011-1111
        ++*s;
    }
    return ch;
}


/*
to encode a codepoint in a utf8 string we first need to find 
the length of the codepoint
then we start from the rightmost byte and loop for each byte of the codepoint
using the length we got before until the first byte (which we skip)
> and (&) with 0x3f so we ignore the first to bits of the codepoint
> or (|) with 0x80 so we make sure that the first two bits are 10
> bitshift the codepoint right 6

finally, we apply the correct length-mask to the first byte

## EXAMPLE

ch € = 0010-0000 1010-1100
ch < 0x10000 
    first = 0xe0 = 1110-0000
    len = 3

str[2] = (ch & 0x3f) | 0x80 = 1010-1100 & 0011-1111 | 1000-0000
       = 1010-1100
ch >>= 6 = 0010-0000 1010-1100 >> 6 = 1000-0010

str[1] = (ch & 0x3f) | 0x80 = 1000-0010 & 0011-1111 | 1000-000
       = 1000-0010
ch >>= 6 = 1000-0010 >> 6 = 0000-0010

str[0] = ch | first_mask = 0000-0010 | 1111-0000
       = 1111-0010

str    = 1111-0010 1000-0010 1010-1100
utf8 € = 1110-0010 1000-0010 1010-1100
*/

usize utf8Encode(char *str, rune codepoint) {
    usize len = 0;
    uint8 first;

    if (codepoint < 0x80) {            // 0000-0000 0000-0000 0000-0000 1000-0000
        first = 0;
        len = 1;
    }
    else if (codepoint < 0x800) {      // 0000-0000 0000-0000 0000-1000 0000-0000
        first = 0xc0; // 1100-0000
        len = 2;
    }
    else if (codepoint < 0x10000) {    // 0000-0000 0000-0001 0000-0000 0000-0000
        first = 0xe0; // 1110-0000
        len = 3;
    }
    else {
        first = 0xf0; // 1111-0000
        len = 4;
    }

    for (usize i = len - 1; i > 0; --i) {
        // 0x3f -> 0011-1111
        // 0x80 -> 1000-0000
        str[i] = (codepoint & 0x3f) | 0x80;
        codepoint >>= 6;
    }

    str[0] = (char)(codepoint | first);
    return len;
}

int utf8Size(const char *str) {
    uint8 c = (uint8)*str;
    for(usize i = 0; i < (sizeof(sizes) / sizeof(*sizes)); ++i) {
        if ((c & sizes[i].mask) == sizes[i].result) {
            return sizes[i].octets;
        }
    }
    return -1;
}

usize utf8CpSize(rune ch) {
    if (ch < 0x80)         return 1;
    else if (ch < 0x800)   return 2;
    else if (ch < 0x10000) return 3;
    return 4;
}
