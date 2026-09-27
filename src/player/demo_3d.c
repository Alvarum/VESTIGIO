#include "player/demo_3d.h"

#include "player/demo_scene.h"
#include "render/gpu_raylib/gpu_renderer.h"
#include "raylib.h"

#include <errno.h>
#include <math.h>
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

static const char *demo_settings_path(const char *requested, char path[2048]) {
    if (requested != NULL)
        return requested;
#ifdef _WIN32
    wchar_t wide[2048];
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", wide, 1800);
    if (length == 0 || length >= 1800)
        return NULL;
    const wchar_t suffix[] = L"\\VESTIGIO";
    memcpy(wide + length, suffix, sizeof(suffix));
    if (!CreateDirectoryW(wide, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return NULL;
    const wchar_t file[] = L"\\visual.settings";
    memcpy(wide + length + (sizeof(suffix) / sizeof(suffix[0]) - 1u),
           file, sizeof(file));
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1,
                               path, 2048, NULL, NULL) != 0 ? path : NULL;
#else
    const char *base = getenv("LOCALAPPDATA");
    if (base == NULL || base[0] == '\0')
        return NULL;
    int length = snprintf(path, 2048, "%s/VESTIGIO", base);
    if (length < 0 || length >= 2048)
        return NULL;
    length = snprintf(path, 2048, "%s/VESTIGIO/visual.settings", base);
    return length >= 0 && length < 2048 ? path : NULL;
#endif
}

static int demo_settings_file_state(const char *path) {
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

static bool demo_apply_visual(VgGpuRenderer *renderer, VgContext *context,
                              const VgDocumentInstance *instance, uint32_t profile,
                              char *error, size_t error_capacity) {
    VgGpuVisualSettings visual = vg_gpu_renderer_default_visual_settings();
    visual.mode = profile == VG_VISUAL_PROFILE_RETRO ? VG_GPU_VISUAL_RETRO
                                                     : VG_GPU_VISUAL_CLEAN;
    VgDocumentEnvironment environment;
    if (!vg_document_instance_environment(instance, &environment))
        return false;
    memcpy(visual.ambient, environment.ambient_linear, sizeof(visual.ambient));
    memcpy(visual.clear_color, environment.clear_linear, sizeof(visual.clear_color));
    memcpy(visual.fog_color, environment.fog_color_linear, sizeof(visual.fog_color));
    visual.fog_enabled = environment.fog_enabled ? 1u : 0u;
    visual.fog_start = environment.fog_start;
    visual.fog_end = environment.fog_end;
    size_t count = vg_document_instance_light_count(instance);
    if (count > VG_GPU_MAX_POINT_LIGHTS)
        return false;
    visual.point_light_count = (uint32_t)count;
    for (size_t i = 0u; i < count; ++i) {
        VgDocumentLightBinding binding;
        VgTransform world;
        if (!vg_document_instance_light_at(instance, i, &binding) ||
            vg_entity_get_world_transform(context, binding.entity, &world) != VG_OK)
            return false;
        visual.lights[i].position = world.position;
        visual.lights[i].radius = binding.range;
        visual.lights[i].intensity = binding.intensity;
        memcpy(visual.lights[i].color, binding.color_linear,
               sizeof(visual.lights[i].color));
    }
    return vg_gpu_renderer_set_visual_settings(renderer, &visual, error, error_capacity);
}

static bool demo_read_model(const char *path, void **out_data, uint64_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    long size = 0;
    bool ok = fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && size <= 1024 * 1024 &&
              fseek(file, 0, SEEK_SET) == 0;
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

static VgInputSample demo_input_sample(bool focused, bool smoke, bool smoke_door,
                                       int frame) {
    VgInputSample sample = {0};
    sample.struct_size = sizeof(sample);
    sample.api_version = VG_API_VERSION;
    if ((smoke && !smoke_door) || (!smoke && IsKeyDown(KEY_W)))
        sample.held |= VG_ACTION_MOVE_FORWARD;
    if (!smoke && IsKeyDown(KEY_S))
        sample.held |= VG_ACTION_MOVE_BACKWARD;
    if (!smoke && IsKeyDown(KEY_D))
        sample.held |= VG_ACTION_MOVE_RIGHT;
    if (!smoke && IsKeyDown(KEY_A))
        sample.held |= VG_ACTION_MOVE_LEFT;
    if (!smoke && IsKeyPressed(KEY_SPACE))
        sample.pressed |= VG_ACTION_JUMP;
    if (!smoke && IsKeyPressed(KEY_E))
        sample.pressed |= VG_ACTION_INTERACT;
    if (smoke_door && (frame == 0 || frame == 91))
        sample.pressed |= VG_ACTION_INTERACT;
    if (smoke_door)
        sample.look_delta_x = frame == 160 ? 400.0f :
                              frame == 162 ? -400.0f : 0.0f;
    else if (smoke)
        sample.look_delta_x = frame == 0 ? 8.0f : 0.0f;
    else if (focused) {
        Vector2 look = GetMouseDelta();
        sample.look_delta_x = look.x;
        sample.look_delta_y = look.y;
    }
    sample.focused = focused ? 1u : 0u;
    return sample;
}

int vg_demo_3d_run(int smoke_frames, const char *capture_path, bool show_colliders,
                   const char *level_path, bool smoke_door,
                   const char *requested_settings_path,
                   const VgSettingsLayer *session_settings) {
    if (smoke_door && smoke_frames == 0)
        smoke_frames = 180;
    if (smoke_door && smoke_frames < 180) {
        (void)fprintf(stderr, "--smoke-door requiere al menos 180 frames\n");
        return 2;
    }
    char default_settings_path[2048] = {0};
    const char *settings_path = smoke_frames > 0 && requested_settings_path == NULL
        ? NULL : demo_settings_path(requested_settings_path, default_settings_path);
    if (requested_settings_path != NULL && requested_settings_path[0] == '\0') {
        (void)fprintf(stderr, "Ruta de settings vacia\n");
        return 2;
    }
    VgSettingsLayer user_settings = {0};
    user_settings.struct_size = sizeof(user_settings);
    user_settings.api_version = VG_API_VERSION;
    const VgSettingsLayer *user = NULL;
    if (settings_path != NULL) {
        int file_state = demo_settings_file_state(settings_path);
        if (file_state == 1) {
            VgSettingsDiagnostic diagnostic = {0};
            diagnostic.struct_size = sizeof(diagnostic);
            diagnostic.api_version = VG_API_VERSION;
            if (vg_settings_load_file(settings_path, &user_settings, &diagnostic) != VG_OK) {
                (void)fprintf(stderr, "Settings de usuario invalidos: %s\n",
                              diagnostic.message);
                return 2;
            }
            user = &user_settings;
        } else if (file_state < 0) {
            (void)fprintf(stderr, "No se pudo leer settings de usuario: %s\n",
                          settings_path);
            return 2;
        }
    }
    VgSettingsLayer settings = {0};
    settings.struct_size = sizeof(settings);
    settings.api_version = VG_API_VERSION;
    VgSettingsDiagnostic settings_error = {0};
    settings_error.struct_size = sizeof(settings_error);
    settings_error.api_version = VG_API_VERSION;
    if (vg_settings_resolve(NULL, user, session_settings, &settings, &settings_error) != VG_OK ||
        settings.internal_width > 1280u || settings.internal_height > 720u) {
        (void)fprintf(stderr, "Configuracion 3D invalida: %s (maximo 1280x720)\n",
                      settings_error.message);
        return 2;
    }
    char model_path[2048] = {0};
    char default_level_path[2048] = {0};
    void *model_data = NULL;
    uint64_t model_size = 0u;
    int path_length = snprintf(model_path, sizeof(model_path), "%sassets/demo/atrium.gltf",
                               GetApplicationDirectory());
    if (path_length < 0 || (size_t)path_length >= sizeof(model_path) ||
        !demo_read_model(model_path, &model_data, &model_size)) {
        (void)fprintf(stderr, "No se pudo leer el modelo 3D: %s\n", model_path);
        return 1;
    }
    path_length = snprintf(default_level_path, sizeof(default_level_path),
                           "%sassets/demo/atrium.level.json",
                           GetApplicationDirectory());
    if (path_length < 0 || (size_t)path_length >= sizeof(default_level_path)) {
        (void)fprintf(stderr, "Ruta de nivel 3D demasiado larga\n");
        free(model_data);
        return 2;
    }
    if (level_path == NULL)
        level_path = default_level_path;

    VgGpuRenderer *renderer = NULL;
    VgContext *context = NULL;
    VgDemoScene *scene = NULL;
    bool gpu_attached = false;
    bool window_ready = false;
    bool cursor_captured = false;
    int exit_code = 1;
    SetTraceLogLevel(LOG_WARNING);
    unsigned int window_flags = FLAG_WINDOW_RESIZABLE;
    if (smoke_frames > 0)
        window_flags |= FLAG_WINDOW_HIDDEN;
    if (settings.fullscreen != 0u)
        window_flags |= FLAG_FULLSCREEN_MODE;
    if (settings.vsync != 0u)
        window_flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(window_flags);
    InitWindow((int)settings.internal_width * 3, (int)settings.internal_height * 3,
               "VESTIGIO - Atrium 3D");
    if (!IsWindowReady()) {
        (void)fprintf(stderr, "No se pudo abrir la ventana OpenGL para la demo 3D\n");
        goto cleanup;
    }
    window_ready = true;
    SetWindowMinSize((int)settings.internal_width, (int)settings.internal_height);
    SetExitKey(KEY_NULL);
    SetTargetFPS(smoke_frames > 0 ? 0 : (int)settings.frame_cap);
    char gpu_error[192] = {0};
    renderer = vg_gpu_renderer_create(
        (VgGpuRendererConfig){settings.internal_width, settings.internal_height}, gpu_error,
        sizeof(gpu_error));
    if (renderer == NULL) {
        (void)fprintf(stderr, "Renderer GPU: %s\n", gpu_error);
        goto cleanup;
    }
    VgGpuInfo gpu = {0};
    if (!vg_gpu_renderer_info(renderer, &gpu)) {
        (void)fprintf(stderr, "No se pudo identificar el dispositivo OpenGL\n");
        goto cleanup;
    }
    (void)printf("GPU: %s / %s / OpenGL %s\n", gpu.vendor, gpu.renderer, gpu.version);
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    if (vg_context_create(&context_desc, &context) != VG_OK) {
        (void)fprintf(stderr, "No se pudo crear el contexto SDK\n");
        goto cleanup;
    }
    VgAssetGpuExecutor executor = vg_gpu_renderer_asset_executor(renderer);
    VgResult result = vg_asset_attach_gpu(context, &executor);
    if (result != VG_OK) {
        (void)fprintf(stderr, "No se pudo conectar el SDK con la GPU: %d\n", result);
        goto cleanup;
    }
    gpu_attached = true;
    result = vg_demo_scene_create(context, level_path, model_data, model_size,
                                  settings.look_sensitivity, &scene);
    free(model_data);
    model_data = NULL;
    if (result != VG_OK) {
        (void)fprintf(stderr, "No se pudo construir la escena 3D desde %s: %d\n",
                      level_path, result);
        goto cleanup;
    }
    if (!demo_apply_visual(renderer, context, vg_demo_scene_document_instance(scene),
                           settings.visual_profile, gpu_error, sizeof(gpu_error))) {
        (void)fprintf(stderr, "Perfil visual GPU: %s\n", gpu_error);
        goto cleanup;
    }
    VgDocumentEnvironment visual_environment;
    (void)vg_document_instance_environment(vg_demo_scene_document_instance(scene),
                                            &visual_environment);
    (void)printf("visual=%s lights=%zu fog=%s\n",
                 settings.visual_profile == VG_VISUAL_PROFILE_RETRO ? "retro" : "clean",
                 vg_document_instance_light_count(vg_demo_scene_document_instance(scene)),
                 visual_environment.fog_enabled ? "linear" : "off");

    VgVec3 start_position = {0};
    if (vg_demo_scene_camera_position(scene, &start_position) != VG_OK)
        goto cleanup;
    VgTransform door_closed_panel = {0}, door_closed_collider = {0};
    VgTransform door_open_panel = {0}, door_open_collider = {0};
    if (smoke_door) {
        if (vg_demo_scene_door_count(scene) == 0u ||
            vg_demo_scene_door_pose(scene, 0u, &door_closed_panel,
                                    &door_closed_collider) != VG_OK ||
            vg_demo_scene_prepare_door_smoke(scene) != VG_OK ||
            vg_demo_scene_door_hint(scene) == NULL) {
            (void)fprintf(stderr, "Smoke puerta: panel/collider no accesible por E\n");
            goto cleanup;
        }
    }
    if (smoke_frames == 0) {
        DisableCursor();
        cursor_captured = true;
    }
    double previous = GetTime();
    int frames = 0;
    while (smoke_frames == 0 || frames < smoke_frames) {
        double now = GetTime();
        if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE))
            break;
        if (smoke_frames == 0 && IsKeyPressed(KEY_F3))
            show_colliders = !show_colliders;
        if (smoke_frames == 0 && IsKeyPressed(KEY_F6)) {
            uint32_t previous_profile = settings.visual_profile;
            settings.visual_profile = previous_profile == VG_VISUAL_PROFILE_CLEAN
                ? VG_VISUAL_PROFILE_RETRO : VG_VISUAL_PROFILE_CLEAN;
            if (!demo_apply_visual(renderer, context, vg_demo_scene_document_instance(scene),
                                   settings.visual_profile, gpu_error, sizeof(gpu_error))) {
                settings.visual_profile = previous_profile;
                (void)fprintf(stderr, "Perfil visual GPU: %s\n", gpu_error);
            } else if (settings_path != NULL) {
                user_settings.present |= VG_SETTING_VISUAL_PROFILE;
                user_settings.visual_profile = settings.visual_profile;
                if (vg_settings_save_file(settings_path, &user_settings,
                                          &settings_error) != VG_OK)
                    (void)fprintf(stderr, "No se guardo perfil visual: %s\n",
                                  settings_error.message);
            } else {
                (void)fprintf(stderr, "No hay ruta de settings para guardar perfil\n");
            }
        }
        bool focused = smoke_frames > 0 || IsWindowFocused();
        if (smoke_frames > 0) {
            if (frames == smoke_frames / 2)
                SetWindowSize(800, 450);
        } else if (focused != cursor_captured) {
            if (focused)
                DisableCursor();
            else
                EnableCursor();
            cursor_captured = focused;
        }
        if (smoke_door && frames == 90) {
            float angle = vg_demo_scene_door_angle(scene, 0u);
            if (angle < 1.4f ||
                vg_demo_scene_door_pose(scene, 0u, &door_open_panel,
                                        &door_open_collider) != VG_OK ||
                fabsf(door_open_panel.position.x -
                      door_closed_panel.position.x) < 0.2f ||
                fabsf(door_open_panel.position.x -
                      door_open_collider.position.x) > 0.001f ||
                fabsf(door_open_panel.position.y -
                      door_open_collider.position.y) > 0.001f) {
                (void)fprintf(stderr, "Smoke puerta: panel o collider no abrio\n");
                goto cleanup;
            }
            if (vg_demo_scene_prepare_door_smoke(scene) != VG_OK ||
                vg_demo_scene_door_hint(scene) == NULL) {
                (void)fprintf(stderr, "Smoke puerta: no se puede apuntar a puerta abierta\n");
                goto cleanup;
            }
        }
        if (smoke_door && frames == 161 && vg_demo_scene_door_hint(scene) != NULL) {
            (void)fprintf(stderr, "Smoke puerta: hint no se limpio al mirar a otro lado\n");
            goto cleanup;
        }
        if (smoke_door && frames == 163 && vg_demo_scene_door_hint(scene) == NULL) {
            (void)fprintf(stderr, "Smoke puerta: hint no reaparecio al mirar a puerta\n");
            goto cleanup;
        }
        VgInputSample sample = demo_input_sample(focused, smoke_frames > 0,
                                                  smoke_door, frames);
        result = vg_demo_scene_submit_input(scene, &sample);
        if (result == VG_OK)
            result = vg_demo_scene_step(scene, smoke_frames > 0 ? 1.0 / 60.0 : now - previous);
        previous = now;
        if (result == VG_OK && !demo_apply_visual(renderer, context,
                              vg_demo_scene_document_instance(scene),
                              settings.visual_profile, gpu_error, sizeof(gpu_error))) {
            (void)fprintf(stderr, "Perfil visual GPU: %s\n", gpu_error);
            goto cleanup;
        }
        if (result == VG_OK)
            result = vg_gpu_renderer_draw_world(renderer, context, vg_demo_scene_world(scene));
        if (result == VG_OK && show_colliders)
            result = vg_gpu_renderer_draw_spatial_debug(renderer, vg_demo_scene_spatial(scene));
        if (result != VG_OK) {
            (void)fprintf(stderr, "Frame 3D %d fallido: %d\n", frames, result);
            goto cleanup;
        }
        vg_gpu_renderer_present_with_hint(renderer, vg_demo_scene_door_hint(scene));
        if (smoke_door && frames == 90 && capture_path != NULL) {
            char open_path[2048];
            int count = snprintf(open_path, sizeof(open_path), "%s.open.png",
                                 capture_path);
            if (count < 0 || (size_t)count >= sizeof(open_path) ||
                !vg_gpu_renderer_capture(renderer, open_path)) {
                (void)fprintf(stderr, "Smoke puerta: no se pudo capturar pose abierta\n");
                goto cleanup;
            }
        }
        ++frames;
    }
    VgFrameStats stats = vg_gpu_renderer_stats(renderer);
    VgVec3 end_position = {0};
    if (vg_demo_scene_camera_position(scene, &end_position) != VG_OK || frames == 0 ||
        stats.draw_calls == 0u || stats.asset_uploads == 0u ||
        stats.readbacks != (smoke_door && capture_path != NULL ? 1u : 0u) ||
        (smoke_frames > 0 && !smoke_door &&
         hypotf(end_position.x - start_position.x, end_position.y - start_position.y) < 0.02f) ||
        (smoke_frames >= 240 && !smoke_door &&
         (end_position.y < 2.3f || end_position.y > 2.6f ||
          fabsf(end_position.z - 1.7f) > 0.05f))) {
        (void)fprintf(stderr, "La demo no produjo movimiento y geometria GPU comprobables "
                      "(frames=%d draws=%llu uploads=%llu readbacks=%llu start=%.2f,%.2f "
                      "end=%.2f,%.2f)\n", frames,
                      (unsigned long long)stats.draw_calls,
                      (unsigned long long)stats.asset_uploads,
                      (unsigned long long)stats.readbacks,
                      (double)start_position.x, (double)start_position.y,
                      (double)end_position.x, (double)end_position.y);
        goto cleanup;
    }
    if (smoke_door) {
        VgTransform final_panel = {0}, final_collider = {0};
        if (vg_demo_scene_door_angle(scene, 0u) > 0.05f ||
            vg_demo_scene_door_pose(scene, 0u, &final_panel,
                                    &final_collider) != VG_OK ||
            fabsf(final_panel.position.x - door_closed_panel.position.x) > 0.01f ||
            fabsf(final_panel.position.y - door_closed_panel.position.y) > 0.01f ||
            fabsf(final_panel.position.x - final_collider.position.x) > 0.001f ||
            fabsf(final_panel.position.y - final_collider.position.y) > 0.001f) {
            (void)fprintf(stderr, "Smoke puerta: cierre no restauro panel y collider\n");
            goto cleanup;
        }
        (void)printf("door smoke: E open/close, angle=%.3f, collider follows panel\n",
                     (double)vg_demo_scene_door_angle(scene, 0u));
    }
    if (capture_path != NULL && !vg_gpu_renderer_capture(renderer, capture_path)) {
        (void)fprintf(stderr, "No se pudo guardar la captura GPU: %s\n", capture_path);
        goto cleanup;
    }
    stats = vg_gpu_renderer_stats(renderer);
    (void)printf("demo_3d frames=%d draws=%llu triangles=%llu uploads=%llu readbacks=%llu "
                 "camera=(%.2f,%.2f,%.2f)\n",
                 frames, (unsigned long long)stats.draw_calls, (unsigned long long)stats.triangles,
                 (unsigned long long)stats.asset_uploads, (unsigned long long)stats.readbacks,
                 (double)end_position.x, (double)end_position.y, (double)end_position.z);
    exit_code = 0;
cleanup:
    free(model_data);
    vg_demo_scene_destroy(scene);
    if (gpu_attached) {
        uint32_t purged = 0u;
        (void)vg_assets_purge_unused(context, &purged);
        (void)vg_asset_flush_gpu(context);
        (void)vg_asset_detach_gpu(context);
    }
    vg_context_destroy(context);
    vg_gpu_renderer_destroy(renderer);
    if (window_ready)
        CloseWindow();
    return exit_code;
}
