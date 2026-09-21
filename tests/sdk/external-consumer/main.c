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
    return state.calls == 31u ? 0 : 7;
}
