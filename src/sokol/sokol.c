#define SOKOL_IMPL

#ifdef _WIN32
#define SOKOL_D3D11
#elif __linux__
#define SOKOL_GLCORE
#else
#define SOKOL_GLES3
#endif

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_time.h"
#include "sokol_fetch.h"