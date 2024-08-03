#include "cr.h"

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_time.h"
#include "colla/tracelog.h"

#include "shared.h"
#include "shader.h"

static void on_load(host_t *host) {
    host->bind.fs.images[SLOT_NoiseTex]       = host->noise_texture;
    host->bind.fs.images[SLOT_BlueNoiseTex]   = host->blue_noise_texture;
}

CR_EXPORT int cr_init(cr_t *ctx) {
    stm_setup();
 
    host_t *host = ctx->userdata;

    host->on_load = on_load;

    host->bind.fs.samplers[SLOT_NoiseSampler] = host->noise_sampler;

    if (host->shader.id) {
        host->destroy_shader(host->shader);
    }
    if (host->pip.id) {
        host->destroy_pipeline(host->pip);
    }

    host->shader = host->make_shader(shader_shader_desc(host->backend));

    host->pip = host->make_pipeline(&(sg_pipeline_desc){
        .shader = host->shader,
        // if the vertex layout doesn't have gaps, don't need to provide strides and offsets
        .layout = {
            .attrs = {
                [ATTR_vs_pos].format = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .depth.pixel_format = SG_PIXELFORMAT_NONE,
        .cull_mode = SG_CULLMODE_BACK,
        .label = "offscreen-pipeline"
    });

    return 0;
}

CR_EXPORT int cr_loop(cr_t *ctx) {
    host_t *host = ctx->userdata;

    uniforms_t uniforms = {
        .Resolution = { host->config.resx, host->config.resy, },
        .Time = (float)stm_sec(stm_now()),
    };
    host->apply_uniform(SG_SHADERSTAGE_FS, SLOT_uniforms, &SG_RANGE(uniforms));

    return 0;
}

CR_EXPORT int cr_close(cr_t *ctx) {
    info("closing!");
    return 0;
}