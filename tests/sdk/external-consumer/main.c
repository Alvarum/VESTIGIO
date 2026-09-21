#include <vestigio/vestigio.h>

#include <stdint.h>

typedef struct ExampleGame {
    uint32_t calls;
} ExampleGame;

static VgResult example_init(VgContext *context, void *user) {
    (void)context;
    ExampleGame *game = user;
    game->calls |= 1u;
    return VG_OK;
}

static VgResult example_world_ready(VgContext *context, VgWorld world, void *user) {
    (void)context;
    (void)world;
    ExampleGame *game = user;
    game->calls |= 2u;
    return VG_OK;
}

static void example_fixed_update(VgContext *context, VgWorld world, float dt_seconds, void *user) {
    (void)context;
    (void)world;
    (void)dt_seconds;
    ExampleGame *game = user;
    game->calls |= 4u;
}

static void example_draw_ui(VgContext *context, VgUiFrame *frame, void *user) {
    (void)context;
    (void)frame;
    ExampleGame *game = user;
    game->calls |= 8u;
}

static void example_shutdown(VgContext *context, void *user) {
    (void)context;
    ExampleGame *game = user;
    game->calls |= 16u;
}

static int test_shared_model_components(void) {
    int failure = 0;
    static const char source[] =
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
        "\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{\"attributes\":{"
        "\"POSITION\":0}}]}],\"buffers\":[{\"byteLength\":36,\"uri\":"
        "\"data:application/"
        "octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}]}";
    static const char invalid_source[] = "{not valid gltf";
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    context_desc.max_assets = 4u;
    context_desc.max_asset_leases = 8u;
    VgContext *context = 0;
    if (vg_context_create(&context_desc, &context) != VG_OK)
        do {
            failure = 10;
            goto cleanup;
        } while (0);
    if (vg_assets_enable_static_model_importer(context) != VG_OK) {
        do {
            failure = 11;
            goto cleanup;
        } while (0);
    }
    VgAssetSourceDesc catalog = {0};
    catalog.struct_size = sizeof(catalog);
    catalog.api_version = VG_API_VERSION;
    catalog.id.bytes[0] = 1u;
    catalog.type = VG_ASSET_TYPE_MESH;
    catalog.importer_version = VG_STATIC_MODEL_IMPORTER_VERSION;
    catalog.version = 1u;
    catalog.source_path = "external/static.gltf";
    catalog.source_data = source;
    catalog.source_size = sizeof(source) - 1u;
    if (vg_asset_catalog_upsert(context, &catalog) != VG_OK) {
        do {
            failure = 12;
            goto cleanup;
        } while (0);
    }
    VgAssetRequest request = {0};
    request.struct_size = sizeof(request);
    request.api_version = VG_API_VERSION;
    request.id = catalog.id;
    request.type = VG_ASSET_TYPE_MESH;
    request.required_residency = VG_ASSET_RESIDENCY_CPU;
    VgAsset asset = {0};
    if (vg_asset_acquire(context, &request, &asset) != VG_OK) {
        do {
            failure = 13;
            goto cleanup;
        } while (0);
    }
    VgWorldDesc world_desc = {sizeof(world_desc), VG_API_VERSION, 100u, 128u};
    VgWorld world = {0};
    if (vg_world_create(context, &world_desc, &world) != VG_OK) {
        do {
            failure = 14;
            goto cleanup;
        } while (0);
    }
    VgMeshRendererDesc renderer = {0};
    renderer.struct_size = sizeof(renderer);
    renderer.api_version = VG_API_VERSION;
    renderer.asset = asset;
    renderer.node_index = VG_RENDER_DEFAULT_INDEX;
    renderer.mesh_index = VG_RENDER_DEFAULT_INDEX;
    renderer.material_override = VG_RENDER_DEFAULT_INDEX;
    renderer.bounds_center = (VgVec3){0.5f, 0.5f, 0.0f};
    renderer.bounds_extent = (VgVec3){0.5f, 0.5f, 0.1f};
    VgEntity entities[100] = {{0}};
    for (uint32_t index = 0u; index < 100u; ++index) {
        if (vg_entity_create(context, world, &entities[index]) != VG_OK ||
            vg_mesh_renderer_set(context, entities[index], &renderer) != VG_OK) {
            do {
                failure = 15;
                goto cleanup;
            } while (0);
        }
    }
    VgAssetCounters counters = {0};
    counters.struct_size = sizeof(counters);
    counters.api_version = VG_API_VERSION;
    if (vg_assets_get_counters(context, &counters) != VG_OK || counters.decode_count != 1u ||
        counters.component_refs != 100u || counters.live_leases != 1u)
        do {
            failure = 16;
            goto cleanup;
        } while (0);

    catalog.version = 2u;
    catalog.source_data = invalid_source;
    catalog.source_size = sizeof(invalid_source) - 1u;
    if (vg_asset_catalog_upsert(context, &catalog) != VG_OK ||
        vg_asset_reload(context, asset) != VG_OK)
        do {
            failure = 17;
            goto cleanup;
        } while (0);
    VgAssetInfo info = {0};
    info.struct_size = sizeof(info);
    info.api_version = VG_API_VERSION;
    if (vg_asset_get_info(context, asset, &info) != VG_OK || info.published_version != 1u ||
        info.last_reload_result == VG_OK)
        do {
            failure = 18;
            goto cleanup;
        } while (0);

    if (vg_asset_release(context, asset) != VG_OK)
        do {
            failure = 19;
            goto cleanup;
        } while (0);
    for (uint32_t index = 0u; index < 99u; ++index)
        if (vg_entity_destroy(context, entities[index]) != VG_OK)
            do {
                failure = 20;
                goto cleanup;
            } while (0);
    counters.struct_size = sizeof(counters);
    counters.api_version = VG_API_VERSION;
    if (vg_assets_get_counters(context, &counters) != VG_OK || counters.component_refs != 1u)
        do {
            failure = 21;
            goto cleanup;
        } while (0);
    uint32_t purged = 99u;
    if (vg_assets_purge_unused(context, &purged) != VG_OK || purged != 0u ||
        vg_world_destroy(context, world) != VG_OK)
        do {
            failure = 22;
            goto cleanup;
        } while (0);
    counters.struct_size = sizeof(counters);
    counters.api_version = VG_API_VERSION;
    if (vg_assets_get_counters(context, &counters) != VG_OK || counters.component_refs != 0u ||
        vg_assets_purge_unused(context, &purged) != VG_OK || purged != 1u)
        do {
            failure = 23;
            goto cleanup;
        } while (0);
    failure = 0;
cleanup:
    vg_context_destroy(context);
    return failure;
}
int main(void) {
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    VgContext *context = 0;
    if (vg_context_create(&context_desc, &context) != VG_OK)
        return 1;

    VgWorldDesc world_desc = {sizeof(world_desc), VG_API_VERSION, 4u, 16u};
    VgWorld world = {0};
    VgEntity camera = {0};
    if (vg_world_create(context, &world_desc, &world) != VG_OK ||
        vg_entity_create(context, world, &camera) != VG_OK) {
        vg_context_destroy(context);
        return 2;
    }
    VgCameraDesc camera_desc = {sizeof(camera_desc),
                                VG_API_VERSION,
                                VG_CAMERA_PERSPECTIVE,
                                0u,
                                1.0471975512f,
                                0.0f,
                                0.05f,
                                500.0f};
    if (vg_camera_set(context, camera, &camera_desc) != VG_OK) {
        vg_context_destroy(context);
        return 3;
    }

    ExampleGame state = {0};
    VgGameCallbacks callbacks = {0};
    callbacks.struct_size = sizeof(callbacks);
    callbacks.api_version = VG_API_VERSION;
    callbacks.user = &state;
    callbacks.init = example_init;
    callbacks.world_ready = example_world_ready;
    callbacks.fixed_update = example_fixed_update;
    callbacks.draw_ui = example_draw_ui;
    callbacks.shutdown = example_shutdown;
    VgGame *game = 0;
    if (vg_game_create(context, 0, &callbacks, &game) != VG_OK ||
        vg_game_set_world(game, world) != VG_OK) {
        vg_context_destroy(context);
        return 4;
    }
    VgStepInfo step = {0};
    step.struct_size = sizeof(step);
    step.api_version = VG_API_VERSION;
    if (vg_game_step(game, 1.0 / 60.0, &step) != VG_OK || step.fixed_steps != 1u) {
        vg_context_destroy(context);
        return 5;
    }
    VgUiFrame frame = {sizeof(frame), VG_API_VERSION, 1u, 640u, 360u, 0.0f, 0u};
    if (vg_game_draw_ui(game, &frame) != VG_OK || vg_game_destroy(game) != VG_OK ||
        vg_world_destroy(context, world) != VG_OK) {
        vg_context_destroy(context);
        return 6;
    }
    vg_context_destroy(context);
    if (state.calls != 31u)
        return 7;
    return test_shared_model_components();
}
