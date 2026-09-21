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
    VG_ERROR_WRONG_TYPE = -14
};

typedef uint32_t VgBackend;
enum { VG_BACKEND_DEFAULT = 0u, VG_BACKEND_OPENGL_33 = 1u };

typedef uint32_t VgLogSeverity;
enum { VG_LOG_DEBUG = 0u, VG_LOG_INFO = 1u, VG_LOG_WARNING = 2u, VG_LOG_ERROR = 3u };

typedef struct VgContext VgContext;
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

/* Runtime transforms use right-handed Z-up coordinates, metres and radians.
 * Rotations are normalized on write. Scale must be finite and strictly
 * positive. Hierarchy operations that would require shear are rejected. */

typedef void (*VgLogFn)(void *user, VgLogSeverity severity, const char *utf8_message);
typedef void *(*VgAllocateFn)(void *user, size_t size);
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
} VgContextDesc;

typedef struct VgWorldDesc {
    uint32_t struct_size;
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

/* Pure query with no runtime allocation. Implemented with the first runtime
 * library target; declared now so consumers can negotiate the contract. */
VG_API VgResult vg_get_version(VgVersion *out_version);

/* Context and world ownership. Descriptors may be zero initialized; a zero
 * max selects an implementation default. A custom allocator must provide both
 * callbacks. All allocations owned by a context use that allocator. */
VG_API VgResult vg_context_create(const VgContextDesc *description, VgContext **out_context);
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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VESTIGIO_VESTIGIO_H */
