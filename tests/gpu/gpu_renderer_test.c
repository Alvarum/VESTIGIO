#include "render/gpu_raylib/gpu_renderer.h"

#include "retro/platform.h"
#include <stdio.h>

static bool has_scene_pixels(const ReTexture *capture) {
    size_t red = 0u, blue = 0u;
    for (size_t i = 0; i < (size_t)capture->width * (size_t)capture->height; i++) {
        RePixel pixel = capture->pixels[i];
        if (pixel.r > 160u && pixel.r > pixel.g * 2u)
            red++;
        if (pixel.b > 140u && pixel.b > pixel.r * 2u)
            blue++;
    }
    return red > 100u && blue > 4u;
}

int main(int argc, char **argv) {
    ReError error = {0};
    RePlatform *platform = re_platform_open(
        (RePlatformConfig){"VESTIGIO GPU verification", 320, 180, true, false}, &error);
    if (!platform) {
        (void)fprintf(stderr, "FAIL platform: %s\n", error.message);
        return 1;
    }
    int result = 1;
    char gpu_error[256] = {0};
    VgGpuRenderer *renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){320u, 180u}, gpu_error, sizeof(gpu_error));
    if (!renderer) {
        (void)fprintf(stderr, "FAIL renderer: %s\n", gpu_error);
        goto cleanup_platform;
    }
    VgGpuInfo info = {0};
    if (!vg_gpu_renderer_info(&info)) {
        (void)fprintf(stderr, "FAIL GPU info\n");
        goto cleanup_renderer;
    }
    const char *invalid_vertex = "#version 330\nthis is not a shader\n";
    if (vg_gpu_renderer_validate_shader(invalid_vertex, "#version 330\nvoid main(){}\n", gpu_error,
                                        sizeof(gpu_error))) {
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
    ReTexture capture = {0};
    if (!re_platform_image_load(capture_path, &capture) || capture.width != 320 ||
        capture.height != 180 || !has_scene_pixels(&capture)) {
        (void)fprintf(stderr, "FAIL GPU pixels\n");
        re_texture_destroy(&capture);
        if (!kept_capture)
            (void)remove(capture_path);
        goto cleanup_renderer;
    }
    re_texture_destroy(&capture);
    if (!kept_capture)
        (void)remove(capture_path);
    if (vg_gpu_renderer_stats(renderer).readbacks != 1u) {
        (void)fprintf(stderr, "FAIL explicit readback accounting\n");
        goto cleanup_renderer;
    }
    vg_gpu_renderer_destroy(renderer);
    renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){640u, 360u}, gpu_error, sizeof(gpu_error));
    if (!renderer) {
        (void)fprintf(stderr, "FAIL 640x360 renderer: %s\n", gpu_error);
        goto cleanup_renderer;
    }
    re_platform_resize(1280, 720);
    if (!vg_gpu_renderer_draw_demo(renderer))
        goto cleanup_renderer;
    vg_gpu_renderer_present(renderer);
    if (vg_gpu_renderer_stats(renderer).readbacks != 0u ||
        !vg_gpu_renderer_capture(renderer, "vestigio-gpu-large-test.png"))
        goto cleanup_renderer;
    capture = (ReTexture){0};
    if (!re_platform_image_load("vestigio-gpu-large-test.png", &capture) || capture.width != 640 ||
        capture.height != 360 || !has_scene_pixels(&capture)) {
        (void)fprintf(stderr, "FAIL 640x360 GPU pixels\n");
        re_texture_destroy(&capture);
        (void)remove("vestigio-gpu-large-test.png");
        goto cleanup_renderer;
    }
    re_texture_destroy(&capture);
    (void)remove("vestigio-gpu-large-test.png");
    (void)printf("PASS GPU mesh/shader/depth/sprite/target/upscale; %s | %s | %s | rlgl=%d\n",
                 info.vendor, info.renderer, info.version, info.rlgl_version);
    result = 0;

cleanup_renderer:
    vg_gpu_renderer_destroy(renderer);
cleanup_platform:
    re_platform_close(platform);
    return result;
}
