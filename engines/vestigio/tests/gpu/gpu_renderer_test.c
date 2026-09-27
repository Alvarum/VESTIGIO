#include "render/gpu_raylib/gpu_renderer.h"

#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef Color TestPixel;
typedef struct TestImage {
    int width, height;
    TestPixel *pixels;
} TestImage;

static bool test_image_load(const char *path, TestImage *out) {
    Image image = LoadImage(path);
    if (!image.data)
        return false;
    out->pixels = LoadImageColors(image);
    out->width = image.width;
    out->height = image.height;
    UnloadImage(image);
    return out->pixels != NULL;
}

static void test_image_destroy(TestImage *image) {
    if (image->pixels)
        UnloadImageColors(image->pixels);
    *image = (TestImage){0};
}

static size_t image_difference(const TestImage *left, const TestImage *right) {
    if (left->pixels == NULL || right->pixels == NULL ||
        left->width != right->width || left->height != right->height)
        return 0u;
    size_t changed = 0u;
    for (size_t index = 0u; index < (size_t)left->width * (size_t)left->height;
         ++index) {
        TestPixel a = left->pixels[index], b = right->pixels[index];
        int delta = abs((int)a.r - (int)b.r) +
                    abs((int)a.g - (int)b.g) + abs((int)a.b - (int)b.b);
        if (delta > 24)
            ++changed;
    }
    return changed;
}

static bool has_scene_pixels(const TestImage *capture) {
    size_t red = 0u, blue = 0u;
    for (size_t i = 0; i < (size_t)capture->width * (size_t)capture->height; i++) {
        TestPixel pixel = capture->pixels[i];
        if (pixel.r > 160u && pixel.r > pixel.g * 2u)
            red++;
        if (pixel.b > 140u && pixel.b > pixel.r * 2u)
            blue++;
    }
    return red > 100u && blue > 4u;
}

int main(int argc, char **argv) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "VESTIGIO GPU verification");
    if (!IsWindowReady()) {
        (void)fprintf(stderr, "FAIL GPU test window\n");
        return 1;
    }
    SetTargetFPS(120);
    int result = 1;
    char gpu_error[256] = {0};
    VgGpuRenderer *renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){320u, 180u}, gpu_error, sizeof(gpu_error));
    if (!renderer) {
        (void)fprintf(stderr, "FAIL renderer: %s\n", gpu_error);
        goto cleanup_window;
    }
    VgGpuInfo info = {0};
    if (!vg_gpu_renderer_info(renderer, &info)) {
        (void)fprintf(stderr, "FAIL GPU info\n");
        goto cleanup_renderer;
    }
    const char *invalid_vertex = "#version 330\nthis is not a shader\n";
    if (vg_gpu_renderer_validate_shader(renderer, invalid_vertex, "#version 330\nvoid main(){}\n",
                                        gpu_error, sizeof(gpu_error))) {
        (void)fprintf(stderr, "FAIL invalid shader accepted\n");
        goto cleanup_renderer;
    }
    for (int i = 0; i < 3; i++) {
        if (!vg_gpu_renderer_draw_demo(renderer))
            goto cleanup_renderer;
        vg_gpu_renderer_present(renderer);
    }
    VgFrameStats before_capture = vg_gpu_renderer_stats(renderer);
    if (before_capture.frame_index != 3u || before_capture.draw_calls != 3u ||
        before_capture.triangles != 26u || before_capture.uploads != 3u ||
        before_capture.readbacks != 0u || before_capture.estimated_gpu_bytes == 0u) {
        (void)fprintf(stderr, "FAIL GPU stats before capture\n");
        goto cleanup_renderer;
    }
    const char *kept_capture = argc > 1 ? argv[1] : NULL;
    const char *capture_path = kept_capture ? kept_capture : "vestigio-gpu-test.png";
    if (!vg_gpu_renderer_capture(renderer, capture_path)) {
        (void)fprintf(stderr, "FAIL GPU capture\n");
        goto cleanup_renderer;
    }
    TestImage capture = {0};
    if (!test_image_load(capture_path, &capture) || capture.width != 320 ||
        capture.height != 180 || !has_scene_pixels(&capture)) {
        (void)fprintf(stderr, "FAIL GPU pixels\n");
        test_image_destroy(&capture);
        if (!kept_capture)
            (void)remove(capture_path);
        goto cleanup_renderer;
    }
    test_image_destroy(&capture);
    if (!kept_capture)
        (void)remove(capture_path);
    if (vg_gpu_renderer_stats(renderer).readbacks != 1u) {
        (void)fprintf(stderr, "FAIL explicit readback accounting\n");
        goto cleanup_renderer;
    }
    const char *clean_path = argc > 2 ? argv[2] : "vestigio-clean-test.png";
    const char *retro_path = argc > 3 ? argv[3] : "vestigio-retro-test.png";
    VgGpuVisualSettings visual = vg_gpu_renderer_default_visual_settings();
    visual.ambient[0] = 0.55f;
    visual.ambient[1] = 0.57f;
    visual.ambient[2] = 0.62f;
    visual.clear_color[0] = 0.02f;
    visual.clear_color[1] = 0.03f;
    visual.clear_color[2] = 0.05f;
    visual.fog_enabled = 1u;
    visual.fog_color[0] = 0.05f;
    visual.fog_color[1] = 0.08f;
    visual.fog_color[2] = 0.12f;
    visual.fog_start = 6.0f;
    visual.fog_end = 16.0f;
    visual.point_light_count = 1u;
    visual.lights[0] = (VgGpuPointLight){
        .position = {0.0f, -1.0f, 2.8f}, .radius = 7.0f,
        .color = {1.0f, 0.68f, 0.36f}, .intensity = 3.5f};
    if (!vg_gpu_renderer_set_visual_settings(renderer, &visual, gpu_error,
                                             sizeof(gpu_error)) ||
        !vg_gpu_renderer_draw_demo(renderer)) {
        (void)fprintf(stderr, "FAIL clean light/fog GPU: %s\n", gpu_error);
        goto cleanup_renderer;
    }
    vg_gpu_renderer_present(renderer);
    if (vg_gpu_renderer_stats(renderer).readbacks != 1u ||
        !vg_gpu_renderer_capture(renderer, clean_path))
        goto cleanup_renderer;
    TestImage clean = {0}, retro = {0};
    if (!test_image_load(clean_path, &clean) ||
        clean.width != 320 || clean.height != 180) {
        (void)fprintf(stderr, "FAIL clean capture\n");
        goto cleanup_renderer;
    }
    VgGpuVisualSettings invalid = visual;
    invalid.ambient[0] = NAN;
    if (vg_gpu_renderer_set_visual_settings(renderer, &invalid, gpu_error,
                                             sizeof(gpu_error)) ||
        gpu_error[0] == '\0') {
        (void)fprintf(stderr, "FAIL invalid visual settings accepted\n");
        test_image_destroy(&clean);
        goto cleanup_renderer;
    }
    if (vg_gpu_renderer_resize_internal(renderer, 0u, 360u, gpu_error,
                                         sizeof(gpu_error)) ||
        gpu_error[0] == '\0' || !vg_gpu_renderer_draw_demo(renderer) ||
        !vg_gpu_renderer_capture(renderer, "vestigio-invalid-state-test.png")) {
        (void)fprintf(stderr, "FAIL rejected settings/resize changed render target\n");
        test_image_destroy(&clean);
        goto cleanup_renderer;
    }
    TestImage unchanged = {0};
    if (!test_image_load("vestigio-invalid-state-test.png", &unchanged) ||
        unchanged.width != 320 || unchanged.height != 180 ||
        image_difference(&clean, &unchanged) != 0u) {
        (void)fprintf(stderr, "FAIL rejected settings changed visual output\n");
        test_image_destroy(&unchanged);
        test_image_destroy(&clean);
        goto cleanup_renderer;
    }
    test_image_destroy(&unchanged);
    (void)remove("vestigio-invalid-state-test.png");
    visual.mode = VG_GPU_VISUAL_RETRO;
    if (!vg_gpu_renderer_set_visual_settings(renderer, &visual, gpu_error,
                                             sizeof(gpu_error)) ||
        !vg_gpu_renderer_draw_demo(renderer)) {
        test_image_destroy(&clean);
        goto cleanup_renderer;
    }
    vg_gpu_renderer_present(renderer);
    if (vg_gpu_renderer_stats(renderer).readbacks != 3u ||
        !vg_gpu_renderer_capture(renderer, retro_path) ||
        !test_image_load(retro_path, &retro)) {
        test_image_destroy(&clean);
        goto cleanup_renderer;
    }
    size_t changed = image_difference(&clean, &retro);
    test_image_destroy(&clean);
    test_image_destroy(&retro);
    if (changed < 200u || vg_gpu_renderer_stats(renderer).readbacks != 4u) {
        (void)fprintf(stderr, "FAIL clean/retro A/B: %zu changed pixels\n", changed);
        goto cleanup_renderer;
    }
    if (argc <= 2) (void)remove(clean_path);
    if (argc <= 3) (void)remove(retro_path);
    if (!vg_gpu_renderer_resize_internal(renderer, 640u, 360u, gpu_error,
                                          sizeof(gpu_error)) ||
        !vg_gpu_renderer_draw_demo(renderer)) {
        (void)fprintf(stderr, "FAIL transactional GPU resize: %s\n", gpu_error);
        goto cleanup_renderer;
    }
    vg_gpu_renderer_present(renderer);
    if (vg_gpu_renderer_stats(renderer).readbacks != 4u ||
        !vg_gpu_renderer_capture(renderer, "vestigio-gpu-resized-test.png"))
        goto cleanup_renderer;
    TestImage resized = {0};
    if (!test_image_load("vestigio-gpu-resized-test.png", &resized) ||
        resized.width != 640 || resized.height != 360) {
        (void)fprintf(stderr, "FAIL resized target dimensions\n");
        test_image_destroy(&resized);
        goto cleanup_renderer;
    }
    test_image_destroy(&resized);
    (void)remove("vestigio-gpu-resized-test.png");
    vg_gpu_renderer_destroy(renderer);
    renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){640u, 360u}, gpu_error, sizeof(gpu_error));
    if (!renderer) {
        (void)fprintf(stderr, "FAIL 640x360 renderer: %s\n", gpu_error);
        goto cleanup_renderer;
    }
    SetWindowSize(1280, 720);
    if (!vg_gpu_renderer_draw_demo(renderer))
        goto cleanup_renderer;
    vg_gpu_renderer_present(renderer);
    if (vg_gpu_renderer_stats(renderer).readbacks != 0u ||
        !vg_gpu_renderer_capture(renderer, "vestigio-gpu-large-test.png"))
        goto cleanup_renderer;
    capture = (TestImage){0};
    if (!test_image_load("vestigio-gpu-large-test.png", &capture) || capture.width != 640 ||
        capture.height != 360 || !has_scene_pixels(&capture)) {
        (void)fprintf(stderr, "FAIL 640x360 GPU pixels\n");
        test_image_destroy(&capture);
        (void)remove("vestigio-gpu-large-test.png");
        goto cleanup_renderer;
    }
    test_image_destroy(&capture);
    (void)remove("vestigio-gpu-large-test.png");
    (void)printf("PASS GPU mesh/shader/depth/sprite/target/upscale; %s | %s | %s | rlgl=%d\n",
                 info.vendor, info.renderer, info.version, info.rlgl_version);
    result = 0;

cleanup_renderer:
    vg_gpu_renderer_destroy(renderer);
cleanup_window:
    CloseWindow();
    return result;
}
