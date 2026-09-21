#include "runtime/runtime_internal.h"

#include <math.h>
#include <string.h>

static bool vg_camera_desc_valid(const VgCameraDesc *description) {
    if (description == NULL || description->struct_size < sizeof(*description) ||
        description->api_version != VG_API_VERSION || description->flags != 0u ||
        !isfinite(description->near_clip_metres) || !isfinite(description->far_clip_metres) ||
        description->near_clip_metres <= 0.0f ||
        description->far_clip_metres <= description->near_clip_metres)
        return false;
    if (description->projection == VG_CAMERA_PERSPECTIVE)
        return isfinite(description->vertical_fov_radians) &&
               description->vertical_fov_radians > 0.0f &&
               description->vertical_fov_radians < 3.14159265358979323846f;
    if (description->projection == VG_CAMERA_ORTHOGRAPHIC)
        return isfinite(description->orthographic_height) &&
               description->orthographic_height > 0.0f;
    return false;
}

VgResult vg_camera_set(VgContext *context, VgEntity entity, const VgCameraDesc *description) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (!vg_camera_desc_valid(description))
        return VG_ERROR_INVALID_ARGUMENT;
    VgCameraDesc camera = {sizeof(camera),
                           VG_API_VERSION,
                           description->projection,
                           description->flags,
                           description->vertical_fov_radians,
                           description->orthographic_height,
                           description->near_clip_metres,
                           description->far_clip_metres};
    world->entities[entity_index].camera = camera;
    world->entities[entity_index].has_camera = true;
    return VG_OK;
}

VgResult vg_camera_get(VgContext *context, VgEntity entity, VgCameraDesc *out_description) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (out_description == NULL ||
        out_description->struct_size <
            offsetof(VgCameraDesc, api_version) + sizeof(out_description->api_version) ||
        out_description->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!world->entities[entity_index].has_camera)
        return VG_ERROR_NOT_FOUND;
    const VgCameraDesc *stored = &world->entities[entity_index].camera;
    uint32_t capacity = out_description->struct_size;
#define VG_WRITE_CAMERA_FIELD(field)                                                               \
    do {                                                                                           \
        if ((uint64_t)capacity >=                                                                  \
            (uint64_t)offsetof(VgCameraDesc, field) + sizeof(out_description->field))              \
            out_description->field = stored->field;                                                \
    } while (0)
    VG_WRITE_CAMERA_FIELD(projection);
    VG_WRITE_CAMERA_FIELD(flags);
    VG_WRITE_CAMERA_FIELD(vertical_fov_radians);
    VG_WRITE_CAMERA_FIELD(orthographic_height);
    VG_WRITE_CAMERA_FIELD(near_clip_metres);
    VG_WRITE_CAMERA_FIELD(far_clip_metres);
#undef VG_WRITE_CAMERA_FIELD
    return VG_OK;
}

VgResult vg_camera_clear(VgContext *context, VgEntity entity) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (!world->entities[entity_index].has_camera)
        return VG_ERROR_NOT_FOUND;
    memset(&world->entities[entity_index].camera, 0, sizeof(VgCameraDesc));
    world->entities[entity_index].has_camera = false;
    return VG_OK;
}
