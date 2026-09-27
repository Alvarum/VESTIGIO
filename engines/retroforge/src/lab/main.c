/* Segundo consumidor del motor: no incluye, enlaza ni conoce fps_game.
 * Ejemplo mínimo de integración con cámara, movimiento y texturas propias. */
#include "retro/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void draw_lab(ReRenderer *r, const ReCamera *camera, const ReWorld *world,
                     const ReTexture *materials, int view, bool map, bool paused) {
    re_renderer_clear(r, re_rgba(17, 23, 35, 255));
    r->wireframe = view == 2;
    re_draw_world(r, camera, world, materials);
    if (view == 1)
        re_draw_depth(r, 14);
    re_rect(r, 0, 0, 480, 33, re_rgba(7, 16, 24, 235));
    re_text(r, 15, 11, "RETROFORGE", 2, re_rgba(119, 230, 206, 255));
    re_text(r, 160, 14, "LABORATORIO / MOTOR SIN JUEGO", 1, re_rgba(201, 216, 222, 255));
    re_rect(r, 0, 243, 480, 27, re_rgba(7, 16, 24, 240));
    re_text(r, 14, 253, "WASD MOVER  RATON MIRAR  ESPACIO SALTAR  ESC PAUSA", 1,
            re_rgba(201, 216, 222, 255));
    re_rect(r, 12, 48, 172, 52, re_rgba(7, 16, 24, 220));
    re_text(r, 21, 57, "F1 PLANO  F2 GEOMETRIA", 1, re_rgba(119, 230, 206, 255));
    re_text(r, 21, 70, "F3 PROFUNDIDAD", 1, re_rgba(201, 216, 222, 255));
    char text[80];
    (void)snprintf(text, sizeof(text), "TRI %zu  Z %.2f", r->stats.rasterized,
                   (double)camera->position.z);
    re_text(r, 21, 83, text, 1, re_rgba(162, 181, 189, 255));
    if (map) {
        re_rect(r, 329, 48, 137, 108, re_rgba(7, 16, 24, 230));
        for (size_t i = 0; i < world->sector_count; i++) {
            const ReSector *s = &world->sectors[i];
            for (size_t e = 0; e < s->count; e++) {
                ReVec2 a = s->vertices[e], b = s->vertices[(e + 1) % s->count];
                re_line(r, 342 + (int)(a.x * 10), 137 - (int)(a.y * 10), 342 + (int)(b.x * 10),
                        137 - (int)(b.y * 10),
                        s->neighbor[e] < 0 ? re_rgba(210, 226, 228, 255)
                                           : re_rgba(83, 136, 146, 255));
            }
        }
        re_rect(r, 340 + (int)(camera->position.x * 10), 135 - (int)(camera->position.y * 10), 5, 5,
                re_rgba(245, 160, 74, 255));
    }
    if (paused) {
        re_rect(r, 111, 118, 258, 45, re_rgba(7, 16, 24, 235));
        re_text(r, 153, 128, "EN PAUSA", 2, re_rgba(245, 160, 74, 255));
        re_text(r, 153, 150, "ESC PARA CONTINUAR", 1, re_rgba(210, 226, 228, 255));
    }
}
int main(int argc, char **argv) {
    int smoke = 0, view = 0;
    const char *capture = nullptr;
    for (int i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "--smoke") || !strcmp(argv[i], "--view")) && i + 1 < argc) {
            bool is_view = !strcmp(argv[i], "--view");
            char *end = nullptr;
            long value = strtol(argv[++i], &end, 10);
            if (end == argv[i] || *end || value < (is_view ? 0 : 1) ||
                value > (is_view ? 4 : 100000))
                return 2;
            if (is_view)
                view = (int)value;
            else
                smoke = (int)value;
        } else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
            capture = argv[++i];
        else {
            (void)fprintf(stderr,
                          "Uso: retro_lab [--smoke frames] [--capture imagen.png] [--view 0..4]\n");
            return 2;
        }
    }
    ReError error = {0};
    ReRenderer r = {0};
    ReTexture materials[RE_MAX_MATERIALS] = {0};
    ReWorld *world = calloc(1, sizeof(*world));
    RePlatform *platform = re_platform_open((RePlatformConfig){.title = "RetroForge | Laboratorio",
                                                               .framebuffer_width = 480,
                                                               .framebuffer_height = 270,
                                                               .hidden = smoke > 0,
                                                               .audio = false,
                                                               .frame_cap = 120u},
                                            &error);
    int result = 1;
    char path[2048];
    if (!platform || !world || !re_platform_asset_path("lab.map", path, sizeof(path)) ||
        !re_world_load(path, world, &error) || !re_renderer_init(&r, 480, 270)) {
        (void)fprintf(stderr, "Laboratorio: %zu: %s\n", error.line, error.message);
        goto cleanup;
    }
    for (int i = 0; i < RE_MAX_MATERIALS; i++) {
        if (!re_texture_init(&materials[i], 32, 32))
            goto cleanup;
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++) {
                bool grid = x % 16 == 0 || y % 16 == 0;
                RePixel colors[4] = {{88, 169, 171, 255},
                                     {83, 98, 116, 255},
                                     {69, 81, 101, 255},
                                     {187, 121, 73, 255}};
                RePixel color = colors[i % 4];
                if (grid)
                    color = re_rgba(27, 42, 55, 255);
                materials[i].pixels[(size_t)y * 32u + (size_t)x] = color;
            }
    }
    ReMarker marker = world->markers[0];
    ReBody body = {.position = marker.position,
                   .radius = 0.26f,
                   .height = 1.7f,
                   .step_height = 0.3f,
                   .sector = marker.sector,
                   .grounded = true};
    ReCamera camera = {.position = re_add3(body.position, re_v3(0, 0, 1.52f)),
                       .yaw = marker.yaw,
                       .fov = 75 * RE_PI / 180,
                       .near_plane = 0.05f,
                       .far_plane = 96};
    if (view == 3) {
        camera.pitch = 0.7f;
        camera.position.z = 2.1f;
    }
    if (view == 4) {
        camera.position = re_v3(0.28f, 3, 1.52f);
        camera.yaw = -RE_PI / 2;
    }
    ReCamera previous_camera = camera;
    ReClock clock = {0};
    ReInput pending = {0};
    double previous = re_platform_time(), total = 0;
    bool paused = false, map = true;
    int frames = 0;
    while (!smoke || frames < smoke) {
        double now = re_platform_time(), elapsed = now - previous;
        previous = now;
        ReInput input = re_platform_input(platform);
        if (input.quit)
            break;
        if (!input.focused)
            paused = true;
        if ((input.pressed & RE_PAUSE) && input.focused)
            paused = !paused;
        if (input.pressed & RE_MAP)
            map = !map;
        if (input.pressed & RE_WIRE)
            view = view == 2 ? 0 : 2;
        if (input.pressed & RE_DEPTH)
            view = view == 1 ? 0 : 1;
        re_platform_capture_mouse(platform, !paused);
        int ticks = re_clock_advance(&clock, elapsed, !paused && !smoke);
        if (!paused)
            re_input_accumulate(&pending, input);
        else
            pending = (ReInput){0};
        for (int tick = 0; tick < ticks; tick++) {
            ReInput consumed = re_input_consume(&pending);
            previous_camera = camera;
            camera.yaw = remainderf(camera.yaw + consumed.look.x * 0.0025f, 2 * RE_PI);
            camera.pitch = re_clamp(camera.pitch - consumed.look.y * 0.0025f, -85 * RE_PI / 180,
                                    85 * RE_PI / 180);
            ReVec2 forward = re_v2(sinf(camera.yaw), cosf(camera.yaw));
            ReVec2 delta = re_add2(re_scale2(forward, consumed.movement.y),
                                   re_scale2(re_v2(forward.y, -forward.x), consumed.movement.x));
            if ((consumed.pressed & RE_JUMP) && body.grounded) {
                body.vertical_speed = 6.2f;
                body.grounded = false;
            }
            re_body_move(world, &body, re_scale2(delta, 3.6f * RE_FIXED_DT), RE_FIXED_DT, 18);
            camera.position = re_add3(body.position, re_v3(0, 0, 1.52f));
        }
        ReCamera interpolated = re_camera_interpolate(previous_camera, camera,
                                                      paused || smoke ? 1 : re_clock_alpha(&clock));
        double start = re_platform_time();
        draw_lab(&r, &interpolated, world, materials, view, map, paused);
        total += re_platform_time() - start;
        re_platform_present(platform, &r);
        frames++;
    }
    result = 0;
    if (capture && !re_platform_capture_png(&r, capture))
        result = 1;
    if (smoke && frames > 0)
        (void)printf("lab frames=%d cpu_render_mean_ms=%.3f\n", frames,
                     total * 1000 / (double)frames);
cleanup:
    for (int i = 0; i < RE_MAX_MATERIALS; i++)
        re_texture_destroy(&materials[i]);
    re_renderer_destroy(&r);
    re_platform_close(platform);
    free(world);
    return result;
}
