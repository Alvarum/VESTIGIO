/* Presentación del FPS. Lee estado const: dibujar jamás avanza temporizadores,
 * consume munición ni mueve enemigos. HUD y menú usan el canvas del motor. */
#include "game.h"
#include <stdio.h>

static const RePixel ink = {10, 17, 22, 255}, paper = {218, 230, 222, 255};
static const RePixel muted = {133, 157, 159, 255}, accent = {240, 158, 69, 255},
                     mint = {103, 216, 193, 255};

static void automap(const FpsGame *g, ReRenderer *r) {
    re_rect(r, 312, 22, 157, 186, re_rgba(7, 16, 20, 232));
    re_text(r, 322, 32, "PLANO / F1", 1, mint);
    float scale = 6;
    for (size_t i = 0; i < g->world.sector_count; i++) {
        const ReSector *s = &g->world.sectors[i];
        for (size_t e = 0; e < s->count; e++) {
            ReVec2 a = s->vertices[e], b = s->vertices[(e + 1) % s->count];
            re_line(r, 334 + (int)(a.x * scale), 194 - (int)(a.y * scale), 334 + (int)(b.x * scale),
                    194 - (int)(b.y * scale),
                    s->neighbor[e] < 0 ? paper : re_rgba(62, 102, 109, 255));
        }
    }
    int px = 334 + (int)(g->player.position.x * scale),
        py = 194 - (int)(g->player.position.y * scale);
    re_rect(r, px - 2, py - 2, 5, 5, accent);
    re_line(r, px, py, px + (int)(sinf(g->camera.yaw) * 9), py - (int)(cosf(g->camera.yaw) * 9),
            accent);
    for (size_t i = 0; i < g->enemy_count; i++)
        if (g->enemies[i].health > 0)
            re_rect(r, 333 + (int)(g->enemies[i].body.position.x * scale),
                    193 - (int)(g->enemies[i].body.position.y * scale), 3, 3,
                    re_rgba(240, 84, 72, 255));
}
static void hud(const FpsGame *g, const FpsArt *art, ReRenderer *r) {
    char text[100];
    int recoil = g->shot_timer > 0.12f ? (int)((g->shot_timer - 0.12f) * 65) : 0;
    int bob = g->player.grounded ? (int)(sinf(g->elapsed * 5) * 1.5f) : 0;
    re_blit(r, &art->weapon, 144, 137 + recoil + bob, 2);
    if (g->shot_timer > g->tuning.shot_interval - 0.05f) {
        re_rect(r, 234, 140 + recoil, 14, 10, accent);
        re_rect(r, 239, 131 + recoil, 5, 29, paper);
        re_rect(r, 225, 143 + recoil, 32, 4, accent);
    }
    RePixel cross = g->hit_timer > 0 ? accent : paper;
    re_line(r, 231, 135, 236, 135, cross);
    re_line(r, 244, 135, 249, 135, cross);
    re_line(r, 240, 126, 240, 131, cross);
    re_line(r, 240, 139, 240, 144, cross);
    if (g->hurt_timer > 0) {
        RePixel damage = re_rgba(197, 44, 38, 125);
        re_rect(r, 0, 0, 480, 9, damage);
        re_rect(r, 0, 9, 8, 231, damage);
        re_rect(r, 472, 9, 8, 231, damage);
    }
    re_rect(r, 0, 238, 480, 32, ink);
    re_rect(r, 0, 238, 480, 1, re_rgba(74, 91, 95, 255));
    re_rect(r, 12, 247, 3, 13, mint);
    re_text(r, 22, 246, "VIDA", 1, muted);
    (void)snprintf(text, sizeof(text), "%03d", g->health);
    re_text(r, 57, 246, text, 2, paper);
    re_rect(r, 117, 245, 1, 19, re_rgba(48, 64, 68, 255));
    re_text(r, 131, 246, "MUNICION", 1, muted);
    (void)snprintf(text, sizeof(text), "%02d", g->ammo);
    re_text(r, 190, 246, text, 2, accent);
    re_text(r, 253, 246, "ACCESO", 1, muted);
    re_text(r, 253, 256, g->key ? "AUTORIZADO" : "BLOQUEADO", 1, g->key ? mint : accent);
    (void)snprintf(text, sizeof(text), "%d / %zu", g->kills, g->enemy_count);
    re_text(r, 393, 246, "GUARDIAS", 1, muted);
    re_text(r, 393, 256, text, 1, paper);
    re_rect(r, 10, 9, 107, 14, re_rgba(10, 17, 22, 210));
    re_text(r, 16, 13, "FOUNDRY / SECTOR", 1, muted);
    if (g->message_timer > 0) {
        re_rect(r, 8, 216, 464, 16, re_rgba(10, 17, 22, 220));
        re_text(r, 16, 221, g->message, 1, accent);
    }
    if (g->map_view)
        automap(g, r);
}
static void menu_choice(ReRenderer *r, int y, const char *text, int index, int selection) {
    if (index == selection) {
        re_rect(r, 26, y - 5, 239, 19, accent);
        re_text(r, 34, y, ">", 1, ink);
    }
    re_text(r, 49, y, text, 1, index == selection ? ink : paper);
}
static void menu(const FpsGame *g, ReRenderer *r) {
    char text[80];
    re_rect(r, 0, 0, 480, 270, re_rgba(6, 12, 18, 120));
    re_rect(r, 0, 0, 289, 270, re_rgba(7, 15, 20, 236));
    re_rect(r, 26, 28, 27, 3, accent);
    re_text(r, 63, 27, "RETROFORGE / 01", 1, muted);
    const char *title = "FOUNDRY";
    const char *subtitle = "RECUPERA EL ACCESO. ENCUENTRA LA SALIDA.";
    if (g->mode == FPS_PAUSED) {
        title = "EN PAUSA";
        subtitle = "TU PROGRESO CONTINUA AQUI.";
    }
    if (g->mode == FPS_OPTIONS) {
        title = "AJUSTES";
        subtitle = "CAMBIOS PARA ESTA SESION.";
    }
    if (g->mode == FPS_WON) {
        title = "A SALVO";
        subtitle = "SECTOR COMPLETADO. ACCESO RESTABLECIDO.";
    }
    if (g->mode == FPS_LOST) {
        title = "SIN SENAL";
        subtitle = "LA INSTALACION SIGUE ACTIVA.";
    }
    re_text(r, 26, 54, title, 3, paper);
    re_text(r, 27, 88, subtitle, 1, muted);
    re_rect(r, 26, 105, 239, 1, re_rgba(54, 77, 81, 255));
    if (g->mode == FPS_TITLE) {
        menu_choice(r, 126, "ENTRAR A LA INSTALACION", 0, g->selection);
        menu_choice(r, 151, "AJUSTES", 1, g->selection);
        menu_choice(r, 176, "SALIR", 2, g->selection);
    } else if (g->mode == FPS_PAUSED) {
        menu_choice(r, 123, "CONTINUAR", 0, g->selection);
        menu_choice(r, 146, "REINICIAR NIVEL", 1, g->selection);
        menu_choice(r, 169, "AJUSTES", 2, g->selection);
        menu_choice(r, 192, "SALIR", 3, g->selection);
    } else if (g->mode == FPS_OPTIONS) {
        (void)snprintf(text, sizeof(text), "VOLUMEN        < %3d >", (int)lroundf(g->volume * 100));
        menu_choice(r, 126, text, 0, g->selection);
        (void)snprintf(text, sizeof(text), "SENSIBILIDAD   < %3d >",
                       (int)lroundf(g->sensitivity * 10000));
        menu_choice(r, 151, text, 1, g->selection);
        menu_choice(r, 176, "VOLVER", 2, g->selection);
        re_text(r, 27, 203, "IZQUIERDA / DERECHA PARA AJUSTAR", 1, muted);
    } else {
        (void)snprintf(text, sizeof(text), "GUARDIAS %d/%zu  TIEMPO %d S", g->kills, g->enemy_count,
                       (int)g->elapsed);
        re_text(r, 27, 119, text, 1, mint);
        menu_choice(r, 150, "VOLVER A INTENTAR", 0, g->selection);
        menu_choice(r, 175, "SALIR", 1, g->selection);
    }
    re_text(r, 27, 229, "ARRIBA / ABAJO    ENTER PARA ELEGIR", 1, muted);
    re_text(r, 27, 249, "C23 + SOFTWARE RENDERER + RAYLIB", 1, re_rgba(81, 114, 120, 255));
    re_rect(r, 306, 191, 156, 62, re_rgba(7, 15, 20, 221));
    re_text(r, 316, 200, "WASD    MOVERSE", 1, paper);
    re_text(r, 316, 213, "RATON   MIRAR / DISPARAR", 1, muted);
    re_text(r, 316, 226, "ESPACIO SALTAR", 1, muted);
    re_text(r, 316, 239, "E       INTERACTUAR", 1, muted);
    re_text(r, 316, 168, "F1-F4: INSPECCION", 1, mint);
}
void fps_game_draw(const FpsGame *g, const FpsArt *art, ReRenderer *r, float alpha) {
    re_renderer_clear(r, re_rgba(13, 20, 28, 255));
    r->wireframe = g->wire_view;
    ReCamera camera = re_camera_interpolate(g->previous_camera, g->camera, alpha);
    re_draw_world(r, &camera, &g->world, art->materials);
    for (size_t i = 0; i < g->enemy_count; i++) {
        const FpsEnemy *enemy = &g->enemies[i];
        int sprite = enemy->state == FPS_CORPSE ? FPS_DEAD
                                                : (enemy->state == FPS_PAIN ? FPS_HURT : FPS_GUARD);
        re_draw_billboard(r, &camera, enemy->body.position, 0.95f, 1.8f, &art->sprites[sprite], 1);
    }
    const int pickup_sprite[4] = {FPS_HEALTH, FPS_AMMO, FPS_KEY, FPS_EXIT};
    for (size_t i = 0; i < g->pickup_count; i++)
        if (g->pickups[i].active) {
            const FpsPickup *pickup = &g->pickups[i];
            re_draw_billboard(r, &camera, pickup->position, 0.8f,
                              pickup->type == FPS_PICK_EXIT ? 2.2f : 1.1f,
                              &art->sprites[pickup_sprite[pickup->type]], 1);
        }
    if (g->depth_view)
        re_draw_depth(r, 28);
    if (g->mode != FPS_TITLE && g->mode != FPS_OPTIONS)
        hud(g, art, r);
    if (g->mode != FPS_PLAYING)
        menu(g, r);
    if (g->stats_view) {
        char stats[100];
        (void)snprintf(stats, sizeof(stats), "TRI %zu / PIX %zu / SECTOR %d", r->stats.rasterized,
                       r->stats.shaded, g->player.sector);
        re_rect(r, 8, 28, 295, 16, re_rgba(7, 15, 20, 225));
        re_text(r, 14, 33, stats, 1, mint);
    }
}
