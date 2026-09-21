#include "assets/asset_registry.h"
#include "runtime/runtime_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define VG_ASSET_LEASE_INDEX_SHIFT 22u
#define VG_ASSET_LEASE_INDEX_MASK UINT64_C(0x3FFFFF)
#define VG_ASSET_LEASE_GENERATION_MASK UINT64_C(0x3FFFFF)
#define VG_ASSET_LEASE_GENERATION_MAX 4194303u
#define VG_ASSET_ERROR_CAPACITY 192u

typedef struct VgAssetCatalogEntry {
    bool used;
    VgAssetSourceDesc source;
    char *path;
    void *source_data;
    void *options_data;
} VgAssetCatalogEntry;

typedef struct VgAssetCpuPayload {
    void *data;
    uint64_t bytes;
    VgAssetDecoder decoder;
} VgAssetCpuPayload;

typedef enum VgAssetUploadSource {
    VG_ASSET_UPLOAD_NONE = 0,
    VG_ASSET_UPLOAD_ACTIVE = 1,
    VG_ASSET_UPLOAD_CANDIDATE = 2
} VgAssetUploadSource;

typedef struct VgAssetResident {
    bool used;
    uint32_t generation;
    VgAssetId id;
    VgAssetType type;
    uint64_t variant;
    uint32_t external_refs;
    uint32_t component_refs;
    uint64_t published_version;
    uint32_t published_importer_version;
    uint8_t published_fingerprint[VG_ASSET_FINGERPRINT_SIZE];
    VgAssetCpuPayload active_cpu;
    VgAssetGpuObject active_gpu;
    uint64_t candidate_version;
    uint32_t candidate_importer_version;
    uint8_t candidate_fingerprint[VG_ASSET_FINGERPRINT_SIZE];
    VgAssetCpuPayload candidate_cpu;
    VgAssetUploadSource upload_source;
    VgResult last_result;
    VgResult last_reload_result;
    char error[VG_ASSET_ERROR_CAPACITY];
} VgAssetResident;

typedef struct VgAssetLeaseSlot {
    bool active;
    bool retired;
    uint32_t generation;
    uint32_t resident_index;
    VgAssetResidency residency;
} VgAssetLeaseSlot;

typedef struct VgAssetPendingRelease {
    bool used;
    VgAssetType type;
    VgAssetGpuObject object;
} VgAssetPendingRelease;

struct VgAssetRegistry {
    uint32_t max_assets;
    uint32_t max_leases;
    VgAssetCatalogEntry *catalog;
    VgAssetResident *residents;
    VgAssetLeaseSlot *leases;
    VgAssetPendingRelease *releases;
    VgAssetDecoder decoders[3];
    VgAssetGpuExecutor gpu;
    bool gpu_attached;
    uint64_t decode_count;
    uint64_t upload_count;
    uint64_t gpu_release_count;
    uint64_t cache_hit_count;
    uint64_t purge_count;
};

static bool vg_asset_id_valid(VgAssetId id) {
    uint8_t value = 0u;
    for (uint32_t index = 0u; index < sizeof(id.bytes); ++index)
        value |= id.bytes[index];
    return value != 0u;
}

static bool vg_asset_id_equal(VgAssetId left, VgAssetId right) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

static void vg_asset_set_error(VgAssetResident *resident, VgResult result, const char *message,
                               bool reload) {
    resident->last_result = result;
    if (reload)
        resident->last_reload_result = result;
    if (message == NULL)
        message = "";
    size_t length = strlen(message);
    if (length >= sizeof(resident->error))
        length = sizeof(resident->error) - 1u;
    memcpy(resident->error, message, length);
    resident->error[length] = '\0';
}

static bool vg_asset_size_valid(uint64_t size) {
    return size <= (uint64_t)SIZE_MAX;
}

static void *vg_asset_copy(VgContext *context, const void *source, uint64_t size) {
    if (size == 0u)
        return NULL;
    if (source == NULL || !vg_asset_size_valid(size))
        return NULL;
    void *copy = vg_runtime_allocate(context, (size_t)size);
    if (copy != NULL)
        memcpy(copy, source, (size_t)size);
    return copy;
}

static void vg_asset_release_catalog_entry(VgContext *context, VgAssetCatalogEntry *entry) {
    vg_runtime_deallocate(context, entry->path);
    vg_runtime_deallocate(context, entry->source_data);
    vg_runtime_deallocate(context, entry->options_data);
    memset(entry, 0, sizeof(*entry));
}

static void vg_asset_destroy_cpu(VgContext *context, VgAssetCpuPayload *payload) {
    if (payload->data != NULL && payload->decoder.destroy != NULL) {
        VgAssetMemory memory = {context->allocator_user, context->allocate, context->deallocate};
        payload->decoder.destroy(payload->decoder.user, &memory, payload->data);
    }
    memset(payload, 0, sizeof(*payload));
}

static VgResult vg_asset_registry_create(VgContext *context) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (context->assets != NULL)
        return VG_OK;
    if (context->max_asset_leases > VG_ASSET_LEASE_INDEX_MASK)
        return VG_ERROR_CAPACITY;

    VgAssetRegistry *registry = vg_runtime_allocate(context, sizeof(*registry));
    if (registry == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memset(registry, 0, sizeof(*registry));
    registry->max_assets = context->max_assets;
    registry->max_leases = context->max_asset_leases;

    registry->catalog =
        vg_runtime_allocate(context, sizeof(*registry->catalog) * registry->max_assets);
    registry->residents =
        vg_runtime_allocate(context, sizeof(*registry->residents) * registry->max_assets);
    registry->leases =
        vg_runtime_allocate(context, sizeof(*registry->leases) * registry->max_leases);
    registry->releases =
        vg_runtime_allocate(context, sizeof(*registry->releases) * registry->max_assets);
    if (registry->catalog == NULL || registry->residents == NULL || registry->leases == NULL ||
        registry->releases == NULL) {
        vg_runtime_deallocate(context, registry->releases);
        vg_runtime_deallocate(context, registry->leases);
        vg_runtime_deallocate(context, registry->residents);
        vg_runtime_deallocate(context, registry->catalog);
        vg_runtime_deallocate(context, registry);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    memset(registry->catalog, 0, sizeof(*registry->catalog) * registry->max_assets);
    memset(registry->residents, 0, sizeof(*registry->residents) * registry->max_assets);
    memset(registry->leases, 0, sizeof(*registry->leases) * registry->max_leases);
    memset(registry->releases, 0, sizeof(*registry->releases) * registry->max_assets);
    for (uint32_t index = 0u; index < registry->max_assets; ++index)
        registry->residents[index].generation = 1u;
    for (uint32_t index = 0u; index < registry->max_leases; ++index)
        registry->leases[index].generation = 1u;
    context->assets = registry;
    return VG_OK;
}

static VgAssetCatalogEntry *vg_asset_find_catalog(VgAssetRegistry *registry, VgAssetId id,
                                                  uint64_t variant) {
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetCatalogEntry *entry = &registry->catalog[index];
        if (entry->used && entry->source.variant == variant &&
            vg_asset_id_equal(entry->source.id, id))
            return entry;
    }
    return NULL;
}

static VgAssetResident *vg_asset_find_resident(VgAssetRegistry *registry, VgAssetId id,
                                               uint64_t variant, uint32_t *out_index) {
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetResident *resident = &registry->residents[index];
        if (resident->used && resident->variant == variant && vg_asset_id_equal(resident->id, id)) {
            if (out_index != NULL)
                *out_index = index;
            return resident;
        }
    }
    return NULL;
}

static VgResult vg_asset_queue_release(VgAssetRegistry *registry, VgAssetType type,
                                       VgAssetGpuObject object) {
    if (object.token == 0u)
        return VG_OK;
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        if (!registry->releases[index].used) {
            registry->releases[index] = (VgAssetPendingRelease){true, type, object};
            return VG_OK;
        }
    }
    return VG_ERROR_CAPACITY;
}

static bool vg_asset_resident_requires_gpu(const VgAssetRegistry *registry,
                                           uint32_t resident_index) {
    for (uint32_t index = 0u; index < registry->max_leases; ++index) {
        const VgAssetLeaseSlot *lease = &registry->leases[index];
        if (lease->active && lease->resident_index == resident_index &&
            (lease->residency & VG_ASSET_RESIDENCY_GPU) != 0u)
            return true;
    }
    return false;
}

static void vg_asset_publish_cpu(VgContext *context, VgAssetResident *resident) {
    vg_asset_destroy_cpu(context, &resident->active_cpu);
    resident->active_cpu = resident->candidate_cpu;
    memset(&resident->candidate_cpu, 0, sizeof(resident->candidate_cpu));
    resident->published_version = resident->candidate_version;
    resident->published_importer_version = resident->candidate_importer_version;
    memcpy(resident->published_fingerprint, resident->candidate_fingerprint,
           sizeof(resident->published_fingerprint));
    resident->candidate_version = 0u;
    resident->candidate_importer_version = 0u;
    memset(resident->candidate_fingerprint, 0, sizeof(resident->candidate_fingerprint));
    resident->last_result = VG_OK;
    resident->last_reload_result = VG_OK;
    resident->error[0] = '\0';
}

static VgResult vg_asset_decode(VgContext *context, uint32_t resident_index,
                                VgAssetCatalogEntry *catalog) {
    VgAssetRegistry *registry = context->assets;
    VgAssetResident *resident = &registry->residents[resident_index];
    bool reload = resident->active_cpu.data != NULL;
    if (resident->candidate_cpu.data != NULL || resident->upload_source != VG_ASSET_UPLOAD_NONE) {
        if (resident->candidate_version == catalog->source.version &&
            resident->candidate_importer_version == catalog->source.importer_version &&
            memcmp(resident->candidate_fingerprint, catalog->source.fingerprint,
                   VG_ASSET_FINGERPRINT_SIZE) == 0)
            return VG_OK;
        return VG_ERROR_CONFLICT;
    }
    VgAssetDecoder decoder = registry->decoders[resident->type];
    if (decoder.decode == NULL || decoder.destroy == NULL) {
        vg_asset_set_error(resident, VG_ERROR_UNSUPPORTED, "No decoder registered", reload);
        return VG_OK;
    }

    void *data = NULL;
    uint64_t bytes = 0u;
    char error[VG_ASSET_ERROR_CAPACITY] = {0};
    VgAssetMemory memory = {context->allocator_user, context->allocate, context->deallocate};
    ++registry->decode_count;
    VgResult result = decoder.decode(decoder.user, &memory, &catalog->source, &data, &bytes, error,
                                     sizeof(error));
    if (result != VG_OK || data == NULL || bytes == 0u) {
        if (data != NULL)
            decoder.destroy(decoder.user, &memory, data);
        if (result == VG_OK)
            result = VG_ERROR_INVALID_ARGUMENT;
        vg_asset_set_error(resident, result, error[0] == '\0' ? "CPU decode failed" : error,
                           reload);
        return VG_OK;
    }

    resident->candidate_cpu = (VgAssetCpuPayload){data, bytes, decoder};
    resident->candidate_version = catalog->source.version;
    resident->candidate_importer_version = catalog->source.importer_version;
    memcpy(resident->candidate_fingerprint, catalog->source.fingerprint,
           sizeof(resident->candidate_fingerprint));
    if (vg_asset_resident_requires_gpu(registry, resident_index) ||
        resident->active_gpu.token != 0u)
        resident->upload_source = VG_ASSET_UPLOAD_CANDIDATE;
    else
        vg_asset_publish_cpu(context, resident);
    return VG_OK;
}

static uint64_t vg_asset_make_handle(const VgContext *context, uint32_t lease_index,
                                     uint32_t generation) {
    return ((uint64_t)VG_HANDLE_TYPE_ASSET << VG_HANDLE_TYPE_SHIFT) |
           ((uint64_t)context->tag << VG_HANDLE_CONTEXT_SHIFT) |
           ((uint64_t)(lease_index + 1u) << VG_ASSET_LEASE_INDEX_SHIFT) | (uint64_t)generation;
}

static VgResult vg_asset_resolve_lease(VgContext *context, VgAsset handle,
                                       VgAssetLeaseSlot **out_lease,
                                       VgAssetResident **out_resident) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (handle.value == VG_INVALID_HANDLE_VALUE)
        return VG_ERROR_INVALID_HANDLE;
    if (vg_runtime_decode_handle(handle.value, VG_HANDLE_TYPE_SHIFT, VG_HANDLE_TYPE_MASK) !=
        VG_HANDLE_TYPE_ASSET)
        return VG_ERROR_WRONG_TYPE;
    if (vg_runtime_decode_handle(handle.value, VG_HANDLE_CONTEXT_SHIFT, VG_HANDLE_CONTEXT_MASK) !=
        context->tag)
        return VG_ERROR_WRONG_CONTEXT;
    if (context->assets == NULL)
        return VG_ERROR_INVALID_HANDLE;
    uint32_t encoded_index = vg_runtime_decode_handle(handle.value, VG_ASSET_LEASE_INDEX_SHIFT,
                                                      VG_ASSET_LEASE_INDEX_MASK);
    if (encoded_index == 0u || encoded_index > context->assets->max_leases)
        return VG_ERROR_INVALID_HANDLE;
    VgAssetLeaseSlot *lease = &context->assets->leases[encoded_index - 1u];
    uint32_t generation =
        vg_runtime_decode_handle(handle.value, 0u, VG_ASSET_LEASE_GENERATION_MASK);
    if (!lease->active || lease->generation != generation ||
        lease->resident_index >= context->assets->max_assets)
        return VG_ERROR_INVALID_HANDLE;
    VgAssetResident *resident = &context->assets->residents[lease->resident_index];
    if (!resident->used)
        return VG_ERROR_INVALID_HANDLE;
    if (out_lease != NULL)
        *out_lease = lease;
    if (out_resident != NULL)
        *out_resident = resident;
    return VG_OK;
}

static VgResult vg_asset_allocate_lease(VgContext *context, uint32_t resident_index,
                                        VgAssetResidency residency, VgAsset *out_asset) {
    VgAssetRegistry *registry = context->assets;
    for (uint32_t index = 0u; index < registry->max_leases; ++index) {
        VgAssetLeaseSlot *lease = &registry->leases[index];
        if (!lease->active && !lease->retired) {
            lease->active = true;
            lease->resident_index = resident_index;
            lease->residency = residency;
            ++registry->residents[resident_index].external_refs;
            out_asset->value = vg_asset_make_handle(context, index, lease->generation);
            return VG_OK;
        }
    }
    return VG_ERROR_CAPACITY;
}

VgResult vg_asset_catalog_upsert(VgContext *context, const VgAssetSourceDesc *source) {
    if (!vg_runtime_context_valid(context) || source == NULL ||
        source->struct_size < sizeof(*source) || source->api_version != VG_API_VERSION ||
        !vg_asset_id_valid(source->id) ||
        (source->type != VG_ASSET_TYPE_TEXTURE && source->type != VG_ASSET_TYPE_MESH) ||
        source->version == 0u || source->source_path == NULL || source->source_path[0] == '\0' ||
        (source->source_size != 0u && source->source_data == NULL) ||
        (source->options_size != 0u && source->options_data == NULL) ||
        !vg_asset_size_valid(source->source_size))
        return VG_ERROR_INVALID_ARGUMENT;
    VgResult result = vg_asset_registry_create(context);
    if (result != VG_OK)
        return result;
    VgAssetRegistry *registry = context->assets;
    VgAssetCatalogEntry *entry = vg_asset_find_catalog(registry, source->id, source->variant);
    if (entry != NULL && entry->source.type != source->type)
        return VG_ERROR_WRONG_TYPE;
    if (entry == NULL) {
        for (uint32_t index = 0u; index < registry->max_assets; ++index) {
            if (!registry->catalog[index].used) {
                entry = &registry->catalog[index];
                break;
            }
        }
        if (entry == NULL)
            return VG_ERROR_CAPACITY;
    }

    size_t path_size = strlen(source->source_path) + 1u;
    char *path = vg_runtime_allocate(context, path_size);
    void *source_data = vg_asset_copy(context, source->source_data, source->source_size);
    void *options_data = vg_asset_copy(context, source->options_data, source->options_size);
    if (path == NULL || (source->source_size != 0u && source_data == NULL) ||
        (source->options_size != 0u && options_data == NULL)) {
        vg_runtime_deallocate(context, options_data);
        vg_runtime_deallocate(context, source_data);
        vg_runtime_deallocate(context, path);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    memcpy(path, source->source_path, path_size);

    if (entry->used)
        vg_asset_release_catalog_entry(context, entry);
    entry->used = true;
    entry->source = *source;
    entry->path = path;
    entry->source_data = source_data;
    entry->options_data = options_data;
    entry->source.source_path = path;
    entry->source.source_data = source_data;
    entry->source.options_data = options_data;
    return VG_OK;
}

VgResult vg_asset_set_decoder(VgContext *context, VgAssetType type, const VgAssetDecoder *decoder) {
    if (!vg_runtime_context_valid(context) || decoder == NULL || decoder->decode == NULL ||
        decoder->destroy == NULL || (type != VG_ASSET_TYPE_TEXTURE && type != VG_ASSET_TYPE_MESH))
        return VG_ERROR_INVALID_ARGUMENT;
    VgResult result = vg_asset_registry_create(context);
    if (result != VG_OK)
        return result;
    context->assets->decoders[type] = *decoder;
    return VG_OK;
}

VgResult vg_asset_attach_gpu(VgContext *context, const VgAssetGpuExecutor *executor) {
    if (!vg_runtime_context_valid(context) || executor == NULL ||
        executor->is_owner_thread == NULL || executor->upload == NULL || executor->release == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!executor->is_owner_thread(executor->user))
        return VG_ERROR_WRONG_THREAD;
    VgResult result = vg_asset_registry_create(context);
    if (result != VG_OK)
        return result;
    if (context->assets->gpu_attached)
        return VG_ERROR_CONFLICT;
    context->assets->gpu = *executor;
    context->assets->gpu_attached = true;
    return VG_OK;
}

VgResult vg_asset_acquire(VgContext *context, const VgAssetRequest *request, VgAsset *out_asset) {
    if (!vg_runtime_context_valid(context) || request == NULL || out_asset == NULL ||
        request->struct_size < sizeof(*request) || request->api_version != VG_API_VERSION ||
        !vg_asset_id_valid(request->id) ||
        (request->type != VG_ASSET_TYPE_TEXTURE && request->type != VG_ASSET_TYPE_MESH))
        return VG_ERROR_INVALID_ARGUMENT;
    VgAssetResidency residency = request->required_residency;
    if (residency == 0u)
        residency = VG_ASSET_RESIDENCY_CPU;
    const uint32_t residency_mask = VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU;
    if ((residency & ~residency_mask) != 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    residency |= VG_ASSET_RESIDENCY_CPU;
    VgResult result = vg_asset_registry_create(context);
    if (result != VG_OK)
        return result;
    VgAssetRegistry *registry = context->assets;
    VgAssetCatalogEntry *catalog = vg_asset_find_catalog(registry, request->id, request->variant);
    if (catalog == NULL)
        return VG_ERROR_NOT_FOUND;
    if (catalog->source.type != request->type)
        return VG_ERROR_WRONG_TYPE;

    uint32_t resident_index = 0u;
    VgAssetResident *resident =
        vg_asset_find_resident(registry, request->id, request->variant, &resident_index);
    bool new_resident = false;
    if (resident == NULL) {
        resident_index = registry->max_assets;
        for (uint32_t index = 0u; index < registry->max_assets; ++index) {
            if (!registry->residents[index].used) {
                resident_index = index;
                break;
            }
        }
        if (resident_index == registry->max_assets)
            return VG_ERROR_CAPACITY;
        resident = &registry->residents[resident_index];
        uint32_t generation = resident->generation == 0u ? 1u : resident->generation;
        memset(resident, 0, sizeof(*resident));
        resident->used = true;
        resident->generation = generation;
        resident->id = request->id;
        resident->type = request->type;
        resident->variant = request->variant;
        resident->last_result = VG_OK;
        resident->last_reload_result = VG_OK;
        new_resident = true;
    } else {
        ++registry->cache_hit_count;
    }

    VgAsset lease = {0};
    result = vg_asset_allocate_lease(context, resident_index, residency, &lease);
    if (result != VG_OK) {
        if (new_resident)
            resident->used = false;
        return result;
    }
    if (resident->active_cpu.data == NULL && resident->candidate_cpu.data == NULL &&
        resident->upload_source == VG_ASSET_UPLOAD_NONE)
        (void)vg_asset_decode(context, resident_index, catalog);
    else if ((residency & VG_ASSET_RESIDENCY_GPU) != 0u && resident->active_gpu.token == 0u &&
             resident->active_cpu.data != NULL && resident->upload_source == VG_ASSET_UPLOAD_NONE)
        resident->upload_source = VG_ASSET_UPLOAD_ACTIVE;
    *out_asset = lease;
    return VG_OK;
}

VgResult vg_asset_clone(VgContext *context, VgAsset source, VgAsset *out_asset) {
    if (out_asset == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    VgAssetLeaseSlot *lease = NULL;
    VgResult result = vg_asset_resolve_lease(context, source, &lease, NULL);
    if (result != VG_OK)
        return result;
    uint32_t resident_index = lease->resident_index;
    VgAssetResidency residency = lease->residency;
    return vg_asset_allocate_lease(context, resident_index, residency, out_asset);
}

VgResult vg_asset_release(VgContext *context, VgAsset asset) {
    VgAssetLeaseSlot *lease = NULL;
    VgAssetResident *resident = NULL;
    VgResult result = vg_asset_resolve_lease(context, asset, &lease, &resident);
    if (result != VG_OK)
        return result;
    if (resident->external_refs == 0u)
        return VG_ERROR_CONFLICT;
    --resident->external_refs;
    lease->active = false;
    lease->resident_index = 0u;
    lease->residency = 0u;
    if (lease->generation >= VG_ASSET_LEASE_GENERATION_MAX)
        lease->retired = true;
    else
        ++lease->generation;
    return VG_OK;
}

VgResult vg_asset_reload(VgContext *context, VgAsset asset) {
    VgAssetLeaseSlot *lease = NULL;
    VgAssetResident *resident = NULL;
    VgResult result = vg_asset_resolve_lease(context, asset, &lease, &resident);
    if (result != VG_OK)
        return result;
    VgAssetCatalogEntry *catalog =
        vg_asset_find_catalog(context->assets, resident->id, resident->variant);
    if (catalog == NULL)
        return VG_ERROR_NOT_FOUND;
    if (resident->published_version == catalog->source.version &&
        resident->published_importer_version == catalog->source.importer_version &&
        memcmp(resident->published_fingerprint, catalog->source.fingerprint,
               VG_ASSET_FINGERPRINT_SIZE) == 0)
        return VG_OK;
    return vg_asset_decode(context, lease->resident_index, catalog);
}

static bool vg_asset_is_ready(const VgAssetResident *resident, VgAssetResidency residency) {
    if (resident->active_cpu.data == NULL)
        return false;
    return (residency & VG_ASSET_RESIDENCY_GPU) == 0u || resident->active_gpu.token != 0u;
}

VgResult vg_asset_get_info(VgContext *context, VgAsset asset, VgAssetInfo *out_info) {
    if (out_info == NULL || out_info->struct_size < sizeof(*out_info) ||
        out_info->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    VgAssetLeaseSlot *lease = NULL;
    VgAssetResident *resident = NULL;
    VgResult result = vg_asset_resolve_lease(context, asset, &lease, &resident);
    if (result != VG_OK)
        return result;
    VgAssetInfoFlags flags = 0u;
    if (resident->active_cpu.data != NULL)
        flags |= VG_ASSET_INFO_CPU_RESIDENT | VG_ASSET_INFO_HAS_USABLE_VERSION;
    if (resident->active_gpu.token != 0u)
        flags |= VG_ASSET_INFO_GPU_RESIDENT;
    if (resident->candidate_cpu.data != NULL || resident->upload_source != VG_ASSET_UPLOAD_NONE)
        flags |= VG_ASSET_INFO_RELOAD_PENDING;
    if (resident->external_refs == 0u && resident->component_refs == 0u)
        flags |= VG_ASSET_INFO_EVICTABLE;
    VgAssetState state = VG_ASSET_FAILED;
    if (vg_asset_is_ready(resident, lease->residency))
        state = VG_ASSET_READY;
    else if (resident->candidate_cpu.data != NULL ||
             resident->upload_source != VG_ASSET_UPLOAD_NONE)
        state = VG_ASSET_LOADING;

    VgAssetCatalogEntry *catalog =
        vg_asset_find_catalog(context->assets, resident->id, resident->variant);
    VgAssetInfo info = {0};
    info.struct_size = sizeof(info);
    info.api_version = VG_API_VERSION;
    info.id = resident->id;
    info.type = resident->type;
    info.state = state;
    info.flags = flags;
    info.external_refs = resident->external_refs;
    info.component_refs = resident->component_refs;
    info.variant = resident->variant;
    info.published_version = resident->published_version;
    info.candidate_version = resident->candidate_version;
    info.last_result = resident->last_result;
    info.last_reload_result = resident->last_reload_result;
    info.source_ram_bytes = catalog == NULL ? 0u : catalog->source.source_size;
    info.derived_ram_bytes = resident->active_cpu.bytes;
    info.staging_ram_bytes = resident->candidate_cpu.bytes;
    info.estimated_gpu_bytes = resident->active_gpu.estimated_bytes;
    *out_info = info;
    return VG_OK;
}

VgResult vg_asset_get_error(VgContext *context, VgAsset asset, char *utf8, uint32_t capacity,
                            uint32_t *out_required) {
    VgAssetResident *resident = NULL;
    VgResult result = vg_asset_resolve_lease(context, asset, NULL, &resident);
    if (result != VG_OK)
        return result;
    size_t required = strlen(resident->error) + 1u;
    if (required > UINT32_MAX)
        return VG_ERROR_CAPACITY;
    if (out_required != NULL)
        *out_required = (uint32_t)required;
    if (utf8 == NULL || capacity < required)
        return VG_ERROR_CAPACITY;
    memcpy(utf8, resident->error, required);
    return VG_OK;
}

VgResult vg_asset_component_retain(VgContext *context, VgAsset asset, VgAssetRef *out_reference) {
    if (out_reference == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    VgAssetLeaseSlot *lease = NULL;
    VgAssetResident *resident = NULL;
    VgResult result = vg_asset_resolve_lease(context, asset, &lease, &resident);
    if (result != VG_OK)
        return result;
    if (resident->component_refs == UINT32_MAX)
        return VG_ERROR_CAPACITY;
    ++resident->component_refs;
    *out_reference = (VgAssetRef){lease->resident_index + 1u, resident->generation};
    return VG_OK;
}

VgResult vg_asset_component_release(VgContext *context, VgAssetRef reference) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (context->assets == NULL || reference.index == 0u ||
        reference.index > context->assets->max_assets)
        return VG_ERROR_INVALID_HANDLE;
    VgAssetResident *resident = &context->assets->residents[reference.index - 1u];
    if (!resident->used || resident->generation != reference.generation ||
        resident->component_refs == 0u)
        return VG_ERROR_INVALID_HANDLE;
    --resident->component_refs;
    return VG_OK;
}

static void vg_asset_upload_failed(VgContext *context, VgAssetResident *resident, VgResult result,
                                   const char *error) {
    if (resident->upload_source == VG_ASSET_UPLOAD_CANDIDATE) {
        bool reload = resident->active_cpu.data != NULL;
        vg_asset_destroy_cpu(context, &resident->candidate_cpu);
        resident->candidate_version = 0u;
        resident->candidate_importer_version = 0u;
        memset(resident->candidate_fingerprint, 0, sizeof(resident->candidate_fingerprint));
        vg_asset_set_error(resident, result, error, reload);
    } else {
        vg_asset_set_error(resident, result, error, false);
    }
    resident->upload_source = VG_ASSET_UPLOAD_NONE;
}

VgResult vg_asset_flush_gpu(VgContext *context) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (context->assets == NULL)
        return VG_OK;
    VgAssetRegistry *registry = context->assets;
    if (!registry->gpu_attached)
        return VG_ERROR_GPU;
    if (!registry->gpu.is_owner_thread(registry->gpu.user))
        return VG_ERROR_WRONG_THREAD;

    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetResident *resident = &registry->residents[index];
        if (!resident->used || resident->upload_source == VG_ASSET_UPLOAD_NONE)
            continue;
        VgAssetCpuPayload *source = resident->upload_source == VG_ASSET_UPLOAD_CANDIDATE
                                        ? &resident->candidate_cpu
                                        : &resident->active_cpu;
        VgAssetGpuObject candidate = {0};
        char error[VG_ASSET_ERROR_CAPACITY] = {0};
        ++registry->upload_count;
        VgResult result = registry->gpu.upload(registry->gpu.user, resident->type, source->data,
                                               source->bytes, &candidate, error, sizeof(error));
        if (result != VG_OK || candidate.token == 0u) {
            if (candidate.token != 0u) {
                registry->gpu.release(registry->gpu.user, resident->type, candidate);
                ++registry->gpu_release_count;
            }
            if (result == VG_OK)
                result = VG_ERROR_GPU;
            vg_asset_upload_failed(context, resident, result,
                                   error[0] == '\0' ? "GPU upload failed" : error);
            continue;
        }
        if (resident->active_gpu.token != 0u) {
            result = vg_asset_queue_release(registry, resident->type, resident->active_gpu);
            if (result != VG_OK) {
                registry->gpu.release(registry->gpu.user, resident->type, candidate);
                ++registry->gpu_release_count;
                vg_asset_upload_failed(context, resident, result, "GPU release queue full");
                continue;
            }
        }
        resident->active_gpu = candidate;
        if (resident->upload_source == VG_ASSET_UPLOAD_CANDIDATE)
            vg_asset_publish_cpu(context, resident);
        resident->upload_source = VG_ASSET_UPLOAD_NONE;
        resident->last_result = VG_OK;
        resident->error[0] = '\0';
    }

    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetPendingRelease *pending = &registry->releases[index];
        if (pending->used) {
            registry->gpu.release(registry->gpu.user, pending->type, pending->object);
            ++registry->gpu_release_count;
            memset(pending, 0, sizeof(*pending));
        }
    }
    return VG_OK;
}

VgResult vg_asset_detach_gpu(VgContext *context) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (context->assets == NULL || !context->assets->gpu_attached)
        return VG_OK;
    VgAssetRegistry *registry = context->assets;
    if (!registry->gpu.is_owner_thread(registry->gpu.user))
        return VG_ERROR_WRONG_THREAD;
    uint32_t available_release_slots = 0u;
    uint32_t active_gpu_objects = 0u;
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        if (!registry->releases[index].used)
            ++available_release_slots;
        if (registry->residents[index].used && registry->residents[index].active_gpu.token != 0u)
            ++active_gpu_objects;
    }
    if (active_gpu_objects > available_release_slots)
        return VG_ERROR_CAPACITY;
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetResident *resident = &registry->residents[index];
        if (resident->used && resident->active_gpu.token != 0u) {
            VgResult result =
                vg_asset_queue_release(registry, resident->type, resident->active_gpu);
            if (result != VG_OK)
                return result;
            resident->active_gpu = (VgAssetGpuObject){0};
            vg_asset_set_error(resident, VG_ERROR_GPU, "GPU executor detached", false);
        }
        if (resident->used && resident->upload_source != VG_ASSET_UPLOAD_NONE)
            vg_asset_upload_failed(context, resident, VG_ERROR_GPU, "GPU executor detached");
    }
    VgResult result = vg_asset_flush_gpu(context);
    if (result != VG_OK)
        return result;
    memset(&registry->gpu, 0, sizeof(registry->gpu));
    registry->gpu_attached = false;
    return VG_OK;
}

VgResult vg_asset_registry_can_destroy(VgContext *context) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    if (context->assets == NULL || !context->assets->gpu_attached)
        return VG_OK;
    return context->assets->gpu.is_owner_thread(context->assets->gpu.user) ? VG_OK
                                                                           : VG_ERROR_WRONG_THREAD;
}

VgResult vg_assets_purge_unused(VgContext *context, uint32_t *out_purged) {
    if (!vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    uint32_t purged = 0u;
    if (context->assets == NULL) {
        if (out_purged != NULL)
            *out_purged = 0u;
        return VG_OK;
    }
    VgAssetRegistry *registry = context->assets;
    uint32_t available_release_slots = 0u;
    uint32_t required_release_slots = 0u;
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        if (!registry->releases[index].used)
            ++available_release_slots;
        VgAssetResident *resident = &registry->residents[index];
        if (resident->used && resident->external_refs == 0u && resident->component_refs == 0u &&
            resident->active_gpu.token != 0u)
            ++required_release_slots;
    }
    if (required_release_slots > available_release_slots)
        return VG_ERROR_CAPACITY;
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetResident *resident = &registry->residents[index];
        if (!resident->used || resident->external_refs != 0u || resident->component_refs != 0u)
            continue;
        VgResult result = vg_asset_queue_release(registry, resident->type, resident->active_gpu);
        if (result != VG_OK)
            return result;
        vg_asset_destroy_cpu(context, &resident->candidate_cpu);
        vg_asset_destroy_cpu(context, &resident->active_cpu);
        uint32_t generation = resident->generation + 1u;
        if (generation == 0u)
            generation = 1u;
        memset(resident, 0, sizeof(*resident));
        resident->generation = generation;
        ++purged;
    }
    registry->purge_count += purged;
    if (out_purged != NULL)
        *out_purged = purged;
    return VG_OK;
}

VgResult vg_assets_get_counters(VgContext *context, VgAssetCounters *out_counters) {
    if (!vg_runtime_context_valid(context) || out_counters == NULL ||
        out_counters->struct_size < sizeof(*out_counters) ||
        out_counters->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    VgAssetCounters counters = {0};
    counters.struct_size = sizeof(counters);
    counters.api_version = VG_API_VERSION;
    if (context->assets == NULL) {
        *out_counters = counters;
        return VG_OK;
    }
    VgAssetRegistry *registry = context->assets;
    counters.decode_count = registry->decode_count;
    counters.upload_count = registry->upload_count;
    counters.gpu_release_count = registry->gpu_release_count;
    counters.cache_hit_count = registry->cache_hit_count;
    counters.purge_count = registry->purge_count;
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetCatalogEntry *entry = &registry->catalog[index];
        if (entry->used) {
            ++counters.catalog_entries;
            counters.source_ram_bytes += entry->source.source_size;
        }
        VgAssetResident *resident = &registry->residents[index];
        if (resident->used) {
            ++counters.resident_entries;
            counters.component_refs += resident->component_refs;
            counters.derived_ram_bytes += resident->active_cpu.bytes;
            counters.staging_ram_bytes += resident->candidate_cpu.bytes;
            counters.estimated_gpu_bytes += resident->active_gpu.estimated_bytes;
            if (resident->active_cpu.data != NULL)
                ++counters.ready;
            else if (resident->candidate_cpu.data != NULL ||
                     resident->upload_source != VG_ASSET_UPLOAD_NONE)
                ++counters.loading;
            else
                ++counters.failed;
            if (resident->external_refs == 0u && resident->component_refs == 0u)
                ++counters.evictable;
        }
        if (registry->releases[index].used) {
            ++counters.pending_gpu_releases;
            counters.estimated_gpu_bytes += registry->releases[index].object.estimated_bytes;
        }
    }
    for (uint32_t index = 0u; index < registry->max_leases; ++index) {
        if (registry->leases[index].active)
            ++counters.live_leases;
    }
    *out_counters = counters;
    return VG_OK;
}

void vg_asset_registry_destroy(VgContext *context) {
    if (!vg_runtime_context_valid(context) || context->assets == NULL)
        return;
    VgAssetRegistry *registry = context->assets;
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        VgAssetResident *resident = &registry->residents[index];
        if (resident->used) {
            vg_asset_destroy_cpu(context, &resident->candidate_cpu);
            vg_asset_destroy_cpu(context, &resident->active_cpu);
            if (resident->active_gpu.token != 0u && registry->gpu_attached)
                registry->gpu.release(registry->gpu.user, resident->type, resident->active_gpu);
        }
    }
    if (registry->gpu_attached && registry->gpu.is_owner_thread(registry->gpu.user)) {
        for (uint32_t index = 0u; index < registry->max_assets; ++index) {
            VgAssetPendingRelease *pending = &registry->releases[index];
            if (pending->used)
                registry->gpu.release(registry->gpu.user, pending->type, pending->object);
        }
    }
    for (uint32_t index = 0u; index < registry->max_assets; ++index) {
        if (registry->catalog[index].used)
            vg_asset_release_catalog_entry(context, &registry->catalog[index]);
    }
    vg_runtime_deallocate(context, registry->releases);
    vg_runtime_deallocate(context, registry->leases);
    vg_runtime_deallocate(context, registry->residents);
    vg_runtime_deallocate(context, registry->catalog);
    context->assets = NULL;
    vg_runtime_deallocate(context, registry);
}
