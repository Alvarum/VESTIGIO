#include "world/spatial_world.h"

VgResult vg_spatial_world_refresh_entity(VgContext *context, VgSpatialScene *scene,
                                         VgSpatialCollider collider,
                                         const VgSpatialColliderDesc *description) {
    if (context == NULL || scene == NULL || description == NULL || description->entity.value == 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    VgTransform world_transform;
    VgResult result = vg_entity_get_world_transform(context, description->entity, &world_transform);
    if (result != VG_OK)
        return result;
    VgSpatialColliderDesc candidate = *description;
    candidate.transform = world_transform;
    if (collider.value == 0u)
        return VG_ERROR_INVALID_HANDLE;
    return vg_spatial_collider_update(scene, collider, &candidate);
}

