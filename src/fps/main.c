/* Punto de composición: une plataforma y juego. Sigue este archivo después
 * del inicio rápido para ver entrada -> ticks -> render -> presentación. */
#include "game.h"
#include "retro/platform.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int smoke_frames = 0, view = 0;
    bool menu_capture = false, no_audio = false;
    const char *capture = nullptr, *map_override = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--smoke") && i + 1 < argc) {
            char *end = nullptr;
            errno = 0;
            long value = strtol(argv[++i], &end, 10);
            if (errno || end == argv[i] || *end || value < 1 || value > 100000) {
                (void)fprintf(stderr, "--smoke requiere 1..100000 frames\n");
                return 2;
            }
            smoke_frames = (int)value;
        } else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
            capture = argv[++i];
        else if (!strcmp(argv[i], "--map") && i + 1 < argc)
            map_override = argv[++i];
        else if (!strcmp(argv[i], "--view") && i + 1 < argc) {
            char *end = nullptr;
            long value = strtol(argv[++i], &end, 10);
            if (end == argv[i] || *end || value < 0 || value > 4)
                return 2;
            view = (int)value;
        } else if (!strcmp(argv[i], "--menu"))
            menu_capture = true;
        else if (!strcmp(argv[i], "--no-audio"))
            no_audio = true;
        else {
            (void)fprintf(stderr, "Uso: retro_fps [--map archivo] [--smoke frames] [--capture "
                                  "imagen.png] [--view 0..4] [--menu] [--no-audio]\n");
            return 2;
        }
    }
    ReError error = {0};
    RePlatform *platform = re_platform_open(
        (RePlatformConfig){"FOUNDRY | RetroForge", 480, 270, smoke_frames > 0, !no_audio}, &error);
    if (!platform) {
        (void)fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    FpsGame *game = calloc(1, sizeof(*game));
    FpsArt art = {0};
    ReRenderer renderer = {0};
    char map[2048];
    int result = 1;
    if (!game || !re_platform_asset_path("foundry.map", map, sizeof(map)) ||
        !fps_game_init(game, map_override ? map_override : map, &error) || !fps_art_init(&art) ||
        !re_renderer_init(&renderer, 480, 270)) {
        (void)fprintf(stderr, "Inicializacion: %s:%zu: %s\n",
                      map_override ? map_override : "foundry.map", error.line, error.message);
        goto cleanup;
    }
    int sounds[5];
    int16_t samples[11025];
    for (int i = 0; i < 5; i++) {
        fps_sound_generate(i, samples, 11025, 44100);
        sounds[i] = re_platform_sound(platform, samples, 11025, 44100);
    }
    if (smoke_frames && !menu_capture)
        fps_game_restart(game);
    if (view == 1) {
        game->camera.pitch = 0.65f;
        game->camera.position.z = 2.3f;
    }
    if (view == 2) {
        game->camera.position = re_v3(0.28f, 3, 1.52f);
        game->camera.yaw = -RE_PI / 2;
    }
    if (view == 3)
        game->depth_view = true;
    if (view == 4) {
        game->wire_view = true;
        game->map_view = true;
        game->stats_view = true;
    }
    game->previous_camera = game->camera;
    ReClock clock = {0};
    ReInput pending = {0};
    double previous = re_platform_time(), render_total = 0, render_max = 0;
    double benchmark_start = previous;
    int frames = 0;
    while (!game->quit && (!smoke_frames || frames < smoke_frames)) {
        double now = re_platform_time(), elapsed = now - previous;
        previous = now;
        ReInput input = re_platform_input(platform);
        fps_game_frame(game, input);
        bool active = game->mode == FPS_PLAYING && input.focused;
        re_platform_capture_mouse(platform, active);
        int ticks = re_clock_advance(&clock, elapsed, active && !smoke_frames);
        if (active)
            re_input_accumulate(&pending, input);
        else
            pending = (ReInput){0};
        for (int tick = 0; tick < ticks; tick++)
            fps_game_tick(game, re_input_consume(&pending), RE_FIXED_DT);
        for (int i = 0; i < 5; i++)
            if (game->sound_events & (1u << (unsigned int)i))
                re_platform_play(platform, sounds[i], game->volume, 0.5f);
        game->sound_events = 0;
        double start = re_platform_time();
        fps_game_draw(game, &art, &renderer, active && !smoke_frames ? re_clock_alpha(&clock) : 1);
        double duration = re_platform_time() - start;
        render_total += duration;
        if (duration > render_max)
            render_max = duration;
        re_platform_present(platform, &renderer);
        frames++;
    }
    double benchmark_seconds = re_platform_time() - benchmark_start;
    result = 0;
    if (capture && !re_platform_capture_png(&renderer, capture)) {
        (void)fprintf(stderr, "No se pudo exportar %s\n", capture);
        result = 1;
    }
    if (smoke_frames)
        (void)printf("frames=%d cpu_render_mean_ms=%.3f cpu_render_max_ms=%.3f frame_mean_ms=%.3f "
                     "triangles=%zu "
                     "pixels=%zu audio=%s\n",
                     frames, render_total * 1000 / (double)frames, render_max * 1000,
                     benchmark_seconds * 1000 / (double)frames, renderer.stats.rasterized,
                     renderer.stats.shaded,
                     re_platform_audio_ready(platform) ? "ready" : "disabled");
cleanup:
    re_renderer_destroy(&renderer);
    fps_art_destroy(&art);
    free(game);
    re_platform_close(platform);
    return result;
}
