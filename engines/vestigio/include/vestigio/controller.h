#ifndef VESTIGIO_CONTROLLER_H
#define VESTIGIO_CONTROLLER_H

#include "vestigio/spatial.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* All lengths are world units, Z is up. The player's feet are at position.z.
 * The admitted volume is a union of overlapping spheres along an upright
 * segment, sampled no farther apart than 0.25 radius. This closely follows a
 * capsule but the narrowest cross-section is about 0.992 radius; callers
 * should use the admitted volume, not assume an exact mathematical capsule.
 * Movement is kinematic; callers supply fixed simulation ticks independently
 * of presentation frame rate. */
typedef struct VgControllerConfig {
    uint32_t struct_size;
    uint32_t api_version;
    float radius;
    float height;
    float eye_height;
    float step_height;
    float ground_snap;
    float min_ground_normal_z;
    float move_speed;
    float gravity;
    float jump_speed;
    float max_fixed_dt;
    uint64_t collision_mask;
} VgControllerConfig;

typedef struct VgControllerState {
    VgVec3 feet;
    float vertical_speed;
    float last_ground_height;
    uint32_t grounded;
    uint32_t hit_wall;
    uint32_t hit_ceiling;
} VgControllerState;

typedef struct VgControllerInput {
    VgVec3 move_world; /* XY direction; normalized/clamped by controller. */
    uint32_t jump_pressed;
} VgControllerInput;

VG_API VgControllerConfig vg_controller_default_config(void);
VG_API VgResult vg_controller_step(VgSpatialScene *scene, const VgControllerConfig *config,
                                   VgEntity ignored_entity, const VgControllerInput *input,
                                   float fixed_dt, VgControllerState *in_out_state);
VG_API VgVec3 vg_controller_eye_position(const VgControllerConfig *config,
                                         const VgControllerState *state);

/* A direct, collision-aware segment probe for simple waypoint followers. It
 * follows the same controller constraints; it is not a pathfinder/navmesh.
 * Segments longer than 20 units or requiring over 2048 fixed ticks return
 * VG_ERROR_UNSUPPORTED rather than a false navigation verdict. */
VG_API VgResult vg_controller_probe_segment(VgSpatialScene *scene,
                                             const VgControllerConfig *config,
                                             VgEntity ignored_entity, VgVec3 start_feet,
                                             VgVec3 end_feet, bool *out_reachable);

#ifdef __cplusplus
}
#endif
#endif
