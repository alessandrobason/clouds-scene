#include "ini.h"

#include "warnings/colla_warn_beg.h"

#include <assert.h>

#include "strstream.h"

static void ini__parse(arena_t *arena, ini_t *ini, const iniopts_t *options);

ini_t iniParse(arena_t *arena, strview_t filename, const iniopts_t *options) {
    file_t fp = fileOpen(*arena, filename, FILE_READ);
    ini_t out = iniParseFile(arena, fp, options);
    fileClose(fp);
    return out;
}

ini_t iniParseFile(arena_t *arena, file_t file, const iniopts_t *options) {
    str_t data = fileReadWholeStrFP(arena, file);
    return iniParseStr(arena, strv(data), options);
}

ini_t iniParseStr(arena_t *arena, strview_t str, const iniopts_t *options) {
    ini_t out = {
        .text = str,
        .tables = NULL,
    };
    ini__parse(arena, &out, options);
    return out;
}

bool iniIsValid(ini_t *ctx) {
    return ctx && !strvIsEmpty(ctx->text);
}

initable_t *iniGetTable(ini_t *ctx, strview_t name) {
    initable_t *t = ctx ? ctx->tables : NULL;
    while (t) {
        if (strvEquals(t->name, name)) {
            return t;
        }
        t = t->next;
    }
    return NULL;
}

inivalue_t *iniGet(initable_t *ctx, strview_t key) {
    inivalue_t *v = ctx ? ctx->values : NULL;
    while (v) {
        if (strvEquals(v->key, key)) {
            return v;
        }
        v = v->next;
    }
    return NULL;
}

iniarray_t iniAsArr(arena_t *arena, inivalue_t *value, char delim) {
    strview_t v = value ? value->value : (strview_t){0};
    if (!delim) delim = ' ';

    strview_t *beg = (strview_t *)arena->current;
    usize count = 0;

    usize start = 0;
    for (usize i = 0; i < v.len; ++i) {
        if (v.buf[i] == delim) {
            strview_t arrval = strvTrim(strvSub(v, start, i));
            if (!strvIsEmpty(arrval)) {
                strview_t *newval = alloc(arena, strview_t);
                *newval = arrval;
                ++count;
            }
            start = i + 1;
        }
    }

    strview_t last = strvTrim(strvSub(v, start, SIZE_MAX));
    if (!strvIsEmpty(last)) {
        strview_t *newval = alloc(arena, strview_t);
        *newval = last;
        ++count;
    }

    return (iniarray_t){
        .values = beg,
        .count = count,
    };
}

uint64 iniAsUInt(inivalue_t *value) {
    strview_t v = value ? value->value : (strview_t){0};
    instream_t in = istrInitLen(v.buf, v.len);
    uint64 out = 0;
    if (!istrGetU64(&in, &out)) {
        out = 0;
    }
    return out;
}

int64 iniAsInt(inivalue_t *value) {
    strview_t v = value ? value->value : (strview_t){0};
    instream_t in = istrInitLen(v.buf, v.len);
    int64 out = 0;
    if (!istrGetI64(&in, &out)) {
        out = 0;
    }
    return out;
}

double iniAsNum(inivalue_t *value) {
    strview_t v = value ? value->value : (strview_t){0};
    instream_t in = istrInitLen(v.buf, v.len);
    double out = 0;
    if (!istrGetDouble(&in, &out)) {
        out = 0.0;
    }
    return out;
}

bool iniAsBool(inivalue_t *value) {
    strview_t v = value ? value->value : (strview_t){0};
    instream_t in = istrInitLen(v.buf, v.len);
    bool out = 0;
    if (!istrGetBool(&in, &out)) {
        out = false;
    }
    return out;
}

// == PRIVATE FUNCTIONS ==============================================================================

#define INIPUSH(head, tail, val)    \
    do {                            \
        if (!head) {                \
            head = val;             \
            tail = val;             \
        }                           \
        else {                      \
            tail->next = val;       \
            val = tail;             \
        }                           \
    } while (0)

static iniopts_t ini__get_options(const iniopts_t *options) {
    iniopts_t out = {
        .key_value_divider = '=',
    };

#define SETOPT(v) out.v = options->v ? options->v : out.v

    if (options) {
        SETOPT(key_value_divider);
        SETOPT(merge_duplicate_keys);
        SETOPT(merge_duplicate_tables);
    }

#undef SETOPT

    return out;
}

static void ini__add_value(arena_t *arena, initable_t *table, instream_t *in, iniopts_t *opts) {
    assert(table);

    strview_t key = strvTrim(istrGetView(in, opts->key_value_divider));
    istrSkip(in, 1);
    strview_t value = strvTrim(istrGetView(in, '\n'));
    istrSkip(in, 1);
    inivalue_t *newval = NULL;
    
    if (opts->merge_duplicate_keys) {
        newval = table->values;
        while (newval) {
            if (strvEquals(newval->key, key)) {
                break;
            }
            newval = newval->next;
        }
    }

    if (newval) {
        newval->value = value;
    }
    else {
        newval = alloc(arena, inivalue_t);
        newval->key = key;
        newval->value = value;

        if (!table->values) {
            table->values = newval;
        }
        else {
            table->tail->next = newval;
        }

        table->tail = newval;
    }
}

static void ini__add_table(arena_t *arena, ini_t *ctx, instream_t *in, iniopts_t *options) {
    istrSkip(in, 1); // skip [
    strview_t name = istrGetView(in, ']');
    istrSkip(in, 1); // skip ]
    initable_t *table = NULL;

    if (options->merge_duplicate_tables) {
        table = ctx->tables;
        while (table) {
            if (strvEquals(table->name, name)) {
                break;
            }
            table = table->next;
        }
    }

    if (!table) {
        table = alloc(arena, initable_t);

        if (!ctx->tables) {
            ctx->tables = table;
        }
        else {
            ctx->tail->next = table;
        }

        ctx->tail = table;
    }

    istrIgnoreAndSkip(in, '\n');
    while (!istrIsFinished(*in)) {
        switch (istrPeek(in)) {
            case '\n': // fallthrough
            case '\r':
                return;
            case '#': // fallthrough
            case ';':
                istrIgnoreAndSkip(in, '\n');
                break;
            default:
                ini__add_value(arena, table, in, options);
                break;
        }
    }
}

static void ini__parse(arena_t *arena, ini_t *ini, const iniopts_t *options) {
    iniopts_t opts = ini__get_options(options);

    initable_t *root = alloc(arena, initable_t);
    root->name = INI_ROOT;
    ini->tables = root;

    instream_t in = istrInitLen(ini->text.buf, ini->text.len);

    while (!istrIsFinished(in)) {
        istrSkipWhitespace(&in);
        switch (istrPeek(&in)) {
            case '[':
                ini__add_table(arena, ini, &in, &opts);
                break;
            case '#': // fallthrough
            case ';':
                istrIgnoreAndSkip(&in, '\n');
                break;
            default:
                ini__add_value(arena, ini->tables, &in, &opts);
                break;
        }
    }
}

#undef INIPUSH

#include "warnings/colla_warn_end.h"
