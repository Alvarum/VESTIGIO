#include "vestigio/controller.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                             \
            (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            ++failures;                                                                 \
        }                                                                                \
    } while (0)

static VgSpatialCollider add_box(VgSpatialScene *scene, uint64_t id, VgVec3 center,
                                 VgVec3 half) {
    VgSpatialColliderDesc desc = {0};
    desc.entity.value = id;
    desc.layer_mask = UINT64_C(1);
    desc.enabled = true;
    desc.shape_type = VG_SPATIAL_SHAPE_BOX;
    desc.transform.rotation.w = 1.0f;
    desc.transform.scale = (VgVec3){1.0f, 1.0f, 1.0f};
    desc.shape.box.center = center;
    desc.shape.box.half_extents = half;
    VgSpatialCollider handle = {0};
    CHECK(vg_spatial_collider_create(scene, &desc, &handle) == VG_OK);
    return handle;
}

static VgSpatialScene *new_scene(void) {
    VgSpatialSceneConfig config = {0};
    config.max_meshes = 4u;
    config.max_colliders = 32u;
    config.max_triangles_per_mesh = 128u;
    VgSpatialScene *scene = NULL;
    CHECK(vg_spatial_scene_create(&config, &scene) == VG_OK);
    return scene;
}

static VgControllerState walk(VgSpatialScene *scene, VgControllerConfig config,
                              VgControllerState state, VgVec3 direction, uint32_t ticks,
                              float *out_max_height) {
    VgControllerInput input = {direction, 0u};
    float maximum = state.feet.z;
    for (uint32_t tick = 0u; tick < ticks; ++tick) {
        VgResult result = vg_controller_step(scene, &config, (VgEntity){0}, &input,
                                              1.0f / 60.0f, &state);
        CHECK(result == VG_OK);
        if (result != VG_OK)
            break;
        if (state.feet.z > maximum)
            maximum = state.feet.z;
    }
    if (out_max_height != NULL)
        *out_max_height = maximum;
    return state;
}

static void test_step_wall_and_navigation(void) {
    VgSpatialScene *scene = new_scene();
    if (scene == NULL)
        return;
    (void)add_box(scene, 1u, (VgVec3){0.0f, 0.0f, -0.1f},
                  (VgVec3){10.0f, 10.0f, 0.1f});
    (void)add_box(scene, 2u, (VgVec3){0.0f, -0.5f, 0.12f},
                  (VgVec3){1.0f, 0.3f, 0.12f});
    (void)add_box(scene, 3u, (VgVec3){0.0f, 2.5f, 1.0f},
                  (VgVec3){2.0f, 0.1f, 1.0f});
    VgControllerConfig config = vg_controller_default_config();
    VgControllerState state = {0};
    state.feet = (VgVec3){0.0f, -3.0f, 0.0f};
    state.grounded = 1u;
    float max_height = 0.0f;
    state = walk(scene, config, state, (VgVec3){0.0f, 1.0f, 0.0f}, 180u, &max_height);
    CHECK(max_height > 0.18f && max_height < 0.38f);
    CHECK(state.feet.y > 1.0f && state.feet.y < 2.13f);
    CHECK(state.feet.z > -0.02f && state.feet.z < 0.03f);
    CHECK(state.grounded != 0u);
    bool reachable = false;
    CHECK(vg_controller_probe_segment(scene, &config, (VgEntity){0},
                                       (VgVec3){0.0f, -3.0f, 0.0f},
                                       (VgVec3){0.0f, 0.5f, 0.0f}, &reachable) == VG_OK);
    CHECK(reachable);
    CHECK(vg_controller_probe_segment(scene, &config, (VgEntity){0},
                                       (VgVec3){0.0f, -3.0f, 0.0f},
                                       (VgVec3){0.0f, 4.0f, 0.0f}, &reachable) == VG_OK);
    CHECK(!reachable);
    CHECK(vg_controller_probe_segment(scene, &config, (VgEntity){0},
                                       (VgVec3){0.0f, -3.0f, 0.0f},
                                       (VgVec3){0.0f, -3.0f, 0.3f}, &reachable) == VG_OK);
    CHECK(!reachable);
    config.max_fixed_dt = 1.0f / 120.0f;
    CHECK(vg_controller_probe_segment(scene, &config, (VgEntity){0},
                                       (VgVec3){0.0f, -3.0f, 0.0f},
                                       (VgVec3){0.0f, 0.5f, 0.0f}, &reachable) == VG_OK);
    CHECK(reachable);
    vg_spatial_scene_destroy(scene);
}

static void test_ceiling_and_spawn_penetration(void) {
    VgSpatialScene *scene = new_scene();
    if (scene == NULL)
        return;
    (void)add_box(scene, 1u, (VgVec3){0.0f, 0.0f, -0.1f},
                  (VgVec3){5.0f, 5.0f, 0.1f});
    (void)add_box(scene, 2u, (VgVec3){0.0f, 0.0f, 2.1f},
                  (VgVec3){2.0f, 2.0f, 0.1f});
    VgControllerConfig config = vg_controller_default_config();
    VgControllerState state = {0};
    state.grounded = 1u;
    VgControllerInput jump = {{0.0f, 0.0f, 0.0f}, 1u};
    CHECK(vg_controller_step(scene, &config, (VgEntity){0}, &jump,
                             1.0f / 60.0f, &state) == VG_OK);
    bool touched_ceiling = false;
    for (uint32_t tick = 0u; tick < 90u; ++tick) {
        VgControllerInput idle = {0};
        CHECK(vg_controller_step(scene, &config, (VgEntity){0}, &idle,
                                 1.0f / 60.0f, &state) == VG_OK);
        touched_ceiling = touched_ceiling || state.hit_ceiling != 0u;
        CHECK(state.feet.z < 0.31f);
    }
    CHECK(touched_ceiling);
    CHECK(state.grounded != 0u);
    VgControllerState inside = {0};
    inside.feet = (VgVec3){0.0f, 0.0f, -0.2f};
    VgControllerInput idle = {0};
    CHECK(vg_controller_step(scene, &config, (VgEntity){0}, &idle,
                             1.0f / 60.0f, &inside) == VG_ERROR_CONFLICT);
    vg_spatial_scene_destroy(scene);
}

static void test_invalid_tick(void) {
    VgSpatialScene *scene = new_scene();
    if (scene == NULL)
        return;
    VgControllerConfig config = vg_controller_default_config();
    VgControllerState state = {0};
    VgControllerInput input = {0};
    CHECK(vg_controller_step(scene, &config, (VgEntity){0}, &input, 0.1f, &state) ==
          VG_ERROR_INVALID_ARGUMENT);
    config.radius = -1.0f;
    CHECK(vg_controller_step(scene, &config, (VgEntity){0}, &input,
                             1.0f / 60.0f, &state) == VG_ERROR_INVALID_ARGUMENT);
    vg_spatial_scene_destroy(scene);
}

static void test_passage_width_and_step_clearance(void) {
    VgControllerConfig config = vg_controller_default_config();
    for (int wide = 0; wide < 2; ++wide) {
        VgSpatialScene *scene = new_scene();
        if (scene == NULL)
            return;
        (void)add_box(scene, 1u, (VgVec3){0.0f, 0.0f, -0.1f},
                      (VgVec3){5.0f, 5.0f, 0.1f});
        float side = wide != 0 ? 0.65f : 0.43f;
        (void)add_box(scene, 2u, (VgVec3){-side, 0.0f, 0.95f},
                      (VgVec3){0.2f, 0.5f, 0.95f});
        (void)add_box(scene, 3u, (VgVec3){side, 0.0f, 0.95f},
                      (VgVec3){0.2f, 0.5f, 0.95f});
        VgControllerState state = {0};
        state.feet = (VgVec3){0.0f, -2.0f, 0.0f};
        state.grounded = 1u;
        state = walk(scene, config, state, (VgVec3){0.0f, 1.0f, 0.0f}, 80u, NULL);
        if (wide != 0)
            CHECK(state.feet.y > 1.0f);
        else
            CHECK(state.feet.y < -0.5f);
        vg_spatial_scene_destroy(scene);
    }

    VgSpatialScene *scene = new_scene();
    if (scene == NULL)
        return;
    (void)add_box(scene, 1u, (VgVec3){0.0f, 0.0f, -0.1f},
                  (VgVec3){5.0f, 5.0f, 0.1f});
    (void)add_box(scene, 2u, (VgVec3){0.0f, 0.0f, 0.12f},
                  (VgVec3){1.0f, 0.3f, 0.12f});
    (void)add_box(scene, 3u, (VgVec3){0.0f, 0.0f, 1.95f},
                  (VgVec3){1.0f, 0.8f, 0.1f});
    VgControllerState state = {0};
    state.feet = (VgVec3){0.0f, -2.0f, 0.0f};
    state.grounded = 1u;
    state = walk(scene, config, state, (VgVec3){0.0f, 1.0f, 0.0f}, 80u, NULL);
    CHECK(state.feet.y < -0.3f);
    CHECK(state.feet.z < 0.24f);
    vg_spatial_scene_destroy(scene);
}

typedef struct CadenceRun {
    VgSpatialScene *spatial;
    VgControllerConfig config;
    VgControllerState state;
    VgResult update_error;
    uint32_t ticks;
} CadenceRun;

static void cadence_update(VgContext *context, VgWorld world, float dt, void *user) {
    (void)context;
    (void)world;
    CadenceRun *run = user;
    VgControllerInput input = {{0.0f, 1.0f, 0.0f}, 0u};
    if (run->update_error == VG_OK)
        run->update_error = vg_controller_step(run->spatial, &run->config, (VgEntity){0},
                                               &input, dt, &run->state);
    ++run->ticks;
}

static CadenceRun simulate_cadence(VgSpatialScene *spatial, uint32_t frames,
                                   double frame_seconds) {
    CadenceRun run = {0};
    run.spatial = spatial;
    run.config = vg_controller_default_config();
    run.state.feet = (VgVec3){0.0f, -3.0f, 0.0f};
    run.state.grounded = 1u;
    VgContext *context = NULL;
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    CHECK(vg_context_create(&context_desc, &context) == VG_OK);
    if (context == NULL)
        return run;
    VgWorld world = {0};
    VgWorldDesc world_desc = {sizeof(world_desc), VG_API_VERSION, 4u, 4u};
    CHECK(vg_world_create(context, &world_desc, &world) == VG_OK);
    VgGameCallbacks callbacks = {0};
    callbacks.struct_size = sizeof(callbacks);
    callbacks.api_version = VG_API_VERSION;
    callbacks.user = &run;
    callbacks.fixed_update = cadence_update;
    VgGame *game = NULL;
    CHECK(vg_game_create(context, NULL, &callbacks, &game) == VG_OK);
    if (game != NULL && world.value != 0u)
        CHECK(vg_game_set_world(game, world) == VG_OK);
    for (uint32_t index = 0u; game != NULL && index < frames; ++index) {
        VgStepInfo info = {0};
        info.struct_size = sizeof(info);
        info.api_version = VG_API_VERSION;
        CHECK(vg_game_step(game, frame_seconds, &info) == VG_OK);
    }
    if (game != NULL)
        CHECK(vg_game_destroy(game) == VG_OK);
    if (world.value != 0u)
        CHECK(vg_world_destroy(context, world) == VG_OK);
    vg_context_destroy(context);
    return run;
}

static void test_render_cadence_independence(void) {
    VgSpatialScene *scene = new_scene();
    if (scene == NULL)
        return;
    (void)add_box(scene, 1u, (VgVec3){0.0f, 0.0f, -0.1f},
                  (VgVec3){5.0f, 5.0f, 0.1f});
    CadenceRun thirty = simulate_cadence(scene, 60u, 1.0 / 30.0);
    CadenceRun one_twenty = simulate_cadence(scene, 240u, 1.0 / 120.0);
    CHECK(thirty.update_error == VG_OK && one_twenty.update_error == VG_OK);
    CHECK(thirty.ticks == 120u && one_twenty.ticks == 120u);
    CHECK(fabsf(thirty.state.feet.y - one_twenty.state.feet.y) < 1.0e-4f);
    CHECK(fabsf(thirty.state.feet.z - one_twenty.state.feet.z) < 1.0e-4f);
    vg_spatial_scene_destroy(scene);
}

static void test_walkable_ramp(void) {
    VgSpatialScene *scene = new_scene();
    if (scene == NULL)
        return;
    (void)add_box(scene, 1u, (VgVec3){0.0f, 0.0f, -0.1f},
                  (VgVec3){5.0f, 5.0f, 0.1f});
    VgVec3 positions[4] = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
                           {-1.0f, 1.0f, 0.4f}, {1.0f, 1.0f, 0.4f}};
    uint32_t indices[6] = {0u, 1u, 2u, 1u, 3u, 2u};
    VgSpatialMeshData data = {positions, 4u, indices, 6u};
    VgSpatialMesh mesh = {0};
    CHECK(vg_spatial_mesh_create(scene, &data, &mesh) == VG_OK);
    VgSpatialColliderDesc desc = {0};
    desc.entity.value = 2u;
    desc.layer_mask = UINT64_C(1);
    desc.enabled = true;
    desc.shape_type = VG_SPATIAL_SHAPE_STATIC_MESH;
    desc.shape.mesh = mesh;
    desc.transform.rotation.w = 1.0f;
    desc.transform.scale = (VgVec3){1.0f, 1.0f, 1.0f};
    VgSpatialCollider handle = {0};
    CHECK(vg_spatial_collider_create(scene, &desc, &handle) == VG_OK);
    VgControllerConfig config = vg_controller_default_config();
    VgControllerState state = {0};
    state.feet = (VgVec3){0.0f, -2.0f, 0.0f};
    state.grounded = 1u;
    float max_height = 0.0f;
    state = walk(scene, config, state, (VgVec3){0.0f, 1.0f, 0.0f}, 100u, &max_height);
    CHECK(max_height > 0.28f);
    CHECK(state.feet.y > 2.0f);
    CHECK(state.feet.z < 0.03f);
    positions[2].z = 3.0f;
    positions[3].z = 3.0f;
    CHECK(vg_spatial_mesh_replace(scene, mesh, &data) == VG_OK);
    state = (VgControllerState){0};
    state.feet = (VgVec3){0.0f, -2.0f, 0.0f};
    state.grounded = 1u;
    max_height = 0.0f;
    state = walk(scene, config, state, (VgVec3){0.0f, 1.0f, 0.0f}, 100u, &max_height);
    CHECK(state.feet.y < 0.2f);
    CHECK(max_height < 0.35f);
    CHECK(vg_spatial_collider_destroy(scene, handle) == VG_OK);
    CHECK(vg_spatial_mesh_destroy(scene, mesh) == VG_OK);
    vg_spatial_scene_destroy(scene);
}

int main(void) {
    test_step_wall_and_navigation();
    test_ceiling_and_spawn_penetration();
    test_invalid_tick();
    test_passage_width_and_step_clearance();
    test_render_cadence_independence();
    test_walkable_ramp();
    if (failures != 0)
        (void)fprintf(stderr, "controller failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
