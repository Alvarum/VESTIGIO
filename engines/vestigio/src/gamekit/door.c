#include "vestigio/door.h"

#include <math.h>
#include <stdlib.h>

#define VG_DOOR_EPSILON 1.0e-5f
#define VG_DOOR_SKIN 0.002f
#define VG_DOOR_MAX_SUBSTEPS 128u
#define VG_DOOR_MAX_ARC_METRES 0.025f

struct VgDoor {
    VgContext *context;
    VgSpatialScene *spatial;
    VgEntity hinge;
    VgEntity panel;
    VgSpatialCollider collider;
    VgSpatialColliderDesc collider_description;
    VgTransform closed_hinge;
    float open_angle;
    float speed;
    float angle;
    float target;
    VgDoorState state;
};

static VgVec3 door_subtract(VgVec3 a, VgVec3 b) {
    return (VgVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static float door_dot(VgVec3 a, VgVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float door_length(VgVec3 a) {
    return sqrtf(door_dot(a, a));
}

static bool door_finite_vec(VgVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static VgQuat door_quat_mul(VgQuat a, VgQuat b) {
    return (VgQuat){
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

static VgVec3 door_rotate(VgQuat q, VgVec3 value) {
    VgVec3 t = {2.0f * (q.y * value.z - q.z * value.y),
                2.0f * (q.z * value.x - q.x * value.z),
                2.0f * (q.x * value.y - q.y * value.x)};
    return (VgVec3){value.x + q.w * t.x + q.y * t.z - q.z * t.y,
                    value.y + q.w * t.y + q.z * t.x - q.x * t.z,
                    value.z + q.w * t.z + q.x * t.y - q.y * t.x};
}

static VgTransform door_hinge_pose(const VgDoor *door, float angle) {
    VgTransform pose = door->closed_hinge;
    float half = angle * 0.5f;
    pose.rotation = door_quat_mul(pose.rotation,
                                  (VgQuat){0.0f, 0.0f, sinf(half), cosf(half)});
    return pose;
}

static bool door_player_valid(const VgControllerConfig *config,
                              const VgControllerState *state) {
    return config->struct_size >= sizeof(*config) && config->api_version == VG_API_VERSION &&
           isfinite(config->radius) && isfinite(config->height) &&
           config->radius > VG_DOOR_SKIN && config->height >= 2.0f * config->radius &&
           config->height <= 4.0f && door_finite_vec(state->feet);
}

static bool door_sphere_overlaps_panel(const VgDoor *door, VgTransform panel,
                                       VgVec3 sphere_center, float radius) {
    VgVec3 delta = door_subtract(sphere_center, panel.position);
    VgQuat inverse = {-panel.rotation.x, -panel.rotation.y,
                       -panel.rotation.z, panel.rotation.w};
    VgVec3 local = door_rotate(inverse, delta);
    const VgSpatialBox *box = &door->collider_description.shape.box;
    float point[3] = {local.x, local.y, local.z};
    float center[3] = {box->center.x * panel.scale.x,
                       box->center.y * panel.scale.y,
                       box->center.z * panel.scale.z};
    float extent[3] = {box->half_extents.x * panel.scale.x,
                       box->half_extents.y * panel.scale.y,
                       box->half_extents.z * panel.scale.z};
    float distance_squared = 0.0f;
    for (size_t axis = 0u; axis < 3u; ++axis) {
        float distance = fabsf(point[axis] - center[axis]) - extent[axis];
        if (distance > 0.0f)
            distance_squared += distance * distance;
    }
    float admitted_radius = radius - VG_DOOR_SKIN;
    return distance_squared < admitted_radius * admitted_radius;
}

static bool door_player_overlaps_panel(const VgDoor *door, VgTransform panel,
                                       const VgControllerConfig *config,
                                       const VgControllerState *state) {
    float line = config->height - 2.0f * config->radius;
    unsigned int segments = (unsigned int)ceilf(line / (config->radius * 0.25f));
    if (segments == 0u)
        segments = 1u;
    if (segments > 64u)
        segments = 64u;
    for (unsigned int i = 0u; i <= segments; ++i) {
        float height = config->radius + line * (float)i / (float)segments;
        VgVec3 center = {state->feet.x, state->feet.y, state->feet.z + height};
        if (door_sphere_overlaps_panel(door, panel, center, config->radius))
            return true;
    }
    return false;
}

static VgResult door_panel_world(const VgDoor *door, VgTransform *out_transform) {
    VgResult result = vg_entity_get_world_transform(door->context, door->panel, out_transform);
    if (result != VG_OK)
        return result;
    return door_finite_vec(out_transform->position) &&
                   door_finite_vec(out_transform->scale) &&
                   out_transform->scale.x > 0.0f && out_transform->scale.y > 0.0f &&
                   out_transform->scale.z > 0.0f
               ? VG_OK : VG_ERROR_UNSUPPORTED;
}

static VgResult door_arc_radius(const VgDoor *door, float *out_radius) {
    VgTransform hinge, panel;
    VgResult result = vg_entity_get_world_transform(door->context, door->hinge, &hinge);
    if (result != VG_OK)
        return result;
    result = door_panel_world(door, &panel);
    if (result != VG_OK)
        return result;
    const VgSpatialBox *box = &door->collider_description.shape.box;
    VgVec3 local_center = {box->center.x * panel.scale.x,
                           box->center.y * panel.scale.y,
                           box->center.z * panel.scale.z};
    VgVec3 offset = door_rotate(panel.rotation, local_center);
    VgVec3 center = {panel.position.x + offset.x, panel.position.y + offset.y,
                     panel.position.z + offset.z};
    VgVec3 half = {box->half_extents.x * panel.scale.x,
                   box->half_extents.y * panel.scale.y,
                   box->half_extents.z * panel.scale.z};
    float radius = door_length(door_subtract(center, hinge.position)) + door_length(half);
    if (!isfinite(radius) || radius <= 0.0f || radius > 20.0f)
        return VG_ERROR_UNSUPPORTED;
    *out_radius = radius;
    return VG_OK;
}

VgResult vg_door_create(const VgDoorDesc *description, VgDoor **out_door) {
    if (description == NULL || out_door == NULL ||
        description->struct_size < sizeof(*description) ||
        description->api_version != VG_API_VERSION || description->context == NULL ||
        description->spatial == NULL || description->hinge.value == 0u ||
        description->panel.value == 0u || description->collider.value == 0u ||
        description->collider_description.entity.value != description->panel.value ||
        description->collider_description.shape_type != VG_SPATIAL_SHAPE_BOX ||
        !description->collider_description.enabled ||
        !door_finite_vec(description->collider_description.shape.box.center) ||
        !door_finite_vec(description->collider_description.shape.box.half_extents) ||
        description->collider_description.shape.box.half_extents.x <= 0.0f ||
        description->collider_description.shape.box.half_extents.y <= 0.0f ||
        description->collider_description.shape.box.half_extents.z <= 0.0f ||
        !isfinite(description->open_angle_radians) ||
        fabsf(description->open_angle_radians) < VG_DOOR_EPSILON ||
        fabsf(description->open_angle_radians) > 3.141593f ||
        !isfinite(description->angular_speed_radians) ||
        description->angular_speed_radians <= 0.0f ||
        description->angular_speed_radians > 10.0f)
        return VG_ERROR_INVALID_ARGUMENT;
    VgEntity parent;
    VgResult result = vg_entity_get_parent(description->context, description->panel, &parent);
    if (result != VG_OK)
        return result;
    if (parent.value != description->hinge.value)
        return VG_ERROR_INVALID_ARGUMENT;
    VgTransform closed;
    result = vg_entity_get_local_transform(description->context, description->hinge, &closed);
    if (result != VG_OK)
        return result;
    VgDoor *door = calloc(1u, sizeof(*door));
    if (door == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    door->context = description->context;
    door->spatial = description->spatial;
    door->hinge = description->hinge;
    door->panel = description->panel;
    door->collider = description->collider;
    door->collider_description = description->collider_description;
    door->closed_hinge = closed;
    door->open_angle = description->open_angle_radians;
    door->speed = description->angular_speed_radians;
    door->state = VG_DOOR_CLOSED;
    result = vg_spatial_world_refresh_entity(door->context, door->spatial,
                                              door->collider, &door->collider_description);
    if (result != VG_OK) {
        free(door);
        return result;
    }
    *out_door = door;
    return VG_OK;
}

void vg_door_destroy(VgDoor *door) {
    free(door);
}

VgResult vg_door_open(VgDoor *door) {
    if (door == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    door->target = door->open_angle;
    door->state = fabsf(door->angle - door->target) < VG_DOOR_EPSILON
                      ? VG_DOOR_OPEN : VG_DOOR_OPENING;
    return VG_OK;
}

VgResult vg_door_close(VgDoor *door) {
    if (door == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    door->target = 0.0f;
    door->state = fabsf(door->angle) < VG_DOOR_EPSILON
                      ? VG_DOOR_CLOSED : VG_DOOR_CLOSING;
    return VG_OK;
}

VgResult vg_door_toggle(VgDoor *door) {
    if (door == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    return fabsf(door->target) < VG_DOOR_EPSILON ? vg_door_open(door)
                                                : vg_door_close(door);
}

VgResult vg_door_step(VgDoor *door, float fixed_dt,
                       const VgControllerConfig *player_config,
                       const VgControllerState *player_state) {
    if (door == NULL || !isfinite(fixed_dt) || fixed_dt <= 0.0f || fixed_dt > 1.0f / 30.0f ||
        (player_config == NULL) != (player_state == NULL) ||
        (player_config != NULL && !door_player_valid(player_config, player_state)))
        return VG_ERROR_INVALID_ARGUMENT;
    float remaining = door->target - door->angle;
    if (fabsf(remaining) < VG_DOOR_EPSILON) {
        door->state = fabsf(door->target) < VG_DOOR_EPSILON ? VG_DOOR_CLOSED : VG_DOOR_OPEN;
        return vg_spatial_world_refresh_entity(door->context, door->spatial,
                                                 door->collider,
                                                 &door->collider_description);
    }
    float move = fminf(fabsf(remaining), door->speed * fixed_dt);
    float radius = 0.0f;
    VgResult result = door_arc_radius(door, &radius);
    if (result != VG_OK)
        return result;
    unsigned int steps = (unsigned int)ceilf(move * radius / VG_DOOR_MAX_ARC_METRES);
    if (steps == 0u)
        steps = 1u;
    if (steps > VG_DOOR_MAX_SUBSTEPS)
        return VG_ERROR_UNSUPPORTED;
    float increment = copysignf(move / (float)steps, remaining);
    for (unsigned int index = 0u; index < steps; ++index) {
        float candidate_angle = door->angle + increment;
        if (index == steps - 1u && move >= fabsf(remaining) - VG_DOOR_EPSILON)
            candidate_angle = door->target;
        VgTransform candidate_pose = door_hinge_pose(door, candidate_angle);
        result = vg_entity_set_local_transform(door->context, door->hinge, &candidate_pose);
        if (result != VG_OK)
            return result;
        VgTransform panel;
        result = door_panel_world(door, &panel);
        if (result == VG_OK && player_config != NULL &&
            door_player_overlaps_panel(door, panel, player_config, player_state)) {
            VgTransform last_safe = door_hinge_pose(door, door->angle);
            (void)vg_entity_set_local_transform(door->context, door->hinge, &last_safe);
            door->state = VG_DOOR_BLOCKED;
            return VG_OK;
        }
        if (result == VG_OK)
            result = vg_spatial_world_refresh_entity(door->context, door->spatial,
                                                      door->collider,
                                                      &door->collider_description);
        if (result != VG_OK) {
            VgTransform last_safe = door_hinge_pose(door, door->angle);
            (void)vg_entity_set_local_transform(door->context, door->hinge, &last_safe);
            return result;
        }
        door->angle = candidate_angle;
    }
    if (fabsf(door->angle - door->target) < VG_DOOR_EPSILON)
        door->state = fabsf(door->target) < VG_DOOR_EPSILON ? VG_DOOR_CLOSED : VG_DOOR_OPEN;
    else
        door->state = fabsf(door->target) < VG_DOOR_EPSILON
                          ? VG_DOOR_CLOSING : VG_DOOR_OPENING;
    return VG_OK;
}

VgDoorState vg_door_state(const VgDoor *door) {
    return door == NULL ? VG_DOOR_BLOCKED : door->state;
}

float vg_door_angle(const VgDoor *door) {
    return door == NULL ? 0.0f : door->angle;
}
