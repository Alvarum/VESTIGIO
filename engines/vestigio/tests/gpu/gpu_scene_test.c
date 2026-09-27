#include "assets/asset_registry.h"
#include "assets/import/gltf_import.h"
#include "render/gpu_raylib/gpu_renderer.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
typedef void *TestThreadHandle;
typedef unsigned long TestThreadResult;
typedef TestThreadResult(__stdcall *TestThreadFunction)(void *);
__declspec(dllimport) TestThreadHandle __stdcall CreateThread(void *attributes, size_t stack_size,
                                                              TestThreadFunction function,
                                                              void *argument, unsigned long flags,
                                                              unsigned long *thread_id);
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(TestThreadHandle handle,
                                                                  unsigned long milliseconds);
__declspec(dllimport) int __stdcall CloseHandle(TestThreadHandle handle);
#else
#include <pthread.h>
#endif

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static void *test_allocate(void *user, uint64_t size) {
    (void)user;
    return size <= SIZE_MAX ? malloc((size_t)size) : NULL;
}

static void test_deallocate(void *user, void *data) {
    (void)user;
    free(data);
}

static int read_file(const char *path, void **out_data, uint64_t *out_size) {
    FILE *file = fopen(path, "rb");
    long size;
    void *data;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    data = malloc((size_t)size);
    if (data == NULL || fread(data, 1u, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return 0;
    }
    fclose(file);
    *out_data = data;
    *out_size = (uint64_t)size;
    return 1;
}

static bool capture_has_scene(const char *path) {
    Image image = LoadImage(path);
    if (image.data == NULL || image.width != 640 || image.height != 360) {
        UnloadImage(image);
        return false;
    }
    Color *pixels = LoadImageColors(image);
    uint64_t wire = 0u, error_magenta = 0u, error_dark = 0u;
    uint64_t mask = 0u, opaque = 0u, blend = 0u;
    if (pixels != NULL) {
        for (uint32_t y = 0u; y < 360u; ++y) {
            for (uint32_t x = 0u; x < 640u; ++x) {
                Color pixel = pixels[(uint64_t)y * 640u + x];
                if (x >= 150u && x < 215u && y >= 70u && y < 130u && pixel.g > 100u &&
                    pixel.r < 80u && pixel.b < 100u)
                    ++wire;
                if (x >= 170u && x < 270u && y >= 175u && y < 275u) {
                    if (pixel.r > 180u && pixel.g < 80u && pixel.b > 180u)
                        ++error_magenta;
                    if (pixel.r >= 10u && pixel.r <= 16u && pixel.g >= 10u && pixel.g <= 16u &&
                        pixel.b >= 10u && pixel.b <= 16u)
                        ++error_dark;
                }
                if (x >= 210u && x < 290u && y >= 130u && y < 205u && pixel.r < 5u &&
                    pixel.g > 245u && pixel.b < 5u && pixel.a == 255u)
                    ++mask;
                if (x >= 285u && x < 355u && y >= 115u && y < 190u && pixel.r < 5u &&
                    pixel.g > 245u && pixel.b < 5u && pixel.a == 255u)
                    ++opaque;
                if (x >= 340u && x < 425u && y >= 120u && y < 210u && pixel.g > 50u &&
                    pixel.a < 240u)
                    ++blend;
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return wire > 30u && wire < 300u && error_magenta > 500u && error_dark > 500u && mask > 1000u &&
           opaque > 1000u && blend > 500u;
}
static uint64_t expected_transparent_hash(void) {
    uint64_t hash = 1469598103934665603ull;
    const uint32_t indices[3] = {1u, 2u, 3u};
    for (uint32_t index = 0u; index < 3u; ++index) {
        uint64_t value = (1ull << 32u) | indices[index];
        hash ^= value;
        hash *= 1099511628211ull;
    }
    return hash;
}
static VgAssetId model_id(void) {
    VgAssetId id = {{0x47, 0x30, 0x33, 0x2d, 0x67, 0x70, 0x75, 0x2d, 0x6d, 0x6f, 0x64, 0x65, 0x6c,
                     0x2d, 0x30, 0x31}};
    return id;
}

static VgAssetCounters asset_counters(VgContext *context) {
    VgAssetCounters counters = {0};
    counters.struct_size = sizeof(counters);
    counters.api_version = VG_API_VERSION;
    if (vg_assets_get_counters(context, &counters) != VG_OK)
        counters.struct_size = 0u;
    return counters;
}

static VgTransform instance_transform(float x, float y, float z) {
    return (VgTransform){{x, y, z}, {0, 0, 0, 1}, {1, 1, 1}};
}

typedef struct WrongThreadProbe {
    VgGpuRenderer *renderer;
    VgAssetGpuExecutor executor;
    VgAssetGpuObject object;
    VgGpuCamera camera;
    bool demo;
    bool capture;
    bool info;
    bool shader;
    bool token_alive;
    bool scene;
} WrongThreadProbe;

#if defined(_WIN32)
static TestThreadResult __stdcall wrong_thread_probe(void *user) {
#else
static void *wrong_thread_probe(void *user) {
#endif
    WrongThreadProbe *probe = (WrongThreadProbe *)user;
    VgGpuInfo info = {0};
    char error[64] = {0};
    probe->demo = vg_gpu_renderer_draw_demo(probe->renderer);
    vg_gpu_renderer_present(probe->renderer);
    vg_gpu_renderer_present_embedded(probe->renderer);
    probe->capture = vg_gpu_renderer_capture(probe->renderer, "wrong-thread-must-not-exist.png");
    probe->info = vg_gpu_renderer_info(probe->renderer, &info);
    probe->shader =
        vg_gpu_renderer_validate_shader(probe->renderer, "#version 330\nvoid main(){}\n",
                                        "#version 330\nvoid main(){}\n", error, sizeof(error));
    probe->token_alive = vg_gpu_renderer_gpu_token_alive(probe->renderer, probe->object.token);
    probe->scene = vg_gpu_renderer_draw_scene(probe->renderer, &probe->camera, NULL, 0u, NULL, 0u);
    probe->executor.release(probe->executor.user, VG_ASSET_TYPE_MESH, probe->object);
    vg_gpu_renderer_destroy(probe->renderer);
#if defined(_WIN32)
    return 0u;
#else
    return NULL;
#endif
}

static void set_test_node_transform(VgStaticModelIr *model) {
    VgModelNodeIr *nodes = VG_MODEL_IR_ARRAY(model, VgModelNodeIr, nodes);
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    memcpy(nodes[0].local_transform, identity, sizeof(identity));
    memcpy(nodes[1].local_transform, identity, sizeof(identity));
    nodes[1].local_transform[0] = 1.5f;
    nodes[1].local_transform[5] = 1.5f;
    nodes[1].local_transform[10] = 1.5f;
    nodes[1].local_transform[12] = 30.0f;
    nodes[0].determinant_sign = 1;
    nodes[1].determinant_sign = 1;
}
static int test_direct_scene(VgGpuRenderer *renderer, const void *source, uint64_t source_size,
                             const char *kept_capture) {
    VgAssetMemory memory = {NULL, test_allocate, test_deallocate};
    VgStaticModelIr *model = NULL;
    uint64_t model_bytes = 0u;
    VgGltfDiagnostic diagnostic;
    CHECK(vg_gltf_import(NULL, &memory, "tests/assets/fixtures/static_scene.gltf", source,
                         source_size, NULL, &model, &model_bytes, &diagnostic) == VG_OK);
    set_test_node_transform(model);
    VgModelMaterialIr *materials = VG_MODEL_IR_ARRAY(model, VgModelMaterialIr, materials);
    materials[0].double_sided = 0u;
    VgAssetGpuExecutor executor = vg_gpu_renderer_asset_executor(renderer);
    VgAssetGpuObject object = {0};
    char error[192] = {0};
    CHECK(executor.upload(executor.user, VG_ASSET_TYPE_MESH, model, model_bytes, &object, error,
                          sizeof(error)) == VG_OK);
    CHECK(object.token != 0u && object.estimated_bytes != 0u);
    CHECK(vg_gpu_renderer_gpu_token_alive(renderer, object.token));
    VgAssetGpuObject rejected = {UINT64_MAX, UINT64_MAX};
    uint32_t saved_magic = model->magic;
    model->magic = 0u;
    CHECK(executor.upload(executor.user, VG_ASSET_TYPE_MESH, model, model_bytes, &rejected, error,
                          sizeof(error)) == VG_ERROR_FORMAT_VERSION);
    model->magic = saved_magic;
    CHECK(rejected.token == UINT64_MAX && rejected.estimated_bytes == UINT64_MAX);
    VgModelNodeIr *nodes = VG_MODEL_IR_ARRAY(model, VgModelNodeIr, nodes);
    float saved_scale = nodes[1].local_transform[0];
    nodes[1].local_transform[0] = NAN;
    rejected = (VgAssetGpuObject){UINT64_MAX, UINT64_MAX};
    CHECK(executor.upload(executor.user, VG_ASSET_TYPE_MESH, model, model_bytes, &rejected, error,
                          sizeof(error)) == VG_ERROR_FORMAT_VERSION);
    nodes[1].local_transform[0] = saved_scale;
    uint32_t saved_alpha = materials[0].alpha_mode;
    materials[0].alpha_mode = 99u;
    CHECK(executor.upload(executor.user, VG_ASSET_TYPE_MESH, model, model_bytes, &rejected, error,
                          sizeof(error)) == VG_ERROR_FORMAT_VERSION);
    materials[0].alpha_mode = saved_alpha;
    uint32_t saved_nodes_offset = model->nodes_offset;
    model->nodes_offset += 1u;
    CHECK(executor.upload(executor.user, VG_ASSET_TYPE_MESH, model, model_bytes, &rejected, error,
                          sizeof(error)) == VG_ERROR_FORMAT_VERSION);
    model->nodes_offset = model->meshes_offset;
    CHECK(executor.upload(executor.user, VG_ASSET_TYPE_MESH, model, model_bytes, &rejected, error,
                          sizeof(error)) == VG_ERROR_FORMAT_VERSION);
    model->nodes_offset = saved_nodes_offset;
    CHECK(rejected.token == UINT64_MAX && rejected.estimated_bytes == UINT64_MAX);
    CHECK(vg_gpu_renderer_gpu_token_alive(renderer, object.token));
    CHECK(vg_gpu_renderer_stats(renderer).asset_uploads == 1u);

    VgGpuModelPacket packets[100] = {0};
    for (uint32_t index = 0u; index < 100u; ++index) {
        packets[index].gpu_token = object.token;
        packets[index].node_index = 1u;
        packets[index].mesh_index = VG_MODEL_NO_INDEX;
        packets[index].material_override = VG_MODEL_NO_INDEX;
        packets[index].transform =
            instance_transform((float)((int)(index % 10u) - 35), (float)(index / 10u), 0.0f);
        packets[index].bounds_center = (VgVec3){0.5f, 0.0f, 0.5f};
        packets[index].bounds_extent = (VgVec3){0.6f, 0.2f, 0.6f};
    }
    packets[0].flags = VG_GPU_PACKET_WIREFRAME;
    packets[0].transform = instance_transform(-37.0f, 3.0f, 4.0f);
    packets[1].flags = VG_GPU_PACKET_FORCE_ERROR_MATERIAL;
    packets[2].transform.scale.x = -1.0f;
    packets[2].transform.position.x = 27.0f;
    packets[99].transform = instance_transform(-30.0f, -100.0f, 0.0f);

    VgGpuSpritePacket sprites[6] = {0};
    sprites[0].gpu_token = object.token;
    sprites[0].texture_index = 0u;
    sprites[0].material_mode = VG_MODEL_ALPHA_MASK;
    sprites[0].position = (VgVec3){-3, 1, 2};
    sprites[0].size = (VgVec3){2.5f, 2.5f, 0};
    sprites[0].tint[0] = sprites[0].tint[1] = sprites[0].tint[2] = sprites[0].tint[3] = 1.0f;
    sprites[0].alpha_cutoff = 0.5f;
    sprites[1] = sprites[0];
    sprites[1].material_mode = VG_MODEL_ALPHA_BLEND;
    sprites[1].position = (VgVec3){3, 7, 1};
    sprites[1].size = (VgVec3){3.0f, 3.0f, 0};
    sprites[1].tint[3] = 0.30f;
    sprites[2] = sprites[1];
    sprites[2].position = (VgVec3){3, 3, 1};
    sprites[2].tint[3] = 0.50f;
    sprites[3] = sprites[2];
    sprites[3].tint[3] = 0.25f;
    sprites[4] = sprites[2];
    sprites[4].position = (VgVec3){0, -100, 2};
    sprites[5] = sprites[0];
    sprites[5].material_mode = VG_MODEL_ALPHA_OPAQUE;
    sprites[5].position = (VgVec3){0, 0, 3};
    sprites[5].size = (VgVec3){2.0f, 2.0f, 0};

    VgGpuCamera camera = {.position = {0, -12, 6},
                          .target = {0, 4, 0.5f},
                          .up = {0, 0, 1},
                          .projection = VG_CAMERA_PERSPECTIVE,
                          .vertical_fov_radians = 1.0471976f,
                          .orthographic_height = 10.0f,
                          .near_clip_metres = 0.1f,
                          .far_clip_metres = 100.0f};
    WrongThreadProbe probe = {
        .renderer = renderer, .executor = executor, .object = object, .camera = camera};
#if defined(_WIN32)
    TestThreadHandle worker = CreateThread(NULL, 0u, wrong_thread_probe, &probe, 0u, NULL);
    CHECK(worker != NULL);
    CHECK(WaitForSingleObject(worker, 0xffffffffu) == 0u);
    CHECK(CloseHandle(worker) != 0);
#else
    pthread_t worker;
    CHECK(pthread_create(&worker, NULL, wrong_thread_probe, &probe) == 0);
    CHECK(pthread_join(worker, NULL) == 0);
#endif
    CHECK(!probe.demo && !probe.capture && !probe.info && !probe.shader && !probe.token_alive &&
          !probe.scene);
    CHECK(!FileExists("wrong-thread-must-not-exist.png"));
    CHECK(vg_gpu_renderer_gpu_token_alive(renderer, object.token));
    CHECK(vg_gpu_renderer_stats(renderer).wrong_thread_calls == 10u);

    CHECK(vg_gpu_renderer_draw_scene(renderer, &camera, packets, 100u, sprites, 6u));
    VgFrameStats stats = vg_gpu_renderer_stats(renderer);
    CHECK(stats.asset_uploads == 1u && stats.resident_gpu_assets == 1u);
    CHECK(stats.packets_submitted == 106u && stats.packets_visible + stats.packets_culled == 106u);
    CHECK(stats.packets_visible >= 90u && stats.packets_culled >= 2u &&
          stats.error_material_draws >= 1u && stats.reversed_winding_draws >= 1u);
    CHECK(stats.draw_calls > 0u && stats.triangles > 0u && stats.readbacks == 0u);
    CHECK(stats.transparent_packets == 3u &&
          stats.transparent_order_hash == expected_transparent_hash());
    const char *capture_path = kept_capture != NULL ? kept_capture : "vestigio-g03-scene.png";
    CHECK(vg_gpu_renderer_capture(renderer, capture_path));
    CHECK(capture_has_scene(capture_path));
    CHECK(vg_gpu_renderer_stats(renderer).readbacks == 1u);
    if (kept_capture == NULL)
        (void)remove(capture_path);
    executor.release(executor.user, VG_ASSET_TYPE_MESH, object);
    CHECK(!vg_gpu_renderer_gpu_token_alive(renderer, object.token));
    CHECK(vg_gpu_renderer_stats(renderer).asset_releases == 1u);
    vg_gltf_model_destroy(&memory, model);
    return 0;
}

static bool fail_next_gpu_upload;

static bool failing_gpu_is_owner(void *user) {
    VgAssetGpuExecutor inner = vg_gpu_renderer_asset_executor((VgGpuRenderer *)user);
    return inner.is_owner_thread(inner.user);
}

static VgResult failing_gpu_upload(void *user, VgAssetType type, const void *cpu_data,
                                   uint64_t cpu_bytes, VgAssetGpuObject *out_object, char *error,
                                   uint32_t error_capacity) {
    if (fail_next_gpu_upload) {
        fail_next_gpu_upload = false;
        if (error != NULL && error_capacity != 0u)
            (void)snprintf(error, error_capacity, "Injected GPU reload failure");
        return VG_ERROR_GPU;
    }
    VgAssetGpuExecutor inner = vg_gpu_renderer_asset_executor((VgGpuRenderer *)user);
    return inner.upload(inner.user, type, cpu_data, cpu_bytes, out_object, error, error_capacity);
}

static void failing_gpu_release(void *user, VgAssetType type, VgAssetGpuObject object) {
    VgAssetGpuExecutor inner = vg_gpu_renderer_asset_executor((VgGpuRenderer *)user);
    inner.release(inner.user, type, object);
}

static VgAssetInfo asset_info(VgContext *context, VgAsset asset) {
    VgAssetInfo info = {0};
    info.struct_size = sizeof(info);
    info.api_version = VG_API_VERSION;
    if (vg_asset_get_info(context, asset, &info) != VG_OK)
        info.struct_size = 0u;
    return info;
}
static int test_registry_sharing_and_reload(VgGpuRenderer *renderer, const void *source,
                                            uint64_t source_size) {
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    context_desc.max_assets = 4u;
    context_desc.max_asset_leases = 16u;
    VgContext *context = NULL;
    CHECK(vg_context_create(&context_desc, &context) == VG_OK);
    CHECK(vg_assets_enable_static_model_importer(context) == VG_OK);
    VgAssetGpuExecutor executor = {renderer, failing_gpu_is_owner, failing_gpu_upload,
                                   failing_gpu_release};
    CHECK(vg_asset_attach_gpu(context, &executor) == VG_OK);

    struct {
        VgAssetSourceDesc source;
        uint64_t future_tail;
    } catalog = {0};
    catalog.source.struct_size = sizeof(catalog);
    catalog.source.api_version = VG_API_VERSION;
    catalog.source.id = model_id();
    catalog.source.type = VG_ASSET_TYPE_MESH;
    catalog.source.importer_version = VG_STATIC_MODEL_IMPORTER_VERSION;
    catalog.source.version = 1u;
    catalog.source.source_path = "tests/assets/fixtures/static_scene.gltf";
    catalog.source.source_data = source;
    catalog.source.source_size = source_size;
    catalog.future_tail = UINT64_C(0xA5A5A5A5A5A5A5A5);
    CHECK(vg_asset_catalog_upsert(context, &catalog.source) == VG_OK);

    VgAssetRequest request = {0};
    request.struct_size = sizeof(request);
    request.api_version = VG_API_VERSION;
    request.id = catalog.source.id;
    request.type = catalog.source.type;
    request.required_residency = VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU;
    VgAsset asset = {0};
    CHECK(vg_asset_acquire(context, &request, &asset) == VG_OK);

    VgWorldDesc world_desc = {sizeof(world_desc), VG_API_VERSION, 112u, 128u};
    VgWorld world = {0};
    CHECK(vg_world_create(context, &world_desc, &world) == VG_OK);
    VgEntity camera_entity = {0};
    CHECK(vg_entity_create(context, world, &camera_entity) == VG_OK);
    VgCameraDesc camera_desc = {sizeof(camera_desc),
                                VG_API_VERSION,
                                VG_CAMERA_PERSPECTIVE,
                                0u,
                                1.0471976f,
                                10.0f,
                                0.1f,
                                100.0f};
    CHECK(vg_camera_set(context, camera_entity, &camera_desc) == VG_OK);
    VgTransform camera_transform = instance_transform(0.0f, -12.0f, 6.0f);
    camera_transform.rotation.x = -0.16910f;
    camera_transform.rotation.w = 0.98560f;
    CHECK(vg_entity_set_local_transform(context, camera_entity, &camera_transform) == VG_OK);

    VgMeshRendererDesc mesh = {0};
    mesh.struct_size = sizeof(mesh);
    mesh.api_version = VG_API_VERSION;
    mesh.asset = asset;
    mesh.node_index = 1u;
    mesh.mesh_index = VG_RENDER_DEFAULT_INDEX;
    mesh.material_override = VG_RENDER_DEFAULT_INDEX;
    mesh.bounds_center = (VgVec3){0.5f, 0.0f, 0.5f};
    mesh.bounds_extent = (VgVec3){0.6f, 0.2f, 0.6f};
    VgEntity entities[100] = {0};
    for (uint32_t index = 0u; index < 100u; ++index) {
        CHECK(vg_entity_create(context, world, &entities[index]) == VG_OK);
        VgTransform transform =
            instance_transform((float)((int)(index % 10u) - 5), (float)(index / 10u), 0.0f);
        CHECK(vg_entity_set_local_transform(context, entities[index], &transform) == VG_OK);
        CHECK(vg_mesh_renderer_set(context, entities[index], &mesh) == VG_OK);
    }
    mesh.flags = VG_RENDER_WIREFRAME;
    CHECK(vg_mesh_renderer_set(context, entities[0], &mesh) == VG_OK);
    mesh.flags = VG_RENDER_NONE;

    VgSpriteRendererDesc sprite = {0};
    sprite.struct_size = sizeof(sprite);
    sprite.api_version = VG_API_VERSION;
    sprite.asset = asset;
    sprite.texture_index = 0u;
    sprite.alpha_mode = VG_ALPHA_MASK;
    sprite.width_metres = 2.5f;
    sprite.height_metres = 2.5f;
    sprite.tint[0] = sprite.tint[1] = sprite.tint[2] = sprite.tint[3] = 1.0f;
    sprite.alpha_cutoff = 0.5f;
    VgEntity sprites[6] = {0};
    const VgVec3 sprite_positions[6] = {{-3, 1, 2}, {3, 7, 1},    {3, 3, 1},
                                        {3, 3, 1},  {0, -100, 2}, {0, 0, 3}};
    for (uint32_t index = 0u; index < 6u; ++index) {
        CHECK(vg_entity_create(context, world, &sprites[index]) == VG_OK);
        VgTransform transform = instance_transform(
            sprite_positions[index].x, sprite_positions[index].y, sprite_positions[index].z);
        CHECK(vg_entity_set_local_transform(context, sprites[index], &transform) == VG_OK);
        if (index >= 1u && index <= 3u) {
            sprite.alpha_mode = VG_ALPHA_BLEND;
            sprite.tint[3] = index == 1u ? 0.30f : (index == 2u ? 0.50f : 0.25f);
        } else if (index == 5u) {
            sprite.alpha_mode = VG_ALPHA_OPAQUE;
            sprite.tint[3] = 1.0f;
        } else {
            sprite.alpha_mode = VG_ALPHA_MASK;
            sprite.tint[3] = 1.0f;
        }
        CHECK(vg_sprite_renderer_set(context, sprites[index], &sprite) == VG_OK);
    }

    CHECK(vg_gpu_renderer_draw_world(renderer, context, world) == VG_OK);
    VgAssetCounters counters = asset_counters(context);
    CHECK(counters.struct_size != 0u && counters.live_leases == 1u &&
          counters.component_refs == 106u);
    CHECK(counters.decode_count == 1u && counters.upload_count == 1u &&
          counters.cache_hit_count == 0u);
    VgFrameStats frame = vg_gpu_renderer_stats(renderer);
    CHECK(frame.packets_submitted == 106u && frame.resident_gpu_assets == 1u &&
          frame.draw_calls != 0u);

    VgMeshRendererDesc readback = {0};
    readback.struct_size = sizeof(readback);
    readback.api_version = VG_API_VERSION;
    CHECK(vg_mesh_renderer_get(context, entities[0], &readback) == VG_OK);
    CHECK(readback.asset.value != VG_INVALID_HANDLE_VALUE && readback.flags == VG_RENDER_WIREFRAME);
    counters = asset_counters(context);
    CHECK(counters.live_leases == 2u && counters.component_refs == 106u);
    CHECK(vg_asset_release(context, readback.asset) == VG_OK);

    catalog.source.version = 2u;
    CHECK(vg_asset_catalog_upsert(context, &catalog.source) == VG_OK);
    CHECK(vg_asset_reload(context, asset) == VG_OK);
    fail_next_gpu_upload = true;
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    VgAssetInfo info = asset_info(context, asset);
    CHECK(info.struct_size != 0u && info.published_version == 1u &&
          info.last_reload_result == VG_ERROR_GPU);
    CHECK(vg_gpu_renderer_draw_world(renderer, context, world) == VG_OK);
    CHECK(vg_gpu_renderer_stats(renderer).draw_calls != 0u);

    catalog.source.version = 3u;
    CHECK(vg_asset_catalog_upsert(context, &catalog.source) == VG_OK);
    CHECK(vg_asset_reload(context, asset) == VG_OK);
    CHECK(vg_gpu_renderer_draw_world(renderer, context, world) == VG_OK);
    info = asset_info(context, asset);
    CHECK(info.published_version == 3u && info.last_reload_result == VG_OK);

    char replacement_error[256] = {0};
    VgGpuRenderer *replacement = vg_gpu_renderer_create(
        (VgGpuRendererConfig){640u, 360u}, replacement_error, sizeof(replacement_error));
    CHECK(replacement != NULL);
    CHECK(vg_gpu_renderer_draw_world(replacement, context, world) == VG_ERROR_CONFLICT);
    CHECK(vg_gpu_renderer_stats(replacement).asset_uploads == 0u);
    CHECK(vg_asset_detach_gpu(context) == VG_OK);
    executor = (VgAssetGpuExecutor){replacement, failing_gpu_is_owner, failing_gpu_upload,
                                    failing_gpu_release};
    CHECK(vg_asset_attach_gpu(context, &executor) == VG_OK);
    CHECK(vg_gpu_renderer_stats(replacement).asset_uploads == 0u);
    CHECK(vg_gpu_renderer_draw_world(replacement, context, world) == VG_OK);
    VgFrameStats replacement_stats = vg_gpu_renderer_stats(replacement);
    CHECK(replacement_stats.asset_uploads == 1u && replacement_stats.resident_gpu_assets == 1u &&
          replacement_stats.packets_submitted == 106u);

    for (uint32_t index = 0u; index < 99u; ++index)
        CHECK(vg_entity_destroy(context, entities[index]) == VG_OK);
    counters = asset_counters(context);
    CHECK(counters.component_refs == 7u);
    CHECK(vg_gpu_renderer_draw_world(replacement, context, world) == VG_OK);
    CHECK(vg_gpu_renderer_stats(replacement).packets_submitted == 7u);
    CHECK(vg_asset_release(context, asset) == VG_OK);
    uint32_t purged = 99u;
    CHECK(vg_assets_purge_unused(context, &purged) == VG_OK && purged == 0u);
    CHECK(vg_world_destroy(context, world) == VG_OK);
    counters = asset_counters(context);
    CHECK(counters.component_refs == 0u && counters.live_leases == 0u);
    CHECK(vg_assets_purge_unused(context, &purged) == VG_OK && purged == 1u);
    CHECK(vg_asset_flush_gpu(context) == VG_OK);
    CHECK(vg_asset_detach_gpu(context) == VG_OK);
    vg_context_destroy(context);
    replacement_stats = vg_gpu_renderer_stats(replacement);
    CHECK(replacement_stats.asset_uploads == 1u && replacement_stats.asset_releases == 1u &&
          replacement_stats.resident_gpu_assets == 0u);
    vg_gpu_renderer_destroy(replacement);
    VgFrameStats stats = vg_gpu_renderer_stats(renderer);
    CHECK(stats.asset_uploads == 3u && stats.asset_releases == 3u &&
          stats.resident_gpu_assets == 0u);
    return 0;
}
int main(int argc, char **argv) {
    const char *fixture = argc > 1 ? argv[1] : "tests/assets/fixtures/static_scene.gltf";
    void *source = NULL;
    uint64_t source_size = 0u;
    CHECK(read_file(fixture, &source, &source_size));
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_RESIZABLE);
    InitWindow(640, 360, "Vestigio G03 GPU scene test");
    CHECK(IsWindowReady());
    char error[192] = {0};
    VgGpuRenderer *renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){640u, 360u}, error, sizeof(error));
    CHECK(renderer != NULL);
    const char *kept_capture = argc > 2 ? argv[2] : NULL;
    CHECK(test_direct_scene(renderer, source, source_size, kept_capture) == 0);
    CHECK(test_registry_sharing_and_reload(renderer, source, source_size) == 0);
    vg_gpu_renderer_destroy(renderer);
    CloseWindow();
    free(source);
    puts("PASS G03 GPU model/sprite packets, culling, shared upload, reload and cleanup");
    return 0;
}
