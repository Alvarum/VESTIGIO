#include "assets/asset_registry.h"
#include "runtime/runtime_internal.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define VG_WORLD_INDEX_SHIFT 34u
#define VG_WORLD_GENERATION_SHIFT 22u
#define VG_ENTITY_INDEX_SHIFT 12u
#define VG_WORLD_RESERVED_MASK UINT64_C(0x3FFFFF)
#define VG_INDEX_MASK UINT64_C(0x3FF)
#define VG_GENERATION_MASK UINT64_C(0xFFF)

static atomic_uint_least32_t vg_next_context_tag = 1u;

static void *vg_default_allocate(void *user, uint64_t size) {
    (void)user;
    if (size > (uint64_t)SIZE_MAX)
        return NULL;
    return malloc((size_t)size);
}

static void vg_default_deallocate(void *user, void *allocation) {
    (void)user;
    free(allocation);
}

static uint16_t vg_claim_context_tag(void) {
    uint_least32_t value = atomic_load_explicit(&vg_next_context_tag, memory_order_relaxed);
    while (value <= VG_HANDLE_CONTEXT_MAX) {
        uint_least32_t next = value + 1u;
        if (atomic_compare_exchange_weak_explicit(&vg_next_context_tag, &value, next,
                                                  memory_order_relaxed, memory_order_relaxed))
            return (uint16_t)value;
    }
    return 0u;
}

uint32_t vg_runtime_decode_handle(uint64_t handle, uint32_t shift, uint64_t mask) {
    return (uint32_t)((handle >> shift) & mask);
}

bool vg_runtime_context_valid(const VgContext *context) {
    return context != NULL && context->magic == VG_CONTEXT_MAGIC;
}

void *vg_runtime_allocate(VgContext *context, size_t size) {
    if (!vg_runtime_context_valid(context) || size == 0u)
        return NULL;
    return context->allocate(context->allocator_user, size);
}

void vg_runtime_deallocate(VgContext *context, void *allocation) {
    if (vg_runtime_context_valid(context) && allocation != NULL)
        context->deallocate(context->allocator_user, allocation);
}

uint64_t vg_runtime_make_world_handle(const VgContext *context, uint32_t world_index,
                                      uint16_t generation) {
    return ((uint64_t)VG_HANDLE_TYPE_WORLD << VG_HANDLE_TYPE_SHIFT) |
           ((uint64_t)context->tag << VG_HANDLE_CONTEXT_SHIFT) |
           ((uint64_t)(world_index + 1u) << VG_WORLD_INDEX_SHIFT) |
           ((uint64_t)generation << VG_WORLD_GENERATION_SHIFT);
}

uint64_t vg_runtime_make_entity_handle(const VgContext *context, uint32_t world_index,
                                       uint16_t world_generation, uint32_t entity_index,
                                       uint16_t entity_generation) {
    return ((uint64_t)VG_HANDLE_TYPE_ENTITY << VG_HANDLE_TYPE_SHIFT) |
           ((uint64_t)context->tag << VG_HANDLE_CONTEXT_SHIFT) |
           ((uint64_t)(world_index + 1u) << VG_WORLD_INDEX_SHIFT) |
           ((uint64_t)world_generation << VG_WORLD_GENERATION_SHIFT) |
           ((uint64_t)(entity_index + 1u) << VG_ENTITY_INDEX_SHIFT) | (uint64_t)entity_generation;
}

VgResult vg_runtime_resolve_world(VgContext *context, VgWorld handle, uint32_t *out_index,
                                  VgWorldState **out_world) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (handle.value == VG_INVALID_HANDLE_VALUE)
        return VG_ERROR_INVALID_HANDLE;
    if (vg_runtime_decode_handle(handle.value, VG_HANDLE_TYPE_SHIFT, VG_HANDLE_TYPE_MASK) !=
        VG_HANDLE_TYPE_WORLD)
        return VG_ERROR_WRONG_TYPE;
    if (vg_runtime_decode_handle(handle.value, VG_HANDLE_CONTEXT_SHIFT, VG_HANDLE_CONTEXT_MASK) !=
        context->tag)
        return VG_ERROR_WRONG_CONTEXT;
    if ((handle.value & VG_WORLD_RESERVED_MASK) != 0u)
        return VG_ERROR_INVALID_HANDLE;
    uint32_t encoded_index =
        vg_runtime_decode_handle(handle.value, VG_WORLD_INDEX_SHIFT, VG_INDEX_MASK);
    if (encoded_index == 0u || encoded_index > context->max_worlds)
        return VG_ERROR_INVALID_HANDLE;
    uint32_t index = encoded_index - 1u;
    VgWorldSlot *slot = &context->worlds[index];
    uint32_t generation =
        vg_runtime_decode_handle(handle.value, VG_WORLD_GENERATION_SHIFT, VG_GENERATION_MASK);
    if (slot->state != VG_WORLD_ACTIVE || slot->generation != generation || slot->world == NULL)
        return VG_ERROR_INVALID_HANDLE;
    if (out_index != NULL)
        *out_index = index;
    if (out_world != NULL)
        *out_world = slot->world;
    return VG_OK;
}

VgResult vg_runtime_resolve_entity(VgContext *context, VgEntity handle, uint32_t *out_world_index,
                                   VgWorldState **out_world, uint32_t *out_entity_index) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (handle.value == VG_INVALID_HANDLE_VALUE)
        return VG_ERROR_INVALID_HANDLE;
    if (vg_runtime_decode_handle(handle.value, VG_HANDLE_TYPE_SHIFT, VG_HANDLE_TYPE_MASK) !=
        VG_HANDLE_TYPE_ENTITY)
        return VG_ERROR_WRONG_TYPE;
    if (vg_runtime_decode_handle(handle.value, VG_HANDLE_CONTEXT_SHIFT, VG_HANDLE_CONTEXT_MASK) !=
        context->tag)
        return VG_ERROR_WRONG_CONTEXT;
    uint32_t encoded_world =
        vg_runtime_decode_handle(handle.value, VG_WORLD_INDEX_SHIFT, VG_INDEX_MASK);
    if (encoded_world == 0u || encoded_world > context->max_worlds)
        return VG_ERROR_INVALID_HANDLE;
    uint32_t world_index = encoded_world - 1u;
    VgWorldSlot *world_slot = &context->worlds[world_index];
    uint32_t world_generation =
        vg_runtime_decode_handle(handle.value, VG_WORLD_GENERATION_SHIFT, VG_GENERATION_MASK);
    if (world_slot->state != VG_WORLD_ACTIVE || world_slot->generation != world_generation ||
        world_slot->world == NULL)
        return VG_ERROR_INVALID_HANDLE;
    uint32_t encoded_entity =
        vg_runtime_decode_handle(handle.value, VG_ENTITY_INDEX_SHIFT, VG_INDEX_MASK);
    if (encoded_entity == 0u || encoded_entity > world_slot->world->entity_capacity)
        return VG_ERROR_INVALID_HANDLE;
    uint32_t entity_index = encoded_entity - 1u;
    VgEntitySlot *entity_slot = &world_slot->world->entities[entity_index];
    uint32_t entity_generation = handle.value & VG_GENERATION_MASK;
    if ((entity_slot->state != VG_ENTITY_ACTIVE && entity_slot->state != VG_ENTITY_PENDING_CREATE &&
         entity_slot->state != VG_ENTITY_PENDING_DESTROY) ||
        entity_slot->generation != entity_generation)
        return VG_ERROR_INVALID_HANDLE;
    if (out_world_index != NULL)
        *out_world_index = world_index;
    if (out_world != NULL)
        *out_world = world_slot->world;
    if (out_entity_index != NULL)
        *out_entity_index = entity_index;
    return VG_OK;
}

VgResult vg_get_version(VgVersion *out_version) {
    if (out_version == NULL || out_version->struct_size < sizeof(VgVersion))
        return VG_ERROR_INVALID_ARGUMENT;
    VgVersion version = {sizeof(VgVersion), VG_API_VERSION, VG_API_VERSION_MAJOR,
                         VG_API_VERSION_MINOR, 0u};
    *out_version = version;
    return VG_OK;
}

VgResult vg_context_create(const VgContextDesc *description, VgContext **out_context) {
    if (out_context == NULL || description == NULL ||
        description->struct_size < offsetof(VgContextDesc, log) + sizeof(description->log) ||
        description->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;

    VgAllocateFn allocate = vg_default_allocate;
    VgDeallocateFn deallocate = vg_default_deallocate;
    void *allocator_user = NULL;
    uint32_t max_worlds = VG_DEFAULT_MAX_WORLDS;
    uint32_t max_assets = VG_DEFAULT_MAX_ASSETS;
    uint32_t max_asset_leases = VG_DEFAULT_MAX_ASSET_LEASES;
    bool has_allocate = description->struct_size >=
                        offsetof(VgContextDesc, allocate) + sizeof(description->allocate);
    bool has_deallocate = description->struct_size >=
                          offsetof(VgContextDesc, deallocate) + sizeof(description->deallocate);
    if (has_allocate != has_deallocate ||
        (has_allocate && (description->allocate == NULL) != (description->deallocate == NULL)))
        return VG_ERROR_INVALID_ARGUMENT;
    if (has_allocate && description->allocate != NULL) {
        allocate = description->allocate;
        deallocate = description->deallocate;
        allocator_user = description->allocator_user;
    }
    if (description->struct_size >=
            offsetof(VgContextDesc, max_worlds) + sizeof(description->max_worlds) &&
        description->max_worlds != 0u)
        max_worlds = description->max_worlds;
    if (description->struct_size >=
            offsetof(VgContextDesc, max_assets) + sizeof(description->max_assets) &&
        description->max_assets != 0u)
        max_assets = description->max_assets;
    if (description->struct_size >=
            offsetof(VgContextDesc, max_asset_leases) + sizeof(description->max_asset_leases) &&
        description->max_asset_leases != 0u)
        max_asset_leases = description->max_asset_leases;
    if (max_worlds == 0u || max_worlds > VG_HANDLE_WORLD_MAX)
        return VG_ERROR_CAPACITY;
    if (max_assets == 0u || max_asset_leases == 0u || max_asset_leases > UINT32_C(0x3FFFFF))
        return VG_ERROR_CAPACITY;

    VgContext *context = allocate(allocator_user, sizeof(*context));
    if (context == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memset(context, 0, sizeof(*context));
    context->magic = VG_CONTEXT_MAGIC;
    context->max_worlds = max_worlds;
    context->max_assets = max_assets;
    context->max_asset_leases = max_asset_leases;
    context->allocator_user = allocator_user;
    context->allocate = allocate;
    context->deallocate = deallocate;
    context->log_user = description->user;
    context->log = description->log;
    context->worlds = allocate(allocator_user, sizeof(*context->worlds) * max_worlds);
    if (context->worlds == NULL) {
        context->magic = 0u;
        deallocate(allocator_user, context);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    memset(context->worlds, 0, sizeof(*context->worlds) * max_worlds);
    for (uint32_t index = 0u; index < max_worlds; ++index)
        context->worlds[index].generation = 1u;
    context->tag = vg_claim_context_tag();
    if (context->tag == 0u) {
        context->magic = 0u;
        deallocate(allocator_user, context->worlds);
        deallocate(allocator_user, context);
        return VG_ERROR_CAPACITY;
    }
    *out_context = context;
    return VG_OK;
}

void vg_context_destroy(VgContext *context) {
    if (!vg_runtime_context_valid(context))
        return;
    if (context->destroying || context->game_callback_depth != 0u) {
        if (context->log != NULL)
            context->log(context->log_user, VG_LOG_ERROR,
                         "vg_context_destroy cannot run from a game callback");
        return;
    }
    VgResult asset_result = vg_asset_registry_can_destroy(context);
    if (asset_result != VG_OK) {
        if (context->log != NULL)
            context->log(context->log_user, VG_LOG_ERROR,
                         "vg_context_destroy must run on the attached GPU owner thread");
        return;
    }
    context->destroying = true;
    vg_game_destroy_all(context);
    for (uint32_t index = 0u; index < context->max_worlds; ++index) {
        if (context->worlds[index].state == VG_WORLD_ACTIVE)
            vg_world_release_state(context, context->worlds[index].world);
    }
    vg_asset_registry_destroy(context);
    VgWorldSlot *worlds = context->worlds;
    VgDeallocateFn deallocate = context->deallocate;
    void *allocator_user = context->allocator_user;
    context->magic = 0u;
    deallocate(allocator_user, worlds);
    deallocate(allocator_user, context);
}
