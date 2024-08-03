#include "src/client_main.c"

#ifndef BUILD_SINGLE
#include "src/colla/build.c"
#define SOKOL_IMPL
#endif
#include "src/sokol/sokol_time.h"