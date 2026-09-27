#ifndef VESTIGIO_DOOR_H
#define VESTIGIO_DOOR_H

#include "vestigio/controller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VgDoor VgDoor;

typedef enum VgDoorState {
    VG_DOOR_CLOSED = 0,
    VG_DOOR_OPENING = 1,
    VG_DOOR_OPEN = 2,
    VG_DOOR_CLOSING = 3,
    VG_DOOR_BLOCKED = 4
} VgDoorState;

/* The hinge is the panel's parent. Rotating its local Z axis moves the panel
 * mesh and the existing kinematic box collider together. Context, spatial
 * scene, entities and collider remain owned by their caller. Positive and
 * negative open angles choose opposite swing directions. */
typedef struct VgDoorDesc {
    uint32_t struct_size;
    uint32_t api_version;
    VgContext *context;
    VgSpatialScene *spatial;
    VgEntity hinge;
    VgEntity panel;
    VgSpatialCollider collider;
    VgSpatialColliderDesc collider_description;
    float open_angle_radians;
    float angular_speed_radians;
} VgDoorDesc;

VG_API VgResult vg_door_create(const VgDoorDesc *description, VgDoor **out_door);
VG_API void vg_door_destroy(VgDoor *door);
VG_API VgResult vg_door_open(VgDoor *door);
VG_API VgResult vg_door_close(VgDoor *door);
VG_API VgResult vg_door_toggle(VgDoor *door);
/* Call once per fixed simulation tick. Pass both player pointers or neither.
 * With a player, each angular substep checks its admitted controller volume
 * before moving the panel. Obstruction leaves the last safe pose solid and
 * reports VG_DOOR_BLOCKED; subsequent ticks retry the requested target. */
VG_API VgResult vg_door_step(VgDoor *door, float fixed_dt,
                             const VgControllerConfig *player_config,
                             const VgControllerState *player_state);
VG_API VgDoorState vg_door_state(const VgDoor *door);
VG_API float vg_door_angle(const VgDoor *door);

#ifdef __cplusplus
}
#endif
#endif
