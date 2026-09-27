#include "gamekit/atrium_animation.h"

#include "vestigio/animation.h"

#include <stdlib.h>

enum { ATRIUM_ACTOR_COUNT = 2 };

struct VgAtriumAnimation {
    VgContext *context;
    VgEntity entities[ATRIUM_ACTOR_COUNT];
    VgRigidInstance actors[ATRIUM_ACTOR_COUNT];
    size_t count;
};

/* One mesh/clip asset, two independent playback clocks. A complete revolution
 * is split at 180 degrees so normalized quaternion lerp keeps its direction. */
static const VgRigidKeyframe kKeys[] = {
    {0.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
    {1.0f, {0.0f, 0.0f, 0.55f}, {0.0f, 0.0f, 0.70710678f, 0.70710678f}, {1.0f, 1.0f, 1.0f}},
    {2.0f, {0.0f, 0.0f, 0.80f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    {3.0f, {0.0f, 0.0f, 0.55f}, {0.0f, 0.0f, 0.70710678f, -0.70710678f}, {1.0f, 1.0f, 1.0f}},
    {4.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}};
static const VgRigidClip kClip = {kKeys, sizeof(kKeys) / sizeof(kKeys[0]), 4.0f, true};

VgResult vg_atrium_animation_create(VgContext *context, VgDocumentInstance *document,
                                    VgAtriumAnimation **out_animation) {
    if (context == NULL || document == NULL || out_animation == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    *out_animation = NULL;
    VgAssetId id;
    VgAsset asset;
    if (!vg_document_instance_asset_at(document, 0u, &id, &asset))
        return VG_ERROR_NOT_FOUND;
    VgAtriumAnimation *animation = calloc(1u, sizeof(*animation));
    if (animation == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    animation->context = context;
    VgWorld world = vg_document_instance_world(document);
    VgResult result = VG_OK;
    for (size_t i = 0u; i < ATRIUM_ACTOR_COUNT; ++i) {
        result = vg_entity_create(context, world, &animation->entities[i]);
        if (result != VG_OK)
            break;
        ++animation->count;
        VgTransform bind = {{i == 0u ? -1.3f : 1.3f, -4.6f, 1.25f},
                            {0.0f, 0.0f, 0.0f, 1.0f},
                            {0.55f, 0.55f, 0.30f}};
        result = vg_entity_set_local_transform(context, animation->entities[i], &bind);
        if (result != VG_OK)
            break;
        VgMeshRendererDesc mesh = {0};
        mesh.struct_size = sizeof(mesh);
        mesh.api_version = VG_API_VERSION;
        mesh.asset = asset;
        mesh.node_index = 1u;
        mesh.mesh_index = VG_RENDER_DEFAULT_INDEX;
        mesh.material_override = VG_RENDER_DEFAULT_INDEX;
        mesh.bounds_extent = (VgVec3){0.5f, 0.5f, 0.5f};
        result = vg_mesh_renderer_set(context, animation->entities[i], &mesh);
        if (result != VG_OK)
            break;
        result = vg_rigid_instance_bind(context, animation->entities[i], &kClip,
                                        i == 0u ? 0.0 : 1.0, &animation->actors[i]);
        if (result != VG_OK)
            break;
    }
    if (result != VG_OK) {
        vg_atrium_animation_destroy(animation);
        return result;
    }
    *out_animation = animation;
    return VG_OK;
}

void vg_atrium_animation_destroy(VgAtriumAnimation *animation) {
    if (animation == NULL)
        return;
    for (size_t i = animation->count; i > 0u; --i)
        (void)vg_entity_destroy(animation->context, animation->entities[i - 1u]);
    free(animation);
}

VgResult vg_atrium_animation_step(VgAtriumAnimation *animation, double elapsed_seconds) {
    if (animation == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    for (size_t i = 0u; i < animation->count; ++i) {
        VgResult result =
            vg_rigid_instance_step(animation->context, &animation->actors[i], elapsed_seconds);
        if (result != VG_OK)
            return result;
    }
    return VG_OK;
}

VgResult vg_atrium_animation_pose(const VgAtriumAnimation *animation, size_t index,
                                  VgTransform *out_pose) {
    if (animation == NULL || index >= animation->count || out_pose == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    return vg_entity_get_world_transform(animation->context, animation->entities[index], out_pose);
}

size_t vg_atrium_animation_count(const VgAtriumAnimation *animation) {
    return animation == NULL ? 0u : animation->count;
}
