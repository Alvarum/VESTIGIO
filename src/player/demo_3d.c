#include "player/demo_3d.h"

#include "player/demo_scene.h"
#include "render/gpu_raylib/gpu_renderer.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

static VgInputSample demo_input_sample(bool focused, bool smoke, int frame) {
    VgInputSample sample = {0};
    sample.struct_size = sizeof(sample);
    sample.api_version = VG_API_VERSION;
    if (smoke || IsKeyDown(KEY_W))
        sample.held |= VG_ACTION_MOVE_FORWARD;
    if (!smoke && IsKeyDown(KEY_S))
        sample.held |= VG_ACTION_MOVE_BACKWARD;
    if (!smoke && IsKeyDown(KEY_D))
        sample.held |= VG_ACTION_MOVE_RIGHT;
    if (!smoke && IsKeyDown(KEY_A))
        sample.held |= VG_ACTION_MOVE_LEFT;
    if (!smoke && IsKeyPressed(KEY_SPACE))
        sample.pressed |= VG_ACTION_JUMP;
    if (smoke)
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
                   const VgSettingsLayer *session_settings) {
    VgSettingsLayer settings = {0};
    settings.struct_size = sizeof(settings);
    settings.api_version = VG_API_VERSION;
    VgSettingsDiagnostic settings_error = {0};
    settings_error.struct_size = sizeof(settings_error);
    settings_error.api_version = VG_API_VERSION;
    if (vg_settings_resolve(NULL, NULL, session_settings, &settings, &settings_error) != VG_OK ||
        settings.internal_width > 1280u || settings.internal_height > 720u) {
        (void)fprintf(stderr, "Configuracion 3D invalida: %s (maximo 1280x720)\n",
                      settings_error.message);
        return 2;
    }
    char model_path[2048] = {0};
    void *model_data = NULL;
    uint64_t model_size = 0u;
    int path_length = snprintf(model_path, sizeof(model_path), "%sassets/demo/atrium.gltf",
                               GetApplicationDirectory());
    if (path_length < 0 || (size_t)path_length >= sizeof(model_path) ||
        !demo_read_model(model_path, &model_data, &model_size)) {
        (void)fprintf(stderr, "No se pudo leer el modelo 3D: %s\n", model_path);
        return 1;
    }

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
    result = vg_demo_scene_create(context, model_data, model_size, settings.look_sensitivity, &scene);
    free(model_data);
    model_data = NULL;
    if (result != VG_OK) {
        (void)fprintf(stderr, "No se pudo construir la escena 3D desde %s: %d\n", model_path,
                      result);
        goto cleanup;
    }

    VgVec3 start_position = {0};
    if (vg_demo_scene_camera_position(scene, &start_position) != VG_OK)
        goto cleanup;
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
        VgInputSample sample = demo_input_sample(focused, smoke_frames > 0, frames);
        result = vg_demo_scene_submit_input(scene, &sample);
        if (result == VG_OK)
            result = vg_demo_scene_step(scene, smoke_frames > 0 ? 1.0 / 60.0 : now - previous);
        previous = now;
        if (result == VG_OK)
            result = vg_gpu_renderer_draw_world(renderer, context, vg_demo_scene_world(scene));
        if (result == VG_OK && show_colliders)
            result = vg_gpu_renderer_draw_spatial_debug(renderer, vg_demo_scene_spatial(scene));
        if (result != VG_OK) {
            (void)fprintf(stderr, "Frame 3D %d fallido: %d\n", frames, result);
            goto cleanup;
        }
        vg_gpu_renderer_present(renderer);
        ++frames;
    }
    VgFrameStats stats = vg_gpu_renderer_stats(renderer);
    VgVec3 end_position = {0};
    if (vg_demo_scene_camera_position(scene, &end_position) != VG_OK || frames == 0 ||
        stats.draw_calls == 0u || stats.asset_uploads == 0u || stats.readbacks != 0u ||
        (smoke_frames > 0 &&
         hypotf(end_position.x - start_position.x, end_position.y - start_position.y) < 0.02f) ||
        (smoke_frames >= 240 &&
         (end_position.y < 2.3f || end_position.y > 2.6f ||
          fabsf(end_position.z - 1.7f) > 0.05f))) {
        (void)fprintf(stderr, "La demo no produjo movimiento y geometria GPU comprobables\n");
        goto cleanup;
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
