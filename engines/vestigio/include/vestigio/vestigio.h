#ifndef VESTIGIO_VESTIGIO_H
#define VESTIGIO_VESTIGIO_H

/* Public VESTIGIO game API contract v0.1.
 *
 * This header is consumable as C11 or C++11 and deliberately contains no
 * RetroForge, raylib, OpenGL, Win32 or WPF types. Functions are introduced by
 * the ticket that implements their complete lifecycle; these declarations
 * establish ABI-safe vocabulary shared by those tickets. */

#include <stddef.h>
#include <stdint.h>

#include "engine_common/input_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t VgBackend;
enum { VG_BACKEND_DEFAULT = 0u, VG_BACKEND_OPENGL_33 = 1u };

typedef uint32_t VgLogSeverity;
enum { VG_LOG_DEBUG = 0u, VG_LOG_INFO = 1u, VG_LOG_WARNING = 2u, VG_LOG_ERROR = 3u };

typedef struct VgContext VgContext;
typedef struct VgGame VgGame;
typedef struct VgWorld {
    uint64_t value;
} VgWorld;
typedef struct VgEntity {
    uint64_t value;
} VgEntity;
typedef struct VgAsset {
    uint64_t value;
} VgAsset;
typedef struct VgVoice {
    uint64_t value;
} VgVoice;

/* Persistent document identity. It is data, never a runtime handle. */
typedef struct VgUuid {
    uint8_t bytes[16];
} VgUuid;

typedef struct VgVec3 {
    float x, y, z;
} VgVec3;
typedef struct VgQuat {
    float x, y, z, w;
} VgQuat;
typedef struct VgTransform {
    VgVec3 position;
    VgQuat rotation;
    VgVec3 scale;
} VgTransform;

typedef uint32_t VgCameraProjection;
enum { VG_CAMERA_PERSPECTIVE = 1u, VG_CAMERA_ORTHOGRAPHIC = 2u };

typedef struct VgCameraDesc {
    uint32_t struct_size;
    uint32_t api_version;
    VgCameraProjection projection;
    uint32_t flags;
    float vertical_fov_radians;
    float orthographic_height;
    float near_clip_metres;
    float far_clip_metres;
} VgCameraDesc;

#define VG_EVENT_PAYLOAD_CAPACITY 64u

typedef struct VgEvent {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t type;
    uint32_t flags;
    VgEntity source;
    VgEntity target;
    uint32_t payload_size;
    uint32_t reserved;
    uint8_t payload[VG_EVENT_PAYLOAD_CAPACITY];
} VgEvent;

typedef struct VgUiFrame {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t frame_index;
    uint32_t viewport_width_pixels;
    uint32_t viewport_height_pixels;
    float interpolation_alpha;
    uint32_t reserved;
} VgUiFrame;

typedef struct VgGameCallbacks {
    uint32_t struct_size;
    uint32_t api_version;
    void *user;
    VgResult (*init)(VgContext *context, void *user);
    VgResult (*world_ready)(VgContext *context, VgWorld world, void *user);
    void (*fixed_update)(VgContext *context, VgWorld world, float dt_seconds, void *user);
    void (*event)(VgContext *context, const VgEvent *event, void *user);
    void (*draw_ui)(VgContext *context, VgUiFrame *frame, void *user);
    void (*shutdown)(VgContext *context, void *user);
} VgGameCallbacks;

typedef struct VgGameDesc {
    uint32_t struct_size;
    uint32_t api_version;
    double fixed_delta_seconds;
    double max_frame_delta_seconds;
    uint32_t max_fixed_steps_per_frame;
    uint32_t event_capacity;
    uint32_t max_events_per_tick;
    uint32_t reserved;
} VgGameDesc;

typedef struct VgStepInfo {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t fixed_steps;
    uint32_t events_dispatched;
    uint32_t dropped_fixed_steps;
    uint32_t pending_events;
    double simulated_seconds;
    double dropped_seconds;
    float interpolation_alpha;
    uint32_t reserved;
} VgStepInfo;

/* AssetId is persistent project identity (UUID bytes in canonical network
 * order). VgAsset is a context-local, generational lease and must never be
 * serialized. Each acquire/clone returns an independent lease. */
typedef struct VgAssetId {
    uint8_t bytes[16];
} VgAssetId;

typedef uint32_t VgAssetType;
enum { VG_ASSET_TYPE_TEXTURE = 1u, VG_ASSET_TYPE_MESH = 2u };

typedef uint32_t VgAssetState;
enum { VG_ASSET_LOADING = 1u, VG_ASSET_READY = 2u, VG_ASSET_FAILED = 3u };

typedef uint32_t VgAssetResidency;
enum { VG_ASSET_RESIDENCY_CPU = 1u << 0u, VG_ASSET_RESIDENCY_GPU = 1u << 1u };

typedef uint32_t VgAssetInfoFlags;
enum {
    VG_ASSET_INFO_CPU_RESIDENT = 1u << 0u,
    VG_ASSET_INFO_GPU_RESIDENT = 1u << 1u,
    VG_ASSET_INFO_RELOAD_PENDING = 1u << 2u,
    VG_ASSET_INFO_EVICTABLE = 1u << 3u,
    VG_ASSET_INFO_HAS_USABLE_VERSION = 1u << 4u
};

#define VG_ASSET_FINGERPRINT_SIZE 32u
#define VG_STATIC_MODEL_IMPORTER_VERSION 1u
#define VG_RENDER_DEFAULT_INDEX UINT32_MAX

typedef uint32_t VgAlphaMode;
enum { VG_ALPHA_OPAQUE = 0u, VG_ALPHA_MASK = 1u, VG_ALPHA_BLEND = 2u };

typedef uint32_t VgRenderFlags;
enum {
    VG_RENDER_NONE = 0u,
    VG_RENDER_WIREFRAME = 1u << 0u,
    VG_RENDER_DISABLE_CULLING = 1u << 1u,
    VG_RENDER_FORCE_ERROR_MATERIAL = 1u << 2u
};

typedef struct VgMeshRendererDesc {
    uint32_t struct_size;
    uint32_t api_version;
    VgAsset asset;
    uint32_t node_index;
    uint32_t mesh_index;
    uint32_t material_override;
    VgRenderFlags flags;
    VgVec3 bounds_center;
    VgVec3 bounds_extent;
} VgMeshRendererDesc;

typedef struct VgSpriteRendererDesc {
    uint32_t struct_size;
    uint32_t api_version;
    VgAsset asset;
    uint32_t texture_index;
    VgAlphaMode alpha_mode;
    VgRenderFlags flags;
    uint32_t reserved;
    float width_metres;
    float height_metres;
    float tint[4];
    float alpha_cutoff;
} VgSpriteRendererDesc;

/* Runtime transforms use right-handed Z-up coordinates, metres and radians.
 * Rotations are normalized on write. Scale must be finite and strictly
 * positive. Hierarchy operations that would require shear are rejected. */

typedef void (*VgLogFn)(void *user, VgLogSeverity severity, const char *utf8_message);
typedef void *(*VgAllocateFn)(void *user, uint64_t size);
typedef void (*VgDeallocateFn)(void *user, void *allocation);

typedef struct VgContextDesc {
    uint32_t struct_size;
    uint32_t api_version;
    VgBackend backend;
    uint32_t flags;
    void *user;
    VgLogFn log;
    void *allocator_user;
    VgAllocateFn allocate;
    VgDeallocateFn deallocate;
    uint32_t max_worlds;
    uint32_t max_assets;
    uint32_t max_asset_leases;
} VgContextDesc;

typedef struct VgWorldDesc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t initial_entity_capacity;
    uint32_t max_entities;
} VgWorldDesc;

typedef uint32_t VgReparentMode;
enum { VG_REPARENT_KEEP_LOCAL = 0u, VG_REPARENT_KEEP_WORLD = 1u };

typedef struct VgVersion {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} VgVersion;

typedef struct VgAssetRequest {
    uint32_t struct_size;
    uint32_t api_version;
    VgAssetId id;
    VgAssetType type;
    VgAssetResidency required_residency;
    uint64_t variant;
} VgAssetRequest;

typedef struct VgAssetInfo {
    uint32_t struct_size;
    uint32_t api_version;
    VgAssetId id;
    VgAssetType type;
    VgAssetState state;
    VgAssetInfoFlags flags;
    uint32_t external_refs;
    uint32_t component_refs;
    uint64_t variant;
    uint64_t published_version;
    uint64_t candidate_version;
    VgResult last_result;
    VgResult last_reload_result;
    uint64_t source_ram_bytes;
    uint64_t derived_ram_bytes;
    uint64_t staging_ram_bytes;
    uint64_t estimated_gpu_bytes;
} VgAssetInfo;

typedef struct VgAssetCounters {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t catalog_entries;
    uint32_t resident_entries;
    uint32_t live_leases;
    uint32_t component_refs;
    uint32_t loading;
    uint32_t ready;
    uint32_t failed;
    uint32_t evictable;
    uint32_t pending_gpu_releases;
    uint64_t source_ram_bytes;
    uint64_t derived_ram_bytes;
    uint64_t staging_ram_bytes;
    uint64_t estimated_gpu_bytes;
    uint64_t decode_count;
    uint64_t upload_count;
    uint64_t gpu_release_count;
    uint64_t cache_hit_count;
    uint64_t purge_count;
} VgAssetCounters;

/* Catalog sources are copied by the context. A zero fingerprint asks the
 * decoder to compute it; a non-zero fingerprint is verified before publish. */
typedef struct VgAssetSourceDesc {
    uint32_t struct_size;
    uint32_t api_version;
    VgAssetId id;
    VgAssetType type;
    uint32_t importer_version;
    uint64_t version;
    uint64_t variant;
    uint8_t fingerprint[VG_ASSET_FINGERPRINT_SIZE];
    const char *source_path;
    const void *source_data;
    uint64_t source_size;
    const void *options_data;
    uint32_t options_size;
} VgAssetSourceDesc;

/* Pure query with no runtime allocation. Implemented with the first runtime
 * library target; declared now so consumers can negotiate the contract. */
VG_API VgResult vg_get_version(VgVersion *out_version);

/* Context and world ownership. Descriptors may be zero initialized; a zero
 * max selects an implementation default. A custom allocator must provide both
 * callbacks. All allocations owned by a context use that allocator. */
VG_API VgResult vg_context_create(const VgContextDesc *description, VgContext **out_context);
/* When a GPU executor is attached, destroy must run on its owner thread. A
 * wrong-thread call logs an error and leaves the context alive and unchanged. */
VG_API void vg_context_destroy(VgContext *context);
VG_API VgResult vg_world_create(VgContext *context, const VgWorldDesc *description,
                                VgWorld *out_world);
VG_API VgResult vg_world_destroy(VgContext *context, VgWorld world);
VG_API VgResult vg_world_reserve_entities(VgContext *context, VgWorld world, uint32_t capacity);

/* Iteration exposes a stable set until end_iteration. Creates and destroys are
 * committed at that boundary; their handles remain usable inside the batch.
 * Nested iteration is rejected. */
VG_API VgResult vg_world_begin_iteration(VgContext *context, VgWorld world,
                                         uint32_t *out_entity_count);
VG_API VgResult vg_world_entity_at(VgContext *context, VgWorld world, uint32_t ordinal,
                                   VgEntity *out_entity);
VG_API VgResult vg_world_end_iteration(VgContext *context, VgWorld world);

VG_API VgResult vg_entity_create(VgContext *context, VgWorld world, VgEntity *out_entity);
VG_API VgResult vg_entity_destroy(VgContext *context, VgEntity entity);
VG_API VgResult vg_entity_get_local_transform(VgContext *context, VgEntity entity,
                                              VgTransform *out_transform);
VG_API VgResult vg_entity_get_world_transform(VgContext *context, VgEntity entity,
                                              VgTransform *out_transform);
VG_API VgResult vg_entity_set_local_transform(VgContext *context, VgEntity entity,
                                              const VgTransform *transform);
VG_API VgResult vg_entity_set_world_transform(VgContext *context, VgEntity entity,
                                              const VgTransform *transform);
VG_API VgResult vg_entity_get_parent(VgContext *context, VgEntity entity, VgEntity *out_parent);
VG_API VgResult vg_entity_set_parent(VgContext *context, VgEntity entity, VgEntity parent,
                                     VgReparentMode mode);

/* Camera is a small runtime component. It describes a view without exposing a
 * renderer or GPU surface. The entity transform supplies its pose. */
VG_API VgResult vg_camera_set(VgContext *context, VgEntity entity, const VgCameraDesc *description);
VG_API VgResult vg_camera_get(VgContext *context, VgEntity entity, VgCameraDesc *out_description);
VG_API VgResult vg_camera_clear(VgContext *context, VgEntity entity);

/* Render components retain their asset independently from the caller's lease.
 * get returns a fresh lease in out_description->asset; release it normally. */
VG_API VgResult vg_mesh_renderer_set(VgContext *context, VgEntity entity,
                                     const VgMeshRendererDesc *description);
VG_API VgResult vg_mesh_renderer_get(VgContext *context, VgEntity entity,
                                     VgMeshRendererDesc *out_description);
VG_API VgResult vg_mesh_renderer_clear(VgContext *context, VgEntity entity);
VG_API VgResult vg_sprite_renderer_set(VgContext *context, VgEntity entity,
                                       const VgSpriteRendererDesc *description);
VG_API VgResult vg_sprite_renderer_get(VgContext *context, VgEntity entity,
                                       VgSpriteRendererDesc *out_description);
VG_API VgResult vg_sprite_renderer_clear(VgContext *context, VgEntity entity);

/* A game instance copies its callback table and owns a bounded FIFO event
 * queue. init runs during create. world_ready runs only after a world resolves
 * successfully. Each fixed tick calls fixed_update and then drains its event
 * budget in FIFO order. draw_ui is explicitly requested by the host, and
 * shutdown runs exactly once after init was entered, including init failure.
 * Callbacks are synchronous. Lifecycle/step/draw re-entry is rejected; event
 * emission from a callback is allowed and remains bounded by the queue. */
VG_API VgResult vg_game_create(VgContext *context, const VgGameDesc *description,
                               const VgGameCallbacks *callbacks, VgGame **out_game);
VG_API VgResult vg_game_set_world(VgGame *game, VgWorld world);
VG_API VgResult vg_game_clear_world(VgGame *game);
VG_API VgResult vg_game_emit_event(VgGame *game, const VgEvent *event);
VG_API VgResult vg_game_submit_input(VgGame *game, const VgInputSample *sample);
VG_API VgResult vg_game_get_input(VgGame *game, VgInputState *out_state);
VG_API VgResult vg_game_step(VgGame *game, double elapsed_seconds, VgStepInfo *out_info);
VG_API VgResult vg_game_draw_ui(VgGame *game, VgUiFrame *frame);
VG_API VgResult vg_game_destroy(VgGame *game);

/* Source registration and decoder setup are transactional. The built-in static
 * model decoder accepts glTF 2.0/GLB sources and uses the source's import limits.
 * Zero required_residency means CPU. GPU residency is completed only when the
 * owning backend flushes its private upload queue. */
VG_API VgResult vg_assets_enable_static_model_importer(VgContext *context);
VG_API VgResult vg_asset_catalog_upsert(VgContext *context, const VgAssetSourceDesc *source);
VG_API VgResult vg_asset_acquire(VgContext *context, const VgAssetRequest *request,
                                 VgAsset *out_asset);
VG_API VgResult vg_asset_clone(VgContext *context, VgAsset source, VgAsset *out_asset);
VG_API VgResult vg_asset_release(VgContext *context, VgAsset asset);
VG_API VgResult vg_asset_reload(VgContext *context, VgAsset asset);
VG_API VgResult vg_asset_get_info(VgContext *context, VgAsset asset, VgAssetInfo *out_info);
VG_API VgResult vg_asset_get_error(VgContext *context, VgAsset asset, char *utf8, uint32_t capacity,
                                   uint32_t *out_required);
VG_API VgResult vg_assets_get_counters(VgContext *context, VgAssetCounters *out_counters);
VG_API VgResult vg_assets_purge_unused(VgContext *context, uint32_t *out_purged);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VESTIGIO_VESTIGIO_H */
