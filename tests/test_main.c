/* Pruebas ejecutables en Debug y Release. CHECK no desaparece con NDEBUG.
 * Los fixtures usan el mapa real; no abren ventana, audio ni GPU. */
#include "game.h"
#include "retro/editor.h"
#include "retro/gameplay.h"
#include "retro/project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);             \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.001f)

static bool geometry(void) {
    CHECK(NEAR(re_length2(re_v2(3, 4)), 5));
    CHECK(NEAR(re_dot2(re_v2(1, 0), re_v2(0, 1)), 0));
    CHECK(NEAR(re_length2(re_normalize2(re_v2(0, 0))), 0));
    ReVec2 nearest = re_closest_segment(re_v2(4, 3), re_v2(0, 0), re_v2(2, 0));
    CHECK(NEAR(nearest.x, 2) && NEAR(nearest.y, 0));
    float t = 0;
    ReVec2 n;
    CHECK(re_ray_segment(re_v2(0, 0), re_v2(0, 1), re_v2(-1, 3), re_v2(1, 3), &t));
    CHECK(NEAR(t, 3));
    CHECK(!re_ray_segment(re_v2(0, 0), re_v2(1, 0), re_v2(-1, 3), re_v2(1, 3), &t));
    CHECK(re_sweep_circle(re_v2(0, 0), re_v2(10, 0), 0.5f, re_v2(3, -2), re_v2(3, 2), &t, &n));
    CHECK(NEAR(t, 0.25f) && n.x < -.99f);
    CHECK(!re_sweep_circle(re_v2(0, 0), re_v2(-10, 0), 0.5f, re_v2(3, -2), re_v2(3, 2), &t, &n));
    CHECK(re_sweep_circle(re_v2(0, 0), re_v2(3, 3), 0.5f, re_v2(3, 3), re_v2(5, 3), &t, &n));
    return true;
}
static bool input_clock(void) {
    ReInput pending = {0};
    re_input_accumulate(&pending,
                        (ReInput){.pressed = RE_JUMP, .held = RE_PRIMARY, .look = {2, 3}});
    re_input_accumulate(&pending, (ReInput){.held = RE_PRIMARY, .look = {4, 5}});
    ReInput first = re_input_consume(&pending), second = re_input_consume(&pending);
    CHECK(first.pressed == RE_JUMP && first.look.x == 6 && first.look.y == 8);
    CHECK(second.pressed == 0 && second.look.x == 0 && second.held == RE_PRIMARY);
    ReClock c = {0};
    CHECK(re_clock_advance(&c, 0.008, true) == 0);
    CHECK(re_clock_advance(&c, 0.010, true) == 1);
    CHECK(re_clock_alpha(&c) > 0 && re_clock_alpha(&c) < 1);
    CHECK(re_clock_advance(&c, 5, true) == 8 && c.dropped_ticks > 0);
    CHECK(re_clock_advance(&c, 10, false) == 0 && re_clock_alpha(&c) == 0);
    CHECK(re_clock_advance(&c, NAN, true) == 0);
    ReCamera a = {.yaw = 179 * RE_PI / 180}, b = {.yaw = -179 * RE_PI / 180};
    CHECK(fabsf(fabsf(re_camera_interpolate(a, b, 0.5f).yaw) - RE_PI) < 0.001f);
    return true;
}
static bool load_fixture(ReWorld *w) {
    ReError error = {0};
    bool ok = re_world_load(RETRO_SOURCE_DIR "/assets/foundry.map", w, &error);
    if (!ok)
        (void)fprintf(stderr, "map:%zu: %s\n", error.line, error.message);
    return ok;
}
static bool maps(void) {
    ReWorld w = {0};
    ReError error = {0};
    CHECK(load_fixture(&w));
    CHECK(w.sector_count == 6 && w.marker_count == 9);
    CHECK(re_world_sector(&w, re_v2(12, 9), -1) == 2);
    CHECK(re_world_sector(&w, re_v2(-1, -1), -1) == -1);
    w.sectors[0].neighbor[2] = 99;
    CHECK(!re_world_validate(&w, &error));
    CHECK(error.line > 0);
    CHECK(load_fixture(&w));
    w.sectors[0].vertices[1] = w.sectors[0].vertices[0];
    CHECK(!re_world_validate(&w, &error));
    CHECK(load_fixture(&w));
    w.markers[0].position.x = -5;
    CHECK(!re_world_validate(&w, &error));
    const char *invalid[] = {
        "retro_map 2\n", "retro_map 1\nsector 0 nan 3 1 0 1 2\n",
        "retro_map 1\nsector 0 0 3 1 0 1 2\nv 0 0\nv 0 4\nv 4 4\nv 4 0\n",
        "retro_map 1\nsector 0 0 3 1 0 1 2\nv 0 0\nv 4 0\nv 4 4\nv 0 4\nlink 1 99 0\n",
        "retro_map 1\nsector 0 0 3 1 0 1 2\nv 0 0\nv 4 0\nv 4 4\nv 0 4\nspawn camera 0 9 9 0 0\n"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        FILE *file = fopen("invalid-test.map", "wb");
        CHECK(file != nullptr);
        bool written = fputs(invalid[i], file) >= 0;
        int closed = fclose(file);
        CHECK(written && closed == 0);
        CHECK(load_fixture(&w));
        size_t original = w.marker_count;
        CHECK(!re_world_load("invalid-test.map", &w, &error));
        CHECK(w.marker_count == original);
        CHECK(error.message[0] != '\0');
    }
    CHECK(remove("invalid-test.map") == 0);
    FILE *binary = fopen("invalid-test.map", "wb");
    CHECK(binary != nullptr);
    const char embedded_null[] = "retro_map 1\n\0sector 0 0 3 1 0 1 2\n";
    bool written =
        fwrite(embedded_null, 1, sizeof(embedded_null) - 1, binary) == sizeof(embedded_null) - 1;
    int closed = fclose(binary);
    CHECK(written && closed == 0);
    CHECK(!re_world_load("invalid-test.map", &w, &error) && error.line == 2);
    CHECK(remove("invalid-test.map") == 0);
    return true;
}
static bool collisions(void) {
    ReWorld w = {0};
    CHECK(load_fixture(&w));
    ReBody b = {.position = {4, 3, 0},
                .radius = .26f,
                .height = 1.7f,
                .step_height = .3f,
                .sector = 0,
                .grounded = true};
    re_body_move(&w, &b, re_v2(-20, 1), RE_FIXED_DT, 18);
    CHECK(b.position.x >= .259f && b.position.y > 3.9f); /* Sweep + sliding. */
    b.position = re_v3(.3f, .3f, 0);
    re_body_move(&w, &b, re_v2(-2, -2), RE_FIXED_DT, 18);
    CHECK(b.position.x >= .259f && b.position.y >= .259f); /* Esquina. */
    b.position = re_v3(7.6f, 9, 0);
    b.sector = 1;
    b.grounded = true;
    re_body_move(&w, &b, re_v2(.6f, 0), RE_FIXED_DT, 18);
    CHECK(b.sector == 2 && NEAR(b.position.z, .25f));
    w.sectors[2].ceiling = 1.5f;
    b.position = re_v3(7.6f, 9, 0);
    b.sector = 1;
    re_body_move(&w, &b, re_v2(2, 0), RE_FIXED_DT, 18);
    CHECK(b.position.x < 8 - .25f);
    CHECK(load_fixture(&w));
    w.sectors[4].ceiling = 0;
    b.position = re_v3(4, 16, 0);
    b.sector = 3;
    re_body_move(&w, &b, re_v2(0, 4), RE_FIXED_DT, 18);
    CHECK(b.position.y < 16.75f);
    CHECK(NEAR(re_world_raycast(&w, re_v3(4, 16, 1), re_v3(0, 1, 0), 30), 1));
    CHECK(re_world_next_sector(&w, 3, 5, 1.7f, .3f) == -1);
    w.sectors[4].ceiling = 3.2f;
    CHECK(re_world_next_sector(&w, 3, 5, 1.7f, .3f) == 4);
    re_body_move(&w, &b, re_v2(0, 3), RE_FIXED_DT, 18);
    CHECK(b.sector == 5);
    b.position = re_v3(4, 3, 0);
    b.sector = 0;
    b.vertical_speed = 6.2f;
    b.grounded = false;
    float highest = 0;
    for (int i = 0; i < 120; i++) {
        re_body_move(&w, &b, re_v2(0, 0), RE_FIXED_DT, 18);
        highest = fmaxf(highest, b.position.z);
    }
    CHECK(highest > .9f && b.grounded && NEAR(b.position.z, 0));
    CHECK(NEAR(re_world_raycast(&w, re_v3(4, 3, 1), re_v3(0, 0, -1), 30), 1));
    return true;
}

/* Regresión de Haunted: el ático comparte exactamente la planta XY del
 * vestíbulo. Sus paredes comienzan a z=3 y no pueden bloquear un jugador de
 * 1,70 m que cruza el portal visible entre los sectores 0 y 1. */
static bool haunted_stacked_portal(void) {
    char path[1024];
    CHECK(snprintf(path, sizeof(path), "%s/assets/studio/levels/house.map", RETRO_SOURCE_DIR) > 0);
    ReWorld world = {0};
    ReError error = {0};
    CHECK(re_world_load(path, &world, &error));
    ReBody player = {.position = {5.35f, 2, 0},
                     .radius = .26f,
                     .height = 1.7f,
                     .step_height = .3f,
                     .sector = 0,
                     .grounded = true};
    for (int tick = 0; tick < 20; tick++)
        re_body_move(&world, &player, re_v2(.08f, 0), RE_FIXED_DT, 18);
    CHECK(player.position.x > 6.2f);
    CHECK(player.sector == 1);

    /* Fuera del intervalo [0.2,0.8] la misma arista sí conserva pared. */
    player = (ReBody){.position = {5.35f, .35f, 0},
                      .radius = .26f,
                      .height = 1.7f,
                      .step_height = .3f,
                      .sector = 0,
                      .grounded = true};
    for (int tick = 0; tick < 20; tick++)
        re_body_move(&world, &player, re_v2(.08f, 0), RE_FIXED_DT, 18);
    CHECK(player.position.x < 5.75f);
    CHECK(player.sector == 0);
    return true;
}

/* Recorre todas las aberturas transitables del showcase en ambos sentidos.
 * Un portal que se dibuja abierto pero no deja pasar al mismo cilindro es una
 * regresión visible como “muralla invisible”. */
static bool haunted_all_portals(void) {
    ReWorld world = {0};
    ReError error = {0};
    CHECK(re_world_load(RETRO_SOURCE_DIR "/assets/studio/levels/house.map", &world, &error));
    for (size_t barrier = 0; barrier < world.barrier_count; barrier++) {
        world.barriers[barrier].open_fraction = 1;
        world.barriers[barrier].blocks = 0;
    }
    for (size_t sector_index = 0; sector_index < world.sector_count; sector_index++) {
        const ReSector *sector = &world.sectors[sector_index];
        for (size_t edge = 0; edge < sector->count; edge++) {
            int neighbor = sector->neighbor[edge];
            if (neighbor < 0)
                continue;
            const ReSector *other = &world.sectors[neighbor];
            float clearance =
                fminf(sector->ceiling, other->ceiling) - fmaxf(sector->floor, other->floor);
            float portal_width = re_length2(re_sub2(sector->vertices[(edge + 1u) % sector->count],
                                                    sector->vertices[edge])) *
                                 (sector->portal_end[edge] - sector->portal_start[edge]);
            if (clearance < 1.7f || portal_width < .8f ||
                other->floor - sector->floor > .3f + RE_EPSILON)
                continue;
            ReVec2 a = sector->vertices[edge];
            ReVec2 b = sector->vertices[(edge + 1u) % sector->count];
            float middle = (sector->portal_start[edge] + sector->portal_end[edge]) * .5f;
            ReVec2 portal = re_add2(a, re_scale2(re_sub2(b, a), middle));
            ReVec2 edge_direction = re_normalize2(re_sub2(b, a));
            ReVec2 inward = re_v2(-edge_direction.y, edge_direction.x);
            ReBody player = {
                .position = {portal.x + inward.x * .6f, portal.y + inward.y * .6f, sector->floor},
                .radius = .26f,
                .height = 1.7f,
                .step_height = .3f,
                .sector = (int)sector_index,
                .grounded = true};
            for (int tick = 0; tick < 20; tick++)
                re_body_move(&world, &player, re_scale2(inward, -.08f), RE_FIXED_DT, 18);
            if (player.sector != neighbor) {
                (void)fprintf(stderr,
                              "portal blocked sector=%zu edge=%zu neighbor=%d pos=%.3f,%.3f\n",
                              sector_index, edge, neighbor, (double)player.position.x,
                              (double)player.position.y);
                return false;
            }
        }
    }
    return true;
}
static bool rendering(void) {
    ReRenderer r = {0};
    ReTexture texture = {0};
    CHECK(!re_renderer_init(&r, -1, 20));
    CHECK(re_renderer_init(&r, 96, 54));
    CHECK(re_texture_init(&texture, 1, 1));
    texture.pixels[0] = re_rgba(240, 30, 20, 255);
    ReCamera c = {.position = {0, 0, 1}, .fov = RE_PI / 2, .near_plane = .05f, .far_plane = 20};
    ReVertex triangle[3] = {{{-1, 2, 0}, 0, 1}, {{1, 2, 0}, 1, 1}, {{0, 2, 2}, .5f, 0}};
    re_draw_triangle(&r, &c, triangle, &texture, 1);
    size_t center = 27u * 96u + 48u;
    CHECK(r.pixels[center].r > 180 && NEAR(r.depth[center], 2));
    for (int i = 0; i < 3; i++)
        triangle[i].position.y = 4;
    texture.pixels[0] = re_rgba(20, 240, 30, 255);
    re_draw_triangle(&r, &c, triangle, &texture, 1);
    CHECK(r.pixels[center].r > 180); /* La cara lejana no tapa a la cercana. */
    for (int i = 0; i < 3; i++)
        triangle[i].position.y = 1;
    texture.pixels[0].a = 0;
    re_draw_triangle(&r, &c, triangle, &texture, 1);
    CHECK(NEAR(r.depth[center], 2)); /* Transparencia tampoco escribe profundidad. */
    texture.pixels[0].a = 255;
    re_draw_triangle(&r, &c, triangle, &texture, 1);
    CHECK(r.pixels[center].g > 200 && NEAR(r.depth[center], 1));
    re_renderer_clear(&r, re_rgba(0, 0, 0, 255));
    triangle[0].position.y = -1;
    triangle[1].position.y = 2;
    triangle[2].position.y = 2;
    re_draw_triangle(&r, &c, triangle, &texture, 1);
    CHECK(r.stats.shaded > 0);
    for (size_t i = 0; i < 96u * 54u; i++)
        CHECK(!isnan(r.depth[i]) && r.depth[i] >= c.near_plane - 0.001f);
    re_draw_depth(&r, 10);
    CHECK(re_capture_ppm(&r, "renderer-test.ppm"));
    CHECK(remove("renderer-test.ppm") == 0);
    re_renderer_destroy(&r);
    re_renderer_destroy(&r);
    re_texture_destroy(&texture);
    return true;
}
static bool renderer_contracts(void) {
    ReRenderer r = {0};
    ReTexture texture = {0};
    CHECK(re_renderer_init(&r, 96, 54));
    CHECK(re_texture_init(&texture, 8, 8));
    float uv[4] = {0};
    CHECK(re_texture_cell_uv(&texture, 4, 4, 3, uv));
    CHECK(NEAR(uv[0], .5f) && NEAR(uv[1], .5f) && NEAR(uv[2], 1) && NEAR(uv[3], 1));
    CHECK(!re_texture_cell_uv(&texture, 3, 4, 0, uv));
    CHECK(!re_texture_cell_uv(&texture, 4, 4, 4, uv));
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            texture.pixels[(size_t)y * 8u + (size_t)x] =
                re_rgba((uint8_t)(x * 30), (uint8_t)(y * 30), 0, 255);
    ReCamera c = {.position = {0, 0, 1}, .fov = RE_PI / 2, .near_plane = .05f, .far_plane = 20};
    /* Oráculo analítico: proyecciones (0,75),(60,39),(48,3). Para el centro
     * del pixel (48,27), UV=(.2679045,.4880637): texel (2,3).
     * Interpolación afín incorrecta escogería (3,3). */
    ReVertex perspective[3] = {{{-1, 1, 0}, 0, 0}, {{1, 4, 0}, 1, 0}, {{0, 2, 2}, 0, 1}};
    re_draw_triangle(&r, &c, perspective, &texture, 1);
    RePixel p = r.pixels[27u * 96u + 48u];
    CHECK(p.r == 54 && p.g == 81);
    CHECK(fabsf(r.depth[27u * 96u + 48u] - 2.291777f) < .001f);
    re_renderer_clear(&r, re_rgba(0, 0, 0, 255));
    ReVertex square[4] = {
        {{-1, 2, 0}, 0, 0}, {{1, 2, 0}, 1, 0}, {{1, 2, 2}, 1, 1}, {{-1, 2, 2}, 0, 1}};
    ReVertex a[3] = {square[0], square[1], square[2]}, b[3] = {square[0], square[2], square[3]};
    re_draw_triangle(&r, &c, a, nullptr, 1);
    re_draw_triangle(&r, &c, b, nullptr, 1);
    size_t covered = 0;
    for (size_t i = 0; i < 96u * 54u; i++)
        if (isfinite(r.depth[i]))
            covered++;
    CHECK(covered == 48u * 48u); /* Sin grieta en la diagonal compartida. */
    re_renderer_clear(&r, re_rgba(0, 0, 0, 255));
    ReVertex wall1[3] = {{{-1, 2, 0}, 0, 0}, {{0, 2, 0}, 0, 0}, {{0, 2, 2}, 0, 0}};
    ReVertex wall2[3] = {{{-1, 2, 0}, 0, 0}, {{0, 2, 2}, 0, 0}, {{-1, 2, 2}, 0, 0}};
    re_draw_triangle(&r, &c, wall1, nullptr, 1);
    re_draw_triangle(&r, &c, wall2, nullptr, 1);
    for (size_t i = 0; i < 64; i++)
        texture.pixels[i] = re_rgba(230, 10, 10, 255);
    re_draw_billboard(&r, &c, re_v3(0, 3, 0), 2, 2, &texture, 1);
    CHECK(r.pixels[27u * 96u + 40u].g > 200); /* Mitad oculta por pared blanca. */
    CHECK(r.pixels[27u * 96u + 56u].r > 150 && r.pixels[27u * 96u + 56u].g < 20);
    re_texture_destroy(&texture);
    re_renderer_destroy(&r);
    return true;
}

static bool dynamic_lighting(void) {
    ReRenderer renderer = {0};
    ReTexture texture = {0};
    CHECK(re_renderer_init(&renderer, 96, 54));
    CHECK(re_texture_init(&texture, 1, 1));
    texture.pixels[0] = re_rgba(80, 70, 60, 255);
    ReCamera camera = {
        .position = {0, 0, 1}, .fov = RE_PI / 2, .near_plane = .05f, .far_plane = 20};
    ReVertex triangle[3] = {{{-1, 2, 0}, 0, 0}, {{1, 2, 0}, 1, 0}, {{0, 2, 2}, .5f, 1}};
    re_draw_triangle(&renderer, &camera, triangle, &texture, .5f);
    size_t center = 27u * 96u + 48u;
    RePixel before = renderer.pixels[center];
    ReLight light = {.id = "test",
                     .kind = RE_LIGHT_POINT,
                     .position = {0, 2, 1},
                     .color = {1, .5f, .25f},
                     .radius = 4,
                     .intensity = 1,
                     .initially_enabled = true};
    bool enabled = true;
    re_apply_lights(&renderer, &camera, &light, &enabled, 1, 0);
    RePixel after = renderer.pixels[center];
    CHECK(after.r > before.r && after.b >= before.b && after.r % 16u == 0);
    re_texture_destroy(&texture);
    re_renderer_destroy(&renderer);
    return true;
}
static FpsGame *new_game(void) {
    FpsGame *g = calloc(1, sizeof(*g));
    ReError error = {0};
    if (!g)
        return nullptr;
    if (!fps_game_init(g, RETRO_SOURCE_DIR "/assets/foundry.map", &error)) {
        (void)fprintf(stderr, "game:%zu: %s\n", error.line, error.message);
        free(g);
        return nullptr;
    }
    return g;
}
static void aim(FpsGame *g, ReVec3 target, ReInput *input) {
    ReVec3 delta = re_sub3(target, g->camera.position);
    float yaw = atan2f(delta.x, delta.y),
          pitch = atan2f(delta.z, sqrtf(delta.x * delta.x + delta.y * delta.y));
    input->look.x = remainderf(yaw - g->camera.yaw, 2 * RE_PI) / g->sensitivity;
    input->look.y = -(pitch - g->camera.pitch) / g->sensitivity;
}
/* Piloto de aceptación: usa movimiento y disparos normales, no teletransporta
 * ni concede llave/vida/munición. Apunta al guardia visible más cercano. */
static void pilot_tick(FpsGame *g, ReVec2 waypoint) {
    ReInput input = {.focused = true};
    ReVec2 offset = re_sub2(waypoint, re_v2(g->player.position.x, g->player.position.y));
    aim(g, re_v3(waypoint.x, waypoint.y, g->camera.position.z), &input);
    int target = -1;
    float nearest = 50;
    for (size_t i = 0; i < g->enemy_count; i++)
        if (g->enemies[i].health > 0) {
            ReVec3 center = re_add3(g->enemies[i].body.position, re_v3(0, 0, 1.1f));
            ReVec3 delta = re_sub3(center, g->camera.position);
            float distance = sqrtf(re_dot3(delta, delta));
            if (distance < nearest &&
                re_world_raycast(&g->world, g->camera.position, re_scale3(delta, 1 / distance),
                                 distance) >= distance - .01f) {
                target = (int)i;
                nearest = distance;
            }
        }
    if (target >= 0) {
        aim(g, re_add3(g->enemies[target].body.position, re_v3(0, 0, 1.1f)), &input);
        input.held = RE_PRIMARY;
    } else if (re_length2(offset) > .12f)
        input.movement.y = 1;
    input.pressed |= RE_INTERACT;
    fps_game_tick(g, input, RE_FIXED_DT);
}
static bool game_journey(void) {
    FpsGame *g = new_game();
    CHECK(g != nullptr);
    CHECK(g->mode == FPS_TITLE);
    fps_game_frame(g, (ReInput){.pressed = RE_ACCEPT, .focused = true});
    CHECK(g->mode == FPS_PLAYING);
    ReVec2 waypoints[] = {{4, 8}, {11.8f, 8}, {12.5f, 8}, {4, 9}, {4, 16}, {4, 19}, {4, 22}};
    for (size_t w = 0; w < sizeof(waypoints) / sizeof(waypoints[0]); w++) {
        int tick = 0;
        while (g->mode == FPS_PLAYING && tick++ < 1800 &&
               re_length2(
                   re_sub2(waypoints[w], re_v2(g->player.position.x, g->player.position.y))) > .15f)
            pilot_tick(g, waypoints[w]);
        if (tick >= 1800)
            (void)fprintf(stderr, "pilot stuck waypoint=%zu pos=%.2f,%.2f health=%d\n", w,
                          (double)g->player.position.x, (double)g->player.position.y, g->health);
        CHECK(tick < 1800);
    }
    CHECK(g->mode == FPS_WON && g->key && g->kills == 3 && g->health > 0);
    fps_game_restart(g);
    CHECK(g->health == 100 && g->ammo == 30 && !g->key && g->kills == 0);
    CHECK(NEAR(g->world.sectors[g->door_sector].ceiling, 0));
    fps_game_frame(g, (ReInput){.pressed = RE_PAUSE, .focused = true});
    CHECK(g->mode == FPS_PAUSED);
    ReVec3 before = g->player.position;
    fps_game_tick(g, (ReInput){.movement = {0, 1}}, RE_FIXED_DT);
    CHECK(NEAR(g->player.position.y, before.y));
    fps_game_frame(g, (ReInput){.pressed = RE_PAUSE, .focused = true});
    fps_game_frame(g, (ReInput){.focused = false});
    CHECK(g->mode == FPS_PAUSED);
    fps_game_restart(g);
    for (int tick = 0; tick < 4000 && g->mode == FPS_PLAYING; tick++)
        fps_game_tick(g, (ReInput){0}, RE_FIXED_DT);
    CHECK(g->mode == FPS_LOST && g->health == 0);
    fps_game_frame(g, (ReInput){.pressed = RE_ACCEPT, .focused = true});
    CHECK(g->mode == FPS_PLAYING && g->health == 100);
    free(g);
    return true;
}
static bool shooting_blocked(void) {
    FpsGame *g = new_game();
    CHECK(g != nullptr);
    fps_game_restart(g);
    /* Fixture dirigido: guardia detrás de una compuerta cerrada. */
    g->enemy_count = 1;
    g->enemies[0].body.position = re_v3(4, 20, 0);
    g->enemies[0].body.sector = 5;
    g->player.position = re_v3(4, 16, 0);
    g->player.sector = 3;
    g->camera.position = re_v3(4, 16, 1.52f);
    ReInput input = {.pressed = RE_PRIMARY};
    aim(g, re_v3(4, 20, 1.1f), &input);
    fps_game_tick(g, input, RE_FIXED_DT);
    CHECK(g->enemies[0].health == 60 && g->ammo == 29);
    g->world.sectors[4].ceiling = 3.2f;
    g->shot_timer = 0;
    input = (ReInput){.pressed = RE_PRIMARY};
    aim(g, re_v3(4, 20, 1.1f), &input);
    fps_game_tick(g, input, RE_FIXED_DT);
    CHECK(g->enemies[0].health == 30);
    free(g);
    return true;
}
static bool solid_actors(void) {
    FpsGame *g = new_game();
    CHECK(g != nullptr);
    fps_game_restart(g);
    g->enemy_count = 1;
    g->enemies[0].body.position = re_v3(4, 3, 0);
    g->enemies[0].body.sector = 0;
    ReInput input = {.movement = {0, 1}};
    for (int tick = 0; tick < 40; tick++)
        fps_game_tick(g, input, RE_FIXED_DT);
    CHECK(g->player.position.y < 2.47f);
    g->enemies[0].health = 0;
    g->enemies[0].state = FPS_CORPSE;
    for (int tick = 0; tick < 30; tick++)
        fps_game_tick(g, input, RE_FIXED_DT);
    CHECK(g->player.position.y > 3.5f);
    free(g);
    return true;
}

static bool studio_world(void) {
    static ReProject project_storage;
    ReProject *project = &project_storage;
    *project = (ReProject){0};
    ReError error = {0};
    CHECK(re_project_load(RETRO_SOURCE_DIR "/assets/studio/haunted.retro", project, &error));
    CHECK(project->world.sector_count == 16 && project->world.barrier_count == 4);
    CHECK(project->character_count == 4 && project->format_version == 2);
    CHECK(project->interactions.rule_count >= 10 && project->interactions.dialogue_count == 3);
    /* Mismo XY, distinta Z: la consulta volumétrica distingue las dos plantas. */
    CHECK(re_world_sector_at(&project->world, re_v3(2, 2, .5f), -1) == 0);
    CHECK(re_world_sector_at(&project->world, re_v3(2, 2, 3.5f), -1) == 12);
    CHECK(re_world_sector(&project->world, re_v2(2, 2), -1) == 0); /* API v1 estable. */
    ReTraceHit door =
        re_world_trace(&project->world, re_v3(18, 2, 3), re_v3(1, 0, 0), 10, RE_BLOCK_SIGHT);
    CHECK(door.barrier == 2 && NEAR(door.distance, 2));
    project->world.barriers[2].open_fraction = 1;
    CHECK(NEAR(re_world_trace(&project->world, re_v3(18, 2, 3), re_v3(1, 0, 0), 10, RE_BLOCK_SIGHT)
                   .distance,
               8)); /* La pared exterior de sector 11 está en x=26. */
    project->world.barriers[2].open_fraction = 0;
    project->world.barriers[2].kind = RE_BARRIER_WINDOW;
    project->world.barriers[2].blocks = RE_BLOCK_MOVEMENT | RE_BLOCK_PROJECTILE;
    project->world.barriers[2].health = 20;
    CHECK(NEAR(re_world_trace(&project->world, re_v3(18, 2, 3), re_v3(1, 0, 0), 10, RE_BLOCK_SIGHT)
                   .distance,
               8));
    CHECK(re_barrier_damage(&project->world, 2, 20));
    CHECK(project->world.barriers[2].open_fraction == 1 && project->world.barriers[2].blocks == 0);
    CHECK(re_world_save_v2("roundtrip-v2.map", &project->world, &error));
    ReWorld loaded = {0};
    CHECK(re_world_load("roundtrip-v2.map", &loaded, &error));
    CHECK(loaded.sector_count == 16 && loaded.marker_count == 10 && loaded.format_version == 2);
    CHECK(remove("roundtrip-v2.map") == 0);
    return true;
}

static bool partial_portals(void) {
    static ReProject project_storage;
    ReProject *project = &project_storage;
    *project = (ReProject){0};
    ReError error = {0};
    CHECK(re_project_load(RETRO_SOURCE_DIR "/assets/studio/haunted.retro", project, &error));
    const ReSector *foyer = &project->world.sectors[0];
    CHECK(project->world.format_version == 4);
    CHECK(NEAR(foyer->portal_start[1], .2f) && NEAR(foyer->portal_end[1], .8f));

    /* La misma arista tiene mamposteria junto a la esquina y una abertura en
     * el centro. Esta prueba protege la coherencia de rayos y barreras. */
    ReTraceHit jamb =
        re_world_trace(&project->world, re_v3(2, .2f, 1), re_v3(1, 0, 0), 20, RE_BLOCK_SIGHT);
    ReTraceHit opening =
        re_world_trace(&project->world, re_v3(2, 2, 1), re_v3(1, 0, 0), 20, RE_BLOCK_SIGHT);
    CHECK(NEAR(jamb.distance, 4) && jamb.barrier == -1);
    CHECK(opening.distance > jamb.distance + 1);

    CHECK(re_world_save_v4("roundtrip-v4.map", &project->world, &error));
    ReWorld loaded = {0};
    CHECK(re_world_load("roundtrip-v4.map", &loaded, &error));
    CHECK(loaded.format_version == 4 && NEAR(loaded.sectors[0].portal_start[1], .2f));
    CHECK(remove("roundtrip-v4.map") == 0);
    return true;
}

static bool reusable_gameplay(void) {
    static ReProject project_storage;
    static ReGameplay gameplay_storage;
    ReProject *project = &project_storage;
    ReGameplay *gameplay = &gameplay_storage;
    *project = (ReProject){0};
    *gameplay = (ReGameplay){0};
    ReError error = {0};
    CHECK(re_project_load(RETRO_SOURCE_DIR "/assets/studio/haunted.retro", project, &error));
    ReCharacterDef *kidnapper = &project->characters[0];
    kidnapper->tracking = RE_TRACK_OMNISCIENT;
    re_gameplay_init(gameplay, &project->world, project->characters, project->character_count, 42);
    ReEntityId pursuer = re_gameplay_spawn(gameplay, 0, re_v3(7, 2, 0), RE_PI * 1.5f, 1);
    CHECK(pursuer.index != UINT16_MAX);
    ReBody player = {.position = {23, 2, 2},
                     .radius = .28f,
                     .height = 1.72f,
                     .step_height = .3f,
                     .sector = 11,
                     .grounded = true};
    bool captured = false;
    for (int tick = 0; tick < 1800 && !captured; tick++) {
        re_gameplay_tick(
            gameplay,
            (ReGameplayInput){.player = &player,
                              .player_eye = re_add3(player.position, re_v3(0, 0, 1.5f)),
                              .player_noise = 0},
            RE_FIXED_DT);
        ReGameplayEvent event;
        while (re_gameplay_event(gameplay, &event))
            captured |= event.kind == RE_EVENT_CAPTURED;
    }
    ReCharacter *actor = re_gameplay_get(gameplay, pursuer);
    CHECK(actor != nullptr && actor->body.sector == 11 && captured);
    CHECK(project->world.barriers[2].open_fraction >= .95f); /* Abrió la puerta de escalera. */
    /* Una captura sólo se emite una vez aunque se procesen más ticks. */
    re_gameplay_tick(gameplay, (ReGameplayInput){.player = &player, .player_eye = player.position},
                     RE_FIXED_DT);
    ReGameplayEvent event;
    CHECK(!re_gameplay_event(gameplay, &event));
    ReEntityId boss = re_gameplay_spawn(gameplay, 1, re_v3(22, 2, 2), 0, 11);
    CHECK(re_gameplay_damage(gameplay, boss, 170, 0));
    re_gameplay_tick(gameplay, (ReGameplayInput){.player = &player, .player_eye = player.position},
                     RE_FIXED_DT);
    bool phase = false;
    while (re_gameplay_event(gameplay, &event))
        phase |= event.kind == RE_EVENT_PHASE_CHANGED && event.source.index == boss.index;
    CHECK(phase);
    const ReCharacter *boss_actor = re_gameplay_get(gameplay, boss);
    CHECK(boss_actor != nullptr && boss_actor->phase == 1);
    int cell = re_character_animation_cell(kidnapper, actor, "chase", 0);
    CHECK(cell >= 4);
    return true;
}

static bool safe_door(void) {
    static ReProject project_storage;
    ReProject *project = &project_storage;
    *project = (ReProject){0};
    ReError error = {0};
    CHECK(re_project_load(RETRO_SOURCE_DIR "/assets/studio/haunted.retro", project, &error));
    project->world.barriers[2].open_fraction = 1;
    ReBody body = {.position = {20, 2, 2}, .radius = .3f, .height = 1.7f, .sector = 10};
    CHECK(!re_barrier_tick(&project->world, 2, 0, 2, RE_FIXED_DT, &body, 1));
    CHECK(project->world.barriers[2].open_fraction == 1);
    body.position.x = 18;
    for (int i = 0; i < 60; i++)
        (void)re_barrier_tick(&project->world, 2, 0, 2, RE_FIXED_DT, &body, 1);
    CHECK(project->world.barriers[2].open_fraction < .01f);
    return true;
}

static bool logic_event(ReInteractionRuntime *runtime, enum ReLogicEventKind kind,
                        const char *source, ReVec3 position) {
    ReLogicEvent event = {.kind = kind, .position = position};
    (void)snprintf(event.source, sizeof(event.source), "%s", source);
    return re_interaction_emit(runtime, event);
}

static bool interaction_journey(void) {
    static ReProject project_storage;
    static ReGameplay gameplay_storage;
    static ReInteractionRuntime runtime_storage;
    ReProject *project = &project_storage;
    ReGameplay *gameplay = &gameplay_storage;
    ReInteractionRuntime *runtime = &runtime_storage;
    *project = (ReProject){0};
    *gameplay = (ReGameplay){0};
    *runtime = (ReInteractionRuntime){0};
    ReError error = {0};
    CHECK(re_project_load(RETRO_SOURCE_DIR "/assets/studio/haunted.retro", project, &error));
    re_gameplay_init(gameplay, &project->world, project->characters, project->character_count, 7);
    ReBody player = {.position = {2, 2, 0},
                     .radius = .28f,
                     .height = 1.72f,
                     .step_height = .3f,
                     .sector = 0,
                     .grounded = true};
    re_interaction_init(runtime, &project->interactions, &project->world, gameplay, 100, 3);
    re_interaction_tick(runtime, &player, RE_FIXED_DT);
    CHECK(re_interaction_objective(runtime, "restore_power") == RE_OBJECTIVE_ACTIVE);
    CHECK(runtime->state.checkpoint_valid && runtime->state.message_time > 0);

    CHECK(logic_event(runtime, RE_LOGIC_INTERACT, "survivor", player.position));
    re_interaction_tick(runtime, &player, RE_FIXED_DT);
    CHECK(re_interaction_dialogue(runtime) != nullptr);
    CHECK(re_interaction_choose(runtime, 0));
    CHECK(strcmp(re_interaction_dialogue(runtime)->id, "survivor_thanks") == 0);
    CHECK(re_interaction_choose(runtime, 0));
    re_interaction_tick(runtime, &player, RE_FIXED_DT);
    CHECK(re_interaction_item_count(runtime, "bandage") == 1);

    CHECK(logic_event(runtime, RE_LOGIC_INTERACT, "fuse_box", player.position));
    re_interaction_tick(runtime, &player, RE_FIXED_DT);
    const ReValue *power = re_interaction_variable(runtime, "power");
    CHECK(power && power->kind == RE_VALUE_BOOL && power->as.boolean);
    CHECK(re_interaction_objective(runtime, "restore_power") == RE_OBJECTIVE_COMPLETE);
    CHECK(runtime->state.light_enabled[2]);

    player.position = re_v3(3, 7, 0);
    player.sector = 13;
    CHECK(logic_event(runtime, RE_LOGIC_ENTITY_DIED, "caretaker-main", player.position));
    re_interaction_tick(runtime, &player, RE_FIXED_DT);
    CHECK(runtime->state.pickup_count == 2 && runtime->state.pickups[1].active);
    re_interaction_tick(runtime, &player, RE_FIXED_DT);
    CHECK(re_interaction_item_count(runtime, "brass_key") == 1);
    CHECK(re_interaction_objective(runtime, "find_key") == RE_OBJECTIVE_COMPLETE);

    CHECK(logic_event(runtime, RE_LOGIC_INTERACT, "stair_panel", player.position));
    re_interaction_tick(runtime, &player, RE_FIXED_DT);
    CHECK(project->world.barriers[2].open_fraction == 1);
    CHECK(re_interaction_item_count(runtime, "brass_key") == 0);

    runtime->state.player_health = 63;
    CHECK(re_save_write("interaction-slot.rfs", project->id, runtime, &player, &error));
    runtime->state.player_health = 1;
    player.position = re_v3(0, 0, 0);
    CHECK(re_save_read("interaction-slot.rfs", project->id, runtime, &player, &error));
    CHECK(runtime->state.player_health == 63 && NEAR(player.position.x, 3) &&
          project->world.barriers[2].open_fraction == 1);
    FILE *corrupt = fopen("interaction-slot.rfs", "r+b");
    CHECK(corrupt != nullptr);
    bool corrupted = fseek(corrupt, -1, SEEK_END) == 0;
    int byte = corrupted ? fgetc(corrupt) : EOF;
    corrupted = corrupted && byte != EOF && fseek(corrupt, -1, SEEK_CUR) == 0 &&
                fputc(byte ^ 0x5a, corrupt) != EOF;
    int corrupt_close = fclose(corrupt);
    CHECK(corrupted && corrupt_close == 0);
    CHECK(!re_save_read("interaction-slot.rfs", project->id, runtime, &player, &error));
    CHECK(runtime->state.player_health == 63); /* Carga corrupta es transaccional. */
    CHECK(remove("interaction-slot.rfs") == 0);
    ReInteractionDefinitions roundtrip = {0};
    CHECK(re_interaction_save_rules("roundtrip.rules", &project->interactions, &error));
    CHECK(re_interaction_load_rules("roundtrip.rules", &roundtrip, &error));
    CHECK(re_interaction_save_dialogues("roundtrip.dialogue", &project->interactions, &error));
    CHECK(re_interaction_load_dialogues("roundtrip.dialogue", &roundtrip, &error));
    CHECK(roundtrip.rule_count == project->interactions.rule_count &&
          roundtrip.dialogue_count == project->interactions.dialogue_count &&
          roundtrip.light_count == project->interactions.light_count);
    CHECK(remove("roundtrip.rules") == 0 && remove("roundtrip.dialogue") == 0);

    /* El retry descarta toda la línea temporal posterior al checkpoint:
     * inventario, reglas one-shot, drops, luces y barreras incluidas. */
    re_interaction_checkpoint_restore(runtime, &player);
    power = re_interaction_variable(runtime, "power");
    CHECK(power && power->kind == RE_VALUE_BOOL && !power->as.boolean);
    CHECK(runtime->state.player_health == 100 && NEAR(player.position.x, 2) &&
          NEAR(player.position.y, 2));
    CHECK(project->world.barriers[2].open_fraction == 0);
    CHECK(re_interaction_item_count(runtime, "bandage") == 0 &&
          re_interaction_item_count(runtime, "brass_key") == 0);
    CHECK(runtime->state.pickup_count == 1 && !runtime->state.game_over &&
          runtime->state.event_count == 0);
    return true;
}

static bool rule_cycle_guard(void) {
    static ReInteractionDefinitions definitions;
    static ReInteractionRuntime runtime;
    definitions = (ReInteractionDefinitions){.rule_count = 1};
    definitions.rules[0] = (ReRuleDefinition){
        .event = RE_LOGIC_CUSTOM, .priority = 1, .action_count = 1, .source = "*"};
    (void)snprintf(definitions.rules[0].id, sizeof(definitions.rules[0].id), "cycle");
    definitions.rules[0].actions[0] = (ReRuleAction){
        .kind = RE_RULE_EMIT_EVENT, .value = {.kind = RE_VALUE_INT, .as.integer = RE_LOGIC_CUSTOM}};
    ReBody player = {.sector = 0};
    re_interaction_init(&runtime, &definitions, nullptr, nullptr, 100, 1);
    CHECK(logic_event(&runtime, RE_LOGIC_CUSTOM, "seed", re_v3(0, 0, 0)));
    re_interaction_tick(&runtime, &player, RE_FIXED_DT);
    CHECK(runtime.state.cycle_limited && runtime.state.event_count == 0);
    return true;
}

/* Verifica el contrato usado por WPF sin escribir los fixtures. La edición
 * permanece en memoria; cerrar el documento descarta deliberadamente el redo
 * final y demuestra que consultar datos anidados no depende de estructuras C. */
static bool editor_document(void) {
    ReEditorDocument *document = nullptr;
    ReError error = {0};
    CHECK(re_editor_open(RETRO_SOURCE_DIR "/assets/studio/haunted.retro", &document, &error));
    CHECK(document != nullptr);

    ReEditorOverview overview = {0};
    CHECK(re_editor_overview(document, &overview));
    CHECK(overview.sector_count > 0 && overview.character_count > 0 && overview.rule_count > 0);
    CHECK(!overview.dirty && !overview.can_undo);

    ReEditorCharacterView character = {0};
    CHECK(re_editor_character(document, 0, &character));
    if (character.animation_count > 0) {
        ReEditorAnimationView animation = {0};
        CHECK(re_editor_animation(document, 0, 0, &animation));
        CHECK(animation.frame_count > 0 && animation.name[0] != '\0');
        ReEditorAnimationFrameView frame = {0};
        CHECK(re_editor_animation_frame(document, 0, 0, 0, &frame));
        CHECK(frame.duration > 0);
    }

    ReEditorSectorView original = {0}, changed = {0}, restored = {0};
    CHECK(re_editor_sector(document, 0, &original));
    CHECK(re_editor_set_property(document, RE_EDITOR_SECTOR, 0, "light", "0.123", &error));
    CHECK(re_editor_sector(document, 0, &changed) && NEAR(changed.light, 0.123f));
    CHECK(re_editor_overview(document, &overview) && overview.dirty && overview.can_undo);
    CHECK(re_editor_undo(document));
    CHECK(re_editor_sector(document, 0, &restored) && NEAR(restored.light, original.light));
    CHECK(re_editor_redo(document));
    CHECK(re_editor_sector(document, 0, &changed) && NEAR(changed.light, 0.123f));
    /* Rechazar una edición después de deshacer debe conservar la rama futura.
     * Antes, begin_command la truncaba aunque el valor no fuera válido. */
    CHECK(re_editor_undo(document));
    CHECK(re_editor_overview(document, &overview));
    uint64_t before_invalid = overview.revision;
    CHECK(!re_editor_set_property(document, RE_EDITOR_SECTOR, 0, "light", "nan", &error));
    CHECK(re_editor_overview(document, &overview) && overview.can_redo &&
          overview.revision == before_invalid);
    CHECK(re_editor_redo(document));
    CHECK(re_editor_sector(document, 0, &changed) && NEAR(changed.light, 0.123f));

    uint64_t revision = overview.revision;
    CHECK(!re_editor_set_property(document, RE_EDITOR_SECTOR, 0, "unknown", "1", &error));
    CHECK(error.message[0] != '\0');
    CHECK(re_editor_overview(document, &overview));
    CHECK(overview.revision >= revision);
    if (overview.light_count > 0) {
        CHECK(re_editor_set_property(document, RE_EDITOR_LIGHT, 0, "intensity", ".75", &error));
        ReEditorLightView light = {0};
        CHECK(re_editor_light(document, 0, &light) && NEAR(light.intensity, .75f));
    }
    if (overview.trigger_count > 0) {
        CHECK(re_editor_set_property(document, RE_EDITOR_TRIGGER, 0, "once", "true", &error));
        ReEditorTriggerView trigger = {0};
        CHECK(re_editor_trigger(document, 0, &trigger) && trigger.once);
    }

    uint32_t placed = 0, duplicate = 0, drop_rule = 0;
    uint32_t original_markers = overview.marker_count;
    uint32_t original_rules = overview.rule_count;
    CHECK(re_editor_create_marker(document, "actor", "caretaker", 1, 1, &placed, &error));
    CHECK(placed == original_markers);
    CHECK(re_editor_move_marker(document, placed, 1.5f, 1.5f, &error));
    ReEditorMarkerView moved = {0};
    CHECK(re_editor_marker(document, placed, &moved));
    CHECK(NEAR(moved.x, 1.5f) && NEAR(moved.y, 1.5f));
    CHECK(re_editor_duplicate_marker(document, placed, &duplicate, &error));
    CHECK(duplicate == original_markers + 1u);
    CHECK(re_editor_delete_marker(document, duplicate, &error));
    CHECK(re_editor_add_drop_rule(document, placed, "brass_key", &drop_rule, &error));
    CHECK(drop_rule == original_rules);
    uint32_t interaction_rule = 0;
    CHECK(re_editor_add_interaction_rule(document, placed, RE_RULE_OPEN_BARRIER, "foyer-door", "",
                                         &interaction_rule, &error));
    CHECK(interaction_rule == original_rules + 1u);
    ReEditorRuleActionView interaction_action = {0};
    CHECK(re_editor_rule_action(document, interaction_rule, 0, &interaction_action));
    CHECK(interaction_action.kind == RE_RULE_OPEN_BARRIER &&
          strcmp(interaction_action.target, "foyer-door") == 0);
    CHECK(!re_editor_delete_marker(document, placed, &error)); /* Regla protege la referencia. */
    CHECK(error.message[0] != '\0');
    re_editor_close(document);
    return true;
}
int main(void) {
    const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {{"geometry", geometry},
                 {"input_clock", input_clock},
                 {"maps", maps},
                 {"collisions", collisions},
                 {"haunted_stacked_portal", haunted_stacked_portal},
                 {"haunted_all_portals", haunted_all_portals},
                 {"rendering", rendering},
                 {"renderer_contracts", renderer_contracts},
                 {"dynamic_lighting", dynamic_lighting},
                 {"game_journey", game_journey},
                 {"shooting_blocked", shooting_blocked},
                 {"solid_actors", solid_actors},
                 {"studio_world", studio_world},
                 {"partial_portals", partial_portals},
                 {"reusable_gameplay", reusable_gameplay},
                 {"safe_door", safe_door},
                 {"interaction_journey", interaction_journey},
                 {"rule_cycle_guard", rule_cycle_guard},
                 {"editor_document", editor_document}};
    int failures = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool ok = tests[i].run();
        (void)printf("%s %s\n", ok ? "PASS" : "FAIL", tests[i].name);
        if (!ok)
            failures++;
    }
    return failures ? 1 : 0;
}
