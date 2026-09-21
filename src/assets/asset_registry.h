#ifndef VESTIGIO_ASSET_REGISTRY_H
#define VESTIGIO_ASSET_REGISTRY_H

#include "vestigio/vestigio.h"

#include <stdbool.h>
#include <stdint.h>

enum { VG_ASSET_FINGERPRINT_SIZE = 32u };

typedef struct VgAssetSourceDesc {
    uint32_t struct_size;
    uint32_t api_version;
    VgAssetId id;
    VgAssetType type;
    uint32_t importer_version;
    uint64_t version;
    uint64_t variant;
    /* Digest of source bytes, dependencies and normalized import options. */
    uint8_t fingerprint[VG_ASSET_FINGERPRINT_SIZE];
    const char *source_path;
    const void *source_data;
    uint64_t source_size;
    const void *options_data;
    uint32_t options_size;
} VgAssetSourceDesc;

typedef struct VgAssetMemory {
    void *user;
    VgAllocateFn allocate;
    VgDeallocateFn deallocate;
} VgAssetMemory;

typedef struct VgAssetDecoder {
    void *user;
    VgResult (*decode)(void *user, const VgAssetMemory *memory, const VgAssetSourceDesc *source,
                       void **out_data, uint64_t *out_bytes, char *error, uint32_t error_capacity);
    void (*destroy)(void *user, const VgAssetMemory *memory, void *data);
} VgAssetDecoder;

typedef struct VgAssetGpuObject {
    uint64_t token;
    uint64_t estimated_bytes;
} VgAssetGpuObject;

typedef struct VgAssetGpuExecutor {
    void *user;
    bool (*is_owner_thread)(void *user);
    VgResult (*upload)(void *user, VgAssetType type, const void *cpu_data, uint64_t cpu_bytes,
                       VgAssetGpuObject *out_object, char *error, uint32_t error_capacity);
    void (*release)(void *user, VgAssetType type, VgAssetGpuObject object);
} VgAssetGpuExecutor;

/* A component owns this private reference independently from a public lease. */
typedef struct VgAssetRef {
    uint32_t index;
    uint32_t generation;
} VgAssetRef;

VgResult vg_asset_catalog_upsert(VgContext *context, const VgAssetSourceDesc *source);
VgResult vg_asset_set_decoder(VgContext *context, VgAssetType type, const VgAssetDecoder *decoder);
VgResult vg_asset_attach_gpu(VgContext *context, const VgAssetGpuExecutor *executor);
VgResult vg_asset_flush_gpu(VgContext *context);
VgResult vg_asset_detach_gpu(VgContext *context);
VgResult vg_asset_component_retain(VgContext *context, VgAsset asset, VgAssetRef *out_reference);
VgResult vg_asset_component_release(VgContext *context, VgAssetRef reference);

/* Called by context teardown after worlds have released component references. */
VgResult vg_asset_registry_can_destroy(VgContext *context);
void vg_asset_registry_destroy(VgContext *context);

#endif
