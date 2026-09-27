#include "player/demo_scene.h"

#include "content/document.h"
#include "content/document_runtime.h"
#include "vestigio/controller.h"
#include "vestigio/door.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct VgDemoScene {
    VgContext *context;
    VgDocument *document;
    VgDocumentInstance *instance;
    VgWorld world;
    VgEntity camera;
    VgGame *game;
    VgSpatialScene *spatial; /* owned by instance */
    VgDoor **doors;
    VgDocumentDoorBinding *door_bindings;
    size_t door_count;
    size_t focused_door;
    VgControllerConfig controller;
    VgControllerState player;
    float yaw;
    float pitch;
    float look_sensitivity;
    VgResult update_error;
};

static const VgAssetId kAtriumAsset = {{0x4a, 0x30, 0x31, 0x2d, 0x61, 0x74, 0x72, 0x69,
                                        0x75, 0x6d, 0x2d, 0x6d, 0x6f, 0x64, 0x65, 0x6c}};

static VgResult demo_resolve_asset(void *user, VgContext *context, VgAssetId id,
                                   VgAssetType type, VgAsset *out_asset) {
    (void)user;
    if (type != VG_ASSET_TYPE_MESH ||
        memcmp(id.bytes, kAtriumAsset.bytes, sizeof(id.bytes)) != 0)
        return VG_ERROR_NOT_FOUND;
    VgAssetRequest request = {0};
    request.struct_size = sizeof(request);
    request.api_version = VG_API_VERSION;
    request.id = id;
    request.type = type;
    request.required_residency = VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU;
    return vg_asset_acquire(context, &request, out_asset);
}

static VgResult demo_find_camera(VgDemoScene *scene, VgTransform *out_transform) {
    for (size_t index = 0u; index < vg_document_instance_entity_count(scene->instance);
         ++index) {
        VgUuid id;
        VgEntity entity;
        VgCameraDesc camera = {0};
        camera.struct_size = sizeof(camera);
        camera.api_version = VG_API_VERSION;
        if (!vg_document_instance_entity_at(scene->instance, index, &id, &entity) ||
            vg_camera_get(scene->context, entity, &camera) != VG_OK)
            continue;
        VgResult result = vg_entity_get_local_transform(scene->context, entity,
                                                         out_transform);
        if (result == VG_OK)
            scene->camera = entity;
        return result;
    }
    return VG_ERROR_NOT_FOUND;
}

static VgResult demo_find_focused_door(VgDemoScene *scene) {
    scene->focused_door = SIZE_MAX;
    if (scene->door_count == 0u)
        return VG_OK;
    VgTransform camera;
    VgResult result = vg_entity_get_local_transform(scene->context, scene->camera,
                                                     &camera);
    if (result != VG_OK)
        return result;
    float cp = cosf(scene->pitch);
    VgSpatialRayQuery query = {0};
    query.origin = camera.position;
    query.direction = (VgVec3){-sinf(scene->yaw) * cp, cosf(scene->yaw) * cp,
                               sinf(scene->pitch)};
    query.max_distance = 3.0f;
    query.layer_mask = UINT64_C(1);
    query.ignored_entity = scene->camera;
    bool hit = false;
    VgSpatialHit impact = {0};
    result = vg_spatial_raycast(scene->spatial, &query, &hit, &impact);
    if (result != VG_OK || !hit)
        return result;
    for (size_t index = 0u; index < scene->door_count; ++index) {
        if (impact.collider.value == scene->door_bindings[index].collider.value) {
            scene->focused_door = index;
            break;
        }
    }
    return VG_OK;
}

static void demo_fixed_update(VgContext *context, VgWorld world, float dt_seconds,
                              void *user) {
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
    for (size_t index = 0u; index < scene->door_count; ++index) {
        result = vg_door_step(scene->doors[index], dt_seconds, &scene->controller,
                              &scene->player);
        if (result != VG_OK) {
            scene->update_error = result;
            return;
        }
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
    result = vg_controller_step(scene->spatial, &scene->controller, scene->camera,
                                &movement, dt_seconds, &scene->player);
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
    result = vg_entity_set_local_transform(context, scene->camera, &camera);
    if (result != VG_OK) {
        scene->update_error = result;
        return;
    }
    result = demo_find_focused_door(scene);
    if (result == VG_OK && (input.pressed & VG_ACTION_INTERACT) != 0u &&
        scene->focused_door != SIZE_MAX)
        result = vg_door_toggle(scene->doors[scene->focused_door]);
    scene->update_error = result;
}

VgResult vg_demo_scene_create(VgContext *context, const char *level_path,
                              const void *model_data, uint64_t model_size,
                              float look_sensitivity, VgDemoScene **out_scene) {
    if (context == NULL || level_path == NULL || model_data == NULL ||
        model_size == 0u || out_scene == NULL || !isfinite(look_sensitivity) ||
        look_sensitivity <= 0.0f)
        return VG_ERROR_INVALID_ARGUMENT;
    *out_scene = NULL;
    VgDemoScene *scene = calloc(1u, sizeof(*scene));
    if (scene == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    scene->context = context;
    scene->look_sensitivity = look_sensitivity;
    scene->controller = vg_controller_default_config();
    scene->controller.height = 1.82f;
    scene->controller.eye_height = 1.7f;
    scene->controller.collision_mask = UINT64_C(1);

    VgResult result = vg_assets_enable_static_model_importer(context);
    if (result != VG_OK)
        goto fail;
    VgAssetSourceDesc source = {0};
    source.struct_size = sizeof(source);
    source.api_version = VG_API_VERSION;
    source.id = kAtriumAsset;
    source.type = VG_ASSET_TYPE_MESH;
    source.importer_version = VG_STATIC_MODEL_IMPORTER_VERSION;
    source.version = 1u;
    source.source_path = "demo/atrium.gltf";
    source.source_data = model_data;
    source.source_size = model_size;
    result = vg_asset_catalog_upsert(context, &source);
    if (result != VG_OK)
        goto fail;
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_document_open_file(level_path, &scene->document, &diagnostic)) {
        (void)fprintf(stderr, "Nivel 3D invalido: %s (%s)\n", diagnostic.message,
                      diagnostic.path);
        result = VG_ERROR_INVALID_ARGUMENT;
        goto fail;
    }
    VgDocumentInstanceDesc instance_desc = {demo_resolve_asset, scene};
    result = vg_document_instantiate(context, scene->document, &instance_desc,
                                     &scene->instance, &diagnostic);
    if (result != VG_OK) {
        (void)fprintf(stderr, "No se pudo instanciar nivel 3D: %s (%s)\n",
                      diagnostic.message, diagnostic.path);
        goto fail;
    }
    scene->world = vg_document_instance_world(scene->instance);
    scene->spatial = vg_document_instance_spatial(scene->instance);
    if (scene->spatial == NULL) {
        result = VG_ERROR_NOT_FOUND;
        goto fail;
    }
    VgTransform camera = {0};
    result = demo_find_camera(scene, &camera);
    if (result != VG_OK)
        goto fail;
    scene->focused_door = SIZE_MAX;
    scene->door_count = vg_document_instance_door_count(scene->instance);
    if (scene->door_count > 0u) {
        scene->doors = calloc(scene->door_count, sizeof(*scene->doors));
        scene->door_bindings = calloc(scene->door_count,
                                     sizeof(*scene->door_bindings));
        if (scene->doors == NULL || scene->door_bindings == NULL) {
            result = VG_ERROR_OUT_OF_MEMORY;
            goto fail;
        }
        for (size_t index = 0u; index < scene->door_count; ++index) {
            if (!vg_document_instance_door_at(scene->instance, index,
                                               &scene->door_bindings[index])) {
                result = VG_ERROR_NOT_FOUND;
                goto fail;
            }
            VgDocumentDoorBinding *binding = &scene->door_bindings[index];
            VgDoorDesc door_desc = {0};
            door_desc.struct_size = sizeof(door_desc);
            door_desc.api_version = VG_API_VERSION;
            door_desc.context = context;
            door_desc.spatial = scene->spatial;
            door_desc.hinge = binding->hinge;
            door_desc.panel = binding->panel;
            door_desc.collider = binding->collider;
            door_desc.collider_description = binding->collider_description;
            door_desc.open_angle_radians = binding->open_angle_radians;
            door_desc.angular_speed_radians = binding->speed_radians_per_second;
            result = vg_door_create(&door_desc, &scene->doors[index]);
            if (result != VG_OK)
                goto fail;
        }
    }
    scene->player.feet = camera.position;
    scene->player.feet.z -= scene->controller.eye_height;
    scene->player.grounded = 1u;
    /* The document camera is the initial authority; controller updates it later. */
    VgQuat rotation = camera.rotation;
    scene->yaw = atan2f(2.0f * (rotation.w * rotation.z +
                               rotation.x * rotation.y),
                        1.0f - 2.0f * (rotation.y * rotation.y +
                                       rotation.z * rotation.z));
    scene->pitch = asinf(fmaxf(-1.0f, fminf(1.0f,
                        2.0f * (rotation.w * rotation.x -
                                rotation.y * rotation.z))));
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
    for (size_t index = 0u; index < scene->door_count; ++index)
        vg_door_destroy(scene->doors != NULL ? scene->doors[index] : NULL);
    free(scene->doors);
    free(scene->door_bindings);
    vg_document_instance_destroy(scene->instance);
    vg_document_destroy(scene->document);
    free(scene);
}

VgResult vg_demo_scene_submit_input(VgDemoScene *scene, const VgInputSample *sample) {
    return scene == NULL ? VG_ERROR_INVALID_ARGUMENT :
           vg_game_submit_input(scene->game, sample);
}

VgResult vg_demo_scene_step(VgDemoScene *scene, double elapsed_seconds) {
    if (scene == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    VgStepInfo info = {0};
    info.struct_size = sizeof(info);
    info.api_version = VG_API_VERSION;
    scene->update_error = VG_OK;
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
    VgResult result = vg_entity_get_local_transform(scene->context, scene->camera,
                                                     &transform);
    if (result == VG_OK)
        *out_position = transform.position;
    return result;
}

const VgSpatialScene *vg_demo_scene_spatial(const VgDemoScene *scene) {
    return scene == NULL ? NULL : scene->spatial;
}

const char *vg_demo_scene_door_hint(const VgDemoScene *scene) {
    if (scene == NULL || scene->focused_door == SIZE_MAX ||
        scene->focused_door >= scene->door_count)
        return NULL;
    VgDoorState state = vg_door_state(scene->doors[scene->focused_door]);
    return state == VG_DOOR_OPEN || state == VG_DOOR_OPENING
               ? "E: cerrar puerta" : "E: abrir puerta";
}

size_t vg_demo_scene_door_count(const VgDemoScene *scene) {
    return scene == NULL ? 0u : scene->door_count;
}

VgResult vg_demo_scene_toggle_door(VgDemoScene *scene, size_t index) {
    if (scene == NULL || index >= scene->door_count)
        return VG_ERROR_INVALID_ARGUMENT;
    return vg_door_toggle(scene->doors[index]);
}

float vg_demo_scene_door_angle(const VgDemoScene *scene, size_t index) {
    return scene == NULL || index >= scene->door_count ? NAN :
           vg_door_angle(scene->doors[index]);
}

typedef struct VgDemoColliderSnapshot {
    VgSpatialCollider target;
    VgTransform transform;
    bool found;
} VgDemoColliderSnapshot;

static void demo_visit_door_collider(void *user, VgSpatialCollider collider,
                                     const VgSpatialColliderDesc *description,
                                     VgVec3 bounds_min, VgVec3 bounds_max) {
    (void)bounds_min;
    (void)bounds_max;
    VgDemoColliderSnapshot *snapshot = user;
    if (collider.value == snapshot->target.value) {
        snapshot->transform = description->transform;
        snapshot->found = true;
    }
}

VgResult vg_demo_scene_door_pose(const VgDemoScene *scene, size_t index,
                                 VgTransform *out_panel, VgTransform *out_collider) {
    if (scene == NULL || index >= scene->door_count || out_panel == NULL ||
        out_collider == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    VgResult result = vg_entity_get_world_transform(scene->context,
                                scene->door_bindings[index].panel, out_panel);
    if (result != VG_OK)
        return result;
    VgDemoColliderSnapshot snapshot = {0};
    snapshot.target = scene->door_bindings[index].collider;
    result = vg_spatial_scene_visit_debug(scene->spatial, demo_visit_door_collider,
                                           &snapshot);
    if (result == VG_OK && snapshot.found)
        *out_collider = snapshot.transform;
    return result != VG_OK ? result : snapshot.found ? VG_OK : VG_ERROR_NOT_FOUND;
}

VgResult vg_demo_scene_prepare_door_smoke(VgDemoScene *scene) {
    if (scene == NULL || scene->door_count == 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    VgTransform panel, collider;
    VgResult result = vg_demo_scene_door_pose(scene, 0u, &panel, &collider);
    if (result != VG_OK)
        return result;
    scene->player.feet = (VgVec3){panel.position.x, panel.position.y - 1.7f,
                                  panel.position.z - 0.95f};
    scene->player.grounded = 1u;
    scene->player.vertical_speed = 0.0f;
    scene->yaw = 0.0f;
    scene->pitch = 0.0f;
    VgTransform camera;
    result = vg_entity_get_local_transform(scene->context, scene->camera, &camera);
    if (result != VG_OK)
        return result;
    camera.position = vg_controller_eye_position(&scene->controller, &scene->player);
    camera.rotation = (VgQuat){0.0f, 0.0f, 0.0f, 1.0f};
    result = vg_entity_set_local_transform(scene->context, scene->camera, &camera);
    return result == VG_OK ? demo_find_focused_door(scene) : result;
}
