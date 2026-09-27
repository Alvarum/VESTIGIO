#include "assets/asset_registry.h"
#include "content/document_runtime.h"
#include "tooling/tool_api.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VG_CONTENT_FIXTURES
#define VG_CONTENT_FIXTURES "tests/content/fixtures"
#endif

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);    \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct ResolverState {
    size_t calls;
    size_t fail_call;
} ResolverState;

static void *decode_allocate(void *user, uint64_t size) {
    (void)user;
    return size <= (uint64_t)SIZE_MAX ? malloc((size_t)size) : NULL;
}

static void decode_deallocate(void *user, void *allocation) {
    (void)user;
    free(allocation);
}

static VgResult decode_mesh(void *user, const VgAssetMemory *memory,
                            const VgAssetSourceDesc *source, void **out_data, uint64_t *out_bytes,
                            char *error, uint32_t error_capacity) {
    (void)user;
    (void)error;
    (void)error_capacity;
    void *data = memory->allocate(memory->user, source->source_size);
    if (data == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memcpy(data, source->source_data, (size_t)source->source_size);
    *out_data = data;
    *out_bytes = source->source_size;
    return VG_OK;
}

static void destroy_mesh(void *user, const VgAssetMemory *memory, void *data) {
    (void)user;
    memory->deallocate(memory->user, data);
}

static VgResult resolve_mesh(void *user, VgContext *context, VgAssetId id, VgAssetType type,
                             VgAsset *out_asset) {
    ResolverState *state = user;
    ++state->calls;
    if (state->fail_call != 0u && state->calls == state->fail_call)
        return VG_ERROR_NOT_FOUND;
    VgAssetRequest request = {0};
    request.struct_size = sizeof(request);
    request.api_version = VG_API_VERSION;
    request.id = id;
    request.type = type;
    request.required_residency = VG_ASSET_RESIDENCY_CPU;
    return vg_asset_acquire(context, &request, out_asset);
}

static bool parse_uuid(const char *text, uint8_t output[16]) {
    size_t output_index = 0u;
    for (size_t index = 0u; text[index] != '\0';) {
        if (text[index] == '-') {
            ++index;
            continue;
        }
        if (output_index == 16u)
            return false;
        char high_text = text[index++];
        char low_text = text[index++];
        int high = high_text <= '9' ? high_text - '0' : high_text - 'a' + 10;
        int low = low_text <= '9' ? low_text - '0' : low_text - 'a' + 10;
        output[output_index++] = (uint8_t)((high << 4) | low);
    }
    return output_index == 16u;
}

static VgDocument *open_fixture(void) {
    char path[1024];
    (void)snprintf(path, sizeof(path), "%s/valid-level.json", VG_CONTENT_FIXTURES);
    VgDocument *document = NULL;
    VgDocumentDiagnostic diagnostic;
    CHECK(vg_document_open_file(path, &document, &diagnostic));
    return document;
}

static VgContext *create_context(uint32_t max_worlds) {
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    description.allocate = decode_allocate;
    description.deallocate = decode_deallocate;
    description.max_worlds = max_worlds;
    description.max_assets = 4u;
    description.max_asset_leases = 4u;
    VgContext *context = NULL;
    CHECK(vg_context_create(&description, &context) == VG_OK);
    if (context == NULL)
        return NULL;

    VgAssetDecoder decoder = {0};
    decoder.decode = decode_mesh;
    decoder.destroy = destroy_mesh;
    CHECK(vg_asset_set_decoder(context, VG_ASSET_TYPE_MESH, &decoder) == VG_OK);
    static const uint8_t mesh_bytes[] = {1u, 2u, 3u, 4u};
    VgAssetSourceDesc source = {0};
    source.struct_size = sizeof(source);
    source.api_version = VG_API_VERSION;
    CHECK(parse_uuid("50000000-0000-0000-0000-000000000001", source.id.bytes));
    source.type = VG_ASSET_TYPE_MESH;
    source.importer_version = 1u;
    source.version = 1u;
    source.source_path = "models/test.mesh";
    source.source_data = mesh_bytes;
    source.source_size = sizeof(mesh_bytes);
    CHECK(vg_asset_catalog_upsert(context, &source) == VG_OK);
    return context;
}

static VgAssetCounters counters(VgContext *context) {
    VgAssetCounters value = {0};
    value.struct_size = sizeof(value);
    value.api_version = VG_API_VERSION;
    CHECK(vg_assets_get_counters(context, &value) == VG_OK);
    return value;
}

static void test_complete_instance_and_cleanup(void) {
    VgDocument *document = open_fixture();
    VgContext *context = create_context(1u);
    if (document == NULL || context == NULL) {
        vg_document_destroy(document);
        vg_context_destroy(context);
        return;
    }
    ResolverState resolver = {0};
    VgDocumentInstanceDesc description = {resolve_mesh, &resolver};
    VgDocumentInstance *instance = NULL;
    VgDocumentDiagnostic diagnostic;
    CHECK(vg_document_instantiate(context, document, &description, &instance, &diagnostic) ==
          VG_OK);
    CHECK(instance != NULL && resolver.calls == 1u);
    CHECK(vg_document_instance_entity_count(instance) == 2u);
    CHECK(vg_document_instance_asset_count(instance) == 1u);
    CHECK(counters(context).live_leases == 1u);
    VgUuid first_id = {{0}};
    VgEntity first_entity = {0};
    CHECK(vg_document_instance_entity_at(instance, 0u, &first_id, &first_entity));
    CHECK(!vg_document_instance_entity_at(instance, 2u, &first_id, &first_entity));

    VgUuid camera_id = {{0}};
    VgUuid child_id = {{0}};
    CHECK(parse_uuid("40000000-0000-0000-0000-000000000002", camera_id.bytes));
    CHECK(parse_uuid("40000000-0000-0000-0000-000000000001", child_id.bytes));
    VgEntity camera = {0};
    VgEntity child = {0};
    CHECK(vg_document_instance_find_entity(instance, camera_id, &camera));
    CHECK(vg_document_instance_find_entity(instance, child_id, &child));
    CHECK(memcmp(first_id.bytes, camera_id.bytes, sizeof(first_id.bytes)) == 0);
    CHECK(first_entity.value == camera.value);
    VgEntity parent = {0};
    CHECK(vg_entity_get_parent(context, child, &parent) == VG_OK && parent.value == camera.value);
    VgTransform transform;
    CHECK(vg_entity_get_local_transform(context, child, &transform) == VG_OK);
    CHECK(fabsf(transform.position.x - 1.25f) < 0.0001f &&
          fabsf(transform.position.y + 2.5f) < 0.0001f &&
          fabsf(transform.scale.y - 2.0f) < 0.0001f);
    VgCameraDesc camera_description = {0};
    camera_description.struct_size = sizeof(camera_description);
    camera_description.api_version = VG_API_VERSION;
    CHECK(vg_camera_get(context, camera, &camera_description) == VG_OK);
    CHECK(fabsf(camera_description.vertical_fov_radians - 1.0471976f) < 0.0001f);

    VgMeshRendererDesc renderer = {0};
    renderer.struct_size = sizeof(renderer);
    renderer.api_version = VG_API_VERSION;
    CHECK(vg_mesh_renderer_get(context, child, &renderer) == VG_OK);
    CHECK(renderer.node_index == 0u);
    CHECK(renderer.mesh_index == VG_RENDER_DEFAULT_INDEX);
    CHECK(renderer.material_override == VG_RENDER_DEFAULT_INDEX);
    CHECK(vg_asset_release(context, renderer.asset) == VG_OK);
    VgSpatialScene *spatial = vg_document_instance_spatial(instance);
    CHECK(spatial != NULL);
    CHECK(vg_spatial_scene_stats(spatial).colliders == 1u);
    VgTransform child_world;
    CHECK(vg_entity_get_world_transform(context, child, &child_world) == VG_OK);
    VgSpatialRayQuery ray = {0};
    ray.origin = (VgVec3){child_world.position.x, child_world.position.y,
                          child_world.position.z + 5.0f};
    ray.direction = (VgVec3){0.0f, 0.0f, -1.0f};
    ray.max_distance = 10.0f;
    ray.layer_mask = UINT64_C(1);
    bool hit = false;
    VgSpatialHit contact = {0};
    CHECK(vg_spatial_raycast(spatial, &ray, &hit, &contact) == VG_OK);
    CHECK(hit && contact.entity.value == child.value);

    vg_document_instance_destroy(instance);
    CHECK(counters(context).live_leases == 0u);
    VgWorldDesc world_description = {sizeof(world_description), VG_API_VERSION, 1u, 1u};
    VgWorld proof = {0};
    CHECK(vg_world_create(context, &world_description, &proof) == VG_OK);
    CHECK(vg_world_destroy(context, proof) == VG_OK);
    vg_document_destroy(document);
    vg_context_destroy(context);
}

static void test_mesh_node_and_instance_isolation(void) {
    VgDocument *document = open_fixture();
    VgContext *context = create_context(2u);
    if (document == NULL || context == NULL) {
        vg_document_destroy(document);
        vg_context_destroy(context);
        return;
    }
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *batch = NULL;
    CHECK(vg_tool_begin(document, 1u, &batch, &diagnostic));
    CHECK(vg_tool_set_component(batch, "40000000-0000-0000-0000-000000000001", "engine.mesh",
                                "{\"version\":1,\"asset\":\"50000000-0000-0000-0000-000000000001\",\"node_index\":2}",
                                &diagnostic));
    CHECK(vg_tool_commit(batch, NULL, &diagnostic));
    ResolverState resolver = {0};
    VgDocumentInstanceDesc description = {resolve_mesh, &resolver};
    VgDocumentInstance *first = NULL;
    VgDocumentInstance *second = NULL;
    CHECK(vg_document_instantiate(context, document, &description, &first, &diagnostic) == VG_OK);
    CHECK(vg_document_instantiate(context, document, &description, &second, &diagnostic) == VG_OK);
    CHECK(first != NULL && second != NULL && first != second);
    if (first != NULL && second != NULL) {
        VgUuid id = {{0}};
        VgEntity entity = {0};
        CHECK(parse_uuid("40000000-0000-0000-0000-000000000001", id.bytes));
        CHECK(vg_document_instance_find_entity(second, id, &entity));
        VgMeshRendererDesc renderer = {0};
        renderer.struct_size = sizeof(renderer);
        renderer.api_version = VG_API_VERSION;
        CHECK(vg_mesh_renderer_get(context, entity, &renderer) == VG_OK);
        CHECK(renderer.node_index == 2u);
        CHECK(vg_asset_release(context, renderer.asset) == VG_OK);
        CHECK(vg_document_instance_spatial(first) != vg_document_instance_spatial(second));
        CHECK(vg_spatial_scene_stats(vg_document_instance_spatial(second)).colliders == 1u);
    }
    vg_document_instance_destroy(first);
    if (second != NULL)
        CHECK(vg_spatial_scene_stats(vg_document_instance_spatial(second)).colliders == 1u);
    vg_document_instance_destroy(second);
    CHECK(counters(context).live_leases == 0u);
    vg_document_destroy(document);
    vg_context_destroy(context);
}

static void test_atrium_document(void) {
    char path[1024];
    (void)snprintf(path, sizeof(path), "%s/../../../assets/demo/atrium.level.json",
                   VG_CONTENT_FIXTURES);
    VgDocument *document = NULL;
    VgDocumentDiagnostic diagnostic;
    CHECK(vg_document_open_file(path, &document, &diagnostic));
    VgContext *context = create_context(1u);
    if (document == NULL || context == NULL) {
        vg_document_destroy(document);
        vg_context_destroy(context);
        return;
    }
    VgAssetSourceDesc source = {0};
    static const uint8_t mesh_bytes[] = {1u, 2u, 3u, 4u};
    source.struct_size = sizeof(source);
    source.api_version = VG_API_VERSION;
    CHECK(parse_uuid("4a30312d-6174-7269-756d-2d6d6f64656c", source.id.bytes));
    source.type = VG_ASSET_TYPE_MESH;
    source.importer_version = 1u;
    source.version = 1u;
    source.source_path = "demo/atrium.gltf";
    source.source_data = mesh_bytes;
    source.source_size = sizeof(mesh_bytes);
    CHECK(vg_asset_catalog_upsert(context, &source) == VG_OK);
    ResolverState resolver = {0};
    VgDocumentInstanceDesc description = {resolve_mesh, &resolver};
    VgDocumentInstance *instance = NULL;
    CHECK(vg_document_instantiate(context, document, &description, &instance, &diagnostic) ==
          VG_OK);
    if (instance != NULL) {
        CHECK(vg_document_instance_entity_count(instance) == 14u);
        CHECK(vg_document_instance_asset_count(instance) == 1u);
        CHECK(resolver.calls == 1u);
        CHECK(vg_spatial_scene_stats(vg_document_instance_spatial(instance)).colliders == 13u);
        VgUuid camera_id = {{0}};
        VgUuid floor_id = {{0}};
        CHECK(parse_uuid("60000000-0000-0000-0000-000000000001", camera_id.bytes));
        CHECK(parse_uuid("60000000-0000-0000-0000-000000000002", floor_id.bytes));
        VgEntity camera = {0};
        VgEntity floor = {0};
        CHECK(vg_document_instance_find_entity(instance, camera_id, &camera));
        CHECK(vg_document_instance_find_entity(instance, floor_id, &floor));
        VgCameraDesc camera_description = {0};
        camera_description.struct_size = sizeof(camera_description);
        camera_description.api_version = VG_API_VERSION;
        CHECK(vg_camera_get(context, camera, &camera_description) == VG_OK);
        VgMeshRendererDesc renderer = {0};
        renderer.struct_size = sizeof(renderer);
        renderer.api_version = VG_API_VERSION;
        CHECK(vg_mesh_renderer_get(context, floor, &renderer) == VG_OK);
        CHECK(renderer.node_index == 0u);
        CHECK(vg_asset_release(context, renderer.asset) == VG_OK);
        VgSpatialRayQuery ray = {0};
        ray.origin = (VgVec3){0.0f, -7.0f, 2.0f};
        ray.direction = (VgVec3){0.0f, 0.0f, -1.0f};
        ray.max_distance = 4.0f;
        ray.layer_mask = UINT64_C(1);
        bool hit = false;
        VgSpatialHit contact = {0};
        CHECK(vg_spatial_raycast(vg_document_instance_spatial(instance), &ray, &hit,
                                 &contact) == VG_OK);
        CHECK(hit && contact.entity.value == floor.value);
    }
    vg_document_instance_destroy(instance);
    CHECK(counters(context).live_leases == 0u);
    vg_document_destroy(document);
    vg_context_destroy(context);
}

static void test_resolver_failure_rolls_back_everything(void) {
    VgDocument *document = open_fixture();
    VgContext *context = create_context(1u);
    if (document == NULL || context == NULL) {
        vg_document_destroy(document);
        vg_context_destroy(context);
        return;
    }
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *batch = NULL;
    CHECK(vg_tool_begin(document, 1u, &batch, &diagnostic));
    CHECK(vg_tool_duplicate_entity(batch, "40000000-0000-0000-0000-000000000001", "$copy", NULL,
                                   &diagnostic));
    CHECK(vg_tool_set_component(
        batch, "$copy", "engine.mesh",
        "{\"version\":1,\"asset\":\"50000000-0000-0000-0000-000000000002\"}", &diagnostic));
    CHECK(vg_tool_commit(batch, NULL, &diagnostic));

    ResolverState resolver = {0};
    resolver.fail_call = 2u;
    VgDocumentInstanceDesc description = {resolve_mesh, &resolver};
    VgDocumentInstance *sentinel = (VgDocumentInstance *)(uintptr_t)1u;
    CHECK(vg_document_instantiate(context, document, &description, &sentinel, &diagnostic) ==
          VG_ERROR_NOT_FOUND);
    CHECK(sentinel == (VgDocumentInstance *)(uintptr_t)1u);
    CHECK(resolver.calls == 2u);
    CHECK(counters(context).live_leases == 0u);

    VgWorldDesc world_description = {sizeof(world_description), VG_API_VERSION, 1u, 1u};
    VgWorld proof = {0};
    CHECK(vg_world_create(context, &world_description, &proof) == VG_OK);
    CHECK(vg_world_destroy(context, proof) == VG_OK);
    vg_document_destroy(document);
    vg_context_destroy(context);
}

static void test_unsupported_collider_rolls_back(void) {
    VgDocument *document = open_fixture();
    VgContext *context = create_context(1u);
    if (document == NULL || context == NULL) {
        vg_document_destroy(document);
        vg_context_destroy(context);
        return;
    }
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *batch = NULL;
    CHECK(vg_tool_begin(document, 1u, &batch, &diagnostic));
    CHECK(vg_tool_set_component(
        batch, "40000000-0000-0000-0000-000000000001", "engine.collider",
        "{\"version\":1,\"shape\":\"box\",\"motion\":\"kinematic\",\"center\":[0,0,0],\"half_extents\":[0.5,0.5,0.5]}",
        &diagnostic));
    CHECK(vg_tool_commit(batch, NULL, &diagnostic));
    ResolverState resolver = {0};
    VgDocumentInstanceDesc description = {resolve_mesh, &resolver};
    VgDocumentInstance *sentinel = (VgDocumentInstance *)(uintptr_t)1u;
    CHECK(vg_document_instantiate(context, document, &description, &sentinel, &diagnostic) ==
          VG_ERROR_INVALID_ARGUMENT);
    CHECK(sentinel == (VgDocumentInstance *)(uintptr_t)1u);
    CHECK(strstr(diagnostic.path, "engine.collider.motion") != NULL);
    CHECK(counters(context).live_leases == 0u);
    VgWorldDesc world_description = {sizeof(world_description), VG_API_VERSION, 1u, 1u};
    VgWorld proof = {0};
    CHECK(vg_world_create(context, &world_description, &proof) == VG_OK);
    CHECK(vg_world_destroy(context, proof) == VG_OK);
    vg_document_destroy(document);
    vg_context_destroy(context);
}

int main(void) {
    test_complete_instance_and_cleanup();
    test_mesh_node_and_instance_isolation();
    test_atrium_document();
    test_resolver_failure_rolls_back_everything();
    test_unsupported_collider_rolls_back();
    if (failures != 0)
        (void)fprintf(stderr, "%d document runtime checks failed\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
