#include "json.h"

#include "warnings/colla_warn_beg.h"

#include <stdio.h>

#include "strstream.h"
#include "file.h"
#include "tracelog.h"

#define json__ensure(c) \
    if (istrGet(in) != (c)) { \
        istrRewindN(in, 1); \
        fatal("wrong character at %zu, should be " #c " but is %c", istrTell(*in), istrPeek(in));\
    }

jsonval_t *json__parse_pair(arena_t *arena, instream_t *in, jsonflags_e flags);
jsonval_t *json__parse_value(arena_t *arena, instream_t *in, jsonflags_e flags);

bool json__is_value_finished(instream_t *in) {
    usize old_pos = istrTell(*in);
    
    istrSkipWhitespace(in);
    switch(istrPeek(in)) {
        case '}': // fallthrough
        case ']': // fallthrough
        case ',':
            return true;
    }

    in->cur = in->start + old_pos;
    return false;
}

void json__parse_null(instream_t *in) {
    strview_t null_view = istrGetViewLen(in, 4);
    
    if (!strvEquals(null_view, strv("null"))) {
        fatal("should be null but is: (%.*s) at %zu", null_view.len, null_view.buf, istrTell(*in));
    }

    if (!json__is_value_finished(in)) {
        fatal("null, should be finished, but isn't at %zu", istrTell(*in));
    }
}

jsonval_t *json__parse_array(arena_t *arena, instream_t *in, jsonflags_e flags) {
    json__ensure('[');

    istrSkipWhitespace(in);

    // if it is an empty array
    if (istrPeek(in) == ']') {
        istrSkip(in, 1);
        return NULL;
    }
    
    jsonval_t *head = json__parse_value(arena, in, flags);
    jsonval_t *cur = head;
    
    while (true) {
        istrSkipWhitespace(in);
        switch (istrGet(in)) {
            case ']':
                return head;
            case ',':
            {
                istrSkipWhitespace(in);
                // trailing comma
                if (istrPeek(in) == ']') {
                    if (flags & JSON_NO_TRAILING_COMMAS) {
                        fatal("trailing comma in array at at %zu: (%c)(%d)", istrTell(*in), *in->cur, *in->cur);
                    }
                    else {
                        continue;
                    }
                }

                jsonval_t *next = json__parse_value(arena, in, flags);
                cur->next = next;
                next->prev = cur;
                cur = next;
                break;
            }
            default:
                istrRewindN(in, 1);
                fatal("unknown char after array at %zu: (%c)(%d)", istrTell(*in), *in->cur, *in->cur);
        }
    }

    return NULL;
}

str_t json__parse_string(arena_t *arena, instream_t *in) {
    istrSkipWhitespace(in);

    json__ensure('"');

    const char *from = in->cur;
    
    for (; !istrIsFinished(*in) && *in->cur != '"'; ++in->cur) {
        if (istrPeek(in) == '\\') {
            ++in->cur;
        }
    }
    
    usize len = in->cur - from;

    str_t out = str(arena, from, len);

    json__ensure('"');

    return out;
}

double json__parse_number(instream_t *in) {
    double value = 0.0;
    istrGetDouble(in, &value);
    return value;
}

bool json__parse_bool(instream_t *in) {
    size_t remaining = istrRemaining(*in);
    if (remaining >= 4 && memcmp(in->cur, "true", 4) == 0) {
        istrSkip(in, 4);
        return true;
    }
    if (remaining >= 5 && memcmp(in->cur, "false", 5) == 0) {
        istrSkip(in, 5);
        return false;
    }
    fatal("unknown boolean at %zu: %.10s", istrTell(*in), in->cur);
    return false;
}

jsonval_t *json__parse_obj(arena_t *arena, instream_t *in, jsonflags_e flags) {
    json__ensure('{');

    istrSkipWhitespace(in);

    // if it is an empty object
    if (istrPeek(in) == '}') {
        istrSkip(in, 1);
        return NULL;
    }

    jsonval_t *head = json__parse_pair(arena, in, flags);
    jsonval_t *cur = head;

    while (true) {
        istrSkipWhitespace(in);
        switch (istrGet(in)) {
            case '}':
                return head;
            case ',':
            {
                istrSkipWhitespace(in);
                // trailing commas
                if (!(flags & JSON_NO_TRAILING_COMMAS) && istrPeek(in) == '}') {
                    return head;
                }

                jsonval_t *next = json__parse_pair(arena, in, flags);
                cur->next = next;
                next->prev = cur;
                cur = next;
                break;
            }
            default:
                istrRewindN(in, 1);
                fatal("unknown char after object at %zu: (%c)(%d)", istrTell(*in), *in->cur, *in->cur);
        }
    }

    return head;
}

jsonval_t *json__parse_pair(arena_t *arena, instream_t *in, jsonflags_e flags) {
    str_t key = json__parse_string(arena, in);

    // skip preamble
    istrSkipWhitespace(in);
    json__ensure(':');

    jsonval_t *out = json__parse_value(arena, in, flags);
    out->key = key;
    return out;
}

jsonval_t *json__parse_value(arena_t *arena, instream_t *in, jsonflags_e flags) {
    jsonval_t *out = alloc(arena, jsonval_t);

    istrSkipWhitespace(in);

    switch (istrPeek(in)) {
        // object
        case '{':
            out->object = json__parse_obj(arena, in, flags);
            out->type = JSON_OBJECT;
            break;
        // array
        case '[':
            out->array = json__parse_array(arena, in, flags);
            out->type = JSON_ARRAY;
            break;
        // string
        case '"':
            out->string = json__parse_string(arena, in);
            out->type = JSON_STRING;
            break;
        // boolean
        case 't': // fallthrough
        case 'f':
            out->boolean = json__parse_bool(in);
            out->type = JSON_BOOL;
            break;
        // null
        case 'n': 
            json__parse_null(in);
            out->type = JSON_NULL;
            break;
        // comment
        case '/':
            fatal("TODO comments");
            break;
        // number
        default:
            out->number = json__parse_number(in);
            out->type = JSON_NUMBER;
            break;
    }

    return out;
}

json_t jsonParse(arena_t *arena, arena_t scratch, strview_t filename, jsonflags_e flags) {
    str_t data = fileReadWholeStr(&scratch, filename);
    json_t json = jsonParseStr(arena, strv(data), flags);
    return json;
}

json_t jsonParseStr(arena_t *arena, strview_t jsonstr, jsonflags_e flags) {
    jsonval_t *root = alloc(arena, jsonval_t);
    root->type = JSON_OBJECT;
    
    instream_t in = istrInitLen(jsonstr.buf, jsonstr.len);
    root->object = json__parse_obj(arena, &in, flags);

    return root;
}

jsonval_t *jsonGet(jsonval_t *node, strview_t key) {
    if (!node) return NULL;

    if (node->type != JSON_OBJECT) {
        err("passed type is not an object");
        return NULL;
    }

    node = node->object;

    while (node) {
        if (strvEquals(strv(node->key), key)) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

#include "warnings/colla_warn_end.h"