#include "platform/gpu_host.h"

#include "content/document_runtime.h"
#include "content/document_internal.h"
#include "platform/win32_embed.h"
#include "raylib.h"
#include "render/gpu_raylib/gpu_renderer.h"
#include "tooling/tool_api.h"
#include "vestigio/controller.h"
#include "vestigio/door.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#undef Rectangle
#undef ShowCursor
#undef CloseWindow
#endif

struct VgGpuHost {
    VgGpuRenderer *renderer;
    void *window;
    unsigned long owner_thread;
    VgContext *context;
    VgDocument *document;
    VgDocumentInstance *edit;
    VgDocumentInstance *play;
    VgDoor **doors;
    size_t door_count;
    VgEntity edit_camera;
    VgEntity play_camera;
    VgControllerConfig controller;
    VgControllerState player;
    void *model_data;
    uint64_t model_size;
    VgAssetId model_id;
    bool model_registered;
    bool gpu_attached;
    float edit_yaw, edit_pitch;
    float play_yaw, play_pitch;
    int32_t edit_camera_mode;
    VgEntity selected_entity;
    bool has_selection;
    char selected_uuid[37];
    VgVec3 selected_center;
    VgVec3 selected_extent;
    uint32_t pick_mask;
    VgVec3 orbit_target;
    float orbit_distance;
    double accumulator;
    bool jump_pending;
    bool interact_pending;
    uint64_t saved_revision;
    char *saved_json;
    size_t saved_length;
    int32_t visual_mode;
};

static _Atomic(VgGpuHost *) active_host;

static bool host_is_current(const VgGpuHost *host) {
    return host && host == atomic_load_explicit(&active_host, memory_order_acquire) &&
           host->owner_thread == vg_win32_thread_id();
}

static void host_error(char *out, size_t capacity, const char *message) {
    if (out && capacity)
        (void)snprintf(out, capacity, "%s", message);
}

/* 1 exists, 0 missing, -1 invalid/inaccessible. Settings paths are UTF-8. */
static int host_settings_file_state(const char *path) {
#ifdef _WIN32
    wchar_t wide[4096];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                            wide, (int)(sizeof(wide) / sizeof(wide[0]))) == 0)
        return -1;
    DWORD attributes = GetFileAttributesW(wide);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ? 1 : -1;
    DWORD failure = GetLastError();
    return failure == ERROR_FILE_NOT_FOUND || failure == ERROR_PATH_NOT_FOUND ? 0 : -1;
#else
    FILE *file = fopen(path, "rb");
    if (file != NULL) { (void)fclose(file); return 1; }
    return errno == ENOENT ? 0 : -1;
#endif
}

static bool host_apply_visual(VgGpuHost *host, const VgDocumentInstance *instance,
                              int32_t mode, char *error, size_t error_capacity) {
    VgGpuVisualSettings visual = vg_gpu_renderer_default_visual_settings();
    visual.mode = (VgGpuVisualMode)mode;
    if (instance != NULL) {
        VgDocumentEnvironment environment;
        if (!vg_document_instance_environment(instance, &environment)) {
            host_error(error, error_capacity, "No se pudo leer ambiente del nivel");
            return false;
        }
        memcpy(visual.ambient, environment.ambient_linear, sizeof(visual.ambient));
        memcpy(visual.clear_color, environment.clear_linear, sizeof(visual.clear_color));
        memcpy(visual.fog_color, environment.fog_color_linear, sizeof(visual.fog_color));
        visual.fog_enabled = environment.fog_enabled ? 1u : 0u;
        visual.fog_start = environment.fog_start;
        visual.fog_end = environment.fog_end;
        size_t count = vg_document_instance_light_count(instance);
        if (count > VG_GPU_MAX_POINT_LIGHTS) {
            host_error(error, error_capacity, "Demasiadas luces en el nivel");
            return false;
        }
        visual.point_light_count = (uint32_t)count;
        for (size_t i = 0u; i < count; ++i) {
            VgDocumentLightBinding binding;
            VgTransform world;
            if (!vg_document_instance_light_at(instance, i, &binding) ||
                vg_entity_get_world_transform(host->context, binding.entity, &world) != VG_OK) {
                host_error(error, error_capacity, "No se pudo leer pose de la luz");
                return false;
            }
            visual.lights[i].position = world.position;
            visual.lights[i].radius = binding.range;
            visual.lights[i].intensity = binding.intensity;
            memcpy(visual.lights[i].color, binding.color_linear,
                   sizeof(visual.lights[i].color));
        }
    }
    return vg_gpu_renderer_set_visual_settings(host->renderer, &visual, error,
                                                error_capacity);
}

static void host_release_play(VgGpuHost *host) {
    for (size_t i = 0u; i < host->door_count; ++i)
        vg_door_destroy(host->doors[i]);
    free(host->doors);
    host->doors = NULL;
    host->door_count = 0u;
    vg_document_instance_destroy(host->play);
    host->play = NULL;
    host->accumulator = 0.0;
    host->jump_pending = false;
    host->interact_pending = false;
}

static void host_release_level(VgGpuHost *host) {
    host_release_play(host);
    vg_document_instance_destroy(host->edit);
    host->edit = NULL;
    vg_document_destroy(host->document);
    host->document = NULL;
    if (host->context != NULL) {
        uint32_t purged = 0u;
        (void)vg_assets_purge_unused(host->context, &purged);
        (void)vg_asset_flush_gpu(host->context);
        if (host->gpu_attached)
            (void)vg_asset_detach_gpu(host->context);
        vg_context_destroy(host->context);
        host->context = NULL;
        host->gpu_attached = false;
    }
    free(host->model_data);
    host->model_data = NULL;
    host->model_size = 0u;
    host->model_registered = false;
    host->accumulator = 0.0;
    host->jump_pending = false;
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    host->edit_camera_mode = 0;
    host->saved_revision = 0u;
    free(host->saved_json);
    host->saved_json = NULL;
    host->saved_length = 0u;
}

static bool host_read_model(const char *path, void **out_data, uint64_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    long size = 0;
    bool ok = fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 &&
              size <= 1024 * 1024 && fseek(file, 0, SEEK_SET) == 0;
    void *data = ok ? malloc((size_t)size) : NULL;
    ok = data != NULL && fread(data, 1u, (size_t)size, file) == (size_t)size;
    (void)fclose(file);
    if (!ok) {
        free(data);
        return false;
    }
    *out_data = data;
    *out_size = (uint64_t)size;
    return true;
}

static VgResult host_resolve_asset(void *user, VgContext *context, VgAssetId id,
                                   VgAssetType type, VgAsset *out_asset) {
    VgGpuHost *host = user;
    static const uint8_t atrium_asset_id[16] = {
        0x4a, 0x30, 0x31, 0x2d, 0x61, 0x74, 0x72, 0x69,
        0x75, 0x6d, 0x2d, 0x6d, 0x6f, 0x64, 0x65, 0x6c};
    if (type != VG_ASSET_TYPE_MESH || host->model_data == NULL)
        return VG_ERROR_UNSUPPORTED;
    if (memcmp(id.bytes, atrium_asset_id, sizeof(atrium_asset_id)) != 0)
        return VG_ERROR_NOT_FOUND;
    if (host->model_registered && memcmp(host->model_id.bytes, id.bytes, 16u) != 0)
        return VG_ERROR_NOT_FOUND;
    if (!host->model_registered) {
        VgAssetSourceDesc source = {0};
        source.struct_size = sizeof(source);
        source.api_version = VG_API_VERSION;
        source.id = id;
        source.type = type;
        source.importer_version = VG_STATIC_MODEL_IMPORTER_VERSION;
        source.version = 1u;
        source.source_path = "demo/atrium.gltf";
        source.source_data = host->model_data;
        source.source_size = host->model_size;
        VgResult result = vg_asset_catalog_upsert(context, &source);
        if (result != VG_OK)
            return result;
        host->model_id = id;
        host->model_registered = true;
    }
    VgAssetRequest request = {0};
    request.struct_size = sizeof(request);
    request.api_version = VG_API_VERSION;
    request.id = id;
    request.type = type;
    request.required_residency = VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU;
    return vg_asset_acquire(context, &request, out_asset);
}

static bool host_find_camera(VgGpuHost *host, VgDocumentInstance *instance,
                             VgEntity *out_camera) {
    size_t count = vg_document_instance_entity_count(instance);
    for (size_t index = 0u; index < count; ++index) {
        VgUuid id;
        VgEntity entity;
        VgCameraDesc camera = {0};
        camera.struct_size = sizeof(camera);
        camera.api_version = VG_API_VERSION;
        if (vg_document_instance_entity_at(instance, index, &id, &entity) &&
            vg_camera_get(host->context, entity, &camera) == VG_OK) {
            *out_camera = entity;
            return true;
        }
    }
    return false;
}

static VgQuat host_rotation(float yaw, float pitch) {
    float sy = sinf(yaw * 0.5f), cy = cosf(yaw * 0.5f);
    float sp = sinf(pitch * 0.5f), cp = cosf(pitch * 0.5f);
    return (VgQuat){cy * sp, sy * sp, sy * cp, cy * cp};
}

static VgResult host_set_camera(VgGpuHost *host, VgEntity entity, VgVec3 position,
                                float yaw, float pitch) {
    VgTransform transform;
    VgResult result = vg_entity_get_local_transform(host->context, entity, &transform);
    if (result != VG_OK)
        return result;
    transform.position = position;
    transform.rotation = host_rotation(yaw, pitch);
    return vg_entity_set_local_transform(host->context, entity, &transform);
}

VgGpuHost *vg_gpu_host_create(void *parent_window, unsigned int width, unsigned int height,
                              char *error, size_t error_capacity) {
    if (!parent_window || width == 0u || height == 0u || width > (unsigned int)INT_MAX ||
        height > (unsigned int)INT_MAX) {
        host_error(error, error_capacity, "Parent o dimensiones de viewport invalidas");
        return NULL;
    }
    VgGpuHost *host = calloc(1u, sizeof(*host));
    if (!host) {
        host_error(error, error_capacity, "Sin memoria para host GPU");
        return NULL;
    }
    host->owner_thread = vg_win32_thread_id();
    host->pick_mask = VG_GPU_PICK_MESH | VG_GPU_PICK_CAMERA |
                      VG_GPU_PICK_LIGHT | VG_GPU_PICK_TRIGGER;
    VgGpuHost *expected = NULL;
    if (!atomic_compare_exchange_strong_explicit(&active_host, &expected, host,
                                                 memory_order_acq_rel, memory_order_acquire)) {
        host_error(error, error_capacity, "Ya existe una superficie GPU en este proceso");
        free(host);
        return NULL;
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_RESIZABLE);
    InitWindow((int)width, (int)height, "VESTIGIO GPU viewport");
    if (!IsWindowReady()) {
        host_error(error, error_capacity, "No se pudo crear ventana/contexto GPU");
        atomic_store_explicit(&active_host, NULL, memory_order_release);
        free(host);
        return NULL;
    }
    SetExitKey(KEY_NULL);
    SetTargetFPS(0);
    host->window = GetWindowHandle();
    if (!vg_win32_embed_window(host->window, parent_window, (int)width, (int)height)) {
        host_error(error, error_capacity, "No se pudo alojar la superficie GPU en WPF");
        CloseWindow();
        atomic_store_explicit(&active_host, NULL, memory_order_release);
        free(host);
        return NULL;
    }
    host->renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){320u, 180u}, error, error_capacity);
    if (!host->renderer) {
        vg_win32_unembed_window(host->window);
        CloseWindow();
        atomic_store_explicit(&active_host, NULL, memory_order_release);
        free(host);
        return NULL;
    }
    host_error(error, error_capacity, "");
    return host;
}

void *vg_gpu_host_window(const VgGpuHost *host) {
    return host_is_current(host) ? host->window : NULL;
}

int32_t vg_gpu_host_render(VgGpuHost *host) {
    if (!host_is_current(host) || !host->renderer)
        return false;
    VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    char visual_error[192] = {0};
    if (!host_apply_visual(host, instance, host->visual_mode,
                           visual_error, sizeof(visual_error)))
        return false;
    if (instance != NULL) {
        if (vg_gpu_renderer_draw_world(host->renderer, host->context,
                                       vg_document_instance_world(instance)) != VG_OK)
            return false;
    } else if (!vg_gpu_renderer_draw_demo(host->renderer)) {
        return false;
    }
    vg_gpu_renderer_present_embedded(host->renderer);
    return true;
}

int32_t vg_gpu_host_visual_mode(const VgGpuHost *host) {
    return host_is_current(host) && host->renderer != NULL ? host->visual_mode : -1;
}

int32_t vg_gpu_host_set_visual_mode(VgGpuHost *host, int32_t mode,
                                    char *error, size_t error_capacity) {
    if (!host_is_current(host) || host->renderer == NULL ||
        (mode != VG_GPU_VISUAL_CLEAN && mode != VG_GPU_VISUAL_RETRO)) {
        host_error(error, error_capacity, "Perfil visual invalido");
        return false;
    }
    VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    if (!host_apply_visual(host, instance, mode, error, error_capacity))
        return false;
    host->visual_mode = mode;
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_load_visual_profile(VgGpuHost *host, const char *path,
                                        char *error, size_t error_capacity) {
    if (!host_is_current(host) || path == NULL || path[0] == '\0') {
        host_error(error, error_capacity, "Ruta de settings invalida");
        return false;
    }
    int file_state = host_settings_file_state(path);
    if (file_state == 0)
        return vg_gpu_host_set_visual_mode(host, VG_VISUAL_PROFILE_CLEAN,
                                            error, error_capacity);
    if (file_state < 0) {
        host_error(error, error_capacity, "No se pudo leer settings visuales");
        return false;
    }
    VgSettingsLayer layer = {0};
    layer.struct_size = sizeof(layer);
    layer.api_version = VG_API_VERSION;
    VgSettingsDiagnostic diagnostic = {0};
    diagnostic.struct_size = sizeof(diagnostic);
    diagnostic.api_version = VG_API_VERSION;
    if (vg_settings_load_file(path, &layer, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    return vg_gpu_host_set_visual_mode(host,
        (layer.present & VG_SETTING_VISUAL_PROFILE) != 0u
            ? (int32_t)layer.visual_profile : VG_VISUAL_PROFILE_CLEAN,
        error, error_capacity);
}

int32_t vg_gpu_host_save_visual_profile(VgGpuHost *host, const char *path,
                                        char *error, size_t error_capacity) {
    if (!host_is_current(host) || path == NULL || path[0] == '\0') {
        host_error(error, error_capacity, "Ruta de settings invalida");
        return false;
    }
    VgSettingsLayer layer = {0};
    layer.struct_size = sizeof(layer);
    layer.api_version = VG_API_VERSION;
    int file_state = host_settings_file_state(path);
    if (file_state == 1) {
        VgSettingsDiagnostic diagnostic = {0};
        diagnostic.struct_size = sizeof(diagnostic);
        diagnostic.api_version = VG_API_VERSION;
        if (vg_settings_load_file(path, &layer, &diagnostic) != VG_OK) {
            host_error(error, error_capacity, diagnostic.message);
            return false;
        }
    } else if (file_state < 0) {
        host_error(error, error_capacity, "No se pudo leer settings visuales");
        return false;
    }
    layer.present |= VG_SETTING_VISUAL_PROFILE;
    layer.visual_profile = (uint32_t)host->visual_mode;
    VgSettingsDiagnostic diagnostic = {0};
    diagnostic.struct_size = sizeof(diagnostic);
    diagnostic.api_version = VG_API_VERSION;
    if (vg_settings_save_file(path, &layer, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    host_error(error, error_capacity, "");
    return true;
}

size_t vg_gpu_host_visual_light_count(const VgGpuHost *host) {
    if (!host_is_current(host))
        return 0u;
    const VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    return vg_document_instance_light_count(instance);
}

int32_t vg_gpu_host_visual_fog_enabled(const VgGpuHost *host) {
    if (!host_is_current(host))
        return false;
    const VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    VgDocumentEnvironment environment;
    return vg_document_instance_environment(instance, &environment) &&
           environment.fog_enabled;
}

int32_t vg_gpu_host_open_level(VgGpuHost *host, const char *level_path,
                               const char *model_path, char *error, size_t error_capacity) {
    if (!host_is_current(host) || level_path == NULL || model_path == NULL ||
        level_path[0] == '\0' || model_path[0] == '\0' || host->document != NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_document_open_file(level_path, &host->document, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    if (!host_read_model(model_path, &host->model_data, &host->model_size)) {
        host_error(error, error_capacity, "No se pudo leer el modelo Atrium");
        goto fail;
    }
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    if (vg_context_create(&context_desc, &host->context) != VG_OK) {
        host_error(error, error_capacity, "No se pudo crear el contexto del nivel");
        goto fail;
    }
    VgAssetGpuExecutor executor = vg_gpu_renderer_asset_executor(host->renderer);
    if (vg_asset_attach_gpu(host->context, &executor) != VG_OK) {
        host_error(error, error_capacity, "No se pudo conectar el nivel a GPU");
        goto fail;
    }
    host->gpu_attached = true;
    if (vg_assets_enable_static_model_importer(host->context) != VG_OK) {
        host_error(error, error_capacity, "No se pudo habilitar el importer glTF");
        goto fail;
    }
    VgDocumentInstanceDesc description = {host_resolve_asset, host};
    if (vg_document_instantiate(host->context, host->document, &description,
                                &host->edit, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        goto fail;
    }
    if (!host_find_camera(host, host->edit, &host->edit_camera)) {
        host_error(error, error_capacity, "El nivel no contiene camara");
        goto fail;
    }
    host->edit_yaw = 0.0f;
    host->edit_pitch = -0.12f;
    host->orbit_target = (VgVec3){0.0f, 1.5f, 0.0f};
    host->orbit_distance = 8.0f;
    if (!vg_document_write_canonical(host->document, &host->saved_json,
                                      &host->saved_length, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        goto fail;
    }
    host->saved_revision = vg_document_revision(host->document);
    host_error(error, error_capacity, "");
    return true;
fail:
    host_release_level(host);
    return false;
}

int32_t vg_gpu_host_set_mode(VgGpuHost *host, int32_t play) {
    if (!host_is_current(host) || host->edit == NULL || (play != 0 && play != 1))
        return false;
    if (play == 0) {
        host_release_play(host);
        return true;
    }
    if (host->play != NULL)
        return true;
    VgDocumentDiagnostic diagnostic = {0};
    VgDocumentInstanceDesc description = {host_resolve_asset, host};
    if (vg_document_instantiate(host->context, host->document, &description,
                                &host->play, &diagnostic) != VG_OK)
        return false;
    if (vg_document_instance_spatial(host->play) == NULL) {
        host_release_play(host);
        return false;
    }
    if (!host_find_camera(host, host->play, &host->play_camera)) {
        host_release_play(host);
        return false;
    }
    VgTransform camera;
    if (vg_entity_get_local_transform(host->context, host->play_camera, &camera) != VG_OK) {
        host_release_play(host);
        return false;
    }
    size_t count = vg_document_instance_door_count(host->play);
    if (count > 0u) {
        host->doors = calloc(count, sizeof(*host->doors));
        if (host->doors == NULL) {
            host_release_play(host);
            return false;
        }
        for (size_t i = 0u; i < count; ++i) {
            VgDocumentDoorBinding binding;
            if (!vg_document_instance_door_at(host->play, i, &binding)) {
                host_release_play(host);
                return false;
            }
            VgDoorDesc door = {0};
            door.struct_size = sizeof(door);
            door.api_version = VG_API_VERSION;
            door.context = host->context;
            door.spatial = vg_document_instance_spatial(host->play);
            door.hinge = binding.hinge;
            door.panel = binding.panel;
            door.collider = binding.collider;
            door.collider_description = binding.collider_description;
            door.open_angle_radians = binding.open_angle_radians;
            door.angular_speed_radians = binding.speed_radians_per_second;
            if (vg_door_create(&door, &host->doors[i]) != VG_OK) {
                host_release_play(host);
                return false;
            }
            ++host->door_count;
        }
    }
    host->controller = vg_controller_default_config();
    host->controller.height = 1.82f;
    host->controller.eye_height = 1.7f;
    host->controller.collision_mask = UINT64_C(1);
    host->player = (VgControllerState){0};
    host->player.feet = camera.position;
    host->player.feet.z -= host->controller.eye_height;
    host->player.last_ground_height = host->player.feet.z;
    host->player.grounded = 1u;
    host->play_yaw = 0.0f;
    host->play_pitch = -0.12f;
    host->accumulator = 0.0;
    host->jump_pending = false;
    host->interact_pending = false;
    return true;
}

int32_t vg_gpu_host_mode(const VgGpuHost *host) {
    if (!host_is_current(host) || host->edit == NULL)
        return -1;
    return host->play != NULL ? 1 : 0;
}

int32_t vg_gpu_host_interact(VgGpuHost *host) {
    if (!host_is_current(host) || host->play == NULL)
        return false;
    host->interact_pending = true;
    return true;
}

size_t vg_gpu_host_door_count(const VgGpuHost *host) {
    return host_is_current(host) && host->play != NULL ? host->door_count : 0u;
}

int32_t vg_gpu_host_door_angle(const VgGpuHost *host, size_t index,
                                float *out_angle) {
    if (!host_is_current(host) || host->play == NULL || index >= host->door_count ||
        out_angle == NULL)
        return false;
    *out_angle = vg_door_angle(host->doors[index]);
    return true;
}

static VgVec3 host_rotate(VgQuat q, VgVec3 vector);

static bool host_step_edit(VgGpuHost *host, double elapsed, float x, float y,
                           int32_t elevation) {
    VgTransform camera;
    if (vg_entity_get_local_transform(host->context, host->edit_camera, &camera) != VG_OK)
        return false;
    float length = hypotf(x, y);
    if (length > 1.0f) { x /= length; y /= length; }
    float dt = (float)fmin(elapsed, 0.1);
    if (host->edit_camera_mode == 1) {
        host->orbit_distance -= y * 3.2f * dt;
        if (host->orbit_distance < 1.0f) host->orbit_distance = 1.0f;
        if (host->orbit_distance > 80.0f) host->orbit_distance = 80.0f;
        float cos_pitch = cosf(host->edit_pitch);
        camera.position = (VgVec3){
            host->orbit_target.x + sinf(host->edit_yaw) * cos_pitch * host->orbit_distance,
            host->orbit_target.y - cosf(host->edit_yaw) * cos_pitch * host->orbit_distance,
            host->orbit_target.z - sinf(host->edit_pitch) * host->orbit_distance};
        return host_set_camera(host, host->edit_camera, camera.position,
                               host->edit_yaw, host->edit_pitch) == VG_OK;
    }
    camera.position.x += (cosf(host->edit_yaw) * x - sinf(host->edit_yaw) * y) * 3.2f * dt;
    camera.position.y += (sinf(host->edit_yaw) * x + cosf(host->edit_yaw) * y) * 3.2f * dt;
    camera.position.z += (float)elevation * 3.2f * dt;
    return host_set_camera(host, host->edit_camera, camera.position,
                           host->edit_yaw, host->edit_pitch) == VG_OK;
}

static bool host_step_play(VgGpuHost *host, double elapsed, float x, float y, bool jump) {
    VgSpatialScene *spatial = vg_document_instance_spatial(host->play);
    if (spatial == NULL)
        return false;
    float length = hypotf(x, y);
    if (length > 1.0f) { x /= length; y /= length; }
    VgControllerInput input = {0};
    input.move_world.x = cosf(host->play_yaw) * x - sinf(host->play_yaw) * y;
    input.move_world.y = sinf(host->play_yaw) * x + cosf(host->play_yaw) * y;
    host->jump_pending = host->jump_pending || jump;
    input.jump_pressed = host->jump_pending ? 1u : 0u;
    host->accumulator += fmin(elapsed, 0.1);
    unsigned int ticks = 0u;
    while (host->accumulator >= 1.0 / 60.0 && ticks++ < 6u) {
        if (host->interact_pending) {
            VgVec3 eye = vg_controller_eye_position(&host->controller, &host->player);
            VgVec3 forward = host_rotate(host_rotation(host->play_yaw, host->play_pitch),
                                          (VgVec3){0.0f, 1.0f, 0.0f});
            VgSpatialRayQuery ray = {eye, forward, 3.0f,
                                     host->controller.collision_mask, host->play_camera};
            VgSpatialHit hit = {0};
            bool found = false;
            if (vg_spatial_raycast(spatial, &ray, &found, &hit) != VG_OK)
                return false;
            if (found) {
                for (size_t i = 0u; i < host->door_count; ++i) {
                    VgDocumentDoorBinding binding;
                    if (!vg_document_instance_door_at(host->play, i, &binding))
                        return false;
                    if (binding.panel.value == hit.entity.value) {
                        if (vg_door_toggle(host->doors[i]) != VG_OK)
                            return false;
                        break;
                    }
                }
            }
            host->interact_pending = false;
        }
        for (size_t i = 0u; i < host->door_count; ++i)
            if (vg_door_step(host->doors[i], 1.0f / 60.0f,
                             &host->controller, &host->player) != VG_OK)
                return false;
        if (vg_controller_step(spatial, &host->controller, host->play_camera,
                               &input, 1.0f / 60.0f, &host->player) != VG_OK)
            return false;
        host->jump_pending = false;
        input.jump_pressed = 0u;
        host->accumulator -= 1.0 / 60.0;
    }
    VgVec3 eye = vg_controller_eye_position(&host->controller, &host->player);
    return host_set_camera(host, host->play_camera, eye,
                           host->play_yaw, host->play_pitch) == VG_OK;
}

int32_t vg_gpu_host_frame(VgGpuHost *host, double elapsed_seconds,
                           float move_x, float move_y, float look_x, float look_y,
                           int32_t jump, int32_t focused) {
    if (!host_is_current(host) || host->edit == NULL || !isfinite(elapsed_seconds) ||
        elapsed_seconds < 0.0 || !isfinite(move_x) || !isfinite(move_y) ||
        !isfinite(look_x) || !isfinite(look_y) || fabsf(look_x) > 10000.0f ||
        fabsf(look_y) > 10000.0f || jump < -1 || jump > 1)
        return false;
    if (focused != 0) {
        float *yaw = host->play != NULL ? &host->play_yaw : &host->edit_yaw;
        float *pitch = host->play != NULL ? &host->play_pitch : &host->edit_pitch;
        *yaw -= look_x * 0.0025f;
        *pitch -= look_y * 0.0025f;
        if (*pitch > 1.25f) *pitch = 1.25f;
        if (*pitch < -1.25f) *pitch = -1.25f;
        bool stepped = host->play != NULL
            ? host_step_play(host, elapsed_seconds, move_x, move_y, jump != 0)
            : host_step_edit(host, elapsed_seconds, move_x, move_y, jump);
        if (!stepped)
            return false;
    } else {
        host->accumulator = 0.0;
        host->jump_pending = false;
        host->interact_pending = false;
    }
    return vg_gpu_host_render(host);
}

uint64_t vg_gpu_host_document_revision(const VgGpuHost *host) {
    return host_is_current(host) && host->document != NULL
        ? vg_document_revision(host->document) : 0u;
}

static void host_format_uuid(VgUuid id, char out[37]) {
    (void)snprintf(out, 37u,
                   "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   id.bytes[0], id.bytes[1], id.bytes[2], id.bytes[3],
                   id.bytes[4], id.bytes[5], id.bytes[6], id.bytes[7],
                   id.bytes[8], id.bytes[9], id.bytes[10], id.bytes[11],
                   id.bytes[12], id.bytes[13], id.bytes[14], id.bytes[15]);
}

static bool host_entity_has_mesh(const VgGpuHost *host, size_t index) {
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document),
                                                    "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY ||
        index >= entities->as.array.count)
        return false;
    const VgJsonNode *components = vg_json_object_get(entities->as.array.items[index],
                                                       "components");
    return vg_json_object_get(components, "engine.mesh") != NULL;
}

size_t vg_gpu_host_entity_count(const VgGpuHost *host) {
    if (!host_is_current(host) || host->edit == NULL)
        return 0u;
    size_t count = 0u;
    for (size_t i = 0u; i < vg_document_instance_entity_count(host->edit); ++i)
        if (host_entity_has_mesh(host, i))
            ++count;
    return count;
}

int32_t vg_gpu_host_entity_at(const VgGpuHost *host, size_t index,
                              char *uuid, size_t uuid_capacity) {
    if (!host_is_current(host) || host->edit == NULL || uuid == NULL || uuid_capacity < 37u)
        return false;
    size_t mesh_index = 0u;
    for (size_t i = 0u; i < vg_document_instance_entity_count(host->edit); ++i) {
        if (!host_entity_has_mesh(host, i))
            continue;
        if (mesh_index++ != index)
            continue;
        VgUuid id;
        VgEntity entity;
        if (!vg_document_instance_entity_at(host->edit, i, &id, &entity))
            return false;
        host_format_uuid(id, uuid);
        return true;
    }
    return false;
}

int32_t vg_gpu_host_select(VgGpuHost *host, const char *uuid) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL || uuid == NULL)
        return false;
    if (uuid[0] == '\0') {
        host->has_selection = false;
        host->selected_uuid[0] = '\0';
        return true;
    }
    char current[37];
    size_t count = vg_document_instance_entity_count(host->edit);
    for (size_t index = 0u; index < count; ++index) {
        VgUuid id;
        VgEntity entity;
        if (!vg_document_instance_entity_at(host->edit, index, &id, &entity) ||
            entity.value == host->edit_camera.value)
            continue;
        host_format_uuid(id, current);
        if (strcmp(current, uuid) != 0)
            continue;
        VgMeshRendererDesc mesh = {0};
        mesh.struct_size = sizeof(mesh);
        mesh.api_version = VG_API_VERSION;
        bool has_mesh = vg_mesh_renderer_get(host->context, entity, &mesh) == VG_OK;
        host->selected_entity = entity;
        host->selected_center = has_mesh ? mesh.bounds_center : (VgVec3){0};
        host->selected_extent = has_mesh ? mesh.bounds_extent
                                         : (VgVec3){0.25f, 0.25f, 0.25f};
        host->has_selection = true;
        (void)snprintf(host->selected_uuid, sizeof(host->selected_uuid), "%s", current);
        if (has_mesh)
            (void)vg_asset_release(host->context, mesh.asset);
        return true;
    }
    return false;
}

int32_t vg_gpu_host_selected_uuid(const VgGpuHost *host, char *uuid, size_t uuid_capacity) {
    if (!host_is_current(host) || !host->has_selection || uuid == NULL || uuid_capacity < 37u)
        return false;
    (void)snprintf(uuid, uuid_capacity, "%s", host->selected_uuid);
    return true;
}

int32_t vg_gpu_host_selected_transform(const VgGpuHost *host,
                                        float position[3], float rotation[4], float scale[3]) {
    if (!host_is_current(host) || host->play != NULL || !host->has_selection ||
        position == NULL || rotation == NULL || scale == NULL)
        return false;
    VgDocumentTransform transform;
    if (!vg_document_entity_transform(host->document, host->selected_uuid, &transform, NULL))
        return false;
    for (size_t i = 0u; i < 3u; ++i) {
        position[i] = (float)transform.position[i];
        scale[i] = (float)transform.scale[i];
    }
    for (size_t i = 0u; i < 4u; ++i)
        rotation[i] = (float)transform.rotation[i];
    return true;
}

static bool host_prepare_instance(VgGpuHost *host, const VgDocument *document,
                                  VgDocumentInstance **out_edit, VgEntity *out_camera,
                                  VgDocumentDiagnostic *diagnostic, bool preserve_camera) {
    VgDocumentInstanceDesc description = {host_resolve_asset, host};
    VgDocumentInstance *candidate = NULL;
    if (vg_document_instantiate(host->context, document, &description,
                                &candidate, diagnostic) != VG_OK)
        return false;
    VgEntity camera;
    if (!host_find_camera(host, candidate, &camera)) {
        vg_document_instance_destroy(candidate);
        host_error(diagnostic->message, sizeof(diagnostic->message),
                   "El nivel no contiene camara");
        return false;
    }
    if (preserve_camera) {
        VgTransform old_transform;
        VgCameraDesc old_camera = {0};
        old_camera.struct_size = sizeof(old_camera);
        old_camera.api_version = VG_API_VERSION;
        if (vg_entity_get_local_transform(host->context, host->edit_camera,
                                           &old_transform) != VG_OK ||
            vg_camera_get(host->context, host->edit_camera, &old_camera) != VG_OK ||
            vg_entity_set_local_transform(host->context, camera, &old_transform) != VG_OK ||
            vg_camera_set(host->context, camera, &old_camera) != VG_OK) {
            vg_document_instance_destroy(candidate);
            host_error(diagnostic->message, sizeof(diagnostic->message),
                       "No se pudo conservar la camara de edicion");
            return false;
        }
    }
    *out_edit = candidate;
    *out_camera = camera;
    return true;
}

static void host_swap_edit(VgGpuHost *host, VgDocumentInstance *candidate,
                           VgEntity camera) {
    char selected[37];
    (void)snprintf(selected, sizeof(selected), "%s", host->selected_uuid);
    vg_document_instance_destroy(host->edit);
    host->edit = candidate;
    host->edit_camera = camera;
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    if (selected[0] != '\0')
        (void)vg_gpu_host_select(host, selected);
}

static bool host_apply_batch(VgGpuHost *host, VgToolBatch *batch,
                             VgToolResult *out_result, char *error, size_t error_capacity) {
    VgDocumentDiagnostic diagnostic = {0};
    char *json = NULL;
    size_t length = 0u;
    VgToolResult preview_result;
    if (!vg_tool_preview(batch, &json, &length, &preview_result, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgDocument *preview = NULL;
    bool opened = vg_document_open_memory("<edit-preview>", json, length,
                                          &preview, &diagnostic);
    free(json);
    if (!opened) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera;
    bool prepared = host_prepare_instance(host, preview, &candidate, &camera,
                                           &diagnostic, true);
    vg_document_destroy(preview);
    if (!prepared) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult committed;
    if (!vg_tool_commit(batch, &committed, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_document_instance_destroy(candidate);
        vg_tool_cancel(batch);
        return false;
    }
    host_swap_edit(host, candidate, camera);
    if (out_result != NULL)
        *out_result = committed;
    host_error(error, error_capacity, "");
    return true;
}

static VgToolBatch *host_begin_edit(VgGpuHost *host, char *error,
                                    size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || host->edit == NULL ||
        host->play != NULL) {
        host_error(error, error_capacity, "La edicion requiere el modo Editar");
        return NULL;
    }
    VgDocumentDiagnostic diagnostic = {0};
    VgToolBatch *batch = NULL;
    if (!vg_tool_begin(host->document, vg_document_revision(host->document),
                       &batch, &diagnostic))
        host_error(error, error_capacity, diagnostic.message);
    return batch;
}

static bool host_result_id(const VgToolResult *result, const char *temporary,
                            char *uuid, size_t uuid_capacity) {
    for (size_t i = 0u; i < result->mapping_count; ++i) {
        if (strcmp(result->mappings[i].temporary, temporary) == 0) {
            (void)snprintf(uuid, uuid_capacity, "%s", result->mappings[i].id);
            return true;
        }
    }
    return false;
}

int32_t vg_gpu_host_add_mesh(VgGpuHost *host, char *uuid, size_t uuid_capacity,
                              char *error, size_t error_capacity) {
    if (uuid == NULL || uuid_capacity < 37u) {
        host_error(error, error_capacity, "Se requiere un UUID de salida");
        return false;
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    static const char *components =
        "{\"engine.mesh\":{\"version\":1,\"asset\":\"4a30312d-6174-7269-756d-2d6d6f64656c\",\"node_index\":2},"
        "\"engine.collider\":{\"version\":1,\"shape\":\"box\",\"motion\":\"static\","
        "\"center\":[0,0,0],\"half_extents\":[0.5,0.5,0.5]}}";
    VgDocumentTransform transform = {
        {0.0, -3.0, 0.95}, {0.0, 0.0, 0.0, 1.0}, {0.65, 0.65, 1.9}};
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_tool_create_entity(batch, "$new-pillar", NULL, &transform,
                               components, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    if (!host_result_id(&result, "$new-pillar", uuid, uuid_capacity)) {
        host_error(error, error_capacity, "No se devolvio el UUID creado");
        return false;
    }
    (void)vg_gpu_host_select(host, uuid);
    return true;
}

int32_t vg_gpu_host_duplicate_selected(VgGpuHost *host, char *uuid, size_t uuid_capacity,
                                        char *error, size_t error_capacity) {
    if (uuid == NULL || uuid_capacity < 37u || !host_is_current(host) ||
        !host->has_selection) {
        host_error(error, error_capacity, "Selecciona un objeto para duplicarlo");
        return false;
    }
    for (size_t i = 0u; i < vg_document_instance_door_count(host->edit); ++i) {
        VgDocumentDoorBinding binding;
        if (!vg_document_instance_door_at(host->edit, i, &binding)) {
            host_error(error, error_capacity, "No se pudo consultar la puerta seleccionada");
            return false;
        }
        if (binding.panel.value == host->selected_entity.value) {
            host_error(error, error_capacity,
                       "No se puede duplicar un panel de puerta sin una bisagra propia");
            return false;
        }
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    VgDocumentTransform transform;
    if (!vg_document_entity_transform(host->document, host->selected_uuid,
                                      &transform, &diagnostic) ||
        !vg_tool_duplicate_entity(batch, host->selected_uuid, "$duplicate", NULL,
                                  &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    transform.position[0] += 1.5;
    if (!vg_tool_set_transform(batch, "$duplicate", &transform, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    if (!host_result_id(&result, "$duplicate", uuid, uuid_capacity)) {
        host_error(error, error_capacity, "No se devolvio el UUID duplicado");
        return false;
    }
    (void)vg_gpu_host_select(host, uuid);
    return true;
}

int32_t vg_gpu_host_set_selected_transform(VgGpuHost *host,
                                            const float position[3],
                                            const float rotation[4],
                                            const float scale[3], char *error,
                                            size_t error_capacity) {
    if (!host_is_current(host) || !host->has_selection || position == NULL ||
        rotation == NULL || scale == NULL) {
        host_error(error, error_capacity, "Selecciona un objeto y transformacion valida");
        return false;
    }
    VgDocumentTransform transform = {0};
    for (size_t i = 0u; i < 3u; ++i) {
        if (!isfinite(position[i]) || !isfinite(scale[i]) || scale[i] <= 0.0f) {
            host_error(error, error_capacity, "Posicion o escala invalida");
            return false;
        }
        transform.position[i] = position[i];
        transform.scale[i] = scale[i];
    }
    double norm = 0.0;
    for (size_t i = 0u; i < 4u; ++i) {
        if (!isfinite(rotation[i])) {
            host_error(error, error_capacity, "Rotacion invalida");
            return false;
        }
        norm += (double)rotation[i] * rotation[i];
    }
    if (norm < 1.0e-10) {
        host_error(error, error_capacity, "Rotacion nula");
        return false;
    }
    norm = sqrt(norm);
    for (size_t i = 0u; i < 4u; ++i)
        transform.rotation[i] = rotation[i] / norm;
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_tool_set_transform(batch, host->selected_uuid, &transform, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    return host_apply_batch(host, batch, NULL, error, error_capacity);
}

int32_t vg_gpu_host_save_level(VgGpuHost *host, const char *path,
                                char *error, size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || host->play != NULL ||
        path == NULL || path[0] == '\0') {
        host_error(error, error_capacity, "Guardar requiere una ruta y el modo Editar");
        return false;
    }
    VgDocumentDiagnostic diagnostic = {0};
    uint64_t revision = vg_document_revision(host->document);
    char *canonical = NULL;
    size_t length = 0u;
    if (!vg_document_write_canonical(host->document, &canonical, &length, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    if (!vg_document_save_atomic(host->document, path, revision, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        free(canonical);
        return false;
    }
    free(host->saved_json);
    host->saved_json = canonical;
    host->saved_length = length;
    host->saved_revision = revision;
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_is_dirty(const VgGpuHost *host) {
    if (!host_is_current(host) || host->document == NULL)
        return false;
    if (vg_document_revision(host->document) == host->saved_revision)
        return false;
    char *canonical = NULL;
    size_t length = 0u;
    if (!vg_document_write_canonical(host->document, &canonical, &length, NULL))
        return true;
    bool dirty = length != host->saved_length || host->saved_json == NULL ||
                 memcmp(canonical, host->saved_json, length) != 0;
    free(canonical);
    return dirty;
}

int32_t vg_gpu_host_reopen_level(VgGpuHost *host, const char *level_path,
                                  const char *model_path, char *error,
                                  size_t error_capacity) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        level_path == NULL || level_path[0] == '\0' || model_path == NULL ||
        model_path[0] == '\0') {
        host_error(error, error_capacity, "Reabrir requiere rutas y el modo Editar");
        return false;
    }
    void *model = NULL;
    uint64_t model_size = 0u;
    if (!host_read_model(model_path, &model, &model_size)) {
        host_error(error, error_capacity, "No se pudo leer el modelo Atrium");
        return false;
    }
    bool same_model = model_size == host->model_size &&
                      memcmp(model, host->model_data, (size_t)model_size) == 0;
    free(model);
    if (!same_model) {
        host_error(error, error_capacity, "El modelo difiere del Atrium cargado");
        return false;
    }
    VgDocumentDiagnostic diagnostic = {0};
    VgDocument *document = NULL;
    if (!vg_document_open_file(level_path, &document, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    char *canonical = NULL;
    size_t length = 0u;
    if (!vg_document_write_canonical(document, &canonical, &length, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_document_destroy(document);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera;
    if (!host_prepare_instance(host, document, &candidate, &camera,
                               &diagnostic, true)) {
        host_error(error, error_capacity, diagnostic.message);
        free(canonical);
        vg_document_destroy(document);
        return false;
    }
    host_swap_edit(host, candidate, camera);
    vg_document_destroy(host->document);
    host->document = document;
    free(host->saved_json);
    host->saved_json = canonical;
    host->saved_length = length;
    host->saved_revision = vg_document_revision(document);
    host_error(error, error_capacity, "");
    return true;
}

static int32_t host_history(VgGpuHost *host, bool undo, char *error,
                             size_t error_capacity) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL) {
        host_error(error, error_capacity, "Historial requiere el modo Editar");
        return false;
    }
    VgDocumentDiagnostic diagnostic = {0};
    uint64_t revision = vg_document_revision(host->document);
    bool moved = undo ? vg_document_undo(host->document, revision, &diagnostic)
                      : vg_document_redo(host->document, revision, &diagnostic);
    if (!moved) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera;
    if (!host_prepare_instance(host, host->document, &candidate, &camera,
                               &diagnostic, true)) {
        char failure[256];
        (void)snprintf(failure, sizeof(failure), "%s", diagnostic.message);
        uint64_t rollback_revision = vg_document_revision(host->document);
        bool rolled_back = undo
            ? vg_document_redo(host->document, rollback_revision, &diagnostic)
            : vg_document_undo(host->document, rollback_revision, &diagnostic);
        host_error(error, error_capacity, rolled_back ? failure : diagnostic.message);
        return false;
    }
    host_swap_edit(host, candidate, camera);
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_undo(VgGpuHost *host, char *error, size_t error_capacity) {
    return host_history(host, true, error, error_capacity);
}

int32_t vg_gpu_host_redo(VgGpuHost *host, char *error, size_t error_capacity) {
    return host_history(host, false, error, error_capacity);
}

uint64_t vg_gpu_host_readbacks(const VgGpuHost *host) {
    return host_is_current(host) && host->renderer != NULL
        ? vg_gpu_renderer_stats(host->renderer).readbacks : 0u;
}

int32_t vg_gpu_host_camera_position(const VgGpuHost *host,
                                     float *x, float *y, float *z) {
    if (!host_is_current(host) || host->edit == NULL || x == NULL || y == NULL || z == NULL)
        return false;
    VgTransform camera;
    VgEntity entity = host->play != NULL ? host->play_camera : host->edit_camera;
    if (vg_entity_get_world_transform(host->context, entity, &camera) != VG_OK)
        return false;
    *x = camera.position.x;
    *y = camera.position.y;
    *z = camera.position.z;
    return true;
}

static VgVec3 host_rotate(VgQuat q, VgVec3 vector) {
    VgVec3 t = {2.0f * (q.y * vector.z - q.z * vector.y),
                2.0f * (q.z * vector.x - q.x * vector.z),
                2.0f * (q.x * vector.y - q.y * vector.x)};
    return (VgVec3){vector.x + q.w * t.x + q.y * t.z - q.z * t.y,
                    vector.y + q.w * t.y + q.z * t.x - q.x * t.z,
                    vector.z + q.w * t.z + q.x * t.y - q.y * t.x};
}

static bool host_ray_bounds(VgVec3 origin, VgVec3 direction, VgVec3 center,
                            VgVec3 extent, float *out_distance) {
    float o[3] = {origin.x, origin.y, origin.z};
    float d[3] = {direction.x, direction.y, direction.z};
    float c[3] = {center.x, center.y, center.z};
    float e[3] = {extent.x, extent.y, extent.z};
    float near_t = 0.0f, far_t = 100000.0f;
    for (size_t axis = 0u; axis < 3u; ++axis) {
        if (fabsf(d[axis]) < 1.0e-7f) {
            if (o[axis] < c[axis] - e[axis] || o[axis] > c[axis] + e[axis])
                return false;
            continue;
        }
        float a = (c[axis] - e[axis] - o[axis]) / d[axis];
        float b = (c[axis] + e[axis] - o[axis]) / d[axis];
        if (a > b) { float swap = a; a = b; b = swap; }
        if (a > near_t) near_t = a;
        if (b < far_t) far_t = b;
        if (near_t > far_t) return false;
    }
    *out_distance = near_t;
    return true;
}

int32_t vg_gpu_host_pick(VgGpuHost *host, float u, float v,
                          char *uuid, size_t uuid_capacity) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        uuid == NULL || uuid_capacity < 37u || !isfinite(u) || !isfinite(v) ||
        u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
        return false;
    uuid[0] = '\0';
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    unsigned int width = 0u, height = 0u;
    if (!vg_win32_embedded_size(host->window, &width, &height) || height == 0u)
        return false;
    float scale = fminf((float)width / 320.0f, (float)height / 180.0f);
    if (scale >= 1.0f)
        scale = floorf(scale);
    if (scale <= 0.0f)
        return false;
    float draw_width = 320.0f * scale, draw_height = 180.0f * scale;
    float x = u * (float)width - ((float)width - draw_width) * 0.5f;
    float y = v * (float)height - ((float)height - draw_height) * 0.5f;
    if (x < 0.0f || x > draw_width || y < 0.0f || y > draw_height)
        return false;
    u = x / draw_width;
    v = y / draw_height;
    VgTransform camera;
    VgCameraDesc camera_desc = {0};
    camera_desc.struct_size = sizeof(camera_desc);
    camera_desc.api_version = VG_API_VERSION;
    if (vg_entity_get_world_transform(host->context, host->edit_camera, &camera) != VG_OK ||
        vg_camera_get(host->context, host->edit_camera, &camera_desc) != VG_OK)
        return false;
    VgVec3 origin = camera.position;
    VgVec3 direction;
    if (camera_desc.projection == VG_CAMERA_ORTHOGRAPHIC) {
        float half_height = camera_desc.orthographic_height * 0.5f;
        float right_offset = (2.0f * u - 1.0f) * half_height * (320.0f / 180.0f);
        float up_offset = (1.0f - 2.0f * v) * half_height;
        VgVec3 lateral = host_rotate(camera.rotation, (VgVec3){right_offset, 0.0f, up_offset});
        origin.x += lateral.x; origin.y += lateral.y; origin.z += lateral.z;
        direction = host_rotate(camera.rotation, (VgVec3){0.0f, 1.0f, 0.0f});
    } else {
        float tan_half = tanf(camera_desc.vertical_fov_radians * 0.5f);
        VgVec3 camera_ray = {(2.0f * u - 1.0f) * tan_half * (320.0f / 180.0f),
                             1.0f, (1.0f - 2.0f * v) * tan_half};
        float ray_length = sqrtf(camera_ray.x * camera_ray.x + camera_ray.y * camera_ray.y +
                                 camera_ray.z * camera_ray.z);
        camera_ray.x /= ray_length;
        camera_ray.y /= ray_length;
        camera_ray.z /= ray_length;
        direction = host_rotate(camera.rotation, camera_ray);
    }
    float nearest = 100000.0f;
    VgUuid chosen = {{0}};
    VgEntity chosen_entity = {0};
    VgVec3 chosen_center = {0}, chosen_extent = {0};
    bool found = false;
    size_t count = vg_document_instance_entity_count(host->edit);
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY || entities->as.array.count != count)
        return false;
    for (size_t index = 0u; index < count; ++index) {
        VgUuid id;
        VgEntity entity;
        VgTransform transform;
        if (!vg_document_instance_entity_at(host->edit, index, &id, &entity) ||
            entity.value == host->edit_camera.value)
            continue;
        const VgJsonNode *components =
            vg_json_object_get(entities->as.array.items[index], "components");
        uint32_t kind = 0u;
        if (vg_json_object_get(components, "engine.mesh") != NULL)
            kind = VG_GPU_PICK_MESH;
        else if (vg_json_object_get(components, "engine.camera") != NULL)
            kind = VG_GPU_PICK_CAMERA;
        else if (vg_json_object_get(components, "engine.light") != NULL)
            kind = VG_GPU_PICK_LIGHT;
        else if (vg_json_object_get(components, "engine.trigger") != NULL)
            kind = VG_GPU_PICK_TRIGGER;
        if ((host->pick_mask & kind) == 0u ||
            vg_entity_get_world_transform(host->context, entity, &transform) != VG_OK)
            continue;
        VgVec3 center = {0};
        VgVec3 extent = {0.25f, 0.25f, 0.25f};
        if (kind == VG_GPU_PICK_MESH) {
            VgMeshRendererDesc mesh = {0};
            mesh.struct_size = sizeof(mesh);
            mesh.api_version = VG_API_VERSION;
            if (vg_mesh_renderer_get(host->context, entity, &mesh) != VG_OK)
                continue;
            center = mesh.bounds_center;
            extent = mesh.bounds_extent;
            (void)vg_asset_release(host->context, mesh.asset);
        }
        VgQuat inverse = {-transform.rotation.x, -transform.rotation.y,
                           -transform.rotation.z, transform.rotation.w};
        VgVec3 offset = {origin.x - transform.position.x,
                         origin.y - transform.position.y,
                         origin.z - transform.position.z};
        VgVec3 local_origin = host_rotate(inverse, offset);
        VgVec3 local_direction = host_rotate(inverse, direction);
        if (transform.scale.x <= 0.0f || transform.scale.y <= 0.0f ||
            transform.scale.z <= 0.0f)
            continue;
        local_origin.x /= transform.scale.x;
        local_origin.y /= transform.scale.y;
        local_origin.z /= transform.scale.z;
        local_direction.x /= transform.scale.x;
        local_direction.y /= transform.scale.y;
        local_direction.z /= transform.scale.z;
        float distance = 0.0f;
        if (host_ray_bounds(local_origin, local_direction, center,
                            extent, &distance) && distance < nearest) {
            nearest = distance;
            chosen = id;
            chosen_entity = entity;
            chosen_center = center;
            chosen_extent = extent;
            found = true;
        }
    }
    if (!found)
        return false;
    host->selected_entity = chosen_entity;
    host->has_selection = true;
    host->selected_center = chosen_center;
    host->selected_extent = chosen_extent;
    (void)snprintf(uuid, uuid_capacity,
                   "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   chosen.bytes[0], chosen.bytes[1], chosen.bytes[2], chosen.bytes[3],
                   chosen.bytes[4], chosen.bytes[5], chosen.bytes[6], chosen.bytes[7],
                   chosen.bytes[8], chosen.bytes[9], chosen.bytes[10], chosen.bytes[11],
                   chosen.bytes[12], chosen.bytes[13], chosen.bytes[14], chosen.bytes[15]);
    (void)snprintf(host->selected_uuid, sizeof(host->selected_uuid), "%s", uuid);
    return true;
}

int32_t vg_gpu_host_frame_selection(VgGpuHost *host) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        !host->has_selection)
        return false;
    VgTransform transform;
    if (vg_entity_get_world_transform(host->context, host->selected_entity,
                                       &transform) != VG_OK)
        return false;
    VgVec3 center = {host->selected_center.x * transform.scale.x,
                     host->selected_center.y * transform.scale.y,
                     host->selected_center.z * transform.scale.z};
    VgVec3 rotated_center = host_rotate(transform.rotation, center);
    host->orbit_target = transform.position;
    host->orbit_target.x += rotated_center.x;
    host->orbit_target.y += rotated_center.y;
    host->orbit_target.z += rotated_center.z;
    float extent_x = fabsf(host->selected_extent.x * transform.scale.x);
    float extent_y = fabsf(host->selected_extent.y * transform.scale.y);
    float extent_z = fabsf(host->selected_extent.z * transform.scale.z);
    float largest = fmaxf(extent_x, fmaxf(extent_y, extent_z));
    host->orbit_distance = fmaxf(2.0f, fminf(60.0f, largest * 3.0f));
    float cos_pitch = cosf(host->edit_pitch);
    VgVec3 eye = {
        host->orbit_target.x + sinf(host->edit_yaw) * cos_pitch * host->orbit_distance,
        host->orbit_target.y - cosf(host->edit_yaw) * cos_pitch * host->orbit_distance,
        host->orbit_target.z - sinf(host->edit_pitch) * host->orbit_distance};
    return host_set_camera(host, host->edit_camera, eye,
                           host->edit_yaw, host->edit_pitch) == VG_OK;
}

int32_t vg_gpu_host_set_camera_mode(VgGpuHost *host, int32_t mode) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        mode < 0 || mode > 2)
        return false;
    VgCameraDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    if (vg_camera_get(host->context, host->edit_camera, &description) != VG_OK)
        return false;
    description.projection = mode == 2 ? VG_CAMERA_ORTHOGRAPHIC : VG_CAMERA_PERSPECTIVE;
    if (mode == 2)
        description.orthographic_height = 10.0f;
    if (vg_camera_set(host->context, host->edit_camera, &description) != VG_OK)
        return false;
    host->edit_camera_mode = mode;
    if (mode == 1 && host->has_selection)
        return vg_gpu_host_frame_selection(host);
    return true;
}

int32_t vg_gpu_host_set_pick_mask(VgGpuHost *host, uint32_t mask) {
    const uint32_t all = VG_GPU_PICK_MESH | VG_GPU_PICK_CAMERA |
                         VG_GPU_PICK_LIGHT | VG_GPU_PICK_TRIGGER;
    if (!host_is_current(host) || (mask & ~all) != 0u)
        return false;
    host->pick_mask = mask;
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    return true;
}

int32_t vg_gpu_host_resize(VgGpuHost *host, unsigned int width, unsigned int height) {
    if (!host_is_current(host) || !host->window || width == 0u || height == 0u ||
        width > (unsigned int)INT_MAX || height > (unsigned int)INT_MAX)
        return false;
    return vg_win32_resize_embedded(host->window, (int)width, (int)height);
}

int32_t vg_gpu_host_size(VgGpuHost *host, unsigned int *width, unsigned int *height) {
    return host_is_current(host) && vg_win32_embedded_size(host->window, width, height);
}

int32_t vg_gpu_host_capture(VgGpuHost *host, const char *path) {
    return host_is_current(host) && host->renderer && path && path[0] != '\0' &&
           vg_gpu_renderer_capture(host->renderer, path);
}

int32_t vg_gpu_host_focus(VgGpuHost *host) {
    return host_is_current(host) && vg_win32_focus_embedded(host->window);
}

int32_t vg_gpu_host_has_focus(const VgGpuHost *host) {
    return host_is_current(host) && vg_win32_embedded_has_focus(host->window);
}

int32_t vg_gpu_host_destroy(VgGpuHost *host) {
    if (!host_is_current(host))
        return false;
    host_release_level(host);
    vg_gpu_renderer_destroy(host->renderer);
    vg_win32_unembed_window(host->window);
    CloseWindow();
    atomic_store_explicit(&active_host, NULL, memory_order_release);
    free(host);
    return true;
}
