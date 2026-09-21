#include "runtime/runtime_internal.h"
#include "world/transform_internal.h"

#include <string.h>

#define VG_NO_PARENT UINT16_MAX

static const VgTransform vg_identity_transform = {
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};

static VgResult vg_world_reserve(VgContext *context, VgWorldState *world, uint32_t capacity) {
    if (capacity <= world->entity_capacity)
        return VG_OK;
    if (capacity > world->max_entities || capacity > VG_HANDLE_ENTITY_MAX)
        return VG_ERROR_CAPACITY;
    VgEntitySlot *entities = vg_runtime_allocate(context, sizeof(*entities) * capacity);
    if (entities == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memset(entities, 0, sizeof(*entities) * capacity);
    if (world->entities != NULL) {
        memcpy(entities, world->entities, sizeof(*entities) * world->entity_capacity);
        vg_runtime_deallocate(context, world->entities);
    }
    for (uint32_t index = world->entity_capacity; index < capacity; ++index) {
        entities[index].generation = 1u;
        entities[index].parent_index = VG_NO_PARENT;
        entities[index].parent_generation = 0u;
    }
    world->entities = entities;
    world->entity_capacity = capacity;
    return VG_OK;
}

static VgResult vg_world_grow(VgContext *context, VgWorldState *world) {
    if (world->entity_capacity >= world->max_entities)
        return VG_ERROR_CAPACITY;
    uint32_t capacity = world->entity_capacity == 0u ? 1u : world->entity_capacity * 2u;
    if (capacity < world->entity_capacity || capacity > world->max_entities)
        capacity = world->max_entities;
    return vg_world_reserve(context, world, capacity);
}

static VgResult vg_resolve_parent(VgContext *context, VgEntity child, VgEntity parent,
                                  uint32_t child_world_index, VgWorldState *child_world,
                                  uint16_t *out_parent_index, uint16_t *out_parent_generation) {
    if (parent.value == VG_INVALID_HANDLE_VALUE) {
        *out_parent_index = VG_NO_PARENT;
        *out_parent_generation = 0u;
        return VG_OK;
    }
    uint32_t parent_world_index = 0u;
    VgWorldState *parent_world = NULL;
    uint32_t parent_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, parent, &parent_world_index, &parent_world,
                                                &parent_index);
    if (result != VG_OK)
        return result;
    if (parent_world_index != child_world_index || parent_world != child_world)
        return VG_ERROR_WRONG_WORLD;
    uint32_t child_index = 0u;
    result = vg_runtime_resolve_entity(context, child, NULL, NULL, &child_index);
    if (result != VG_OK)
        return result;
    if (parent_index == child_index)
        return VG_ERROR_CONFLICT;
    uint32_t ancestor = parent_index;
    uint16_t ancestor_generation = child_world->entities[parent_index].generation;
    uint32_t depth = 0u;
    while (ancestor != VG_NO_PARENT) {
        if (ancestor == child_index)
            return VG_ERROR_CONFLICT;
        if (ancestor >= child_world->entity_capacity || depth++ >= child_world->entity_capacity)
            return VG_ERROR_CONFLICT;
        VgEntitySlot *ancestor_slot = &child_world->entities[ancestor];
        if (ancestor_slot->generation != ancestor_generation)
            return VG_ERROR_INVALID_HANDLE;
        ancestor = ancestor_slot->parent_index;
        ancestor_generation = ancestor_slot->parent_generation;
    }
    *out_parent_index = (uint16_t)parent_index;
    *out_parent_generation = child_world->entities[parent_index].generation;
    return VG_OK;
}

static bool vg_entity_is_present(uint8_t state) {
    return state == VG_ENTITY_ACTIVE || state == VG_ENTITY_PENDING_CREATE ||
           state == VG_ENTITY_PENDING_DESTROY || state == VG_ENTITY_PENDING_CANCEL;
}

static VgResult vg_entity_prepare_destroy(const VgWorldState *world, uint32_t entity_index,
                                          bool *detach, VgTransform *detached_transforms) {
    const VgEntitySlot *slot = &world->entities[entity_index];
    memset(detach, 0, sizeof(*detach) * world->entity_capacity);
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        const VgEntitySlot *child = &world->entities[index];
        if (!vg_entity_is_present(child->state) || child->parent_index != entity_index ||
            child->parent_generation != slot->generation)
            continue;
        VgMatrix matrix;
        VgResult result =
            vg_world_entity_matrix(world, index, UINT32_MAX, NULL, VG_NO_PARENT, 0u, &matrix);
        if (result != VG_OK)
            return result;
        if (!vg_matrix_to_transform(matrix, &detached_transforms[index]))
            return VG_ERROR_UNSUPPORTED;
        detach[index] = true;
    }
    return VG_OK;
}

static VgResult vg_entity_finalize_destroy(VgWorldState *world, uint32_t entity_index) {
    bool detach[VG_HANDLE_ENTITY_MAX] = {false};
    VgTransform detached_transforms[VG_HANDLE_ENTITY_MAX];
    VgResult result = vg_entity_prepare_destroy(world, entity_index, detach, detached_transforms);
    if (result != VG_OK)
        return result;
    VgEntitySlot *slot = &world->entities[entity_index];
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        VgEntitySlot *child = &world->entities[index];
        if (detach[index]) {
            child->local = detached_transforms[index];
            child->parent_index = VG_NO_PARENT;
            child->parent_generation = 0u;
        }
    }
    if (slot->state == VG_ENTITY_ACTIVE || slot->state == VG_ENTITY_PENDING_DESTROY)
        --world->active_count;
    if (slot->generation >= VG_HANDLE_GENERATION_MAX) {
        slot->state = VG_ENTITY_RETIRED;
    } else {
        ++slot->generation;
        slot->state = VG_ENTITY_FREE;
    }
    slot->parent_index = VG_NO_PARENT;
    slot->parent_generation = 0u;
    slot->local = vg_identity_transform;
    return VG_OK;
}

void vg_world_release_state(VgContext *context, VgWorldState *world) {
    if (world == NULL)
        return;
    vg_runtime_deallocate(context, world->entities);
    vg_runtime_deallocate(context, world);
}

VgResult vg_world_create(VgContext *context, const VgWorldDesc *description, VgWorld *out_world) {
    if (context == NULL || context->magic != VG_CONTEXT_MAGIC || out_world == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    uint32_t initial_capacity = VG_DEFAULT_INITIAL_ENTITIES;
    uint32_t max_entities = VG_HANDLE_ENTITY_MAX;
    if (description != NULL) {
        if (description->struct_size < sizeof(VgWorldDesc) ||
            description->api_version != VG_API_VERSION)
            return VG_ERROR_INVALID_ARGUMENT;
        if (description->initial_entity_capacity != 0u)
            initial_capacity = description->initial_entity_capacity;
        if (description->max_entities != 0u)
            max_entities = description->max_entities;
    }
    if (max_entities == 0u || max_entities > VG_HANDLE_ENTITY_MAX ||
        initial_capacity > max_entities)
        return VG_ERROR_CAPACITY;

    uint32_t world_index = context->max_worlds;
    for (uint32_t index = 0u; index < context->max_worlds; ++index) {
        if (context->worlds[index].state == VG_WORLD_FREE) {
            world_index = index;
            break;
        }
    }
    if (world_index == context->max_worlds)
        return VG_ERROR_CAPACITY;

    VgWorldState *world = vg_runtime_allocate(context, sizeof(*world));
    if (world == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memset(world, 0, sizeof(*world));
    world->max_entities = max_entities;
    VgResult result = vg_world_reserve(context, world, initial_capacity);
    if (result != VG_OK) {
        vg_world_release_state(context, world);
        return result;
    }
    VgWorldSlot *slot = &context->worlds[world_index];
    slot->world = world;
    slot->state = VG_WORLD_ACTIVE;
    VgWorld handle = {vg_runtime_make_world_handle(context, world_index, slot->generation)};
    *out_world = handle;
    return VG_OK;
}

VgResult vg_world_destroy(VgContext *context, VgWorld handle) {
    uint32_t world_index = 0u;
    VgWorldState *world = NULL;
    VgResult result = vg_runtime_resolve_world(context, handle, &world_index, &world);
    if (result != VG_OK)
        return result;
    if (world->iterating)
        return VG_ERROR_REENTRANT;
    vg_world_release_state(context, world);
    VgWorldSlot *slot = &context->worlds[world_index];
    slot->world = NULL;
    if (slot->generation >= VG_HANDLE_GENERATION_MAX) {
        slot->state = VG_WORLD_RETIRED;
    } else {
        ++slot->generation;
        slot->state = VG_WORLD_FREE;
    }
    return VG_OK;
}

VgResult vg_world_reserve_entities(VgContext *context, VgWorld handle, uint32_t capacity) {
    VgWorldState *world = NULL;
    VgResult result = vg_runtime_resolve_world(context, handle, NULL, &world);
    if (result != VG_OK)
        return result;
    return vg_world_reserve(context, world, capacity);
}

VgResult vg_world_begin_iteration(VgContext *context, VgWorld handle, uint32_t *out_entity_count) {
    VgWorldState *world = NULL;
    VgResult result = vg_runtime_resolve_world(context, handle, NULL, &world);
    if (result != VG_OK)
        return result;
    if (out_entity_count == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (world->iterating)
        return VG_ERROR_REENTRANT;
    world->iterating = true;
    world->iteration_count = world->active_count;
    *out_entity_count = world->iteration_count;
    return VG_OK;
}

VgResult vg_world_entity_at(VgContext *context, VgWorld handle, uint32_t ordinal,
                            VgEntity *out_entity) {
    uint32_t world_index = 0u;
    VgWorldState *world = NULL;
    VgResult result = vg_runtime_resolve_world(context, handle, &world_index, &world);
    if (result != VG_OK)
        return result;
    if (out_entity == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!world->iterating)
        return VG_ERROR_REENTRANT;
    uint32_t count = 0u;
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        uint8_t state = world->entities[index].state;
        bool visible = state == VG_ENTITY_ACTIVE || state == VG_ENTITY_PENDING_DESTROY;
        if (visible && count++ == ordinal) {
            VgEntity entity = {vg_runtime_make_entity_handle(
                context, world_index, context->worlds[world_index].generation, index,
                world->entities[index].generation)};
            *out_entity = entity;
            return VG_OK;
        }
    }
    return VG_ERROR_NOT_FOUND;
}

VgResult vg_world_end_iteration(VgContext *context, VgWorld handle) {
    VgWorldState *world = NULL;
    VgResult result = vg_runtime_resolve_world(context, handle, NULL, &world);
    if (result != VG_OK)
        return result;
    if (!world->iterating)
        return VG_ERROR_REENTRANT;
    bool detach[VG_HANDLE_ENTITY_MAX];
    VgTransform detached_transforms[VG_HANDLE_ENTITY_MAX];
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        if (world->entities[index].state == VG_ENTITY_PENDING_DESTROY ||
            world->entities[index].state == VG_ENTITY_PENDING_CANCEL) {
            result = vg_entity_prepare_destroy(world, index, detach, detached_transforms);
            if (result != VG_OK)
                return result;
        }
    }
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        if (world->entities[index].state == VG_ENTITY_PENDING_DESTROY ||
            world->entities[index].state == VG_ENTITY_PENDING_CANCEL) {
            result = vg_entity_finalize_destroy(world, index);
            if (result != VG_OK)
                return result;
        }
    }
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        if (world->entities[index].state == VG_ENTITY_PENDING_CREATE) {
            world->entities[index].state = VG_ENTITY_ACTIVE;
            ++world->active_count;
        }
    }
    world->iteration_count = 0u;
    world->iterating = false;
    return VG_OK;
}

VgResult vg_entity_create(VgContext *context, VgWorld handle, VgEntity *out_entity) {
    uint32_t world_index = 0u;
    VgWorldState *world = NULL;
    VgResult result = vg_runtime_resolve_world(context, handle, &world_index, &world);
    if (result != VG_OK)
        return result;
    if (out_entity == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    uint32_t entity_index = world->entity_capacity;
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        if (world->entities[index].state == VG_ENTITY_FREE) {
            entity_index = index;
            break;
        }
    }
    if (entity_index == world->entity_capacity) {
        result = vg_world_grow(context, world);
        if (result != VG_OK)
            return result;
        for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
            if (world->entities[index].state == VG_ENTITY_FREE) {
                entity_index = index;
                break;
            }
        }
    }
    if (entity_index >= world->entity_capacity)
        return VG_ERROR_CAPACITY;
    VgEntitySlot *slot = &world->entities[entity_index];
    slot->local = vg_identity_transform;
    slot->parent_index = VG_NO_PARENT;
    slot->parent_generation = 0u;
    slot->state = world->iterating ? VG_ENTITY_PENDING_CREATE : VG_ENTITY_ACTIVE;
    if (!world->iterating)
        ++world->active_count;
    VgEntity entity = {vg_runtime_make_entity_handle(context, world_index,
                                                     context->worlds[world_index].generation,
                                                     entity_index, slot->generation)};
    *out_entity = entity;
    return VG_OK;
}

VgResult vg_entity_destroy(VgContext *context, VgEntity entity) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    VgEntitySlot *slot = &world->entities[entity_index];
    if (slot->state == VG_ENTITY_PENDING_DESTROY)
        return VG_ERROR_CONFLICT;
    if (slot->state == VG_ENTITY_PENDING_CREATE) {
        slot->state = VG_ENTITY_PENDING_CANCEL;
        return VG_OK;
    }
    if (world->iterating) {
        slot->state = VG_ENTITY_PENDING_DESTROY;
        return VG_OK;
    }
    return vg_entity_finalize_destroy(world, entity_index);
}

VgResult vg_entity_get_local_transform(VgContext *context, VgEntity entity,
                                       VgTransform *out_transform) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (out_transform == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    *out_transform = world->entities[entity_index].local;
    return VG_OK;
}

VgResult vg_entity_get_world_transform(VgContext *context, VgEntity entity,
                                       VgTransform *out_transform) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (out_transform == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    VgMatrix matrix;
    result =
        vg_world_entity_matrix(world, entity_index, UINT32_MAX, NULL, VG_NO_PARENT, 0u, &matrix);
    if (result != VG_OK)
        return result;
    VgTransform transform;
    if (!vg_matrix_to_transform(matrix, &transform))
        return VG_ERROR_UNSUPPORTED;
    *out_transform = transform;
    return VG_OK;
}

VgResult vg_entity_set_local_transform(VgContext *context, VgEntity entity,
                                       const VgTransform *transform) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    VgTransform sanitized;
    if (!vg_transform_sanitize(transform, &sanitized))
        return VG_ERROR_INVALID_ARGUMENT;
    result = vg_world_validate_transform_change(world, entity_index, sanitized,
                                                world->entities[entity_index].parent_index,
                                                world->entities[entity_index].parent_generation);
    if (result != VG_OK)
        return result;
    world->entities[entity_index].local = sanitized;
    return VG_OK;
}

VgResult vg_entity_set_world_transform(VgContext *context, VgEntity entity,
                                       const VgTransform *transform) {
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result = vg_runtime_resolve_entity(context, entity, NULL, &world, &entity_index);
    if (result != VG_OK)
        return result;
    VgTransform sanitized;
    if (!vg_transform_sanitize(transform, &sanitized))
        return VG_ERROR_INVALID_ARGUMENT;
    VgTransform local = sanitized;
    uint16_t parent_index = world->entities[entity_index].parent_index;
    if (parent_index != VG_NO_PARENT) {
        VgMatrix parent_matrix;
        VgMatrix inverse;
        result = vg_world_entity_matrix(world, parent_index, UINT32_MAX, NULL, VG_NO_PARENT, 0u,
                                        &parent_matrix);
        if (result != VG_OK)
            return result;
        if (!vg_matrix_inverse_affine(parent_matrix, &inverse) ||
            !vg_matrix_to_transform(vg_matrix_multiply(inverse, vg_transform_matrix(sanitized)),
                                    &local))
            return VG_ERROR_UNSUPPORTED;
    }
    result = vg_world_validate_transform_change(world, entity_index, local, parent_index,
                                                world->entities[entity_index].parent_generation);
    if (result != VG_OK)
        return result;
    world->entities[entity_index].local = local;
    return VG_OK;
}

VgResult vg_entity_get_parent(VgContext *context, VgEntity entity, VgEntity *out_parent) {
    uint32_t world_index = 0u;
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result =
        vg_runtime_resolve_entity(context, entity, &world_index, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (out_parent == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    uint16_t parent_index = world->entities[entity_index].parent_index;
    VgEntity parent = {VG_INVALID_HANDLE_VALUE};
    if (parent_index != VG_NO_PARENT) {
        if (parent_index >= world->entity_capacity ||
            world->entities[parent_index].generation !=
                world->entities[entity_index].parent_generation)
            return VG_ERROR_INVALID_HANDLE;
        parent.value = vg_runtime_make_entity_handle(
            context, world_index, context->worlds[world_index].generation, parent_index,
            world->entities[parent_index].generation);
    }
    *out_parent = parent;
    return VG_OK;
}

VgResult vg_entity_set_parent(VgContext *context, VgEntity entity, VgEntity parent,
                              VgReparentMode mode) {
    uint32_t world_index = 0u;
    VgWorldState *world = NULL;
    uint32_t entity_index = 0u;
    VgResult result =
        vg_runtime_resolve_entity(context, entity, &world_index, &world, &entity_index);
    if (result != VG_OK)
        return result;
    if (mode != VG_REPARENT_KEEP_LOCAL && mode != VG_REPARENT_KEEP_WORLD)
        return VG_ERROR_INVALID_ARGUMENT;
    uint16_t parent_index = VG_NO_PARENT;
    uint16_t parent_generation = 0u;
    result = vg_resolve_parent(context, entity, parent, world_index, world, &parent_index,
                               &parent_generation);
    if (result != VG_OK)
        return result;
    VgTransform local = world->entities[entity_index].local;
    if (mode == VG_REPARENT_KEEP_WORLD) {
        VgMatrix old_world;
        result = vg_world_entity_matrix(world, entity_index, UINT32_MAX, NULL, VG_NO_PARENT, 0u,
                                        &old_world);
        if (result != VG_OK)
            return result;
        if (parent_index == VG_NO_PARENT) {
            if (!vg_matrix_to_transform(old_world, &local))
                return VG_ERROR_UNSUPPORTED;
        } else {
            VgMatrix parent_world;
            VgMatrix inverse;
            result = vg_world_entity_matrix(world, parent_index, UINT32_MAX, NULL, VG_NO_PARENT, 0u,
                                            &parent_world);
            if (result != VG_OK)
                return result;
            if (!vg_matrix_inverse_affine(parent_world, &inverse) ||
                !vg_matrix_to_transform(vg_matrix_multiply(inverse, old_world), &local))
                return VG_ERROR_UNSUPPORTED;
        }
    }
    result = vg_world_validate_transform_change(world, entity_index, local, parent_index,
                                                parent_generation);
    if (result != VG_OK)
        return result;
    world->entities[entity_index].local = local;
    world->entities[entity_index].parent_index = parent_index;
    world->entities[entity_index].parent_generation = parent_generation;
    return VG_OK;
}
