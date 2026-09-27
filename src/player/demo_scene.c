#include "player/demo_scene.h"
#include "vestigio/controller.h"

#include <math.h>
#include <stdlib.h>

struct VgDemoScene {
    VgContext *context;
    VgWorld world;
    VgEntity camera;
    VgAsset model;
    VgGame *game;
    VgSpatialScene *spatial;
    VgControllerConfig controller;
    VgControllerState player;
    float yaw;
    float pitch;
    float look_sensitivity;
    VgResult update_error;
};

static VgTransform demo_transform(float x, float y, float z, float sx, float sy, float sz) {
    return (VgTransform){{x, y, z}, {0.0f, 0.0f, 0.0f, 1.0f}, {sx, sy, sz}};
}

static VgResult demo_add_mesh(VgDemoScene *scene, uint32_t node, VgTransform transform) {
    VgEntity entity = {0};
    VgResult result = vg_entity_create(scene->context, scene->world, &entity);
    if (result != VG_OK)
        return result;
    result = vg_entity_set_local_transform(scene->context, entity, &transform);
    if (result != VG_OK)
        return result;
    VgMeshRendererDesc mesh = {0};
    mesh.struct_size = sizeof(mesh);
    mesh.api_version = VG_API_VERSION;
    mesh.asset = scene->model;
    mesh.node_index = node;
    mesh.mesh_index = VG_RENDER_DEFAULT_INDEX;
    mesh.material_override = VG_RENDER_DEFAULT_INDEX;
    mesh.bounds_extent = (VgVec3){0.5f, 0.5f, 0.5f};
    result = vg_mesh_renderer_set(scene->context, entity, &mesh);
    if (result != VG_OK)
        return result;
    VgSpatialColliderDesc collider = {0};
    collider.entity = entity;
    collider.layer_mask = UINT64_C(1);
    collider.shape_type = VG_SPATIAL_SHAPE_BOX;
    collider.enabled = true;
    collider.shape.box.half_extents = (VgVec3){0.5f, 0.5f, 0.5f};
    result = vg_entity_get_world_transform(scene->context, entity, &collider.transform);
    if (result != VG_OK)
        return result;
    VgSpatialCollider handle = {0};
    return vg_spatial_collider_create(scene->spatial, &collider, &handle);
}

static void demo_fixed_update(VgContext *context, VgWorld world, float dt_seconds, void *user) {
    (void)world;
    VgDemoScene *scene = user;
    VgInputState input = {0};
    input.struct_size = sizeof(input);
    input.api_version = VG_API_VERSION;
    VgResult result = vg_game_get_input(scene->game, &input);
    if (result != VG_OK) {
        scene->update_error = result;
        return;
    }
    scene->yaw -= input.look_delta_x * scene->look_sensitivity;
    scene->pitch -= input.look_delta_y * scene->look_sensitivity;
    if (scene->pitch > 1.25f)
        scene->pitch = 1.25f;
    if (scene->pitch < -1.25f)
        scene->pitch = -1.25f;

    float forward = (input.held & VG_ACTION_MOVE_FORWARD) != 0u ? 1.0f : 0.0f;
    forward -= (input.held & VG_ACTION_MOVE_BACKWARD) != 0u ? 1.0f : 0.0f;
    float strafe = (input.held & VG_ACTION_MOVE_RIGHT) != 0u ? 1.0f : 0.0f;
    strafe -= (input.held & VG_ACTION_MOVE_LEFT) != 0u ? 1.0f : 0.0f;
    float length = sqrtf(forward * forward + strafe * strafe);
    if (length > 1.0f) {
        forward /= length;
        strafe /= length;
    }
    float sine_yaw = sinf(scene->yaw), cosine_yaw = cosf(scene->yaw);
    VgControllerInput movement = {0};
    movement.move_world.x = -sine_yaw * forward + cosine_yaw * strafe;
    movement.move_world.y = cosine_yaw * forward + sine_yaw * strafe;
    movement.jump_pressed = (input.pressed & VG_ACTION_JUMP) != 0u ? 1u : 0u;
    result = vg_controller_step(scene->spatial, &scene->controller, scene->camera, &movement,
                                dt_seconds, &scene->player);
    if (result != VG_OK) {
        scene->update_error = result;
        return;
    }
    VgTransform camera;
    result = vg_entity_get_local_transform(context, scene->camera, &camera);
    if (result != VG_OK) {
        scene->update_error = result;
        return;
    }
    camera.position = vg_controller_eye_position(&scene->controller, &scene->player);
    float half_yaw = scene->yaw * 0.5f, half_pitch = scene->pitch * 0.5f;
    float sy = sinf(half_yaw), cy = cosf(half_yaw);
    float sp = sinf(half_pitch), cp = cosf(half_pitch);
    camera.rotation = (VgQuat){cy * sp, sy * sp, sy * cp, cy * cp};
    scene->update_error = vg_entity_set_local_transform(context, scene->camera, &camera);
}

VgResult vg_demo_scene_create(VgContext *context, const void *model_data, uint64_t model_size,
                              float look_sensitivity, VgDemoScene **out_scene) {
    if (context == NULL || model_data == NULL || model_size == 0u || out_scene == NULL ||
        !isfinite(look_sensitivity) || look_sensitivity <= 0.0f)
        return VG_ERROR_INVALID_ARGUMENT;
    *out_scene = NULL;
    VgDemoScene *scene = calloc(1u, sizeof(*scene));
    if (scene == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    scene->context = context;
    scene->pitch = -0.12f;
    scene->look_sensitivity = look_sensitivity;
    scene->controller = vg_controller_default_config();
    scene->controller.height = 1.82f;
    scene->controller.eye_height = 1.7f;
    scene->controller.collision_mask = UINT64_C(1);
    scene->player.feet = (VgVec3){0.0f, -7.0f, 0.0f};
    scene->player.grounded = 1u;
    VgSpatialSceneConfig spatial_config = {0};
    spatial_config.max_meshes = 4u;
    spatial_config.max_colliders = 32u;
    spatial_config.max_triangles_per_mesh = 128u;
    VgResult result = vg_spatial_scene_create(&spatial_config, &scene->spatial);
    if (result != VG_OK)
        goto fail;
    result = vg_assets_enable_static_model_importer(context);
    if (result != VG_OK)
        goto fail;

    VgAssetSourceDesc source = {0};
    source.struct_size = sizeof(source);
    source.api_version = VG_API_VERSION;
    source.id = (VgAssetId){{0x4a, 0x30, 0x31, 0x2d, 0x61, 0x74, 0x72, 0x69,
                             0x75, 0x6d, 0x2d, 0x6d, 0x6f, 0x64, 0x65, 0x6c}};
    source.type = VG_ASSET_TYPE_MESH;
    source.importer_version = VG_STATIC_MODEL_IMPORTER_VERSION;
    source.version = 1u;
    source.source_path = "demo/atrium.gltf";
    source.source_data = model_data;
    source.source_size = model_size;
    result = vg_asset_catalog_upsert(context, &source);
    if (result != VG_OK)
        goto fail;
    VgAssetRequest request = {0};
    request.struct_size = sizeof(request);
    request.api_version = VG_API_VERSION;
    request.id = source.id;
    request.type = source.type;
    request.required_residency = VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU;
    result = vg_asset_acquire(context, &request, &scene->model);
    if (result != VG_OK)
        goto fail;

    VgWorldDesc world_desc = {sizeof(world_desc), VG_API_VERSION, 16u, 32u};
    result = vg_world_create(context, &world_desc, &scene->world);
    if (result != VG_OK)
        goto fail;
    result = vg_entity_create(context, scene->world, &scene->camera);
    if (result != VG_OK)
        goto fail;
    VgCameraDesc camera_desc = {sizeof(camera_desc), VG_API_VERSION, VG_CAMERA_PERSPECTIVE,
                                0u, 1.0471976f, 0.0f, 0.05f, 80.0f};
    result = vg_camera_set(context, scene->camera, &camera_desc);
    if (result != VG_OK)
        goto fail;
    VgTransform camera = demo_transform(0.0f, -7.0f, 1.7f, 1.0f, 1.0f, 1.0f);
    float sp = sinf(scene->pitch * 0.5f), cp = cosf(scene->pitch * 0.5f);
    camera.rotation = (VgQuat){sp, 0.0f, 0.0f, cp};
    result = vg_entity_set_local_transform(context, scene->camera, &camera);
    if (result != VG_OK)
        goto fail;

    /* Three authored glTF nodes are instanced through public mesh components. */
    result = demo_add_mesh(scene, 0u, demo_transform(0.0f, 1.5f, -0.1f, 16.0f, 20.0f, 0.2f));
    if (result != VG_OK)
        goto fail;
    result = demo_add_mesh(scene, 1u, demo_transform(0.0f, 3.5f, 1.35f, 1.4f, 1.4f, 2.7f));
    if (result != VG_OK)
        goto fail;
    for (int side = -1; side <= 1; side += 2) {
        for (int row = 0; row < 3; ++row) {
            result = demo_add_mesh(scene, 2u,
                                   demo_transform((float)side * 3.8f, -2.0f + (float)row * 4.2f,
                                                  0.95f, 0.65f, 0.65f, 1.9f));
            if (result != VG_OK)
                goto fail;
        }
    }
    result = demo_add_mesh(scene, 1u, demo_transform(-1.8f, 3.5f, 0.22f, 0.35f, 2.5f, 0.3f));
    if (result != VG_OK)
        goto fail;
    result = demo_add_mesh(scene, 1u, demo_transform(1.8f, 3.5f, 0.22f, 0.35f, 2.5f, 0.3f));
    if (result != VG_OK)
        goto fail;
    result = demo_add_mesh(scene, 1u, demo_transform(0.0f, -1.0f, 0.12f, 2.0f, 0.65f, 0.24f));
    if (result != VG_OK)
        goto fail;
    for (int side = -1; side <= 1; side += 2) {
        result = demo_add_mesh(scene, 2u,
                               demo_transform((float)side * 1.0f, 1.3f, 0.95f,
                                              0.65f, 1.0f, 1.9f));
        if (result != VG_OK)
            goto fail;
    }

    VgGameCallbacks callbacks = {0};
    callbacks.struct_size = sizeof(callbacks);
    callbacks.api_version = VG_API_VERSION;
    callbacks.user = scene;
    callbacks.fixed_update = demo_fixed_update;
    result = vg_game_create(context, NULL, &callbacks, &scene->game);
    if (result != VG_OK)
        goto fail;
    result = vg_game_set_world(scene->game, scene->world);
    if (result != VG_OK)
        goto fail;
    *out_scene = scene;
    return VG_OK;
fail:
    vg_demo_scene_destroy(scene);
    return result;
}

void vg_demo_scene_destroy(VgDemoScene *scene) {
    if (scene == NULL)
        return;
    if (scene->game != NULL)
        (void)vg_game_destroy(scene->game);
    if (scene->world.value != 0u)
        (void)vg_world_destroy(scene->context, scene->world);
    vg_spatial_scene_destroy(scene->spatial);
    if (scene->model.value != 0u)
        (void)vg_asset_release(scene->context, scene->model);
    free(scene);
}

VgResult vg_demo_scene_submit_input(VgDemoScene *scene, const VgInputSample *sample) {
    return scene == NULL ? VG_ERROR_INVALID_ARGUMENT : vg_game_submit_input(scene->game, sample);
}

VgResult vg_demo_scene_step(VgDemoScene *scene, double elapsed_seconds) {
    if (scene == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    VgStepInfo info = {0};
    info.struct_size = sizeof(info);
    info.api_version = VG_API_VERSION;
    VgResult result = vg_game_step(scene->game, elapsed_seconds, &info);
    return result == VG_OK ? scene->update_error : result;
}

VgWorld vg_demo_scene_world(const VgDemoScene *scene) {
    return scene == NULL ? (VgWorld){0} : scene->world;
}

VgResult vg_demo_scene_camera_position(const VgDemoScene *scene, VgVec3 *out_position) {
    if (scene == NULL || out_position == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    VgTransform transform;
    VgResult result = vg_entity_get_local_transform(scene->context, scene->camera, &transform);
    if (result == VG_OK)
        *out_position = transform.position;
    return result;
}

const VgSpatialScene *vg_demo_scene_spatial(const VgDemoScene *scene) {
    return scene == NULL ? NULL : scene->spatial;
}
