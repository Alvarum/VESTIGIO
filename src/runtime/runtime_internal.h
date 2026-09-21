#ifndef VESTIGIO_RUNTIME_INTERNAL_H
#define VESTIGIO_RUNTIME_INTERNAL_H

#include "vestigio/vestigio.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    VG_HANDLE_TYPE_WORLD = 1u,
    VG_HANDLE_TYPE_ENTITY = 2u,
    VG_HANDLE_TYPE_ASSET = 3u,
    VG_HANDLE_CONTEXT_MAX = 65535u,
    VG_HANDLE_WORLD_MAX = 1023u,
    VG_HANDLE_ENTITY_MAX = 1023u,
    VG_HANDLE_GENERATION_MAX = 4095u,
    VG_DEFAULT_MAX_WORLDS = 64u,
    VG_DEFAULT_INITIAL_ENTITIES = 16u,
    VG_DEFAULT_MAX_ASSETS = 256u,
    VG_DEFAULT_MAX_ASSET_LEASES = 1024u,
    VG_CONTEXT_MAGIC = 0x56474358u
};

#define VG_HANDLE_TYPE_SHIFT 60u
#define VG_HANDLE_CONTEXT_SHIFT 44u
#define VG_HANDLE_TYPE_MASK UINT64_C(0xF)
#define VG_HANDLE_CONTEXT_MASK UINT64_C(0xFFFF)

typedef enum VgEntityState {
    VG_ENTITY_FREE = 0,
    VG_ENTITY_ACTIVE = 1,
    VG_ENTITY_PENDING_CREATE = 2,
    VG_ENTITY_PENDING_DESTROY = 3,
    VG_ENTITY_PENDING_CANCEL = 4,
    VG_ENTITY_RETIRED = 5
} VgEntityState;

typedef struct VgEntitySlot {
    VgTransform local;
    uint16_t generation;
    uint16_t parent_index;
    uint16_t parent_generation;
    uint8_t state;
} VgEntitySlot;

typedef struct VgWorldState {
    VgEntitySlot *entities;
    uint32_t entity_capacity;
    uint32_t max_entities;
    uint32_t active_count;
    uint32_t iteration_count;
    bool iterating;
} VgWorldState;

typedef enum VgWorldSlotState {
    VG_WORLD_FREE = 0,
    VG_WORLD_ACTIVE = 1,
    VG_WORLD_RETIRED = 2
} VgWorldSlotState;

typedef struct VgWorldSlot {
    VgWorldState *world;
    uint16_t generation;
    uint8_t state;
} VgWorldSlot;

typedef struct VgAssetRegistry VgAssetRegistry;

struct VgContext {
    uint32_t magic;
    uint16_t tag;
    uint16_t reserved;
    uint32_t max_worlds;
    uint32_t max_assets;
    uint32_t max_asset_leases;
    VgWorldSlot *worlds;
    VgAssetRegistry *assets;
    void *allocator_user;
    VgAllocateFn allocate;
    VgDeallocateFn deallocate;
    void *log_user;
    VgLogFn log;
};

bool vg_runtime_context_valid(const VgContext *context);
uint32_t vg_runtime_decode_handle(uint64_t handle, uint32_t shift, uint64_t mask);
void *vg_runtime_allocate(VgContext *context, size_t size);
void vg_runtime_deallocate(VgContext *context, void *allocation);
uint64_t vg_runtime_make_world_handle(const VgContext *context, uint32_t world_index,
                                      uint16_t generation);
uint64_t vg_runtime_make_entity_handle(const VgContext *context, uint32_t world_index,
                                       uint16_t world_generation, uint32_t entity_index,
                                       uint16_t entity_generation);
VgResult vg_runtime_resolve_world(VgContext *context, VgWorld handle, uint32_t *out_index,
                                  VgWorldState **out_world);
VgResult vg_runtime_resolve_entity(VgContext *context, VgEntity handle, uint32_t *out_world_index,
                                   VgWorldState **out_world, uint32_t *out_entity_index);
void vg_world_release_state(VgContext *context, VgWorldState *world);

#endif
