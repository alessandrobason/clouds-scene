#pragma once

#include "sokol/sokol_gfx.h"

struct host_t;

typedef sg_shader (*make_shader_f)(const sg_shader_desc* desc);
typedef sg_pipeline (*make_pipeline_f)(const sg_pipeline_desc* desc);
typedef void (*destroy_shader_f)(sg_shader shd);
typedef void (*destroy_pipeline_f)(sg_pipeline shd);
typedef void (*apply_uniform_f)(sg_shader_stage stage, int ub_index, const sg_range* data);

typedef void (*on_load_f)(struct host_t *host);

typedef struct host_t {
    sg_backend backend;
    sg_pipeline pip;
    sg_bindings bind;
    sg_shader shader;
    sg_image noise_texture;
    sg_image blue_noise_texture;
    sg_sampler noise_sampler;

    on_load_f on_load;

    make_shader_f      make_shader;
    make_pipeline_f    make_pipeline;
    destroy_shader_f   destroy_shader;
    destroy_pipeline_f destroy_pipeline;
    apply_uniform_f    apply_uniform;
} host_t;

#define RESX 900
#define RESY 450
#define WINX 1800
#define WINY 1000