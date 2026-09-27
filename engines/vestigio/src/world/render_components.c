#include "runtime/runtime_internal.h"

#include <math.h>
#include <string.h>

static bool vec3_finite(VgVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool mesh_desc_valid(const VgMeshRendererDesc *description) {
    const uint32_t flag_mask =
        VG_RENDER_WIREFRAME | VG_RENDER_DISABLE_CULLING | VG_RENDER_FORCE_ERROR_MATERIAL;
    return description != NULL && description->struct_size >= sizeof(*description) &&
           description->api_version == VG_API_VERSION &&
           description->asset.value != VG_INVALID_HANDLE_VALUE &&
           (description->flags & ~flag_mask) == 0u && vec3_finite(description->bounds_center) &&
           vec3_finite(description->bounds_extent) && description->bounds_extent.x >= 0.0f &&
           description->bounds_extent.y >= 0.0f && description->bounds_extent.z >= 0.0f;
}

static bool sprite_desc_valid(const VgSpriteRendererDesc *description) {
    const uint32_t flag_mask = VG_RENDER_DISABLE_CULLING | VG_RENDER_FORCE_ERROR_MATERIAL;
    if (description == NULL || description->struct_size < sizeof(*description) ||
        description->api_version != VG_API_VERSION ||
        description->asset.value == VG_INVALID_HANDLE_VALUE || description->reserved != 0u ||
        (description->flags & ~flag_mask) != 0u || description->alpha_mode > VG_ALPHA_BLEND ||
        !isfinite(description->width_metres) || !isfinite(description->height_metres) ||
        description->width_metres <= 0.0f || description->height_metres <= 0.0f ||
        !isfinite(description->alpha_cutoff) || description->alpha_cutoff < 0.0f ||
        description->alpha_cutoff > 1.0f)
        return false;
    for (uint32_t index = 0u; index < 4u; ++index)
        if (!isfinite(description->tint[index]) || description->tint[index] < 0.0f ||
            description->tint[index] > 1.0f)
            return false;
    return true;
}

static VgResult require_mesh_asset(VgContext *context, VgAsset asset) {
    VgAssetInfo info = {0};
    info.struct_size = sizeof(info);
    info.api_version = VG_API_VERSION;
    VgResult result = vg_asset_get_info(context, asset, &info);
    if (result != VG_OK)
        return result;
    return info.type == VG_ASSET_TYPE_MESH ? VG_OK : VG_ERROR_WRONG_TYPE;
}

VgResult vg_mesh_renderer_set(VgContext *context, VgEntity entity,
                              const VgMeshRendererDesc *description) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (!mesh_desc_valid(description))
        return VG_ERROR_INVALID_ARGUMENT;
    result = require_mesh_asset(context, description->asset);
    if (result != VG_OK)
        return result;
    VgAssetRef retained = {0};
    result = vg_asset_component_retain(context, description->asset, &retained);
    if (result != VG_OK)
        return result;
    VgEntitySlot *slot = &world->entities[entity_index];
    if (slot->has_mesh_renderer) {
        result = vg_asset_component_release(context, slot->mesh_asset);
        if (result != VG_OK) {
            (void)vg_asset_component_release(context, retained);
            return result;
        }
    }
    slot->mesh_renderer = *description;
    slot->mesh_renderer.struct_size = sizeof(slot->mesh_renderer);
    slot->mesh_renderer.api_version = VG_API_VERSION;
    slot->mesh_renderer.asset.value = VG_INVALID_HANDLE_VALUE;
    slot->mesh_asset = retained;
    slot->has_mesh_renderer = true;
    return VG_OK;
}

VgResult vg_mesh_renderer_get(VgContext *context, VgEntity entity,
                              VgMeshRendererDesc *out_description) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (out_description == NULL || out_description->struct_size < sizeof(*out_description) ||
        out_description->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    VgEntitySlot *slot = &world->entities[entity_index];
    if (!slot->has_mesh_renderer)
        return VG_ERROR_NOT_FOUND;
    VgAsset asset = {0};
    result = vg_asset_component_acquire(context, slot->mesh_asset,
                                        VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU, &asset);
    if (result != VG_OK)
        return result;
    VgMeshRendererDesc description = slot->mesh_renderer;
    description.asset = asset;
    *out_description = description;
    return VG_OK;
}

VgResult vg_mesh_renderer_clear(VgContext *context, VgEntity entity) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    VgEntitySlot *slot = &world->entities[entity_index];
    if (!slot->has_mesh_renderer)
        return VG_ERROR_NOT_FOUND;
    result = vg_asset_component_release(context, slot->mesh_asset);
    if (result != VG_OK)
        return result;
    memset(&slot->mesh_renderer, 0, sizeof(slot->mesh_renderer));
    slot->mesh_asset = (VgAssetRef){0};
    slot->has_mesh_renderer = false;
    return VG_OK;
}

VgResult vg_sprite_renderer_set(VgContext *context, VgEntity entity,
                                const VgSpriteRendererDesc *description) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (!sprite_desc_valid(description))
        return VG_ERROR_INVALID_ARGUMENT;
    result = require_mesh_asset(context, description->asset);
    if (result != VG_OK)
        return result;
    VgAssetRef retained = {0};
    result = vg_asset_component_retain(context, description->asset, &retained);
    if (result != VG_OK)
        return result;
    VgEntitySlot *slot = &world->entities[entity_index];
    if (slot->has_sprite_renderer) {
        result = vg_asset_component_release(context, slot->sprite_asset);
        if (result != VG_OK) {
            (void)vg_asset_component_release(context, retained);
            return result;
        }
    }
    slot->sprite_renderer = *description;
    slot->sprite_renderer.struct_size = sizeof(slot->sprite_renderer);
    slot->sprite_renderer.api_version = VG_API_VERSION;
    slot->sprite_renderer.asset.value = VG_INVALID_HANDLE_VALUE;
    slot->sprite_asset = retained;
    slot->has_sprite_renderer = true;
    return VG_OK;
}

VgResult vg_sprite_renderer_get(VgContext *context, VgEntity entity,
                                VgSpriteRendererDesc *out_description) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (out_description == NULL || out_description->struct_size < sizeof(*out_description) ||
        out_description->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    VgEntitySlot *slot = &world->entities[entity_index];
    if (!slot->has_sprite_renderer)
        return VG_ERROR_NOT_FOUND;
    VgAsset asset = {0};
    result = vg_asset_component_acquire(context, slot->sprite_asset,
                                        VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU, &asset);
    if (result != VG_OK)
        return result;
    VgSpriteRendererDesc description = slot->sprite_renderer;
    description.asset = asset;
    *out_description = description;
    return VG_OK;
}

VgResult vg_sprite_renderer_clear(VgContext *context, VgEntity entity) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    VgEntitySlot *slot = &world->entities[entity_index];
    if (!slot->has_sprite_renderer)
        return VG_ERROR_NOT_FOUND;
    result = vg_asset_component_release(context, slot->sprite_asset);
    if (result != VG_OK)
        return result;
    memset(&slot->sprite_renderer, 0, sizeof(slot->sprite_renderer));
    slot->sprite_asset = (VgAssetRef){0};
    slot->has_sprite_renderer = false;
    return VG_OK;
}
