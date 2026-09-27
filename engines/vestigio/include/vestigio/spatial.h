#ifndef VESTIGIO_SPATIAL_H
#define VESTIGIO_SPATIAL_H

#include "vestigio/vestigio.h"

#include <stdbool.h>
#include <stdint.h>

/* Public S01 contract. Right-handed Z-up world units. Coordinates/shape
 * values are finite and bounded to +/-1e6; query distances/radii to 1e5.
 * Mesh, sphere and capsule require uniform positive scale; boxes allow
 * positive non-uniform scale. No rigid-body solver or shear is supported.
 * Default capacity: 64 meshes, 1024 colliders, 8192 triangles per mesh;
 * configured capacity cannot exceed 65535 of each. Tangencies are accepted
 * within VG_SPATIAL_EPSILON world units. */
#define VG_SPATIAL_EPSILON 1.0e-5f

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VgSpatialScene VgSpatialScene;

typedef struct VgSpatialMesh {
    uint64_t value;
} VgSpatialMesh;

typedef struct VgSpatialCollider {
    uint64_t value;
} VgSpatialCollider;

typedef uint32_t VgSpatialShapeType;
enum {
    VG_SPATIAL_SHAPE_BOX = 1u,
    VG_SPATIAL_SHAPE_SPHERE = 2u,
    VG_SPATIAL_SHAPE_CAPSULE = 3u,
    VG_SPATIAL_SHAPE_STATIC_MESH = 4u
};

typedef struct VgSpatialSceneConfig {
    uint32_t max_meshes;
    uint32_t max_colliders;
    uint32_t max_triangles_per_mesh;
    void *allocator_user;
    VgAllocateFn allocate;
    VgDeallocateFn deallocate;
} VgSpatialSceneConfig;

typedef struct VgSpatialMeshData {
    const VgVec3 *positions;
    uint32_t position_count;
    const uint32_t *indices;
    uint32_t index_count;
} VgSpatialMeshData;

typedef struct VgSpatialBox {
    VgVec3 center;
    VgVec3 half_extents;
} VgSpatialBox;

typedef struct VgSpatialSphere {
    VgVec3 center;
    float radius;
} VgSpatialSphere;

typedef struct VgSpatialCapsule {
    VgVec3 point_a;
    VgVec3 point_b;
    float radius;
} VgSpatialCapsule;

typedef struct VgSpatialColliderDesc {
    VgEntity entity;
    uint64_t layer_mask;
    VgTransform transform;
    VgSpatialShapeType shape_type;
    bool enabled;
    union {
        VgSpatialBox box;
        VgSpatialSphere sphere;
        VgSpatialCapsule capsule;
        VgSpatialMesh mesh;
    } shape;
} VgSpatialColliderDesc;

typedef struct VgSpatialRayQuery {
    VgVec3 origin;
    VgVec3 direction;
    float max_distance;
    uint64_t layer_mask;
    VgEntity ignored_entity;
} VgSpatialRayQuery;

typedef struct VgSpatialSphereQuery {
    VgVec3 center;
    float radius;
    uint64_t layer_mask;
    VgEntity ignored_entity;
} VgSpatialSphereQuery;

typedef struct VgSpatialSphereSweep {
    VgVec3 center;
    float radius;
    VgVec3 direction;
    float max_distance;
    uint64_t layer_mask;
    VgEntity ignored_entity;
} VgSpatialSphereSweep;

typedef struct VgSpatialHit {
    VgEntity entity;
    VgSpatialCollider collider;
    VgVec3 position;
    VgVec3 normal;
    float distance;
    float fraction;
} VgSpatialHit;

/* Overlap and initially-penetrating sweep: distance is negative depth,
 * fraction is zero, position is closest collider surface, and normal points
 * from collider toward query center. A tangent contact has distance zero.
 * Sweep parallel to a tangent surface does not block lateral movement. */

typedef struct VgSpatialStats {
    uint32_t meshes;
    uint32_t colliders;
    uint64_t revision;
    uint64_t broadphase_tests;
    uint64_t triangle_tests;
} VgSpatialStats;

typedef void (*VgSpatialDebugVisitFn)(void *user, VgSpatialCollider collider,
                                      const VgSpatialColliderDesc *description, VgVec3 bounds_min,
                                      VgVec3 bounds_max);
/* Triangles are delivered in mesh-local coordinates. Apply the collider's
 * authoritative transform from vg_spatial_scene_visit_debug for world-space
 * overlays. The visitor sees the current replacement, never stale geometry.
 * Do not mutate the scene from either debug visitor callback. */
typedef void (*VgSpatialTriangleVisitFn)(void *user, uint32_t triangle_index,
                                         VgVec3 a, VgVec3 b, VgVec3 c);

VgResult vg_spatial_scene_create(const VgSpatialSceneConfig *config, VgSpatialScene **out_scene);
void vg_spatial_scene_destroy(VgSpatialScene *scene);

VgResult vg_spatial_mesh_create(VgSpatialScene *scene, const VgSpatialMeshData *data,
                                VgSpatialMesh *out_mesh);
VgResult vg_spatial_mesh_replace(VgSpatialScene *scene, VgSpatialMesh mesh,
                                 const VgSpatialMeshData *data);
VgResult vg_spatial_mesh_destroy(VgSpatialScene *scene, VgSpatialMesh mesh);

VgResult vg_spatial_collider_create(VgSpatialScene *scene,
                                    const VgSpatialColliderDesc *description,
                                    VgSpatialCollider *out_collider);
VgResult vg_spatial_collider_update(VgSpatialScene *scene, VgSpatialCollider collider,
                                    const VgSpatialColliderDesc *description);
VgResult vg_spatial_collider_destroy(VgSpatialScene *scene, VgSpatialCollider collider);

VgResult vg_spatial_raycast(VgSpatialScene *scene, const VgSpatialRayQuery *query,
                            bool *out_hit, VgSpatialHit *out_result);
VgResult vg_spatial_overlap_sphere(VgSpatialScene *scene, const VgSpatialSphereQuery *query,
                                  bool *out_hit, VgSpatialHit *out_result);
VgResult vg_spatial_sweep_sphere(VgSpatialScene *scene, const VgSpatialSphereSweep *query,
                                bool *out_hit, VgSpatialHit *out_result);

VgSpatialStats vg_spatial_scene_stats(const VgSpatialScene *scene);
VgResult vg_spatial_scene_visit_debug(const VgSpatialScene *scene, VgSpatialDebugVisitFn visit,
                                      void *user);
VgResult vg_spatial_mesh_visit_triangles(const VgSpatialScene *scene, VgSpatialMesh mesh,
                                         VgSpatialTriangleVisitFn visit, void *user);

/* Copy the authoritative world transform, including parents, to an existing
 * collider after entity/parent edits. The caller owns the collider lifecycle. */
VgResult vg_spatial_world_refresh_entity(VgContext *context, VgSpatialScene *scene,
                                         VgSpatialCollider collider,
                                         const VgSpatialColliderDesc *description);

#ifdef __cplusplus
}
#endif
#endif
