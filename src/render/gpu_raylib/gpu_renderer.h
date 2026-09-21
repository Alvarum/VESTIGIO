#ifndef VESTIGIO_GPU_RENDERER_H
#define VESTIGIO_GPU_RENDERER_H

#include "render/render_contract.h"
#include <stddef.h>

typedef struct VgGpuRenderer VgGpuRenderer;

typedef struct VgGpuRendererConfig {
    uint32_t internal_width;
    uint32_t internal_height;
} VgGpuRendererConfig;

typedef struct VgGpuInfo {
    char vendor[128];
    char renderer[128];
    char version[128];
    int32_t rlgl_version;
} VgGpuInfo;

/* Requires the owner thread to have an active raylib graphics context. */
VgGpuRenderer *vg_gpu_renderer_create(VgGpuRendererConfig config, char *error,
                                      size_t error_capacity);
void vg_gpu_renderer_destroy(VgGpuRenderer *renderer);

/* G01 diagnostic scene: indexed by the backend as real mesh/billboard draws. */
bool vg_gpu_renderer_draw_demo(VgGpuRenderer *renderer);
void vg_gpu_renderer_present(VgGpuRenderer *renderer);

/* Explicit capture is the only G01 path that reads pixels back from the GPU. */
bool vg_gpu_renderer_capture(VgGpuRenderer *renderer, const char *path);
bool vg_gpu_renderer_validate_shader(const char *vertex_source, const char *fragment_source,
                                     char *error, size_t error_capacity);
bool vg_gpu_renderer_info(VgGpuInfo *out_info);
VgFrameStats vg_gpu_renderer_stats(const VgGpuRenderer *renderer);

#endif /* VESTIGIO_GPU_RENDERER_H */
