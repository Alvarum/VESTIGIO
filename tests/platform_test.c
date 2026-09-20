/* Prueba de integración real: ventana oculta, OpenGL, escalado, imagen PNG y
 * redimensionamiento. Se ejecuta por separado de las pruebas sin ventana. */
#include "retro/platform.h"
#include <stdio.h>

int main(void) {
    ReError error = {0};
    ReRenderer renderer = {0};
    ReTexture capture = {0};
    RePlatform *platform = re_platform_open(
        (RePlatformConfig){"RetroForge verification", 480, 270, true, false}, &error);
    if (!platform) {
        (void)fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    int result = 1;
    if (!re_renderer_init(&renderer, 480, 270))
        goto cleanup;
    re_renderer_clear(&renderer, re_rgba(31, 97, 163, 255));
    re_rect(&renderer, 0, 0, 240, 135, re_rgba(210, 70, 30, 255));
    re_rect(&renderer, 240, 135, 240, 135, re_rgba(20, 180, 110, 255));
    re_platform_resize(1000, 700);
    /* GLFW recibe mensajes al cerrar el frame. Repetimos presentación después
     * de procesarlos para capturar la superficie con el tamaño definitivo. */
    for (int i = 0; i < 3; i++)
        re_platform_present(platform, &renderer);
    if (!re_platform_capture_window("window-test.png") ||
        !re_platform_image_load("window-test.png", &capture))
        goto cleanup;
    if (capture.width != 1000 || capture.height != 700)
        goto cleanup;
    RePixel border = capture.pixels[0];
    RePixel top_left = capture.pixels[100u * 1000u + 50u];
    RePixel bottom_right = capture.pixels[600u * 1000u + 900u];
    if (border.r != 5 || border.g != 8 || top_left.r != 210 || top_left.g != 70 ||
        bottom_right.g != 180 || bottom_right.b != 110)
        goto cleanup;
    re_texture_destroy(&capture);
    re_platform_resize(1440, 810);
    for (int i = 0; i < 3; i++)
        re_platform_present(platform, &renderer);
    if (!re_platform_capture_window("window-test.png") ||
        !re_platform_image_load("window-test.png", &capture))
        goto cleanup;
    if (capture.width != 1440 || capture.height != 810 || capture.pixels[0].r != 210)
        goto cleanup;
    result = 0;
    (void)printf("PASS window resize, GPU readback, nearest scaling, letterbox, PNG load/export\n");
cleanup:
    if (result)
        (void)fprintf(stderr, "FAIL platform presentation (%d x %d)\n", capture.width,
                      capture.height);
    (void)remove("window-test.png");
    re_texture_destroy(&capture);
    re_renderer_destroy(&renderer);
    re_platform_close(platform);
    return result;
}
