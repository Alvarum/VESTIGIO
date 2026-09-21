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

#if defined(_WIN32) && defined(VG_SHARED)
#if defined(VG_BUILD)
#define VG_API __declspec(dllexport)
#else
#define VG_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(VG_SHARED)
#define VG_API __attribute__((visibility("default")))
#else
#define VG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define VG_API_VERSION_MAJOR 0u
#define VG_API_VERSION_MINOR 1u
#define VG_API_VERSION ((VG_API_VERSION_MAJOR << 16u) | VG_API_VERSION_MINOR)
#define VG_INVALID_HANDLE_VALUE UINT64_C(0)

typedef int32_t VgResult;
enum {
    VG_OK = 0,
    VG_ERROR_INVALID_ARGUMENT = -1,
    VG_ERROR_INVALID_HANDLE = -2,
    VG_ERROR_WRONG_CONTEXT = -3,
    VG_ERROR_NOT_FOUND = -4,
    VG_ERROR_CAPACITY = -5,
    VG_ERROR_OUT_OF_MEMORY = -6,
    VG_ERROR_IO = -7,
    VG_ERROR_FORMAT_VERSION = -8,
    VG_ERROR_UNSUPPORTED = -9,
    VG_ERROR_GPU = -10,
    VG_ERROR_CONFLICT = -11,
    VG_ERROR_REENTRANT = -12,
    VG_ERROR_WRONG_WORLD = -13,
    VG_ERROR_WRONG_TYPE = -14,
    VG_ERROR_WRONG_THREAD = -15
};

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

typedef uint64_t VgActionSet;
#define VG_ACTION_JUMP (UINT64_C(1) << 0u)
#define VG_ACTION_PRIMARY (UINT64_C(1) << 1u)
#define VG_ACTION_INTERACT (UINT64_C(1) << 2u)
#define VG_ACTION_PAUSE (UINT64_C(1) << 3u)
#define VG_ACTION_ACCEPT (UINT64_C(1) << 4u)
#define VG_ACTION_UI_UP (UINT64_C(1) << 5u)
#define VG_ACTION_UI_DOWN (UINT64_C(1) << 6u)
#define VG_ACTION_UI_LEFT (UINT64_C(1) << 7u)
#define VG_ACTION_UI_RIGHT (UINT64_C(1) << 8u)
#define VG_ACTION_MAP (UINT64_C(1) << 9u)
#define VG_ACTION_WIREFRAME (UINT64_C(1) << 10u)
#define VG_ACTION_DEPTH (UINT64_C(1) << 11u)
#define VG_ACTION_STATS (UINT64_C(1) << 12u)
#define VG_ACTION_QUICK_SAVE (UINT64_C(1) << 13u)
#define VG_ACTION_QUICK_LOAD (UINT64_C(1) << 14u)
#define VG_ACTION_MOVE_FORWARD (UINT64_C(1) << 15u)
#define VG_ACTION_MOVE_BACKWARD (UINT64_C(1) << 16u)
#define VG_ACTION_MOVE_LEFT (UINT64_C(1) << 17u)
#define VG_ACTION_MOVE_RIGHT (UINT64_C(1) << 18u)

/* Stable physical codes use USB HID keyboard usages. Pointer buttons occupy
 * the private 0x0001xxxx range so persisted bindings are host independent. */
typedef uint32_t VgInputCode;
#define VG_INPUT_KEY_A UINT32_C(0x04)
#define VG_INPUT_KEY_D UINT32_C(0x07)
#define VG_INPUT_KEY_E UINT32_C(0x08)
#define VG_INPUT_KEY_S UINT32_C(0x16)
#define VG_INPUT_KEY_W UINT32_C(0x1A)
#define VG_INPUT_KEY_ENTER UINT32_C(0x28)
#define VG_INPUT_KEY_ESCAPE UINT32_C(0x29)
#define VG_INPUT_KEY_SPACE UINT32_C(0x2C)
#define VG_INPUT_KEY_F1 UINT32_C(0x3A)
#define VG_INPUT_KEY_F2 UINT32_C(0x3B)
#define VG_INPUT_KEY_F3 UINT32_C(0x3C)
#define VG_INPUT_KEY_F4 UINT32_C(0x3D)
#define VG_INPUT_KEY_F5 UINT32_C(0x3E)
#define VG_INPUT_KEY_F9 UINT32_C(0x42)
#define VG_INPUT_KEY_RIGHT UINT32_C(0x4F)
#define VG_INPUT_KEY_LEFT UINT32_C(0x50)
#define VG_INPUT_KEY_DOWN UINT32_C(0x51)
#define VG_INPUT_KEY_UP UINT32_C(0x52)
#define VG_INPUT_MOUSE_PRIMARY UINT32_C(0x00010001)

typedef struct VgInputSample {
    uint32_t struct_size;
    uint32_t api_version;
    VgActionSet pressed;
    VgActionSet held;
    VgActionSet released;
    float look_delta_x;
    float look_delta_y;
    uint32_t focused;
    uint32_t reserved;
} VgInputSample;

typedef struct VgInputState {
    uint32_t struct_size;
    uint32_t api_version;
    VgActionSet pressed;
    VgActionSet held;
    VgActionSet released;
    float look_delta_x;
    float look_delta_y;
    uint32_t focused;
    uint32_t reserved;
    uint64_t tick_index;
} VgInputState;

#define VG_SETTINGS_MAX_BINDINGS 32u

typedef uint32_t VgSettingMask;
enum {
    VG_SETTING_INTERNAL_RESOLUTION = 1u << 0u,
    VG_SETTING_FULLSCREEN = 1u << 1u,
    VG_SETTING_VSYNC = 1u << 2u,
    VG_SETTING_FRAME_CAP = 1u << 3u,
    VG_SETTING_LOOK_SENSITIVITY = 1u << 4u,
    VG_SETTING_BINDINGS = 1u << 5u,
    VG_SETTINGS_ALL = (1u << 6u) - 1u,
    VG_SETTINGS_APPLY_IMMEDIATE =
        VG_SETTING_FRAME_CAP | VG_SETTING_LOOK_SENSITIVITY | VG_SETTING_BINDINGS,
    VG_SETTINGS_RECREATE_TARGETS = VG_SETTING_INTERNAL_RESOLUTION,
    VG_SETTINGS_RECREATE_SURFACE = VG_SETTING_FULLSCREEN | VG_SETTING_VSYNC
};

typedef struct VgInputBinding {
    VgActionSet action;
    VgInputCode code;
    uint32_t reserved;
} VgInputBinding;

typedef struct VgSettingsLayer {
    uint32_t struct_size;
    uint32_t api_version;
    VgSettingMask present;
    uint32_t internal_width;
    uint32_t internal_height;
    uint32_t fullscreen;
    uint32_t vsync;
    uint32_t frame_cap;
    float look_sensitivity;
    uint32_t binding_count;
    uint32_t reserved;
    VgInputBinding bindings[VG_SETTINGS_MAX_BINDINGS];
} VgSettingsLayer;

typedef struct VgSettingsChanges {
    uint32_t struct_size;
    uint32_t api_version;
    VgSettingMask immediate;
    VgSettingMask recreate_targets;
    VgSettingMask recreate_surface;
} VgSettingsChanges;

typedef struct VgSettingsDiagnostic {
    uint32_t struct_size;
    uint32_t api_version;
    VgResult result;
    uint32_t line;
    char message[160];
} VgSettingsDiagnostic;

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

/* Settings layers resolve in project -> user -> session order without mutating
 * any layer. VSync and frame cap affect presentation pacing only; fixed_delta
 * remains owned by VgGameDesc. Save publishes by durable temporary file plus
 * atomic replacement, so a failed write preserves the prior file. */
VG_API VgResult vg_settings_defaults(VgSettingsLayer *out_settings);
VG_API VgResult vg_settings_validate(const VgSettingsLayer *settings,
                                     VgSettingsDiagnostic *out_diagnostic);
VG_API VgResult vg_settings_resolve(const VgSettingsLayer *project, const VgSettingsLayer *user,
                                    const VgSettingsLayer *session, VgSettingsLayer *out_resolved,
                                    VgSettingsDiagnostic *out_diagnostic);
VG_API VgResult vg_settings_diff(const VgSettingsLayer *before, const VgSettingsLayer *after,
                                 VgSettingsChanges *out_changes);
VG_API VgResult vg_settings_load_file(const char *utf8_path, VgSettingsLayer *out_settings,
                                      VgSettingsDiagnostic *out_diagnostic);
VG_API VgResult vg_settings_save_file(const char *utf8_path, const VgSettingsLayer *settings,
                                      VgSettingsDiagnostic *out_diagnostic);

/* Asset acquisition resolves a catalog entry already registered by the
 * project/content layer. Zero required_residency means CPU. GPU residency is
 * completed only when the owning backend flushes its private upload queue. */
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
