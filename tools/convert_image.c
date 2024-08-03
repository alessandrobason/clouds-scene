#define STB_IMAGE_IMPLEMENTATION
#include "../src/stb_image.h"

#include "../src/colla/build.c"

void convert_image(arena_t scratch, const char *from, const char *to) {
    int w, h;
    uchar *p = stbi_load(from, &w, &h, NULL, 4);

    file_t fp = fileOpen(scratch, strv(to), FILE_WRITE);

    uint16 width = w, height = h;
    fileWrite(fp, &width, sizeof(width));
    fileWrite(fp, &height, sizeof(height));
    fileWrite(fp, p, w * h * 4);
    fileClose(fp);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fatal("usage: %s [ input images... ]", argv[0]);
    }

    arena_t arena = arenaMake(ARENA_VIRTUAL, MB(1));

    for (int i = 1; i < argc; ++i) {
        arena_t scratch = arena;

        const char *from = argv[i];
        strview_t dir, name;
        fileSplitPath(strv(from), &dir, &name, NULL);
        str_t to = strFmt(&scratch, "%v/%v.raw", dir, name);

        convert_image(scratch, from, to.buf);
        info("converted %s to %v", from, to);
    }

    return 0;
}