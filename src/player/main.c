/* Reproductor genérico de proyectos RetroForge.
 *
 * Un proyecto creado con acciones incorporadas se ejecuta con este binario:
 * no necesita recompilar C. Las extensiones nativas pueden reemplazar este
 * punto de composición y seguir usando retro_core + retro_gameplay. */
#include "retro/platform.h"
#include "retro/project.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PlayerApp {
    ReProject project;
    ReGameplay gameplay;
    ReInteractionRuntime interaction;
    ReBody player;
    ReCamera previous_camera, camera;
    ReTexture materials[RE_MAX_MATERIALS];
    ReTexture sprites[RE_MAX_CHARACTER_DEFS];
    ReTexture pickup_sprites[2]; /* 0: botiquin; 1: llave/objeto de mision. */
    ReTexture title_art;
    char actor_ids[RE_MAX_ENTITIES][32];
    /* 0..2: manuales; 3: autosave; 4: guardado rápido. */
    char save_paths[5][1024];
    ReWorld initial_world;
    ReGameplay initial_gameplay;
    ReInteractionState initial_interaction;
    ReBody initial_player;
    ReCamera initial_camera;
    float elapsed;
    int menu_selection, pause_selection, save_slot, dialogue_selection;
    bool title, paused, quit;
} PlayerApp;

static void colors(PlayerApp *app) {
    static const RePixel palette[] = {{83, 99, 108, 255}, {48, 59, 65, 255}, {33, 41, 48, 255},
                                      {173, 91, 50, 255}, {74, 85, 94, 255}, {74, 164, 151, 180}};
    for (int i = 0; i < RE_MAX_MATERIALS; i++) {
        (void)re_texture_init(&app->materials[i], 8, 8);
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

static void fallback_sprite(ReTexture *texture, const ReCharacterDef *definition, size_t seed) {
    int cell_width = definition->cell_width > 0 ? definition->cell_width : 32;
    int cell_height = definition->cell_height > 0 ? definition->cell_height : 48;
    int columns = 8, rows = 8;
    (void)re_texture_init(texture, cell_width * columns, cell_height * rows);
    for (int cell = 0; cell < columns * rows; cell++)
        for (int y = 0; y < cell_height; y++)
            for (int x = 0; x < cell_width; x++) {
                int pose = cell % 4;
                int center = cell_width / 2 + pose - 2;
                int head_y = cell_height / 6, head_radius = cell_width / 6;
                int dx = x - center, dy = y - head_y;
                bool hair = dx * dx + dy * dy <= head_radius * head_radius;
                bool face =
                    dx * dx + dy * dy <= (head_radius - 2) * (head_radius - 2) && y >= head_y - 1;
                int torso_top = cell_height / 3, torso_bottom = cell_height * 2 / 3;
                int half_body = cell_width / 5 + (y - torso_top) / 10;
                bool coat = y >= torso_top && y <= torso_bottom && abs(x - center) <= half_body;
                bool left_arm =
                    y >= torso_top && y < torso_bottom && abs(x - (center - half_body - 2)) <= 2;
                bool right_arm =
                    y >= torso_top && y < torso_bottom && abs(x - (center + half_body + 2)) <= 2;
                bool left_leg = y > torso_bottom && y < cell_height - 2 &&
                                abs(x - (center - cell_width / 8 - pose % 2)) <= 2;
                bool right_leg = y > torso_bottom && y < cell_height - 2 &&
                                 abs(x - (center + cell_width / 8 + pose % 2)) <= 2;
                uint8_t red = (uint8_t)(150u + (unsigned int)(seed % 3u) * 28u +
                                        (unsigned int)(cell % 3) * 8u);
                RePixel pixel = re_rgba(0, 0, 0, 0);
                if (hair)
                    pixel = re_rgba(35, 25, 28, 255);
                if (face)
                    pixel = re_rgba(207, 158, 132, 255);
                if (coat || left_arm || right_arm)
                    pixel = re_rgba(red, (uint8_t)(48 + pose * 7), 62, 255);
                if (left_leg || right_leg)
                    pixel = re_rgba(38, 42, 49, 255);
                size_t px = (size_t)(cell % columns) * (size_t)cell_width + (size_t)x;
                size_t py = (size_t)(cell / columns) * (size_t)cell_height + (size_t)y;
                texture->pixels[py * (size_t)texture->width + px] = pixel;
            }
}

static bool app_init(PlayerApp *app, const char *manifest, ReError *error) {
    if (!re_project_load(manifest, &app->project, error))
        return false;
    colors(app);
    create_pickup_sprites(app);
    for (size_t i = 0; i < app->project.character_count; i++) {
        char path[RE_PROJECT_PATH * 2];
        if (strcmp(app->project.characters[i].sprite, "none") == 0 ||
            !re_project_path(&app->project, app->project.characters[i].sprite, path,
                             sizeof(path)) ||
            !re_platform_image_load(path, &app->sprites[i]))
            fallback_sprite(&app->sprites[i], &app->project.characters[i], i);
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
    for (size_t i = 0; i < 5; i++)
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
    app->dialogue_selection = 0;
    app->paused = false;
}

static void interact(PlayerApp *app) {
    ReVec3 forward = re_camera_forward(&app->camera);
    if (re_interaction_interact(&app->interaction, app->camera.position, forward, 2.2f))
        return;
    /* En proyectos con reglas, una puerta sólo cambia mediante una acción
     * declarada. El fallback conserva compatibilidad con proyectos v1. */
    if (app->project.interactions.rule_count)
        return;
    ReTraceHit hit =
        re_world_trace(&app->project.world, app->camera.position, forward, 1.5f, RE_BLOCK_MOVEMENT);
    if (hit.barrier >= 0 && app->project.world.barriers[hit.barrier].kind == RE_BARRIER_DOOR)
        app->project.world.barriers[hit.barrier].open_fraction = 1;
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

static void save_game(PlayerApp *app, size_t index, const char *label) {
    ReError error = {0};
    if (index >= 5 || !app->save_paths[index][0] ||
        !re_save_write(app->save_paths[index], app->project.id, &app->interaction, &app->player,
                       &error)) {
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
            } else if (app->project.death_policy == RE_DEATH_LIMITED_LIVES &&
                       app->interaction.state.player_lives > 0 &&
                       app->interaction.state.checkpoint_valid) {
                int lives = app->interaction.state.player_lives - 1;
                re_interaction_checkpoint_restore(&app->interaction, &app->player);
                app->interaction.state.player_lives = lives;
                app->camera.position = re_add3(app->player.position, re_v3(0, 0, 1.55f));
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
    if (input.pressed & RE_PRIMARY)
        shoot(app);
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
        if (event.kind == RE_EVENT_PLAYER_DAMAGE) {
            app->interaction.state.player_health =
                app->interaction.state.player_health > event.value
                    ? app->interaction.state.player_health - event.value
                    : 0;
            if (!app->interaction.state.player_health)
                (void)re_interaction_emit(
                    &app->interaction,
                    (ReLogicEvent){.kind = RE_LOGIC_PLAYER_DIED, .source = "player"});
        }
    }
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
    const char *clip = actor->state == RE_CHARACTER_CHASE ? "chase" : "idle";
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
        float uv[4];
        if (!re_texture_cell_uv(texture, definition->cell_width, definition->cell_height, 0, uv))
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
    re_rect(renderer, 0, 247, 480, 23, re_rgba(9, 14, 19, 235));
    char hud[128];
    (void)snprintf(hud, sizeof(hud), "VIDA %03d  VIDAS %d  LLAVE %u  F5/F9",
                   app->interaction.state.player_health, app->interaction.state.player_lives,
                   re_interaction_item_count(&app->interaction, "brass_key"));
    re_text(renderer, 12, 255, hud, 1, re_rgba(220, 226, 218, 255));
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

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
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
        size_t length = (size_t)(cursor - word);
        if (line && line + 1u + length > columns && output + 1u < capacity) {
            destination[output++] = '\n';
            line = 0;
        } else if (line && output + 1u < capacity) {
            destination[output++] = ' ';
            line++;
        }
        size_t available = capacity - output - 1u;
        size_t copy = length < available ? length : available;
        memcpy(destination + output, word, copy);
        output += copy;
        line += copy;
    }
    destination[output] = '\0';
}

int main(int argc, char **argv) {
    const char *override = nullptr, *capture = nullptr;
    int smoke_frames = 0;
    bool show_menu = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--project") == 0 && i + 1 < argc)
            override = argv[++i];
        else if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
            capture = argv[++i];
        else if (strcmp(argv[i], "--menu") == 0)
            show_menu = true;
        else if (strcmp(argv[i], "--smoke") == 0 && i + 1 < argc) {
            char *end = nullptr;
            errno = 0;
            long value = strtol(argv[++i], &end, 10);
            if (errno || end == argv[i] || *end || value < 1 || value > 100000)
                return 2;
            smoke_frames = (int)value;
        } else {
            (void)fprintf(stderr, "Uso: retro_player [--project archivo] [--smoke frames] "
                                  "[--capture png] [--menu]\n");
            return 2;
        }
    }
    ReError error = {0};
    RePlatform *platform = re_platform_open(
        (RePlatformConfig){"RetroForge Player", 480, 270, smoke_frames > 0, smoke_frames == 0},
        &error);
    if (!platform)
        return 1;
    char manifest[2048] = {0};
    PlayerApp *app = calloc(1, sizeof(*app));
    ReRenderer renderer = {0};
    double *render_samples = nullptr;
    int result = 1;
    if (!override && re_platform_application_path("project.retro", manifest, sizeof(manifest))) {
        FILE *probe = fopen(manifest, "rb");
        if (probe)
            (void)fclose(probe);
        else
            (void)re_platform_asset_path("studio/haunted.retro", manifest, sizeof(manifest));
    }
    if (!app || (!override && manifest[0] == '\0') ||
        !app_init(app, override ? override : manifest, &error) ||
        !re_renderer_init(&renderer, 480, 270)) {
        (void)fprintf(stderr, "Proyecto:%zu: %s\n", error.line, error.message);
        goto cleanup;
    }
    if (smoke_frames && !show_menu)
        app->title = false;
    ReClock clock = {0};
    ReInput pending = {0};
    double previous = re_platform_time();
    double render_total = 0, render_max = 0;
    render_samples =
        smoke_frames > 0 ? calloc((size_t)smoke_frames, sizeof(*render_samples)) : nullptr;
    int frames = 0;
    while (!app->quit && (!smoke_frames || frames < smoke_frames)) {
        double now = re_platform_time();
        ReInput input = re_platform_input(platform);
        app->quit |= input.quit;
        re_platform_capture_mouse(platform, input.focused && !app->title && !app->paused &&
                                                !app->interaction.state.game_over &&
                                                !app->interaction.state.won &&
                                                !app->interaction.state.dialogue_pauses);
        int ticks = re_clock_advance(&clock, now - previous, input.focused);
        previous = now;
        re_input_accumulate(&pending, input);
        for (int i = 0; i < ticks; i++)
            tick(app, re_input_consume(&pending));
        double render_start = re_platform_time();
        draw(app, &renderer, re_clock_alpha(&clock));
        double render_elapsed = re_platform_time() - render_start;
        render_total += render_elapsed;
        if (render_samples)
            render_samples[frames] = render_elapsed;
        if (render_elapsed > render_max)
            render_max = render_elapsed;
        re_platform_present(platform, &renderer);
        frames++;
    }
    if (smoke_frames) {
        double p95 = render_max;
        if (render_samples && frames > 0) {
            qsort(render_samples, (size_t)frames, sizeof(*render_samples), compare_double);
            size_t index = ((size_t)frames * 95u + 99u) / 100u - 1u;
            p95 = render_samples[index];
        }
        size_t memory = sizeof(*app) + (size_t)renderer.width * (size_t)renderer.height *
                                           (sizeof(*renderer.pixels) + sizeof(*renderer.depth));
        memory += texture_bytes(&app->title_art);
        for (int i = 0; i < RE_MAX_MATERIALS; i++)
            memory += texture_bytes(&app->materials[i]);
        for (size_t i = 0; i < app->project.character_count; i++)
            memory += texture_bytes(&app->sprites[i]);
        (void)printf("frames=%d render_mean_ms=%.3f render_p95_ms=%.3f render_max_ms=%.3f "
                     "entities=%zu lights=%zu cpu_memory_mib=%.2f\n",
                     frames, frames ? render_total * 1000.0 / (double)frames : 0, p95 * 1000.0,
                     render_max * 1000.0, app->project.world.marker_count,
                     app->project.interactions.light_count, (double)memory / (1024.0 * 1024.0));
    }
    result = 0;
    if (capture && !re_platform_capture_png(&renderer, capture))
        result = 1;
cleanup:
    free(render_samples);
    re_renderer_destroy(&renderer);
    if (app) {
        for (int i = 0; i < RE_MAX_MATERIALS; i++)
            re_texture_destroy(&app->materials[i]);
        for (size_t i = 0; i < app->project.character_count; i++)
            re_texture_destroy(&app->sprites[i]);
        for (size_t i = 0; i < 2; i++)
            re_texture_destroy(&app->pickup_sprites[i]);
        re_texture_destroy(&app->title_art);
    }
    free(app);
    re_platform_close(platform);
    return result;
}
