/* Reproductor genérico de proyectos RetroForge.
 *
 * Un proyecto creado con acciones incorporadas se ejecuta con este binario:
 * no necesita recompilar C. Las extensiones nativas pueden reemplazar este
 * punto de composición y seguir usando retro_core + retro_gameplay. */
#include "retro/session.h"
#include "retro/character_art.h"
#include "retro/platform.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RE_SESSION_MAX_PROJECTILES = 64 };

/* Los proyectiles son estado transitorio de la sesiÃ³n. Se reservan junto con
 * el resto del juego para que disparar no asigne memoria dentro del tick. */
typedef struct ReSessionProjectile {
    bool active;
    ReVec3 position;
    ReVec3 velocity;
    float radius;
    float life;
    int damage;
} ReSessionProjectile;

struct ReGameSession {
    ReRenderer renderer;
    ReClock clock;
    ReInput pending;
    bool preview;
    ReProject project;
    ReGameplay gameplay;
    ReInteractionRuntime interaction;
    ReBody player;
    ReCamera previous_camera, camera;
    ReTexture materials[RE_MAX_MATERIALS];
    ReTexture sprites[RE_MAX_CHARACTER_DEFS];
    ReTexture pickup_sprites[2]; /* 0: botiquin; 1: llave/objeto de mision. */
    ReTexture projectile_sprite;
    ReTexture title_art;
    ReSessionProjectile projectiles[RE_SESSION_MAX_PROJECTILES];
    char actor_ids[RE_MAX_ENTITIES][32];
    /* 0..2: manuales; 3: autosave; 4: guardado rápido. */
    char save_paths[5][1024];
    ReWorld initial_world;
    ReGameplay initial_gameplay;
    ReInteractionState initial_interaction;
    ReBody initial_player;
    ReCamera initial_camera;
    float elapsed, weapon_cooldown, weapon_flash, damage_flash;
    unsigned int summon_serial;
    float frame_ms;
    int menu_selection, pause_selection, save_slot, dialogue_selection;
    bool title, paused, quit, show_stats;
};
typedef struct ReGameSession PlayerApp;

static bool colors(PlayerApp *app) {
    static const RePixel palette[] = {{83, 99, 108, 255}, {48, 59, 65, 255}, {33, 41, 48, 255},
                                      {173, 91, 50, 255}, {74, 85, 94, 255}, {74, 164, 151, 180}};
    for (int i = 0; i < RE_MAX_MATERIALS; i++) {
        if (!re_texture_init(&app->materials[i], 8, 8))
            return false;
        RePixel base = palette[(size_t)i % 6u];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                /* Patron pequeno y repetible: conserva el pixel art, pero da
                 * escala visual a las superficies y evita grandes manchas de
                 * color plano. Los materiales reales podran reemplazarlo. */
                int mortar = (i % 3 == 0) && (y == 0 || (x + (y / 4) * 4) % 8 == 0);
                int grain = ((x * 13 + y * 7 + i * 5) & 7) == 0;
                int shade = mortar ? -22 : (grain ? 10 : 0);
                int red = (int)base.r + shade, green = (int)base.g + shade;
                int blue = (int)base.b + shade;
                app->materials[i].pixels[(size_t)y * 8u + (size_t)x] = re_rgba(
                    (uint8_t)re_clamp((float)red, 0, 255), (uint8_t)re_clamp((float)green, 0, 255),
                    (uint8_t)re_clamp((float)blue, 0, 255), 255);
            }
    }
    return true;
}

static bool load_project_materials(PlayerApp *app) {
    for (size_t i = 0; i < RE_MAX_MATERIALS; i++) {
        if (!app->project.material_files[i][0])
            continue;
        char path[RE_PROJECT_PATH * 2];
        ReTexture loaded = {0};
        if (!re_project_path(&app->project, app->project.material_files[i], path, sizeof(path)) ||
            !re_platform_image_load(path, &loaded))
            return false;
        re_texture_destroy(&app->materials[i]);
        app->materials[i] = loaded;
    }
    return true;
}

static void create_pickup_sprites(PlayerApp *app) {
    for (size_t sprite = 0; sprite < 2; sprite++) {
        (void)re_texture_init(&app->pickup_sprites[sprite], 16, 16);
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                bool visible = false;
                RePixel color = re_rgba(0, 0, 0, 0);
                if (sprite == 0) {
                    bool case_body = x >= 2 && x <= 13 && y >= 5 && y <= 13;
                    bool handle = x >= 5 && x <= 10 && y >= 2 && y <= 5;
                    visible = case_body || handle;
                    color = (x >= 7 && x <= 8) || (y >= 8 && y <= 9) ? re_rgba(230, 226, 204, 255)
                                                                     : re_rgba(147, 46, 42, 255);
                } else {
                    int dx = x - 5, dy = y - 5;
                    bool ring = dx * dx + dy * dy <= 16 && dx * dx + dy * dy >= 5;
                    bool shaft = y >= 4 && y <= 6 && x >= 8 && x <= 14;
                    bool teeth = (x == 12 || x == 14) && y >= 6 && y <= 9;
                    visible = ring || shaft || teeth;
                    color = re_rgba(232, 178, 65, 255);
                }
                if (visible)
                    app->pickup_sprites[sprite].pixels[(size_t)y * 16u + (size_t)x] = color;
            }
    }
}

static bool create_projectile_sprite(PlayerApp *app) {
    if (!re_texture_init(&app->projectile_sprite, 12, 12))
        return false;
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++) {
            int dx = x - 5, dy = y - 5;
            int radius = dx * dx + dy * dy;
            if (radius <= 30) {
                RePixel color = radius <= 5    ? re_rgba(255, 244, 184, 255)
                                : radius <= 14 ? re_rgba(255, 133, 48, 255)
                                               : re_rgba(176, 32, 28, 210);
                app->projectile_sprite.pixels[(size_t)y * 12u + (size_t)x] = color;
            }
        }
    return true;
}

static bool app_init(PlayerApp *app, const ReProject *project) {
    app->project = *project;
    if (!colors(app) || !load_project_materials(app))
        return false;
    create_pickup_sprites(app);
    if (!create_projectile_sprite(app))
        return false;
    for (size_t i = 0; i < app->project.character_count; i++) {
        char path[RE_PROJECT_PATH * 2];
        if (strcmp(app->project.characters[i].sprite, "none") == 0 ||
            !re_project_path(&app->project, app->project.characters[i].sprite, path,
                             sizeof(path)) ||
            !re_platform_image_load(path, &app->sprites[i]))
            if (!re_character_placeholder(&app->sprites[i], &app->project.characters[i], i))
                return false;
    }
    if (app->project.title_art[0]) {
        char path[RE_PROJECT_PATH * 2];
        if (re_project_path(&app->project, app->project.title_art, path, sizeof(path)))
            (void)re_platform_image_load(path, &app->title_art);
    }
    re_gameplay_init(&app->gameplay, &app->project.world, app->project.characters,
                     app->project.character_count, 0x51A7u);
    const ReMarker *start = nullptr;
    for (size_t i = 0; i < app->project.world.marker_count; i++) {
        const ReMarker *marker = &app->project.world.markers[i];
        if (strcmp(marker->kind, "player") == 0)
            start = marker;
        if (strcmp(marker->kind, "actor") == 0)
            for (size_t d = 0; d < app->project.character_count; d++)
                if (strcmp(marker->definition, app->project.characters[d].id) == 0) {
                    ReEntityId id = re_gameplay_spawn(&app->gameplay, d, marker->position,
                                                      marker->yaw, marker->sector);
                    if (id.index != UINT16_MAX)
                        (void)snprintf(app->actor_ids[id.index], sizeof(app->actor_ids[id.index]),
                                       "%s", marker->id);
                }
    }
    if (!start)
        return false;
    app->player = (ReBody){.position = start->position,
                           .radius = .28f,
                           .height = 1.72f,
                           .step_height = .3f,
                           .sector = start->sector,
                           .grounded = true};
    app->camera = (ReCamera){.position = re_add3(start->position, re_v3(0, 0, 1.55f)),
                             .yaw = start->yaw,
                             .fov = 82 * RE_PI / 180.0f,
                             .near_plane = .05f,
                             .far_plane = 80};
    app->previous_camera = app->camera;
    re_interaction_init(&app->interaction, &app->project.interactions, &app->project.world,
                        &app->gameplay, 100, 3);
    static const char *const names[] = {"slot-1.rfs", "slot-2.rfs", "slot-3.rfs", "autosave.rfs",
                                        "quicksave.rfs"};
    for (size_t i = 0; !app->preview && i < 5; i++)
        (void)re_platform_user_path(app->project.id, names[i], app->save_paths[i],
                                    sizeof(app->save_paths[i]));
    /* Estas copias son la plantilla de una partida nueva. ReGameplay contiene
     * punteros prestados; reset_game los repara después de copiar por valor. */
    app->initial_world = app->project.world;
    app->initial_gameplay = app->gameplay;
    app->initial_interaction = app->interaction.state;
    app->initial_player = app->player;
    app->initial_camera = app->camera;
    app->title = true;
    return true;
}

static void reset_game(PlayerApp *app) {
    app->project.world = app->initial_world;
    app->gameplay = app->initial_gameplay;
    app->gameplay.world = &app->project.world;
    app->gameplay.definitions = app->project.characters;
    app->interaction.world = &app->project.world;
    app->interaction.gameplay = &app->gameplay;
    app->interaction.state = app->initial_interaction;
    app->player = app->initial_player;
    app->camera = app->initial_camera;
    app->previous_camera = app->camera;
    app->elapsed = 0;
    app->weapon_cooldown = 0;
    app->weapon_flash = 0;
    app->damage_flash = 0;
    app->summon_serial = 0;
    memset(app->projectiles, 0, sizeof(app->projectiles));
    app->dialogue_selection = 0;
    app->paused = false;
}

static void interact(PlayerApp *app) {
    ReVec3 forward = re_camera_forward(&app->camera);
    if (re_interaction_interact(&app->interaction, app->camera.position, forward, 2.2f))
        return;
    ReTraceHit hit =
        re_world_trace(&app->project.world, app->camera.position, forward, 2.2f, RE_BLOCK_MOVEMENT);
    if (hit.barrier >= 0 && app->project.world.barriers[hit.barrier].kind == RE_BARRIER_DOOR) {
        ReBarrier *barrier = &app->project.world.barriers[hit.barrier];
        /* Una puerta nueva se abre por defecto; una puerta conectada conserva
         * sus condiciones. La presencia de reglas ajenas no debe bloquearla. */
        bool controlled = false, direct = false;
        for (size_t i = 0; i < app->project.interactions.rule_count; i++) {
            const ReRuleDefinition *rule = &app->project.interactions.rules[i];
            if (rule->event == RE_LOGIC_INTERACT && strcmp(rule->source, barrier->id) == 0)
                direct = true;
            for (size_t j = 0; j < rule->action_count; j++)
                if ((rule->actions[j].kind == RE_RULE_OPEN_BARRIER ||
                     rule->actions[j].kind == RE_RULE_CLOSE_BARRIER) &&
                    strcmp(rule->actions[j].target, barrier->id) == 0)
                    controlled = true;
        }
        if (direct) {
            ReLogicEvent event = {.kind = RE_LOGIC_INTERACT, .position = app->player.position};
            (void)snprintf(event.source, sizeof(event.source), "%s", barrier->id);
            (void)re_interaction_emit(&app->interaction, event);
        } else if (!controlled)
            barrier->open_fraction = 1;
        else {
            (void)snprintf(app->interaction.state.message, sizeof(app->interaction.state.message),
                           "Esta puerta se abre mediante un objeto o interruptor");
            app->interaction.state.message_time = 3;
        }
    }
}

static void shoot(PlayerApp *app) {
    ReVec3 direction = re_camera_forward(&app->camera);
    ReTraceHit obstacle = re_world_trace(&app->project.world, app->camera.position, direction, 30,
                                         RE_BLOCK_PROJECTILE);
    int selected = -1;
    float nearest = obstacle.distance;
    for (size_t i = 0; i < RE_MAX_ENTITIES; i++) {
        ReCharacter *actor = &app->gameplay.characters[i];
        if (!actor->active || actor->state == RE_CHARACTER_DEAD)
            continue;
        const ReCharacterDef *definition = &app->project.characters[actor->definition];
        ReVec3 center = re_add3(actor->body.position, re_v3(0, 0, definition->height * .5f));
        ReVec3 delta = re_sub3(center, app->camera.position);
        float along = re_dot3(delta, direction);
        if (along <= 0 || along >= nearest)
            continue;
        ReVec3 lateral = re_sub3(delta, re_scale3(direction, along));
        float hit_radius = fmaxf(definition->radius, definition->height * .28f);
        if (re_dot3(lateral, lateral) <= hit_radius * hit_radius) {
            selected = (int)i;
            nearest = along;
        }
    }
    if (selected >= 0) {
        ReCharacter *actor = &app->gameplay.characters[selected];
        bool was_alive = actor->state != RE_CHARACTER_DEAD;
        ReEntityId id = {.index = (uint16_t)selected, .generation = actor->generation};
        (void)re_gameplay_damage(&app->gameplay, id, 40, .18f);
        ReLogicEvent event = {.kind = actor->state == RE_CHARACTER_DEAD && was_alive
                                          ? RE_LOGIC_ENTITY_DIED
                                          : RE_LOGIC_ENTITY_DAMAGED,
                              .position = actor->body.position,
                              .value = {.kind = RE_VALUE_INT, .as.integer = 40}};
        (void)snprintf(event.source, sizeof(event.source), "%s", app->actor_ids[selected]);
        (void)re_interaction_emit(&app->interaction, event);
        return;
    }
    if (obstacle.barrier >= 0)
        (void)re_barrier_damage(&app->project.world, (size_t)obstacle.barrier, 25);
}

static void damage_player(PlayerApp *app, int damage) {
    if (damage <= 0 || app->interaction.state.player_health <= 0)
        return;
    app->interaction.state.player_health = app->interaction.state.player_health > damage
                                               ? app->interaction.state.player_health - damage
                                               : 0;
    app->damage_flash = .22f;
    if (!app->interaction.state.player_health)
        (void)re_interaction_emit(&app->interaction,
                                  (ReLogicEvent){.kind = RE_LOGIC_PLAYER_DIED, .source = "player"});
}

static void spawn_enemy_projectile(PlayerApp *app, const ReGameplayEvent *event) {
    ReCharacter *source = re_gameplay_get(&app->gameplay, event->source);
    if (!source)
        return;
    const ReCharacterDef *definition = &app->project.characters[source->definition];
    ReSessionProjectile *projectile = nullptr;
    for (size_t i = 0; i < RE_SESSION_MAX_PROJECTILES; i++)
        if (!app->projectiles[i].active) {
            projectile = &app->projectiles[i];
            break;
        }
    if (!projectile)
        return;

    ReVec3 origin = re_add3(source->body.position, re_v3(0, 0, definition->height * .62f));
    ReVec3 target = re_add3(app->player.position, re_v3(0, 0, app->player.height * .52f));
    ReVec3 direction = re_sub3(target, origin);
    float length = sqrtf(re_dot3(direction, direction));
    if (length < RE_EPSILON)
        return;
    direction = re_scale3(direction, 1.0f / length);
    *projectile = (ReSessionProjectile){.active = true,
                                        .position = origin,
                                        .velocity = re_scale3(direction, 8.5f),
                                        .radius = .14f,
                                        .life = 4,
                                        .damage = event->value};
}

static bool projectile_hits_player(ReVec3 start, ReVec3 end, const ReBody *player, float radius,
                                   float *fraction) {
    ReVec3 delta = re_sub3(end, start);
    ReVec2 horizontal = re_v2(delta.x, delta.y);
    ReVec2 to_player = re_v2(player->position.x - start.x, player->position.y - start.y);
    float length_squared = re_dot2(horizontal, horizontal);
    float t = length_squared > RE_EPSILON
                  ? re_clamp(re_dot2(to_player, horizontal) / length_squared, 0, 1)
                  : 0;
    ReVec3 closest = re_add3(start, re_scale3(delta, t));
    float dx = closest.x - player->position.x, dy = closest.y - player->position.y;
    float combined = player->radius + radius;
    bool vertical = closest.z >= player->position.z - radius &&
                    closest.z <= player->position.z + player->height + radius;
    if (vertical && dx * dx + dy * dy <= combined * combined) {
        *fraction = t;
        return true;
    }
    return false;
}

static void tick_projectiles(PlayerApp *app, float dt) {
    for (size_t i = 0; i < RE_SESSION_MAX_PROJECTILES; i++) {
        ReSessionProjectile *projectile = &app->projectiles[i];
        if (!projectile->active)
            continue;
        ReVec3 step = re_scale3(projectile->velocity, dt);
        float distance = sqrtf(re_dot3(step, step));
        ReVec3 direction =
            distance > RE_EPSILON ? re_scale3(step, 1.0f / distance) : re_v3(0, 0, 0);
        ReTraceHit wall = re_world_trace(&app->project.world, projectile->position, direction,
                                         distance, RE_BLOCK_PROJECTILE);
        ReVec3 end = re_add3(projectile->position, step);
        float player_fraction = 0;
        bool player_hit = projectile_hits_player(projectile->position, end, &app->player,
                                                 projectile->radius, &player_fraction);
        if (player_hit && player_fraction * distance <= wall.distance + .001f) {
            damage_player(app, projectile->damage);
            projectile->active = false;
            continue;
        }
        if (wall.distance < distance - .001f) {
            projectile->active = false;
            continue;
        }
        projectile->position = end;
        projectile->life -= dt;
        if (projectile->life <= 0)
            projectile->active = false;
    }
}

static bool spawn_minion(PlayerApp *app, const ReGameplayEvent *event) {
    ReCharacter *source = re_gameplay_get(&app->gameplay, event->source);
    if (!source)
        return false;
    size_t definition_index = app->project.character_count;
    for (size_t i = 0; i < app->project.character_count; i++)
        if (strcmp(app->project.characters[i].id, "caretaker") == 0) {
            definition_index = i;
            break;
        }
    if (definition_index == app->project.character_count)
        return false;

    static const ReVec2 offsets[] = {{1.4f, 0},    {-1.4f, 0},    {0, 1.4f},     {0, -1.4f},
                                     {1.2f, 1.2f}, {-1.2f, 1.2f}, {1.2f, -1.2f}, {-1.2f, -1.2f}};
    for (size_t candidate = 0; candidate < sizeof(offsets) / sizeof(offsets[0]); candidate++) {
        ReVec3 position =
            re_add3(source->body.position, re_v3(offsets[candidate].x, offsets[candidate].y, .05f));
        int sector = re_world_sector_at(&app->project.world, position, source->body.sector);
        if (sector < 0)
            continue;
        position.z = app->project.world.sectors[sector].floor;
        ReVec2 player_delta = re_sub2(re_v2(position.x, position.y),
                                      re_v2(app->player.position.x, app->player.position.y));
        if (re_length2(player_delta) < 1.2f)
            continue;
        bool occupied = false;
        for (size_t i = 0; i < RE_MAX_ENTITIES; i++) {
            const ReCharacter *actor = &app->gameplay.characters[i];
            if (!actor->active || actor->state == RE_CHARACTER_DEAD)
                continue;
            ReVec2 delta = re_sub2(re_v2(position.x, position.y),
                                   re_v2(actor->body.position.x, actor->body.position.y));
            if (re_length2(delta) < .9f) {
                occupied = true;
                break;
            }
        }
        if (occupied)
            continue;
        ReEntityId spawned =
            re_gameplay_spawn(&app->gameplay, definition_index, position, source->yaw, sector);
        if (spawned.index == UINT16_MAX)
            return false;
        (void)snprintf(app->actor_ids[spawned.index], sizeof(app->actor_ids[spawned.index]),
                       "warden-minion-%u", ++app->summon_serial);
        return true;
    }
    return false;
}

static void save_game(PlayerApp *app, size_t index, const char *label) {
    if (app->preview)
        return; /* Una prueba nunca escribe partidas del usuario. */
    ReError error = {0};
    if (index >= 5 || !app->save_paths[index][0] ||
        !re_save_write(app->save_paths[index], app->project.id, &app->interaction, &app->player,
                       &error)) {
        /* El autoguardado no debe reemplazar un mensaje narrativo ni arrancar
         * el showcase con un error en pantalla. El fallo sigue siendo visible
         * en stderr para diagnostico; guardados pedidos por el jugador si se
         * comunican en el HUD porque requieren una accion correctiva. */
        if (index == 3) {
            (void)fprintf(stderr, "Autoguardado: %s\n", error.message);
            return;
        }
        (void)snprintf(app->interaction.state.message, sizeof(app->interaction.state.message),
                       "No se pudo guardar: %.120s", error.message);
        app->interaction.state.message_time = 4;
        return;
    }
    (void)snprintf(app->interaction.state.message, sizeof(app->interaction.state.message), "%s",
                   label);
    app->interaction.state.message_time = 3;
}

static bool load_game(PlayerApp *app, size_t index) {
    if (app->preview)
        return false;
    ReError error = {0};
    if (index >= 5 || !app->save_paths[index][0] ||
        !re_save_read(app->save_paths[index], app->project.id, &app->interaction, &app->player,
                      &error)) {
        (void)snprintf(app->interaction.state.message, sizeof(app->interaction.state.message),
                       "No se pudo cargar: %.120s", error.message);
        app->interaction.state.message_time = 4;
        return false;
    }
    app->camera.position = re_add3(app->player.position, re_v3(0, 0, 1.55f));
    app->previous_camera = app->camera;
    memset(app->projectiles, 0, sizeof(app->projectiles));
    (void)snprintf(app->interaction.state.message, sizeof(app->interaction.state.message),
                   "Partida cargada");
    app->interaction.state.message_time = 3;
    return true;
}

static size_t visible_dialogue_choices(const PlayerApp *app, const ReDialogueNode *node) {
    size_t count = 0;
    for (size_t i = 0; i < node->choice_count; i++) {
        const ReDialogueChoice *choice = &node->choices[i];
        bool allowed = true;
        if (choice->condition_variable[0] && strcmp(choice->condition_variable, "-") != 0) {
            const ReValue *value =
                re_interaction_variable(&app->interaction, choice->condition_variable);
            allowed = value && value->kind == RE_VALUE_BOOL &&
                      value->as.boolean == choice->condition_value;
        }
        if (allowed)
            count++;
    }
    return count;
}

static void tick(PlayerApp *app, ReInput input) {
    if (input.pressed & RE_STATS)
        app->show_stats = !app->show_stats;
    if (app->title) {
        if (input.pressed & RE_UP)
            app->menu_selection = (app->menu_selection + 3) % 4;
        if (input.pressed & RE_DOWN)
            app->menu_selection = (app->menu_selection + 1) % 4;
        if (app->menu_selection == 2 && (input.pressed & RE_LEFT))
            app->save_slot = (app->save_slot + 2) % 3;
        if (app->menu_selection == 2 && (input.pressed & RE_RIGHT))
            app->save_slot = (app->save_slot + 1) % 3;
        if (input.pressed & RE_ACCEPT) {
            if (app->menu_selection == 0)
                reset_game(app);
            else if (app->menu_selection == 1 && !load_game(app, 3) && !load_game(app, 4))
                return;
            else if (app->menu_selection == 2 && !load_game(app, (size_t)app->save_slot))
                return;
            else if (app->menu_selection == 3) {
                app->quit = true;
                return;
            }
            app->title = false;
        }
        return;
    }
    if (app->interaction.state.won) {
        if (input.pressed & RE_ACCEPT)
            app->title = true;
        return;
    }
    if (app->interaction.state.game_over) {
        if (input.pressed & RE_ACCEPT) {
            if (app->project.death_policy == RE_DEATH_RESTART_LEVEL) {
                reset_game(app);
            } else if (app->project.death_policy == RE_DEATH_LAST_CHECKPOINT &&
                       app->interaction.state.checkpoint_valid) {
                re_interaction_checkpoint_restore(&app->interaction, &app->player);
                app->camera.position = re_add3(app->player.position, re_v3(0, 0, 1.55f));
                app->previous_camera = app->camera;
                memset(app->projectiles, 0, sizeof(app->projectiles));
            } else if (app->project.death_policy == RE_DEATH_LIMITED_LIVES &&
                       app->interaction.state.player_lives > 0 &&
                       app->interaction.state.checkpoint_valid) {
                int lives = app->interaction.state.player_lives - 1;
                re_interaction_checkpoint_restore(&app->interaction, &app->player);
                app->interaction.state.player_lives = lives;
                app->camera.position = re_add3(app->player.position, re_v3(0, 0, 1.55f));
                app->previous_camera = app->camera;
                memset(app->projectiles, 0, sizeof(app->projectiles));
            } else {
                reset_game(app);
                app->title = true;
            }
        }
        return;
    }
    if (input.pressed & RE_PAUSE)
        app->paused = !app->paused;
    if (app->paused) {
        if (input.pressed & RE_UP)
            app->pause_selection = (app->pause_selection + 3) % 4;
        if (input.pressed & RE_DOWN)
            app->pause_selection = (app->pause_selection + 1) % 4;
        if (input.pressed & RE_LEFT)
            app->save_slot = (app->save_slot + 2) % 3;
        if (input.pressed & RE_RIGHT)
            app->save_slot = (app->save_slot + 1) % 3;
        if (input.pressed & RE_ACCEPT) {
            if (app->pause_selection == 0)
                app->paused = false;
            else if (app->pause_selection == 1)
                save_game(app, (size_t)app->save_slot, "Ranura manual guardada");
            else if (app->pause_selection == 2)
                (void)load_game(app, (size_t)app->save_slot);
            else {
                app->title = true;
                app->paused = false;
            }
        }
        return;
    }
    const ReDialogueNode *dialogue = re_interaction_dialogue(&app->interaction);
    if (dialogue) {
        size_t choices = visible_dialogue_choices(app, dialogue);
        if (choices) {
            if (input.pressed & RE_UP)
                app->dialogue_selection =
                    (app->dialogue_selection + (int)choices - 1) % (int)choices;
            if (input.pressed & RE_DOWN)
                app->dialogue_selection = (app->dialogue_selection + 1) % (int)choices;
        } else
            app->dialogue_selection = 0;
        if (input.pressed & RE_ACCEPT) {
            if (re_interaction_choose(&app->interaction, (size_t)app->dialogue_selection))
                app->dialogue_selection = 0;
        }
        if (dialogue->pauses_world)
            return;
    }
    if (input.pressed & RE_QUICK_SAVE)
        save_game(app, 4, "Guardado rápido creado");
    if (input.pressed & RE_QUICK_LOAD)
        (void)load_game(app, 4);
    if (app->interaction.state.dialogue_pauses)
        return;
    app->weapon_cooldown = fmaxf(0, app->weapon_cooldown - RE_FIXED_DT);
    app->weapon_flash = fmaxf(0, app->weapon_flash - RE_FIXED_DT);
    app->damage_flash = fmaxf(0, app->damage_flash - RE_FIXED_DT);
    app->previous_camera = app->camera;
    app->camera.yaw += input.look.x * .0025f;
    app->camera.pitch = re_clamp(app->camera.pitch - input.look.y * .0025f, -85 * RE_PI / 180.0f,
                                 85 * RE_PI / 180.0f);
    ReVec2 forward = re_v2(sinf(app->camera.yaw), cosf(app->camera.yaw));
    ReVec2 right = re_v2(cosf(app->camera.yaw), -sinf(app->camera.yaw));
    ReVec2 movement =
        re_add2(re_scale2(right, input.movement.x), re_scale2(forward, input.movement.y));
    if (re_length2(movement) > 1)
        movement = re_normalize2(movement);
    if ((input.pressed & RE_JUMP) && app->player.grounded)
        app->player.vertical_speed = 6.2f;
    re_body_move(&app->project.world, &app->player, re_scale2(movement, 3.5f * RE_FIXED_DT),
                 RE_FIXED_DT, 18);
    app->camera.position = re_add3(app->player.position, re_v3(0, 0, 1.55f));
    if (input.pressed & RE_INTERACT)
        interact(app);
    if (((input.pressed | input.held) & RE_PRIMARY) && app->weapon_cooldown <= 0) {
        shoot(app);
        app->weapon_cooldown = .24f;
        app->weapon_flash = .07f;
    }
    re_gameplay_tick(&app->gameplay,
                     (ReGameplayInput){.player = &app->player,
                                       .player_eye = app->camera.position,
                                       .player_noise = re_length2(movement) > .2f ? 8 : 0},
                     RE_FIXED_DT);
    ReGameplayEvent event;
    while (re_gameplay_event(&app->gameplay, &event)) {
        if (event.kind == RE_EVENT_CAPTURED) {
            ReLogicEvent logic = {.kind = RE_LOGIC_CAPTURED, .position = event.position};
            if (event.source.index < RE_MAX_ENTITIES)
                (void)snprintf(logic.source, sizeof(logic.source), "%s",
                               app->actor_ids[event.source.index]);
            (void)re_interaction_emit(&app->interaction, logic);
        }
        if (event.kind == RE_EVENT_PLAYER_DAMAGE)
            damage_player(app, event.value);
        if (event.kind == RE_EVENT_PROJECTILE)
            spawn_enemy_projectile(app, &event);
        if (event.kind == RE_EVENT_SUMMON && !spawn_minion(app, &event)) {
            /* Gameplay reserva el cupo al emitir. Si no existe un punto vÃ¡lido
             * devolvemos el cupo para que una arena llena no anule la fase. */
            ReCharacter *source = re_gameplay_get(&app->gameplay, event.source);
            if (source && source->summoned)
                source->summoned--;
        }
    }
    tick_projectiles(app, RE_FIXED_DT);
    bool had_checkpoint = app->interaction.state.checkpoint_valid;
    ReVec3 previous_checkpoint = app->interaction.state.checkpoint_player.position;
    int previous_checkpoint_health = app->interaction.state.checkpoint_health;
    re_interaction_tick(&app->interaction, &app->player, RE_FIXED_DT);
    ReVec3 current_checkpoint = app->interaction.state.checkpoint_player.position;
    bool checkpoint_changed =
        !had_checkpoint || previous_checkpoint.x != current_checkpoint.x ||
        previous_checkpoint.y != current_checkpoint.y ||
        previous_checkpoint.z != current_checkpoint.z ||
        previous_checkpoint_health != app->interaction.state.checkpoint_health;
    if (app->interaction.state.checkpoint_valid && checkpoint_changed)
        save_game(app, 3, "Checkpoint y autoguardado creados");
    app->elapsed += RE_FIXED_DT;
}

static void draw_actor(PlayerApp *app, ReRenderer *renderer, const ReCamera *camera,
                       const ReCharacter *actor) {
    const ReCharacterDef *definition = &app->project.characters[actor->definition];
    bool attacking = actor->state == RE_CHARACTER_WINDUP || actor->state == RE_CHARACTER_RECOVERY;
    const char *clip = attacking ? (definition->capture_game_over ? "capture" : "attack")
                       : actor->state == RE_CHARACTER_CHASE ? "chase"
                                                            : "idle";
    int cell = re_character_animation_cell(definition, actor, clip, camera->yaw);
    const ReTexture *texture = &app->sprites[actor->definition];
    /* Un atlas nunca se muestra entero como recuperacion de error. Una celda
     * invalida cae en la primera pose, lo que mantiene visible al personaje
     * sin revelar la hoja completa ni ocultar el fallo de configuracion. */
    float uv[4];
    if (!re_texture_cell_uv(texture, definition->cell_width, definition->cell_height, cell, uv))
        cell = 0;
    if (!re_texture_cell_uv(texture, definition->cell_width, definition->cell_height, cell, uv))
        return;
    re_draw_billboard_region(renderer, camera, actor->body.position, definition->radius * 2.6f,
                             definition->height, texture, 1, uv[0], uv[1], uv[2], uv[3]);
}

static void draw_npc(PlayerApp *app, ReRenderer *renderer, const ReCamera *camera,
                     const ReMarker *marker) {
    for (size_t i = 0; i < app->project.character_count; i++) {
        const ReCharacterDef *definition = &app->project.characters[i];
        if (strcmp(marker->definition, definition->id) != 0)
            continue;
        const ReTexture *texture = &app->sprites[i];
        /* Los NPC sin IA también reproducen su clip idle. Un personaje local
         * temporal permite reutilizar exactamente la selección de fotogramas
         * y direcciones del runtime, sin mantener un segundo algoritmo. */
        ReCharacter preview = {.yaw = marker->yaw, .animation_time = app->elapsed};
        int cell = re_character_animation_cell(definition, &preview, "idle", camera->yaw);
        float uv[4];
        if (!re_texture_cell_uv(texture, definition->cell_width, definition->cell_height, cell, uv))
            return;
        re_draw_billboard_region(renderer, camera, marker->position, definition->radius * 2.6f,
                                 definition->height, texture, 1, uv[0], uv[1], uv[2], uv[3]);
        return;
    }
}

static void wrap_text(const char *source, char *destination, size_t capacity, size_t columns);

/* Copia una imagen con recorte "cover" y muestreo nearest. La portada puede
 * tener cualquier tamaño: no se fuerza al autor a preprocesarla a 480x270.
 * Elegimos enteros y el centro del texel para conservar bordes de pixel art. */
static void draw_cover(ReRenderer *renderer, const ReTexture *texture) {
    if (!texture->pixels || texture->width <= 0 || texture->height <= 0)
        return;
    int64_t width_scale = (int64_t)texture->width * renderer->height;
    int64_t height_scale = (int64_t)texture->height * renderer->width;
    bool crop_width = width_scale > height_scale;
    int source_width = crop_width
                           ? (int)((int64_t)texture->height * renderer->width / renderer->height)
                           : texture->width;
    int source_height = crop_width
                            ? texture->height
                            : (int)((int64_t)texture->width * renderer->height / renderer->width);
    int source_x = (texture->width - source_width) / 2;
    int source_y = (texture->height - source_height) / 2;
    for (int y = 0; y < renderer->height; y++) {
        int sample_y =
            source_y + (int)(((int64_t)y * source_height + source_height / 2) / renderer->height);
        for (int x = 0; x < renderer->width; x++) {
            int sample_x =
                source_x + (int)(((int64_t)x * source_width + source_width / 2) / renderer->width);
            renderer->pixels[(size_t)y * (size_t)renderer->width + (size_t)x] =
                texture->pixels[(size_t)sample_y * (size_t)texture->width + (size_t)sample_x];
        }
    }
}

static void draw(PlayerApp *app, ReRenderer *renderer, float alpha) {
    re_renderer_clear(renderer, re_rgba(12, 18, 25, 255));
    ReCamera camera = re_camera_interpolate(app->previous_camera, app->camera, alpha);
    re_draw_world(renderer, &camera, &app->project.world, app->materials);
    for (size_t i = 0; i < RE_MAX_ENTITIES; i++)
        if (app->gameplay.characters[i].active &&
            app->gameplay.characters[i].state != RE_CHARACTER_DEAD)
            draw_actor(app, renderer, &camera, &app->gameplay.characters[i]);
    for (size_t i = 0; i < RE_SESSION_MAX_PROJECTILES; i++)
        if (app->projectiles[i].active)
            re_draw_billboard(renderer, &camera, app->projectiles[i].position, .28f, .28f,
                              &app->projectile_sprite, 1);
    for (size_t i = 0; i < app->interaction.state.pickup_count; i++) {
        const ReRuntimePickup *pickup = &app->interaction.state.pickups[i];
        if (pickup->active) {
            size_t sprite =
                strstr(pickup->item, "bandage") || strstr(pickup->item, "health") ? 0u : 1u;
            re_draw_billboard(renderer, &camera, pickup->position, .45f, .45f,
                              &app->pickup_sprites[sprite], 1);
        }
    }
    if (app->project.character_count)
        for (size_t i = 0; i < app->project.world.marker_count; i++)
            if (strcmp(app->project.world.markers[i].kind, "npc") == 0)
                draw_npc(app, renderer, &camera, &app->project.world.markers[i]);
    re_apply_lights(renderer, &camera, app->project.interactions.lights,
                    app->interaction.state.light_enabled, app->project.interactions.light_count,
                    app->elapsed);
    /* RetÃ­cula y arma son feedback del control, no decoraciÃ³n: permiten leer
     * con claridad la direcciÃ³n del hitscan y su cadencia. */
    re_rect(renderer, 237, 134, 7, 1, re_rgba(236, 228, 196, 230));
    re_rect(renderer, 240, 131, 1, 7, re_rgba(236, 228, 196, 230));
    re_rect(renderer, 218, 236, 44, 18, re_rgba(32, 38, 43, 255));
    re_rect(renderer, 226, 228, 28, 15, re_rgba(68, 75, 78, 255));
    re_rect(renderer, 232, 224, 16, 8, re_rgba(132, 106, 70, 255));
    if (app->weapon_flash > 0) {
        re_rect(renderer, 235, 214, 10, 12, re_rgba(255, 210, 84, 255));
        re_rect(renderer, 231, 218, 18, 4, re_rgba(255, 134, 45, 255));
    }
    if (app->damage_flash > 0) {
        RePixel warning = re_rgba(184, 38, 34, 255);
        re_rect(renderer, 0, 0, 480, 4, warning);
        re_rect(renderer, 0, 243, 480, 4, warning);
        re_rect(renderer, 0, 0, 4, 247, warning);
        re_rect(renderer, 476, 0, 4, 247, warning);
    }
    /* Una barrera cerrada debe comunicar su intención antes de que el jugador
     * la confunda con un fallo de colisión. El rayo usa exactamente la misma
     * máscara y geometría que los disparos y el movimiento. */
    ReVec3 look = re_v3(sinf(camera.yaw) * cosf(camera.pitch),
                        cosf(camera.yaw) * cosf(camera.pitch), sinf(camera.pitch));
    ReTraceHit facing =
        re_world_trace(&app->project.world, camera.position, look, 1.8f, RE_BLOCK_MOVEMENT);
    if (facing.barrier >= 0 && app->project.world.barriers[facing.barrier].open_fraction < .95f &&
        app->interaction.state.message_time <= 0) {
        const ReBarrier *barrier = &app->project.world.barriers[facing.barrier];
        re_rect(renderer, 132, 218, 216, 20, re_rgba(8, 12, 17, 230));
        re_text(renderer, barrier->kind == RE_BARRIER_DOOR ? 181 : 147, 225,
                barrier->kind == RE_BARRIER_DOOR ? "E  INTERACTUAR" : "DISPARA PARA ROMPER VIDRIO",
                1, re_rgba(240, 178, 86, 255));
    }
    re_rect(renderer, 0, 247, 480, 23, re_rgba(9, 14, 19, 235));
    char hud[128];
    (void)snprintf(hud, sizeof(hud), "VIDA %03d  VIDAS %d  LLAVE %u  F5/F9",
                   app->interaction.state.player_health, app->interaction.state.player_lives,
                   re_interaction_item_count(&app->interaction, "brass_key"));
    re_text(renderer, 12, 255, hud, 1, re_rgba(220, 226, 218, 255));
    for (size_t i = 0; i < app->project.interactions.objective_count; i++)
        if (app->interaction.state.objectives[i] == RE_OBJECTIVE_ACTIVE) {
            re_rect(renderer, 8, 8, 306, 18, re_rgba(5, 9, 13, 235));
            re_text(renderer, 14, 14, app->project.interactions.objectives[i].title, 1,
                    re_rgba(240, 178, 86, 255));
            break;
        }
    if (app->show_stats) {
        char stats[96];
        float fps = app->frame_ms > .01f ? 1000 / app->frame_ms : 0;
        (void)snprintf(stats, sizeof(stats), "%.1f FPS  %.2f MS  T%zu P%zu", (double)fps,
                       (double)app->frame_ms, renderer->stats.rasterized, renderer->stats.shaded);
        re_rect(renderer, 4, 4, 190, 17, re_rgba(5, 8, 12, 220));
        re_text(renderer, 9, 9, stats, 1, re_rgba(154, 218, 187, 255));
    }
    if (app->interaction.state.message_time > 0) {
        re_rect(renderer, 8, 218, 464, 22, re_rgba(8, 12, 17, 220));
        re_text(renderer, 14, 226, app->interaction.state.message, 1, re_rgba(240, 178, 86, 255));
    }
    const ReDialogueNode *dialogue = re_interaction_dialogue(&app->interaction);
    if (dialogue) {
        re_rect(renderer, 28, 145, 424, 96, re_rgba(7, 11, 16, 244));
        re_text(renderer, 42, 156, dialogue->speaker, 2, re_rgba(235, 166, 75, 255));
        char wrapped[256];
        wrap_text(dialogue->text, wrapped, sizeof(wrapped), 62);
        re_text(renderer, 42, 180, wrapped, 1, re_rgba(225, 230, 224, 255));
        int row = 0;
        for (size_t i = 0; i < dialogue->choice_count; i++) {
            const ReDialogueChoice *choice = &dialogue->choices[i];
            bool allowed = true;
            if (choice->condition_variable[0] && strcmp(choice->condition_variable, "-") != 0) {
                const ReValue *value =
                    re_interaction_variable(&app->interaction, choice->condition_variable);
                allowed = value && value->kind == RE_VALUE_BOOL &&
                          value->as.boolean == choice->condition_value;
            }
            if (!allowed)
                continue;
            RePixel color = row == app->dialogue_selection ? re_rgba(250, 188, 92, 255)
                                                           : re_rgba(180, 190, 190, 255);
            re_text(renderer, 48, 207 + row * 11, choice->text, 1, color);
            row++;
        }
        if (!dialogue->choice_count)
            re_text(renderer, 48, 217, "ENTER PARA CONTINUAR", 1, re_rgba(190, 200, 200, 255));
    }
    if (app->interaction.state.game_over) {
        re_rect(renderer, 70, 82, 340, 104, re_rgba(8, 12, 17, 238));
        re_text(renderer, 126, 105, "HAS SIDO CAPTURADO", 2, re_rgba(232, 82, 67, 255));
        re_text(renderer, 116, 151, "ENTER: ULTIMO CHECKPOINT", 1, re_rgba(220, 226, 218, 255));
    }
    if (app->interaction.state.won) {
        re_rect(renderer, 70, 76, 340, 116, re_rgba(8, 12, 17, 242));
        re_text(renderer, 148, 103, "ESCAPASTE", 3, re_rgba(108, 218, 171, 255));
        re_text(renderer, 124, 159, "ENTER: MENU PRINCIPAL", 1, re_rgba(220, 226, 218, 255));
    }
    if (app->paused) {
        re_rect(renderer, 118, 56, 244, 158, re_rgba(8, 12, 17, 244));
        re_text(renderer, 190, 72, "PAUSA", 2, re_rgba(235, 166, 75, 255));
        char save_entry[48], load_entry[48];
        (void)snprintf(save_entry, sizeof(save_entry), "GUARDAR RANURA %d", app->save_slot + 1);
        (void)snprintf(load_entry, sizeof(load_entry), "CARGAR RANURA %d", app->save_slot + 1);
        const char *const entries[] = {"CONTINUAR", save_entry, load_entry, "MENU PRINCIPAL"};
        for (int i = 0; i < 4; i++)
            re_text(renderer, 151, 108 + i * 21, entries[i], 1,
                    i == app->pause_selection ? re_rgba(250, 188, 92, 255)
                                              : re_rgba(205, 215, 215, 255));
        re_text(renderer, 151, 195, "FLECHAS + ENTER", 1, re_rgba(125, 145, 150, 255));
    }
    if (app->title) {
        draw_cover(renderer, &app->title_art);
        re_rect(renderer, 0, 0, 268, 270, re_rgba(4, 8, 12, 218));
        re_rect(renderer, 25, 26, 4, 188, re_rgba(220, 137, 48, 255));
        re_text(renderer, 48, 46, "CASA DE LA", 2, re_rgba(234, 164, 72, 255));
        re_text(renderer, 48, 68, "NIEBLA", 3, re_rgba(240, 184, 92, 255));
        re_text(renderer, 49, 99, "RETROFORGE SHOWCASE", 1, re_rgba(164, 183, 187, 255));
        char load_entry[48];
        (void)snprintf(load_entry, sizeof(load_entry), "CARGAR RANURA %d", app->save_slot + 1);
        const char *const entries[] = {"NUEVA PARTIDA", "CONTINUAR", load_entry, "SALIR"};
        for (int i = 0; i < 4; i++)
            re_text(renderer, 49, 132 + i * 19, entries[i], 1,
                    i == app->menu_selection ? re_rgba(250, 188, 92, 255)
                                             : re_rgba(205, 215, 215, 255));
        re_text(renderer, 49, 218, "FLECHAS + ENTER", 1, re_rgba(125, 145, 150, 255));
    }
}

static size_t texture_bytes(const ReTexture *texture) {
    if (!texture->pixels || texture->width <= 0 || texture->height <= 0)
        return 0;
    return (size_t)texture->width * (size_t)texture->height * sizeof(*texture->pixels);
}

/* Inserta saltos sin partir palabras. La UI bitmap no depende de fuentes del
 * sistema, por lo que el ancho se expresa en caracteres monoespaciados. */
static void wrap_text(const char *source, char *destination, size_t capacity, size_t columns) {
    size_t output = 0, line = 0;
    const char *cursor = source;
    if (!capacity)
        return;
    while (*cursor && output + 1u < capacity) {
        while (*cursor == ' ')
            cursor++;
        const char *word = cursor;
        while (*cursor && *cursor != ' ')
            cursor++;
        size_t bytes = (size_t)(cursor - word);
        size_t characters = 0;
        for (const unsigned char *p = (const unsigned char *)word;
             p < (const unsigned char *)cursor; p++)
            if ((*p & 0xC0u) != 0x80u)
                characters++;
        if (line && line + 1u + characters > columns && output + 1u < capacity) {
            destination[output++] = '\n';
            line = 0;
        } else if (line && output + 1u < capacity) {
            destination[output++] = ' ';
            line++;
        }
        size_t available = capacity - output - 1u;
        size_t copy = bytes < available ? bytes : available;
        memcpy(destination + output, word, copy);
        output += copy;
        line += characters;
    }
    destination[output] = '\0';
}

/* API de sesión. Todos los recursos pertenecen a este handle y se liberan
 * simétricamente, incluso si falla la creación a mitad de la carga. */
int re_session_create(const ReProject *project, int preview, int menu, ReGameSession **out,
                      ReError *error) {
    if (!project || !out || !error)
        return 0;
    *out = nullptr;
    ReGameSession *session = calloc(1, sizeof(*session));
    if (!session)
        return 0;
    session->preview = preview != 0;
    if (!app_init(session, project) || !re_renderer_init(&session->renderer, 480, 270)) {
        (void)snprintf(error->message, sizeof(error->message),
                       "No se pudo crear la sesión: revisa el inicio del jugador y los recursos");
        re_session_destroy(session);
        return 0;
    }
    session->title = menu != 0;
    *out = session;
    *error = (ReError){0};
    return 1;
}

void re_session_destroy(ReGameSession *session) {
    if (!session)
        return;
    for (int i = 0; i < RE_MAX_MATERIALS; i++)
        re_texture_destroy(&session->materials[i]);
    for (size_t i = 0; i < RE_MAX_CHARACTER_DEFS; i++)
        re_texture_destroy(&session->sprites[i]);
    for (size_t i = 0; i < 2; i++)
        re_texture_destroy(&session->pickup_sprites[i]);
    re_texture_destroy(&session->projectile_sprite);
    re_texture_destroy(&session->title_art);
    re_renderer_destroy(&session->renderer);
    free(session);
}

void re_session_frame(ReGameSession *session, double elapsed, float move_x, float move_y,
                      float look_x, float look_y, uint32_t pressed, uint32_t held, int focused,
                      int single_step) {
    if (!session)
        return;
    ReInput input = {.movement = {move_x, move_y},
                     .look = {look_x, look_y},
                     .pressed = pressed,
                     .held = held,
                     .focused = focused != 0};
    if (elapsed > 0 && elapsed < 1)
        session->frame_ms = (float)(elapsed * 1000);
    /* Perder foco descarta eventos pendientes y tiempo: ni disparos tardíos
     * ni recuperación de ticks al regresar a la ventana del editor. */
    if (!focused && !single_step) {
        session->pending = (ReInput){0};
        session->clock.accumulator = 0;
    } else {
        re_input_accumulate(&session->pending, input);
        int ticks = single_step ? 1 : re_clock_advance(&session->clock, elapsed, true);
        for (int i = 0; i < ticks; i++)
            tick(session, re_input_consume(&session->pending));
    }
    draw(session, &session->renderer, re_clock_alpha(&session->clock));
}

const ReRenderer *re_session_renderer(const ReGameSession *session) {
    return session ? &session->renderer : nullptr;
}

int re_session_copy_pixels(const ReGameSession *session, void *destination, uint32_t bytes) {
    const size_t required = 480u * 270u * 4u;
    if (!session || !destination || bytes < required)
        return 0;
    memcpy(destination, session->renderer.pixels, required);
    return 1;
}

int re_session_flags(const ReGameSession *session) {
    if (!session)
        return 1;
    bool capture = !session->title && !session->paused && !session->interaction.state.game_over &&
                   !session->interaction.state.won && !session->interaction.state.dialogue_pauses;
    return (session->quit ? 1 : 0) | (capture ? 2 : 0);
}

size_t re_session_memory(const ReGameSession *session) {
    if (!session)
        return 0;
    size_t total = sizeof(*session) + 480u * 270u * 8u + texture_bytes(&session->title_art);
    for (size_t i = 0; i < RE_MAX_MATERIALS; i++)
        total += texture_bytes(&session->materials[i]);
    for (size_t i = 0; i < RE_MAX_CHARACTER_DEFS; i++)
        total += texture_bytes(&session->sprites[i]);
    for (size_t i = 0; i < 2; i++)
        total += texture_bytes(&session->pickup_sprites[i]);
    total += texture_bytes(&session->projectile_sprite);
    return total;
}
