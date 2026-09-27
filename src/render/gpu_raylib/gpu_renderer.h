#ifndef VESTIGIO_GPU_RENDERER_H
#define VESTIGIO_GPU_RENDERER_H

#include "assets/asset_registry.h"
#include "render/render_contract.h"
#include "vestigio/spatial.h"
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

#define VG_GPU_MAX_POINT_LIGHTS 4u

typedef enum VgGpuVisualMode {
    VG_GPU_VISUAL_CLEAN = 0,
    VG_GPU_VISUAL_RETRO = 1
} VgGpuVisualMode;

typedef struct VgGpuPointLight {
    VgVec3 position;
    float radius;
    float color[3];
    float intensity;
} VgGpuPointLight;

typedef struct VgGpuVisualSettings {
    VgGpuVisualMode mode;
    uint32_t point_light_count;
    float ambient[3];
    float clear_color[3];
    uint32_t fog_enabled;
    float fog_color[3];
    float fog_start;
    float fog_end;
    VgGpuPointLight lights[VG_GPU_MAX_POINT_LIGHTS];
} VgGpuVisualSettings;

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

/* Editor-only GPU overlay. The axis handles use a stable screen-space size so
 * the visual handle and its hit target agree at any supported camera distance. */
typedef struct VgGpuGizmo {
    VgVec3 position;
    VgQuat rotation;
    int32_t operation; /* 0 move, 1 rotate, 2 scale */
} VgGpuGizmo;

/* Requires the owner thread to have an active raylib graphics context. */
VgGpuRenderer *vg_gpu_renderer_create(VgGpuRendererConfig config, char *error,
                                      size_t error_capacity);
void vg_gpu_renderer_destroy(VgGpuRenderer *renderer);
VgGpuVisualSettings vg_gpu_renderer_default_visual_settings(void);
/* Validates the whole candidate before changing the active GPU profile. */
bool vg_gpu_renderer_set_visual_settings(VgGpuRenderer *renderer,
                                         const VgGpuVisualSettings *settings,
                                         char *error, size_t error_capacity);
/* Replaces the offscreen target only after a new GPU target was created. */
bool vg_gpu_renderer_resize_internal(VgGpuRenderer *renderer, uint32_t width,
                                     uint32_t height, char *error,
                                     size_t error_capacity);

/* G01 diagnostic scene: indexed by the backend as real mesh/billboard draws. */
bool vg_gpu_renderer_draw_demo(VgGpuRenderer *renderer);
void vg_gpu_renderer_present(VgGpuRenderer *renderer);
/* Presents the GPU target with a short window-space interaction hint.
 * The hint is composited on the GPU after scaling, without framebuffer readback. */
void vg_gpu_renderer_present_with_hint(VgGpuRenderer *renderer, const char *hint);
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
/* Draw the scene's actual collider shapes over the current GPU target for
 * spatial debugging. Call after draw_world and before present. */
VgResult vg_gpu_renderer_draw_spatial_debug(VgGpuRenderer *renderer,
                                            const VgSpatialScene *spatial);
bool vg_gpu_renderer_draw_gizmos(VgGpuRenderer *renderer, const VgGpuGizmo *gizmos, size_t count);
/* Returns 0 for no handle, or 1/2/3 for X/Y/Z. u,v are normalized to the
 * internal scene image, excluding the letterboxed area of the host window. */
int32_t vg_gpu_renderer_gizmo_hit(const VgGpuRenderer *renderer, const VgGpuGizmo *gizmos,
                                  size_t count, float u, float v);
/* Unit screen-space tangent of the handle nearest u,v. For rotation this is
 * the ring tangent; for move/scale it follows the projected axis. */
bool vg_gpu_renderer_gizmo_drag_direction(const VgGpuRenderer *renderer, const VgGpuGizmo *gizmos,
                                          size_t count, float u, float v, int32_t axis,
                                          float *out_x, float *out_y);
bool vg_gpu_renderer_gpu_token_alive(const VgGpuRenderer *renderer, uint64_t token);

#endif /* VESTIGIO_GPU_RENDERER_H */
