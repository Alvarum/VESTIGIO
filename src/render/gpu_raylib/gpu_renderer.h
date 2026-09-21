#ifndef VESTIGIO_GPU_RENDERER_H
#define VESTIGIO_GPU_RENDERER_H

#include "assets/asset_registry.h"
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

typedef enum VgGpuPacketFlags {
    VG_GPU_PACKET_NONE = 0u,
    VG_GPU_PACKET_WIREFRAME = 1u << 0u,
    VG_GPU_PACKET_DISABLE_CULLING = 1u << 1u,
    VG_GPU_PACKET_FORCE_ERROR_MATERIAL = 1u << 2u
} VgGpuPacketFlags;

typedef struct VgGpuCamera {
    VgVec3 position;
    VgVec3 target;
    VgVec3 up;
    VgCameraProjection projection;
    float vertical_fov_radians;
    float orthographic_height;
    float near_clip_metres;
    float far_clip_metres;
} VgGpuCamera;

/* One packet renders one imported node/mesh instance. Bounds are local to the
 * selected mesh/node geometry; the backend applies node_world and the packet
 * transform exactly once. VG_MODEL_NO_INDEX selects mesh 0 or its source material. */
typedef struct VgGpuModelPacket {
    uint64_t gpu_token;
    uint32_t node_index;
    uint32_t mesh_index;
    uint32_t material_override;
    uint32_t flags;
    VgTransform transform;
    VgVec3 bounds_center;
    VgVec3 bounds_extent;
} VgGpuModelPacket;

typedef struct VgGpuSpritePacket {
    uint64_t gpu_token;
    uint32_t texture_index;
    uint32_t material_mode;
    uint32_t flags;
    uint32_t reserved;
    VgVec3 position;
    VgVec3 size;
    float tint[4];
    float alpha_cutoff;
} VgGpuSpritePacket;

/* Requires the owner thread to have an active raylib graphics context. */
VgGpuRenderer *vg_gpu_renderer_create(VgGpuRendererConfig config, char *error,
                                      size_t error_capacity);
void vg_gpu_renderer_destroy(VgGpuRenderer *renderer);

/* G01 diagnostic scene: indexed by the backend as real mesh/billboard draws. */
bool vg_gpu_renderer_draw_demo(VgGpuRenderer *renderer);
void vg_gpu_renderer_present(VgGpuRenderer *renderer);
/* WPF owns the thread message pump; this variant swaps without PollInputEvents. */
void vg_gpu_renderer_present_embedded(VgGpuRenderer *renderer);

/* Explicit capture is the only G01 path that reads pixels back from the GPU. */
bool vg_gpu_renderer_capture(VgGpuRenderer *renderer, const char *path);
bool vg_gpu_renderer_validate_shader(VgGpuRenderer *renderer, const char *vertex_source,
                                     const char *fragment_source, char *error,
                                     size_t error_capacity);
bool vg_gpu_renderer_info(const VgGpuRenderer *renderer, VgGpuInfo *out_info);
VgFrameStats vg_gpu_renderer_stats(const VgGpuRenderer *renderer);

/* All renderer and executor callbacks are owner-thread only. A wrong-thread
 * release is rejected and recorded; R02 checks ownership before invoking it.
 * The executor user pointer is borrowed; detach it before destroy. */
VgAssetGpuExecutor vg_gpu_renderer_asset_executor(VgGpuRenderer *renderer);
bool vg_gpu_renderer_draw_scene(VgGpuRenderer *renderer, const VgGpuCamera *camera,
                                const VgGpuModelPacket *models, uint32_t model_count,
                                const VgGpuSpritePacket *sprites, uint32_t sprite_count);
VgResult vg_gpu_renderer_draw_world(VgGpuRenderer *renderer, VgContext *context, VgWorld world);
bool vg_gpu_renderer_gpu_token_alive(const VgGpuRenderer *renderer, uint64_t token);

#endif /* VESTIGIO_GPU_RENDERER_H */
