#include "assets/asset_registry.h"
#include "vestigio/vestigio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression);                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct FakeDecoder {
    uint32_t calls;
    uint32_t destroys;
    VgResult failure;
} FakeDecoder;

typedef struct FakeGpu {
    bool owner;
    bool fail_upload;
    bool return_partial;
    uint32_t uploads;
    uint32_t releases;
    uint64_t next_token;
} FakeGpu;

typedef struct TrackingAllocator {
    uint64_t calls;
    uint64_t fail_call;
    uint64_t outstanding;
} TrackingAllocator;

static void *tracking_allocate(void *user, uint64_t size) {
    TrackingAllocator *allocator = user;
    ++allocator->calls;
    if (allocator->fail_call != 0u && allocator->calls == allocator->fail_call)
        return NULL;
    if (size > (uint64_t)SIZE_MAX)
        return NULL;
    void *allocation = malloc((size_t)size);
    if (allocation != NULL)
        ++allocator->outstanding;
    return allocation;
}

static void tracking_deallocate(void *user, void *allocation) {
    TrackingAllocator *allocator = user;
    if (allocation != NULL) {
        if (allocator->outstanding == 0u)
            abort();
        --allocator->outstanding;
        free(allocation);
    }
}

static VgAssetId asset_id(uint8_t seed) {
    VgAssetId id = {{0}};
    for (uint32_t index = 0u; index < sizeof(id.bytes); ++index)
        id.bytes[index] = (uint8_t)(seed + index);
    return id;
}

static VgResult fake_decode(void *user, const VgAssetMemory *memory,
                            const VgAssetSourceDesc *source, void **out_data, uint64_t *out_bytes,
                            char *error, uint32_t error_capacity) {
    FakeDecoder *decoder = user;
    ++decoder->calls;
    if (decoder->failure != VG_OK) {
        if (error_capacity > 0u)
            (void)snprintf(error, error_capacity, "injected decode failure");
        return decoder->failure;
    }
    if (source->source_size == 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    void *copy = memory->allocate(memory->user, (size_t)source->source_size);
    if (copy == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memcpy(copy, source->source_data, (size_t)source->source_size);
    *out_data = copy;
    *out_bytes = source->source_size;
    return VG_OK;
}

static void fake_destroy(void *user, const VgAssetMemory *memory, void *data) {
    FakeDecoder *decoder = user;
    ++decoder->destroys;
    memory->deallocate(memory->user, data);
}

static bool fake_is_owner(void *user) {
    return ((FakeGpu *)user)->owner;
}

static VgResult fake_upload(void *user, VgAssetType type, const void *cpu_data, uint64_t cpu_bytes,
                            VgAssetGpuObject *out_object, char *error, uint32_t error_capacity) {
    FakeGpu *gpu = user;
    ++gpu->uploads;
    if (type == 0u || cpu_data == NULL || cpu_bytes == 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    if (gpu->fail_upload) {
        if (gpu->return_partial)
            *out_object = (VgAssetGpuObject){++gpu->next_token, 17u};
        if (error_capacity > 0u)
            (void)snprintf(error, error_capacity, "injected upload failure");
        return VG_ERROR_GPU;
    }
    *out_object = (VgAssetGpuObject){++gpu->next_token, cpu_bytes * 4u};
    return VG_OK;
}

static void fake_release(void *user, VgAssetType type, VgAssetGpuObject object) {
    FakeGpu *gpu = user;
    if (type != 0u && object.token != 0u)
        ++gpu->releases;
}

static VgAssetSourceDesc source(VgAssetId id, uint64_t version, uint8_t fingerprint,
                                const void *data, uint64_t size) {
    VgAssetSourceDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    description.id = id;
    description.type = VG_ASSET_TYPE_TEXTURE;
    description.importer_version = 1u;
    description.version = version;
    description.fingerprint[0] = fingerprint;
    description.source_path = "textures/shared.texture";
    description.source_data = data;
    description.source_size = size;
    return description;
}

static VgAssetRequest request(VgAssetId id, VgAssetResidency residency) {
    VgAssetRequest value = {0};
    value.struct_size = sizeof(value);
    value.api_version = VG_API_VERSION;
    value.id = id;
    value.type = VG_ASSET_TYPE_TEXTURE;
    value.required_residency = residency;
    return value;
}

static VgAssetInfo info(VgContext *context, VgAsset asset) {
    VgAssetInfo value = {0};
    value.struct_size = sizeof(value);
    value.api_version = VG_API_VERSION;
    if (vg_asset_get_info(context, asset, &value) != VG_OK)
        value.struct_size = 0u;
    return value;
}

static VgAssetCounters counters(VgContext *context) {
    VgAssetCounters value = {0};
    value.struct_size = sizeof(value);
    value.api_version = VG_API_VERSION;
    if (vg_assets_get_counters(context, &value) != VG_OK)
        value.struct_size = 0u;
    return value;
}

static int test_shared_leases_component_and_purge(void) {
    VgContextDesc context_description = {0};
    context_description.struct_size = sizeof(context_description);
    context_description.api_version = VG_API_VERSION;
    context_description.max_assets = 8u;
    context_description.max_asset_leases = 16u;
    VgContext *context = NULL;
    VgContext *foreign_context = NULL;
    CHECK(vg_context_create(&context_description, &context) == VG_OK);
    CHECK(vg_context_create(&context_description, &foreign_context) == VG_OK);

    FakeDecoder decoder_state = {0};
    VgAssetDecoder decoder = {&decoder_state, fake_decode, fake_destroy};
    CHECK(vg_asset_set_decoder(context, VG_ASSET_TYPE_TEXTURE, &decoder) == VG_OK);
    const uint8_t bytes[] = {1u, 2u, 3u, 4u};
    VgAssetId id = asset_id(1u);
    VgAssetSourceDesc description = source(id, 1u, 1u, bytes, sizeof(bytes));
    VgAssetSourceDesc invalid_source = description;
    invalid_source.api_version = VG_API_VERSION + 1u;
    CHECK(vg_asset_catalog_upsert(context, &invalid_source) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(vg_asset_catalog_upsert(context, &description) == VG_OK);
    VgAssetSourceDesc moved_source = description;
    moved_source.source_path = "renamed/shared.texture";
    CHECK(vg_asset_catalog_upsert(context, &moved_source) == VG_OK);
    CHECK(counters(context).catalog_entries == 1u);

    VgAssetRequest acquire = request(id, VG_ASSET_RESIDENCY_GPU);
    VgAssetRequest invalid_request = acquire;
    invalid_request.api_version = VG_API_VERSION + 1u;
    VgAsset untouched = {UINT64_C(0xA55A)};
    CHECK(vg_asset_acquire(context, &invalid_request, &untouched) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(untouched.value == UINT64_C(0xA55A));
    VgAsset first = {0};
    VgAsset second = {0};
    CHECK(vg_asset_acquire(context, &acquire, &first) == VG_OK);
    CHECK(vg_asset_acquire(context, &acquire, &second) == VG_OK);
    CHECK(first.value != second.value);
    CHECK(decoder_state.calls == 1u);
    VgAssetCounters state = counters(context);
    CHECK(state.struct_size != 0u);
    CHECK(state.catalog_entries == 1u && state.resident_entries == 1u);
    CHECK(state.live_leases == 2u && state.cache_hit_count == 1u);
    CHECK(info(context, first).state == VG_ASSET_LOADING);

    VgAssetInfo wrong = {0};
    wrong.struct_size = sizeof(wrong);
    wrong.api_version = VG_API_VERSION;
    CHECK(vg_asset_get_info(foreign_context, first, &wrong) == VG_ERROR_WRONG_CONTEXT);
    VgWorld world = {0};
    CHECK(vg_world_create(context, NULL, &world) == VG_OK);
    CHECK(vg_asset_get_info(context, (VgAsset){world.value}, &wrong) == VG_ERROR_WRONG_TYPE);
    wrong.api_version = VG_API_VERSION + 1u;
    wrong.flags = UINT32_C(0xA5A5);
    CHECK(vg_asset_get_info(context, first, &wrong) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(wrong.flags == UINT32_C(0xA5A5));
    VgAssetCounters invalid_counters = {0};
    invalid_counters.struct_size = sizeof(invalid_counters);
    invalid_counters.api_version = VG_API_VERSION + 1u;
    invalid_counters.catalog_entries = UINT32_C(0x5A5A);
    CHECK(vg_assets_get_counters(context, &invalid_counters) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(invalid_counters.catalog_entries == UINT32_C(0x5A5A));

    FakeGpu gpu = {false, false, false, 0u, 0u, 40u};
    VgAssetGpuExecutor executor = {&gpu, fake_is_owner, fake_upload, fake_release};
    CHECK(vg_asset_attach_gpu(context, &executor) == VG_ERROR_WRONG_THREAD);
    gpu.owner = true;
    CHECK(vg_asset_attach_gpu(context, &executor) == VG_OK);
    gpu.owner = false;
    CHECK(vg_asset_flush_gpu(context) == VG_ERROR_WRONG_THREAD);
    CHECK(gpu.uploads == 0u);
    gpu.owner = true;
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    CHECK(gpu.uploads == 1u);
    VgAssetInfo ready = info(context, first);
    CHECK(ready.state == VG_ASSET_READY);
    CHECK((ready.flags & VG_ASSET_INFO_GPU_RESIDENT) != 0u);

    VgAsset clone = {0};
    CHECK(vg_asset_clone(context, first, &clone) == VG_OK);
    VgAssetRef component = {0};
    CHECK(vg_asset_component_retain(context, first, &component) == VG_OK);
    CHECK(vg_asset_release(context, first) == VG_OK);
    CHECK(vg_asset_release(context, first) == VG_ERROR_INVALID_HANDLE);
    CHECK(info(context, second).state == VG_ASSET_READY);
    CHECK(vg_asset_release(context, second) == VG_OK);
    CHECK(vg_asset_release(context, clone) == VG_OK);
    uint32_t purged = 99u;
    CHECK(vg_assets_purge_unused(context, &purged) == VG_OK);
    CHECK(purged == 0u);
    CHECK(vg_asset_component_release(context, component) == VG_OK);
    CHECK(vg_asset_component_release(context, component) == VG_ERROR_INVALID_HANDLE);
    CHECK(vg_assets_purge_unused(context, &purged) == VG_OK);
    CHECK(purged == 1u);
    state = counters(context);
    CHECK(state.resident_entries == 0u && state.pending_gpu_releases == 1u);
    CHECK(state.estimated_gpu_bytes != 0u);
    CHECK(vg_asset_detach_gpu(context) == VG_OK);
    state = counters(context);
    CHECK(state.pending_gpu_releases == 0u && state.estimated_gpu_bytes == 0u);
    CHECK(gpu.releases == 1u);
    CHECK(decoder_state.destroys == 1u);

    CHECK(vg_world_destroy(context, world) == VG_OK);
    vg_context_destroy(foreign_context);
    vg_context_destroy(context);
    return 0;
}

static int test_transactional_reload_and_failures(void) {
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    description.max_assets = 8u;
    description.max_asset_leases = 16u;
    VgContext *context = NULL;
    CHECK(vg_context_create(&description, &context) == VG_OK);
    FakeDecoder decoder_state = {0};
    VgAssetDecoder decoder = {&decoder_state, fake_decode, fake_destroy};
    CHECK(vg_asset_set_decoder(context, VG_ASSET_TYPE_TEXTURE, &decoder) == VG_OK);
    FakeGpu gpu = {true, false, false, 0u, 0u, 100u};
    VgAssetGpuExecutor executor = {&gpu, fake_is_owner, fake_upload, fake_release};
    CHECK(vg_asset_attach_gpu(context, &executor) == VG_OK);

    const uint8_t v1[] = {10u, 11u};
    const uint8_t v2[] = {20u, 21u, 22u};
    VgAssetId id = asset_id(20u);
    VgAssetSourceDesc catalog = source(id, 1u, 1u, v1, sizeof(v1));
    CHECK(vg_asset_catalog_upsert(context, &catalog) == VG_OK);
    VgAssetRequest acquire = request(id, VG_ASSET_RESIDENCY_GPU);
    VgAsset asset = {0};
    CHECK(vg_asset_acquire(context, &acquire, &asset) == VG_OK);
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    CHECK(info(context, asset).published_version == 1u);
    uint64_t original_gpu_bytes = info(context, asset).estimated_gpu_bytes;

    catalog = source(id, 2u, 2u, v2, sizeof(v2));
    CHECK(vg_asset_catalog_upsert(context, &catalog) == VG_OK);
    decoder_state.failure = VG_ERROR_FORMAT_VERSION;
    CHECK(vg_asset_reload(context, asset) == VG_OK);
    VgAssetInfo after_parse_failure = info(context, asset);
    CHECK(after_parse_failure.state == VG_ASSET_READY);
    CHECK(after_parse_failure.published_version == 1u);
    CHECK(after_parse_failure.last_reload_result == VG_ERROR_FORMAT_VERSION);
    CHECK(after_parse_failure.estimated_gpu_bytes == original_gpu_bytes);
    char error[64] = {0};
    uint32_t required = 0u;
    CHECK(vg_asset_get_error(context, asset, error, sizeof(error), &required) == VG_OK);
    CHECK(required > 1u && strstr(error, "decode") != NULL);

    decoder_state.failure = VG_OK;
    catalog = source(id, 3u, 3u, v2, sizeof(v2));
    CHECK(vg_asset_catalog_upsert(context, &catalog) == VG_OK);
    CHECK(vg_asset_reload(context, asset) == VG_OK);
    CHECK((info(context, asset).flags & VG_ASSET_INFO_RELOAD_PENDING) != 0u);
    gpu.fail_upload = true;
    gpu.return_partial = true;
    uint32_t releases_before = gpu.releases;
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    VgAssetInfo after_gpu_failure = info(context, asset);
    CHECK(after_gpu_failure.state == VG_ASSET_READY);
    CHECK(after_gpu_failure.published_version == 1u);
    CHECK(after_gpu_failure.last_reload_result == VG_ERROR_GPU);
    CHECK(gpu.releases == releases_before + 1u);

    gpu.fail_upload = false;
    gpu.return_partial = false;
    catalog = source(id, 4u, 4u, v2, sizeof(v2));
    CHECK(vg_asset_catalog_upsert(context, &catalog) == VG_OK);
    CHECK(vg_asset_reload(context, asset) == VG_OK);
    releases_before = gpu.releases;
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    VgAssetInfo replaced = info(context, asset);
    CHECK(replaced.state == VG_ASSET_READY && replaced.published_version == 4u);
    CHECK(replaced.derived_ram_bytes == sizeof(v2));
    CHECK(gpu.releases == releases_before + 1u);

    decoder_state.failure = VG_ERROR_OUT_OF_MEMORY;
    catalog = source(id, 5u, 5u, v1, sizeof(v1));
    CHECK(vg_asset_catalog_upsert(context, &catalog) == VG_OK);
    CHECK(vg_asset_reload(context, asset) == VG_OK);
    CHECK(info(context, asset).published_version == 4u);

    VgAssetId failed_id = asset_id(80u);
    catalog = source(failed_id, 1u, 1u, v1, sizeof(v1));
    CHECK(vg_asset_catalog_upsert(context, &catalog) == VG_OK);
    VgAssetRequest failed_request = request(failed_id, VG_ASSET_RESIDENCY_CPU);
    VgAsset failed_asset = {0};
    CHECK(vg_asset_acquire(context, &failed_request, &failed_asset) == VG_OK);
    CHECK(info(context, failed_asset).state == VG_ASSET_FAILED);

    CHECK(vg_asset_release(context, failed_asset) == VG_OK);
    CHECK(vg_asset_release(context, asset) == VG_OK);
    uint32_t purged = 0u;
    CHECK(vg_assets_purge_unused(context, &purged) == VG_OK);
    CHECK(purged == 2u);
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    VgAssetCounters final = counters(context);
    CHECK(final.resident_entries == 0u && final.live_leases == 0u);
    CHECK(final.derived_ram_bytes == 0u && final.staging_ram_bytes == 0u);
    CHECK(final.estimated_gpu_bytes == 0u);
    vg_context_destroy(context);
    return 0;
}

static int test_context_destroy_requires_gpu_owner(void) {
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    description.max_assets = 2u;
    description.max_asset_leases = 2u;
    VgContext *context = NULL;
    CHECK(vg_context_create(&description, &context) == VG_OK);
    FakeDecoder decoder_state = {0};
    VgAssetDecoder decoder = {&decoder_state, fake_decode, fake_destroy};
    CHECK(vg_asset_set_decoder(context, VG_ASSET_TYPE_TEXTURE, &decoder) == VG_OK);
    FakeGpu gpu = {true, false, false, 0u, 0u, 500u};
    VgAssetGpuExecutor executor = {&gpu, fake_is_owner, fake_upload, fake_release};
    CHECK(vg_asset_attach_gpu(context, &executor) == VG_OK);
    const uint8_t bytes[] = {8u, 9u};
    VgAssetId id = asset_id(120u);
    VgAssetSourceDesc catalog = source(id, 1u, 1u, bytes, sizeof(bytes));
    CHECK(vg_asset_catalog_upsert(context, &catalog) == VG_OK);
    VgAssetRequest acquire = request(id, VG_ASSET_RESIDENCY_GPU);
    VgAsset asset = {0};
    CHECK(vg_asset_acquire(context, &acquire, &asset) == VG_OK);
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    CHECK(info(context, asset).state == VG_ASSET_READY);

    gpu.owner = false;
    vg_context_destroy(context);
    CHECK(gpu.releases == 0u && decoder_state.destroys == 0u);
    CHECK(info(context, asset).state == VG_ASSET_READY);
    gpu.owner = true;
    vg_context_destroy(context);
    CHECK(gpu.releases == 1u && decoder_state.destroys == 1u);
    return 0;
}

static int test_registry_and_catalog_oom_are_transactional(void) {
    for (uint64_t failure = 1u; failure <= 5u; ++failure) {
        TrackingAllocator allocator = {0};
        VgContextDesc description = {0};
        description.struct_size = sizeof(description);
        description.api_version = VG_API_VERSION;
        description.allocator_user = &allocator;
        description.allocate = tracking_allocate;
        description.deallocate = tracking_deallocate;
        description.max_assets = 2u;
        description.max_asset_leases = 2u;
        VgContext *context = NULL;
        CHECK(vg_context_create(&description, &context) == VG_OK);
        FakeDecoder decoder_state = {0};
        VgAssetDecoder decoder = {&decoder_state, fake_decode, fake_destroy};
        allocator.fail_call = allocator.calls + failure;
        CHECK(vg_asset_set_decoder(context, VG_ASSET_TYPE_TEXTURE, &decoder) ==
              VG_ERROR_OUT_OF_MEMORY);
        CHECK(allocator.outstanding == 2u);
        vg_context_destroy(context);
        CHECK(allocator.outstanding == 0u);
    }

    TrackingAllocator allocator = {0};
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    description.allocator_user = &allocator;
    description.allocate = tracking_allocate;
    description.deallocate = tracking_deallocate;
    description.max_assets = 2u;
    description.max_asset_leases = 2u;
    VgContext *context = NULL;
    CHECK(vg_context_create(&description, &context) == VG_OK);
    FakeDecoder decoder_state = {0};
    VgAssetDecoder decoder = {&decoder_state, fake_decode, fake_destroy};
    CHECK(vg_asset_set_decoder(context, VG_ASSET_TYPE_TEXTURE, &decoder) == VG_OK);
    const uint8_t old_bytes[] = {1u, 2u};
    const uint8_t new_bytes[] = {3u, 4u, 5u};
    VgAssetId id = asset_id(150u);
    VgAssetSourceDesc old_source = source(id, 1u, 1u, old_bytes, sizeof(old_bytes));
    CHECK(vg_asset_catalog_upsert(context, &old_source) == VG_OK);
    allocator.fail_call = allocator.calls + 1u;
    VgAssetSourceDesc new_source = source(id, 2u, 2u, new_bytes, sizeof(new_bytes));
    CHECK(vg_asset_catalog_upsert(context, &new_source) == VG_ERROR_OUT_OF_MEMORY);
    allocator.fail_call = 0u;
    VgAssetRequest acquire = request(id, VG_ASSET_RESIDENCY_CPU);
    VgAsset asset = {0};
    CHECK(vg_asset_acquire(context, &acquire, &asset) == VG_OK);
    VgAssetInfo loaded = info(context, asset);
    CHECK(loaded.state == VG_ASSET_READY && loaded.published_version == 1u);
    CHECK(loaded.derived_ram_bytes == sizeof(old_bytes));
    CHECK(vg_asset_release(context, asset) == VG_OK);
    uint32_t purged = 0u;
    CHECK(vg_assets_purge_unused(context, &purged) == VG_OK && purged == 1u);
    vg_context_destroy(context);
    CHECK(allocator.outstanding == 0u);
    return 0;
}

int main(void) {
    CHECK(test_shared_leases_component_and_purge() == 0);
    CHECK(test_transactional_reload_and_failures() == 0);
    CHECK(test_context_destroy_requires_gpu_owner() == 0);
    CHECK(test_registry_and_catalog_oom_are_transactional() == 0);
    puts("PASS asset IDs, leases, dedupe, transactional CPU/GPU ownership and purge");
    return 0;
}
