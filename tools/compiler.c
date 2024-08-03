#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

#define TLOG_NO_MSGBOX

#include "../src/colla/tracelog.c"
#include "../src/colla/str.c"
#include "../src/colla/arena.c"
#include "../src/colla/file.c"
#include "../src/colla/ini.c"
#include "../src/colla/format.c"
#include "../src/colla/strstream.c"
#include "../src/colla/vmem.c"
#include "../src/colla/json.c"

typedef struct {
    strview_t key;
    strview_t val;
    bool not_unique;
} hnode_t;

typedef struct {
    hnode_t *keys;
    uint32 count;
    uint32 unique_count;
} hmap_t;

#define FNV_PRIME  0x01000193u
#define FNV_OFFSET 0x811c9dc5u

uint32 hash_fnv1_32(const void *buf, usize len) {
    const uint8 *data = buf;
    uint32 hash = FNV_OFFSET;

    for (usize i = 0; i < len; ++i) {
        hash = (hash * FNV_PRIME) ^ data[i];
    }
    
    return hash;
}

uint32 hash_node(hnode_t *node) {
    uint32 hash_key = hash_fnv1_32(node->key.buf, node->key.len);
    uint32 hash_val = hash_fnv1_32(node->val.buf, node->val.len);
    return hash_key ^ hash_val;
}

bool hnodes_equals(hnode_t *a, hnode_t *b) {
    return strvEquals(a->key, b->key);
}

hmap_t hmapInit(arena_t *arena, uint32 log2_count) {
    uint32 count = 1 << log2_count;
    return (hmap_t){
        .count = count,
        .keys = alloc(arena, hnode_t, count),
    };
}

bool hmapPush(hmap_t *map, hnode_t *new_node) {
    uint32 hash = hash_node(new_node);
    hash &= map->count - 1;

    for (uint32 i = 0; i < map->count; ++i) {
        hnode_t *node = &map->keys[hash];

        // if is tombstone
        if (strvIsEmpty(node->key)) {
            *node = *new_node;
            map->unique_count++;
            return true;
        }

        // if not unique, skip
        if (hnodes_equals(node, new_node)) {
            node->not_unique = true;
            return false;
        }

        // linear probe
        hash = (hash + 1) & (map->count - 1);
    }

    fatal("hashmap is full??");
    return true;
}

str_t runCommand(arena_t *arena, arena_t scratch, strview_t cmd) {
    str_t out = {0};

    str_t cmd_full = strFmt(&scratch, "%v > bin/output", cmd);
    // str_t cmd_aft = strFmt(&scratch, "%v > bin/after");

    if (system(cmd_full.buf)) {
        err("couldn't run command");
        return out;
    }

    return fileReadWholeStr(arena, strv("bin/output"));
}

ini_t compLoadCache(arena_t *arena) {
    return iniParse(arena, strv("bin/cache.ini"), NULL);
}

ini_t compCreateCache(arena_t *arena, arena_t scratch) {
    str_t cmd = str(&scratch, "\"C:/Program Files/Microsoft Visual Studio/2022/Preview/VC/Auxiliary/Build/vcvars64.bat\" >nul 2>nul && set > bin/cache.ini");

    if (system(cmd.buf)) {
        err("couldn't run %v", cmd);
        return (ini_t){0};
    }

    return compLoadCache(arena);
}

ini_t compLoadOrCreateCache(arena_t *arena, arena_t scratch) {
    // try to load cache
    ini_t cache = compLoadCache(arena);

    if (!iniIsValid(&cache)) {
        info("No cache found, creating a new one");

        cache = compCreateCache(arena, scratch);
        
        if (!iniIsValid(&cache)) {
            fatal("Could not create compiler cache!");
            return (ini_t){0};
        }
    }

    return cache;
}

bool compInitFromFile(arena_t *arena) {
    ini_t ini = iniParse(arena, strv("bin/cache.ini"), NULL);
    if (!iniIsValid(&ini)) {
        err("couldn't parse cache.ini");
        return false;
    }

    inivalue_t *v = iniGetTable(&ini, INI_ROOT)->values;
    while (v) {
        arena_t s = *arena;
        str_t key = str(&s, v->key);
        str_t val = str(&s, v->value);
        _putenv_s(key.buf, val.buf);
        v = v->next;
    }

    return true;
}

bool compInitFromScratch(arena_t *arena, arena_t scratch) {
    str_t cmd = str(&scratch, "\"C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build/vcvars64.bat\" >nul 2>nul && set > bin/cache.ini");

    if (system(cmd.buf)) {
        err("couldn't run command: \n%v", cmd);
        return false;
    }

    return compInitFromFile(arena);
}

bool compInitEnv(arena_t *arena, arena_t scratch) {
    ini_t cache = compLoadOrCreateCache(arena, scratch);
    if (!iniIsValid(&cache)) {
        return 1;
    }

    inivalue_t *v = iniGetTable(&cache, INI_ROOT)->values;
    for (; v; v=v->next) {
        if (strvIsEmpty(v->value)) {
            continue;
        }

        arena_t tmp = scratch;
        str_t key = str(&tmp, v->key);
        str_t val = str(&tmp, v->value);
        errno_t error = _putenv_s(key.buf, val.buf);
        if (error) {
            fatal("could not set environmental value: %v=%v: %u", key, val, error);
        }
    }

    return true;
}

typedef struct clargs_t {
    strview_t arg;
    struct clargs_t *next;
} clargs_t;

typedef struct {
    strview_t infile;
    strview_t outfile;
    bool compile_only;
    int start_args;
    int end_args;
} args_t;

int main(int argc, char **argv) {
    arena_t arena = arenaMake(ARENA_VIRTUAL, GB(1));
    arena_t scratch = arenaMake(ARENA_VIRTUAL, GB(1));

    clargs_t *clargs = NULL;
    clargs_t *linkarsg = NULL;

    args_t args = {
        .infile = strv("build.c"),
        .outfile = strv("out"),
    };

    for (int i = 1; i < argc; ++i) {
        strview_t arg = strv(argv[i]);
        if (strvEquals(arg, strv("-f"))) {
            if (++i >= argc) {
                fatal("passing -f option but no input file passed!");
            }
            args.infile = strv(argv[i]);
        }
        else if (strvEquals(arg, strv("-c"))) {
            args.compile_only = true;
        }
        else if (strvStartsWith(arg, '/')) {
            clargs_t *a = alloc(&arena, clargs_t);
            a->arg = arg;
            a->next = clargs;
            clargs = a;
        }
        else if (strvEquals(arg, strv("-l"))) {
            if (i + 1 < argc) {
                clargs_t *a = alloc(&arena, clargs_t);
                a->arg = strv(argv[++i]);
                a->next = linkarsg;
                linkarsg = a;
            }
        }
        else {
            args.outfile = arg;
            args.start_args = min(i + 1, argc);
            args.end_args = argc;
            break;
        }
    }

    if (!compInitEnv(&arena, scratch)) {
        fatal("couldn't initialise environment");
    }

    strview_t cl = strv("cl -nologo /DUNICODE /Zi /std:clatest /DEBUG /Fd\"bin/cl.pdb\" /Fo\"bin\"\\");

    str_t compile_cmd = str(&arena, cl);

    for_each (arg, clargs) {
        compile_cmd = strFmt(&arena, "%v %v", compile_cmd, arg->arg);
    }

    compile_cmd = strFmt(&arena, "%v /Fe: bin/%v %v /link /SUBSYSTEM:CONSOLE", compile_cmd, args.outfile, args.infile);

    for_each(arg, linkarsg) {
        compile_cmd = strFmt(&arena, "%v %v", compile_cmd, arg->arg);
    }

    if(system(compile_cmd.buf)) {
        fatal("Compiler error");
        return 1;
    }

    if (args.compile_only) {
        return 0;
    }

    outstream_t run = ostrInit(&arena);
    ostrPrintf(&run, "\"bin\\%v\"", args.outfile);

    for (int i = args.start_args; i < args.end_args; ++i) {
        ostrPrintf(&run, " %s", argv[i]);
    }

    str_t run_cmd = ostrAsStr(&run);

    int result = system(run_cmd.buf);
    if (result) {
        err("error code: %#x", result);
        fatal("%v failed with code %u", run_cmd, result);
    }
}