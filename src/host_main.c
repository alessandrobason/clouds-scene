#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_fetch.h"

#include "colla/tracelog.h"
#include "colla/file.h"
#include "colla/ini.h"
#include "colla/strstream.h"

#include "stb_image.h"

// #include "shader.h"

#include "cr.h"
#include "shared.h"
#include "display-shd.h"

#if COLLA_WIN
#define ASSET_DIR "assets/"
#else
#define ASSET_DIR "/assets/projects/clouds/data/"
#endif

static struct {
    sg_pass offscreen_pass;
    int still_loading;
    bool just_loaded;

    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } display;

    sg_pass_action pass_action;
    arena_t arena;
    uint64 last_write;

    host_t host;
    cr_t cr;
} state = {0};

static void slog(const char* tag, uint32 log_level, uint32 log_item_id, const char* message_or_null, uint32 line_nr, const char* filename_or_null, void* user_data);

static void checkReload(void);

static void image_load_callback(const sfetch_response_t *res) {
    if (res->finished) {
        if (res->failed) {
            const char *errors[] = {
                "NO_ERROR",
                "FILE_NOT_FOUND",
                "NO_BUFFER",
                "BUFFER_TOO_SMALL",
                "UNEXPECTED_EOF",
                "INVALID_HTTP_STATUS",
                "CANCELLED"
            };
            fatal("could not load %s: %s", res->path, errors[res->error_code]);
        }

        const uint8 *data = res->data.ptr;
        
        uint16 width = *((uint16*)data);
        data += sizeof(uint16);
        uint16 height = *((uint16*)data);
        data += sizeof(uint16);
        const uint8 *pixels = data;

        sg_image **userdata = res->user_data;
        sg_image *image = *userdata;
        *image = sg_make_image(&(sg_image_desc){
            .width = width,
            .height = height,
            .data.subimage[0][0] = { pixels, width * height * 4 },
        });

        state.still_loading--;
    }
}

static void load_image_async(const char *path, usize max_size, sg_image *image) {
    uint8 *texture_buf = alloc(&state.arena, uint8, max_size);

    sfetch_send(&(sfetch_request_t){
        .path = path,
        .callback = image_load_callback,
        .buffer = {
            .ptr = texture_buf,
            .size = max_size,
        },
        .user_data = SFETCH_RANGE(image),
    });
    state.still_loading++;
}

void init(void) {
    stm_setup();
    state.arena = arenaMake(ARENA_VIRTUAL, MB(5));

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog,
    });

    sfetch_setup(&(sfetch_desc_t){
        .logger.func = slog,
    });

    stbi_set_flip_vertically_on_load(true);

    state.cr.userdata = &state.host;
    state.just_loaded = true;
    state.host.backend = sg_query_backend();
    state.host.make_shader = sg_make_shader;
    state.host.make_pipeline = sg_make_pipeline;
    state.host.destroy_shader = sg_destroy_shader;
    state.host.destroy_pipeline = sg_destroy_pipeline;
    state.host.apply_uniform = sg_apply_uniforms;

    load_image_async(ASSET_DIR "noise.raw", KB(264), &state.host.noise_texture);
    load_image_async(ASSET_DIR "blue-noise.raw", MB(4) + KB(256), &state.host.blue_noise_texture);

    state.host.noise_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .label = "noise-sampler",
    });

    // create render target
    sg_image offscreen_rt = sg_make_image(&(sg_image_desc) {
        .render_target = true,
        .width = state.host.config.resx,
        .height = state.host.config.resy,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "offscreen-rendertarget",
    }); 

    // create vbufs for full screen triangles

    // fullscreen triangle (no uv)
    float pos[] = {
        -1, -1, 
        -1,  3, 
         3, -1,
    };

    // fullscreen triangle (with uv)
    float posuv[] = {
        -1, -1,  0, 0,
        -1,  3,  0, 2,
         3, -1,  2, 0,
    };

    state.host.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(pos),
        .label = "pos"
    });

    state.display.bind = (sg_bindings){
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(posuv),
            .label = "pos-uv",
        }),
        .fs = {
            .images[SLOT_OffscreenRT] = offscreen_rt,
            .samplers[SLOT_Sampler] = state.host.noise_sampler,
        },
    };

    state.display.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(posuv),
        .label = "pos-uv",
    });

    // create display pipeline

    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_display_vs_in_pos].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_display_vs_in_uv].format  = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .cull_mode = SG_CULLMODE_BACK,
        .label = "display-pipeline"
    });

    // setup both pass actions

    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { 
            .load_action = SG_LOADACTION_CLEAR, 
            .clear_value = { 0.2627f, 0.2627f, 0.2823f, 1.0f } 
        }
    };

    state.offscreen_pass = (sg_pass){
        .attachments = sg_make_attachments(&(sg_attachments_desc){
            .colors[0].image = offscreen_rt,
            // .depth_stencil.image = offscreen_depth,
            .label = "offscreen-attachments",
        }),
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
        },
        .label = "offscreen-pass",
    };

#if COLLA_WIN
    state.last_write = fileGetTime(state.arena, strv("assets/shader.glsl"));
#endif

    if (!crOpen(&state.cr, strv("bin/client.dll"))) {
        fatal("crOpen failed!");
    }
}

void frame(void) {
    checkReload();

    sfetch_dowork();

    if (state.still_loading <= 0) {
        if (state.just_loaded) {
            state.just_loaded = false;
            state.host.on_load(&state.host);
        }

        // offscreen
        {
            sg_begin_pass(&state.offscreen_pass);

            sg_apply_pipeline(state.host.pip);
            sg_apply_bindings(&state.host.bind);
        
            crStep(&state.cr);

            sg_draw(0, 3, 1);

            sg_end_pass();
        }

        // display
        {
            sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });

            sg_apply_pipeline(state.display.pip);
            sg_apply_bindings(&state.display.bind);

            float winw = sapp_widthf();
            float winh = sapp_heightf();

            float scalex = winw / state.host.config.resx;
            float scaley = winh / state.host.config.resy;

            float scale = scalex > scaley ? scalex : scaley;

            float width = state.host.config.resx * scale;
            float height = state.host.config.resy * scale;

            sg_apply_viewportf((winw - width) / 2.f, (winh - height) / 2.f, width, height, false);

            sg_draw(0, 3, 1);

            sg_end_pass();
        }
    }
    else {
        sg_begin_pass(&(sg_pass){ .action = state.display.pass_action, .swapchain = sglue_swapchain() });
        sg_end_pass();
    }

    sg_commit();
}

void save_config() {
#if !COLLA_EMC
    file_t fp = fileOpen(state.arena, strv("config.ini"), FILE_WRITE);
    if (!fileIsValid(fp)) {
        err("couldn't save config");
        return;
    }

    filePrintf(state.arena, fp, "resolution x  = %d\n", state.host.config.resx);
    filePrintf(state.arena, fp, "resolution y  = %d\n", state.host.config.resy);
    filePrintf(state.arena, fp, "window width  = %d\n", sapp_width());
    filePrintf(state.arena, fp, "window height = %d\n", sapp_height());

    fileClose(fp);
#endif
}

void cleanup(void) {
    crClose(&state.cr, true);
    sg_shutdown();
    sfetch_shutdown();

    save_config();

    arenaCleanup(&state.arena);
}

void load_config() {
    state.host.config = (config_t) {
        .resx = DEFAULT_RESX,
        .resy = DEFAULT_RESY,
        .winx = DEFAULT_WINX,
        .winy = DEFAULT_WINY,
    };
#if !COLLA_EMC
    if (!fileExists("config.ini")) {
        return;
    }

    uint8 tmpbuf[4096] = {0};
    arena_t tmparena = arenaMake(ARENA_STATIC, sizeof(tmpbuf), tmpbuf);

    ini_t ini = iniParse(&tmparena, strv("config.ini"), NULL);
    initable_t *root = iniGetTable(&ini, INI_ROOT);

    int resx = iniAsInt(iniGet(root, strv("resolution x")));
    int resy = iniAsInt(iniGet(root, strv("resolution y")));
    int winx = iniAsInt(iniGet(root, strv("window width")));
    int winy = iniAsInt(iniGet(root, strv("window height")));

    if (resx > 0 && resy > 0) {
        state.host.config.resx = resx;
        state.host.config.resy = resy;
    }

    if (winx > 0 && winy > 0) {
        state.host.config.winx = winx;
        state.host.config.winy = winy;
    }
#endif
}

sapp_desc sokol_main(int argc, char* argv[]) {
    load_config();

    return (sapp_desc) {
        .width = state.host.config.winx,
        .height = state.host.config.winy,
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .window_title = "Volumetric Clouds",
    };
}

static void slog(const char* tag, uint32 log_level, uint32 log_item_id, const char* message_or_null, uint32 line_nr, const char* filename_or_null, void* user_data) {
    const char *msg = message_or_null ? message_or_null : "";
    const char *file = filename_or_null ? filename_or_null : "";

    switch (log_level) {
        case 0: fatal("%s:%u gfx: %s", file, line_nr, msg); break;
        case 1:   err("%s:%u gfx: %s", file, line_nr, msg); break;
        case 2:  warn("%s:%u gfx: %s", file, line_nr, msg); break;
        case 3:  info("%s:%u gfx: %s", file, line_nr, msg); break;
    }
}

static void checkReload(void) {
#if COLLA_WIN
    uint64 now = fileGetTime(state.arena, strv("assets/shader.glsl"));
    if (now > state.last_write) {
        state.last_write = now;
        system("rebuild");
    }
 
    crReload(&state.cr);   
#endif
}