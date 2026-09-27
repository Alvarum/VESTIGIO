/* Reglas del primer FPS. No hay llamadas de raylib ni dependencia del reloj
 * real: las pruebas pueden reproducir las mismas entradas tick a tick. */
#include "game.h"
#include <stdio.h>
#include <string.h>

static void message(FpsGame *g, const char *text) {
    (void)snprintf(g->message, sizeof(g->message), "%s", text);
    g->message_timer = 2.5f;
}
static ReBody actor(ReMarker m, float radius, float height) {
    return (ReBody){.position = m.position,
                    .radius = radius,
                    .height = height,
                    .step_height = 0.3f,
                    .sector = m.sector,
                    .grounded = true};
}
static bool init_error(ReError *error, size_t line, const char *text) {
    error->line = line;
    (void)snprintf(error->message, sizeof(error->message), "%s", text);
    return false;
}
bool fps_game_init(FpsGame *g, const char *map, ReError *error) {
    if (!re_world_load(map, &g->initial_world, error))
        return false;
    int players = 0, doors = 0, keys = 0, exits = 0, enemies = 0, pickups = 0;
    for (size_t i = 0; i < g->initial_world.marker_count; i++) {
        const ReMarker *m = &g->initial_world.markers[i];
        bool player = strcmp(m->kind, "player") == 0, guard = strcmp(m->kind, "guard") == 0;
        if (player)
            players++;
        else if (guard)
            enemies++;
        else if (strcmp(m->kind, "door") == 0)
            doors++;
        else if (strcmp(m->kind, "key") == 0) {
            keys++;
            pickups++;
        } else if (strcmp(m->kind, "exit") == 0) {
            exits++;
            pickups++;
        } else if (strcmp(m->kind, "health") == 0 || strcmp(m->kind, "ammo") == 0)
            pickups++;
        else
            return init_error(error, m->source_line, "Marcador desconocido para este juego");
        if (player || guard) {
            const ReSector *s = &g->initial_world.sectors[m->sector];
            if (fabsf(m->position.z - s->floor) > RE_EPSILON || s->ceiling - m->position.z < 1.7f)
                return init_error(error, m->source_line,
                                  "Actor debe comenzar en el suelo y caber de pie");
            ReVec2 p = re_v2(m->position.x, m->position.y);
            for (size_t e = 0; e < s->count; e++)
                if (s->neighbor[e] < 0 &&
                    re_length2(re_sub2(p, re_closest_segment(p, s->vertices[e],
                                                             s->vertices[(e + 1) % s->count]))) <
                        0.3f)
                    return init_error(error, m->source_line, "Actor demasiado cerca de una pared");
        }
    }
    if (players != 1 || doors != 1 || keys != 1 || exits != 1 || enemies > FPS_MAX_ENEMIES ||
        pickups > FPS_MAX_PICKUPS)
        return init_error(
            error, 0, "FPS requiere 1 player, 1 door, 1 key, 1 exit; max 16 guard y 32 objetos");
    /* Una puerta es un sector vacío: ningún actor u objeto puede empezar en
     * el volumen que cerramos al iniciar la partida. */
    for (size_t i = 0; i < g->initial_world.marker_count; i++)
        if (!strcmp(g->initial_world.markers[i].kind, "door"))
            for (size_t j = 0; j < g->initial_world.marker_count; j++)
                if (i != j &&
                    g->initial_world.markers[j].sector == g->initial_world.markers[i].sector)
                    return init_error(error, g->initial_world.markers[j].source_line,
                                      "La puerta debe ocupar un sector sin otros marcadores");
    g->tuning = (FpsTuning){.move_speed = 3.6f,
                            .jump_speed = 6.2f,
                            .gravity = 18,
                            .shot_interval = 0.24f,
                            .enemy_speed = 1.35f,
                            .attack_interval = 0.85f,
                            .shot_damage = 30,
                            .enemy_health = 60,
                            .attack_damage = 12};
    g->sensitivity = 0.0025f;
    g->volume = 0.6f;
    fps_game_restart(g);
    g->mode = FPS_TITLE;
    return true;
}
void fps_game_restart(FpsGame *g) {
    g->world = g->initial_world;
    memset(g->enemies, 0, sizeof(g->enemies));
    memset(g->pickups, 0, sizeof(g->pickups));
    g->enemy_count = 0;
    g->pickup_count = 0;
    g->health = 100;
    g->ammo = 30;
    g->kills = 0;
    g->key = false;
    g->door_opening = false;
    g->shot_timer = 0;
    g->hurt_timer = 0;
    g->hit_timer = 0;
    g->elapsed = 0;
    g->message_timer = 0;
    g->message[0] = '\0';
    g->sound_events = 0;
    g->selection = 0;
    g->quit = false;
    g->door_sector = -1;
    g->mode = FPS_PLAYING;
    for (size_t i = 0; i < g->world.marker_count; i++) {
        ReMarker m = g->world.markers[i];
        if (!strcmp(m.kind, "player")) {
            g->player = actor(m, 0.26f, 1.7f);
            g->camera = (ReCamera){.position = re_add3(m.position, re_v3(0, 0, 1.52f)),
                                   .yaw = m.yaw,
                                   .fov = 75 * RE_PI / 180,
                                   .near_plane = 0.05f,
                                   .far_plane = 96};
            g->previous_camera = g->camera;
        } else if (!strcmp(m.kind, "guard")) {
            g->enemies[g->enemy_count++] = (FpsEnemy){.body = actor(m, 0.28f, 1.65f),
                                                      .health = g->tuning.enemy_health,
                                                      .state = FPS_IDLE};
        } else if (!strcmp(m.kind, "door")) {
            g->door_sector = m.sector;
            g->door_height = g->world.sectors[m.sector].ceiling;
            g->world.sectors[m.sector].ceiling = g->world.sectors[m.sector].floor;
        } else {
            enum FpsPickupType type = FPS_PICK_HEALTH;
            if (!strcmp(m.kind, "ammo"))
                type = FPS_PICK_AMMO;
            if (!strcmp(m.kind, "key"))
                type = FPS_PICK_KEY;
            if (!strcmp(m.kind, "exit"))
                type = FPS_PICK_EXIT;
            g->pickups[g->pickup_count++] = (FpsPickup){type, m.position, true};
        }
    }
    message(g, "ENCUENTRA LA LLAVE DEL SECTOR DE SALIDA");
}

void fps_game_frame(FpsGame *g, ReInput input) {
    if (input.quit)
        g->quit = true;
    if (!input.focused && g->mode == FPS_PLAYING) {
        g->mode = FPS_PAUSED;
        g->selection = 0;
    }
    if (input.pressed & RE_MAP)
        g->map_view = !g->map_view;
    if (input.pressed & RE_WIRE)
        g->wire_view = !g->wire_view;
    if (input.pressed & RE_DEPTH)
        g->depth_view = !g->depth_view;
    if (input.pressed & RE_STATS)
        g->stats_view = !g->stats_view;
    if (input.pressed & RE_PAUSE) {
        if (g->mode == FPS_PLAYING) {
            g->mode = FPS_PAUSED;
            g->selection = 0;
        } else if (g->mode == FPS_PAUSED && input.focused)
            g->mode = FPS_PLAYING;
        else if (g->mode == FPS_OPTIONS) {
            g->mode = g->options_parent;
            g->selection = 0;
        }
    }
    if (g->mode == FPS_PLAYING)
        return;
    int choices = g->mode == FPS_PAUSED ? 4 : 3;
    if (g->mode == FPS_WON || g->mode == FPS_LOST)
        choices = 2;
    if (input.pressed & RE_DOWN)
        g->selection = (g->selection + 1) % choices;
    if (input.pressed & RE_UP)
        g->selection = (g->selection + choices - 1) % choices;
    if (g->mode == FPS_OPTIONS) {
        int direction = (input.pressed & RE_RIGHT ? 1 : 0) - (input.pressed & RE_LEFT ? 1 : 0);
        if (g->selection == 0)
            g->volume = re_clamp(g->volume + (float)direction * 0.1f, 0, 1);
        if (g->selection == 1)
            g->sensitivity = re_clamp(g->sensitivity + (float)direction * 0.0005f, 0.0005f, 0.008f);
        if ((input.pressed & RE_ACCEPT) && g->selection == 2) {
            g->mode = g->options_parent;
            g->selection = 0;
        }
        return;
    }
    if (!(input.pressed & RE_ACCEPT))
        return;
    if (g->mode == FPS_WON || g->mode == FPS_LOST) {
        if (g->selection == 0)
            fps_game_restart(g);
        else
            g->quit = true;
    } else if (g->mode == FPS_TITLE) {
        if (g->selection == 0)
            fps_game_restart(g);
        else if (g->selection == 1) {
            g->options_parent = g->mode;
            g->mode = FPS_OPTIONS;
            g->selection = 0;
        } else
            g->quit = true;
    } else if (g->mode == FPS_PAUSED) {
        if (g->selection == 0)
            g->mode = FPS_PLAYING;
        else if (g->selection == 1)
            fps_game_restart(g);
        else if (g->selection == 2) {
            g->options_parent = g->mode;
            g->mode = FPS_OPTIONS;
            g->selection = 0;
        } else
            g->quit = true;
    }
}

static float ray_actor(ReVec3 origin, ReVec3 direction, const ReBody *body, float maximum) {
    ReVec2 offset = re_v2(origin.x - body->position.x, origin.y - body->position.y);
    ReVec2 d = re_v2(direction.x, direction.y);
    float aa = re_dot2(d, d), bb = re_dot2(offset, d),
          cc = re_dot2(offset, offset) - body->radius * body->radius;
    float best = maximum;
    float discriminant = bb * bb - aa * cc;
    if (aa > RE_EPSILON && discriminant >= 0) {
        float t = (-bb - sqrtf(discriminant)) / aa;
        float z = origin.z + t * direction.z;
        if (t >= 0 && t < best && z >= body->position.z && z <= body->position.z + body->height)
            best = t;
    }
    if (fabsf(direction.z) > RE_EPSILON) {
        float caps[2] = {body->position.z, body->position.z + body->height};
        for (int i = 0; i < 2; i++) {
            float t = (caps[i] - origin.z) / direction.z;
            ReVec2 hit = re_add2(offset, re_scale2(d, t));
            if (t >= 0 && t < best && re_dot2(hit, hit) <= body->radius * body->radius)
                best = t;
        }
    }
    return best;
}
static void shoot(FpsGame *g) {
    if (g->ammo <= 0) {
        message(g, "SIN MUNICION - BUSCA SUMINISTROS");
        return;
    }
    g->ammo--;
    g->shot_timer = g->tuning.shot_interval;
    g->sound_events |= FPS_SOUND_SHOT;
    ReVec3 direction = re_camera_forward(&g->camera);
    float nearest = re_world_raycast(&g->world, g->camera.position, direction, 80);
    int target = -1;
    for (size_t i = 0; i < g->enemy_count; i++)
        if (g->enemies[i].health > 0) {
            float hit = ray_actor(g->camera.position, direction, &g->enemies[i].body, nearest);
            if (hit < nearest) {
                nearest = hit;
                target = (int)i;
            }
        }
    if (target >= 0) {
        FpsEnemy *enemy = &g->enemies[target];
        enemy->health -= g->tuning.shot_damage;
        g->hit_timer = 0.12f;
        enemy->timer = 0.18f;
        enemy->state = FPS_PAIN;
        if (enemy->health <= 0) {
            enemy->state = FPS_CORPSE;
            g->kills++;
        }
    }
}
/* La política de qué actores son sólidos pertenece al juego. Reutilizamos el
 * barrido del motor contra una cápsula degenerada (un punto) con radios sumados.
 * Los cadáveres dejan de bloquear. Si los intervalos Z no se solapan, se puede
 * pasar por encima; no se confunde la cámara con el volumen del jugador. */
static ReVec2 limit_actors(const FpsGame *g, const ReBody *body, ReVec2 delta) {
    ReVec2 p = re_v2(body->position.x, body->position.y);
    float first = 1;
    for (size_t i = 0; i <= g->enemy_count; i++) {
        const ReBody *other = i == g->enemy_count ? &g->player : &g->enemies[i].body;
        if (other == body || (i < g->enemy_count && g->enemies[i].health <= 0))
            continue;
        if (body->position.z >= other->position.z + other->height ||
            body->position.z + body->height <= other->position.z)
            continue;
        ReVec2 center = re_v2(other->position.x, other->position.y), normal;
        float time;
        if (re_sweep_circle(p, delta, body->radius + other->radius, center, center, &time, &normal))
            first = fminf(first, fmaxf(0, time - 0.0001f));
    }
    return re_scale2(delta, first);
}

static void update_enemies(FpsGame *g, float dt) {
    for (size_t i = 0; i < g->enemy_count; i++) {
        FpsEnemy *enemy = &g->enemies[i];
        if (enemy->health <= 0)
            continue;
        enemy->timer = fmaxf(0, enemy->timer - dt);
        if (enemy->state == FPS_PAIN && enemy->timer > 0)
            continue;
        ReVec3 eyes = re_add3(enemy->body.position, re_v3(0, 0, 1.35f));
        ReVec3 toward = re_sub3(g->camera.position, eyes);
        float distance = sqrtf(re_dot3(toward, toward));
        bool visible = distance < 22 && distance > RE_EPSILON &&
                       re_world_raycast(&g->world, eyes, re_scale3(toward, 1 / distance),
                                        distance) >= distance - 0.01f;
        if (enemy->state == FPS_IDLE && !visible)
            continue;
        if (visible && distance < 1.25f) {
            enemy->state = FPS_ATTACK;
            if (enemy->timer <= 0) {
                g->health -= g->tuning.attack_damage;
                g->hurt_timer = 0.22f;
                g->sound_events |= FPS_SOUND_HURT;
                enemy->timer = g->tuning.attack_interval;
            }
            continue;
        }
        enemy->state = FPS_CHASE;
        ReVec2 target = re_v2(g->player.position.x, g->player.position.y);
        const ReSector *s = &g->world.sectors[enemy->body.sector];
        if (enemy->body.sector != g->player.sector) {
            int next = re_world_next_sector(&g->world, enemy->body.sector, g->player.sector,
                                            enemy->body.height, enemy->body.step_height);
            if (next < 0)
                continue;
            for (size_t e = 0; e < s->count; e++)
                if (s->neighbor[e] == next) {
                    ReVec2 a = s->vertices[e], b = s->vertices[(e + 1) % s->count];
                    /* Punto medio más un pequeño avance al otro lado. Evita que
                     * la tolerancia de pertenencia retenga al actor en la frontera. */
                    ReVec2 edge = re_normalize2(re_sub2(b, a));
                    target = re_add2(re_scale2(re_add2(a, b), 0.5f),
                                     re_scale2(re_v2(edge.y, -edge.x), 0.4f));
                    break;
                }
        }
        ReVec2 delta = re_sub2(target, re_v2(enemy->body.position.x, enemy->body.position.y));
        float travel = fminf(re_length2(delta), g->tuning.enemy_speed * dt);
        ReVec2 displacement =
            limit_actors(g, &enemy->body, re_scale2(re_normalize2(delta), travel));
        re_body_move(&g->world, &enemy->body, displacement, dt, g->tuning.gravity);
    }
}
void fps_game_tick(FpsGame *g, ReInput input, float dt) {
    if (g->mode != FPS_PLAYING)
        return;
    g->previous_camera = g->camera;
    g->elapsed += dt;
    g->shot_timer = fmaxf(0, g->shot_timer - dt);
    g->hurt_timer = fmaxf(0, g->hurt_timer - dt);
    g->hit_timer = fmaxf(0, g->hit_timer - dt);
    g->message_timer = fmaxf(0, g->message_timer - dt);
    g->camera.yaw = remainderf(g->camera.yaw + input.look.x * g->sensitivity, 2 * RE_PI);
    g->camera.pitch = re_clamp(g->camera.pitch - input.look.y * g->sensitivity, -85 * RE_PI / 180,
                               85 * RE_PI / 180);
    ReVec2 forward = re_v2(sinf(g->camera.yaw), cosf(g->camera.yaw));
    ReVec2 right = re_v2(forward.y, -forward.x);
    ReVec2 movement =
        re_add2(re_scale2(forward, input.movement.y), re_scale2(right, input.movement.x));
    if (re_length2(movement) > 1)
        movement = re_normalize2(movement);
    if ((input.pressed & RE_JUMP) && g->player.grounded) {
        g->player.vertical_speed = g->tuning.jump_speed;
        g->player.grounded = false;
    }
    ReVec2 displacement =
        limit_actors(g, &g->player, re_scale2(movement, g->tuning.move_speed * dt));
    re_body_move(&g->world, &g->player, displacement, dt, g->tuning.gravity);
    g->camera.position = re_add3(g->player.position, re_v3(0, 0, 1.52f));
    if ((input.held & RE_PRIMARY || input.pressed & RE_PRIMARY) && g->shot_timer <= 0)
        shoot(g);
    if ((input.pressed & RE_INTERACT) && g->door_sector >= 0 && !g->door_opening) {
        const ReSector *door = &g->world.sectors[g->door_sector];
        ReVec2 p = re_v2(g->player.position.x, g->player.position.y);
        bool near = false;
        for (size_t e = 0; e < door->count; e++)
            if (re_length2(re_sub2(p, re_closest_segment(p, door->vertices[e],
                                                         door->vertices[(e + 1) % door->count]))) <
                1.6f)
                near = true;
        if (near) {
            if (g->key) {
                g->door_opening = true;
                g->sound_events |= FPS_SOUND_DOOR;
                message(g, "ACCESO AUTORIZADO");
            } else
                message(g, "NECESITAS LA LLAVE NARANJA");
        }
    }
    if (g->door_opening) {
        ReSector *door = &g->world.sectors[g->door_sector];
        door->ceiling = fminf(g->door_height, door->ceiling + dt * 1.5f);
    }
    for (size_t i = 0; i < g->pickup_count; i++) {
        FpsPickup *p = &g->pickups[i];
        ReVec3 difference = re_sub3(p->position, g->player.position);
        if (!p->active || fabsf(difference.z) > 0.8f ||
            re_length2(re_v2(difference.x, difference.y)) > 0.85f)
            continue;
        if (p->type == FPS_PICK_EXIT) {
            g->mode = FPS_WON;
            g->sound_events |= FPS_SOUND_WIN;
            return;
        }
        if (p->type == FPS_PICK_HEALTH) {
            if (g->health >= 100)
                continue;
            g->health = g->health + 35 > 100 ? 100 : g->health + 35;
            message(g, "BOTIQUIN +35");
        } else if (p->type == FPS_PICK_AMMO) {
            g->ammo += 18;
            message(g, "MUNICION +18");
        } else {
            g->key = true;
            message(g, "LLAVE ADQUIRIDA - REGRESA A LA COMPUERTA");
        }
        p->active = false;
        g->sound_events |= FPS_SOUND_PICKUP;
    }
    update_enemies(g, dt);
    if (g->health <= 0) {
        g->health = 0;
        g->mode = FPS_LOST;
        g->selection = 0;
    }
}
