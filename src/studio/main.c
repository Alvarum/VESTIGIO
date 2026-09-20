/* RetroForge Studio: editor visual nativo para proyectos, mapas y personajes.
 *
 * El editor manipula los mismos ReWorld/ReGameplay que usa el ejecutable final.
 * El modo Probar trabaja sobre una copia: detenerlo descarta puertas abiertas,
 * enemigos movidos y cualquier otro estado de simulación. */
#include "raygui.h"
#include "raylib.h"
#include "retro/project.h"
#include "retro/render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#endif

enum { HISTORY_CAPACITY = 32 };
typedef struct StudioHistory {
    ReWorld *states;
    int count, cursor;
} StudioHistory;
typedef struct Studio {
    ReProject project;
    ReWorld runtime_world;
    ReGameplay gameplay;
    ReInteractionRuntime interaction;
    ReBody player;
    ReCamera camera;
    ReRenderer renderer;
    ReTexture materials[RE_MAX_MATERIALS], actor_sprite;
    char actor_ids[RE_MAX_ENTITIES][32];
    Texture2D screen;
    StudioHistory history;
    int selected_sector, selected_vertex, selected_actor, selected_rule, selected_dialogue;
    int selected_animation, selected_frame, link_sector, link_edge;
    int workspace;
    int tutorial_step;
    float elapsed, autosave_timer;
    float floor, zoom;
    Vector2 pan;
    bool preview, playing, dragging, dirty, autosave_available, help_open;
    char status[256];
} Studio;

static void status(Studio *s, const char *message) {
    (void)snprintf(s->status, sizeof(s->status), "%s", message);
}

static bool history_init(StudioHistory *history, const ReWorld *world) {
    history->states = calloc(HISTORY_CAPACITY, sizeof(*history->states));
    if (!history->states)
        return false;
    history->states[0] = *world;
    history->count = 1;
    history->cursor = 0;
    return true;
}

static void history_push(Studio *studio) {
    StudioHistory *history = &studio->history;
    if (history->cursor + 1 < history->count)
        history->count = history->cursor + 1;
    if (history->count == HISTORY_CAPACITY) {
        memmove(&history->states[0], &history->states[1],
                (HISTORY_CAPACITY - 1u) * sizeof(*history->states));
        history->count--;
        history->cursor--;
    }
    history->states[history->count++] = studio->project.world;
    history->cursor = history->count - 1;
    studio->dirty = true;
}

static void history_move(Studio *studio, int direction) {
    int next = studio->history.cursor + direction;
    if (next < 0 || next >= studio->history.count)
        return;
    studio->history.cursor = next;
    studio->project.world = studio->history.states[next];
    studio->selected_sector = -1;
    studio->dirty = true;
    status(studio, direction < 0 ? "Deshacer" : "Rehacer");
}

static Vector2 screen_point(ReVec2 point, Rectangle canvas, const Studio *studio) {
    return (Vector2){canvas.x + canvas.width * .5f + (point.x + studio->pan.x) * studio->zoom,
                     canvas.y + canvas.height * .5f - (point.y + studio->pan.y) * studio->zoom};
}

static ReVec2 world_point(Vector2 point, Rectangle canvas, const Studio *studio) {
    return re_v2((point.x - canvas.x - canvas.width * .5f) / studio->zoom - studio->pan.x,
                 -(point.y - canvas.y - canvas.height * .5f) / studio->zoom - studio->pan.y);
}

static bool on_floor(const ReSector *sector, float floor) {
    return fabsf(sector->floor - floor) < .13f;
}

static void draw_plan(Studio *studio, Rectangle canvas) {
    DrawRectangleRec(canvas, (Color){20, 25, 31, 255});
    BeginScissorMode((int)canvas.x, (int)canvas.y, (int)canvas.width, (int)canvas.height);
    float grid = studio->zoom;
    float start_x = fmodf(canvas.x + canvas.width * .5f + studio->pan.x * grid, grid);
    float start_y = fmodf(canvas.y + canvas.height * .5f - studio->pan.y * grid, grid);
    for (float x = canvas.x + start_x; x < canvas.x + canvas.width; x += grid)
        DrawLine((int)x, (int)canvas.y, (int)x, (int)(canvas.y + canvas.height),
                 (Color){39, 47, 55, 255});
    for (float y = canvas.y + start_y; y < canvas.y + canvas.height; y += grid)
        DrawLine((int)canvas.x, (int)y, (int)(canvas.x + canvas.width), (int)y,
                 (Color){39, 47, 55, 255});
    for (size_t i = 0; i < studio->project.world.sector_count; i++) {
        const ReSector *sector = &studio->project.world.sectors[i];
        if (!on_floor(sector, studio->floor))
            continue;
        Color line = (int)i == studio->selected_sector ? (Color){245, 166, 76, 255}
                                                       : (Color){120, 155, 164, 255};
        for (size_t edge = 0; edge < sector->count; edge++) {
            Vector2 a = screen_point(sector->vertices[edge], canvas, studio);
            Vector2 b = screen_point(sector->vertices[(edge + 1u) % sector->count], canvas, studio);
            DrawLineEx(a, b, (int)i == studio->selected_sector ? 3 : 2, line);
            if ((int)i == studio->selected_sector)
                DrawCircleV(a, 6, (int)edge == studio->selected_vertex ? YELLOW : line);
        }
        ReVec2 center = {0};
        for (size_t v = 0; v < sector->count; v++)
            center = re_add2(center, sector->vertices[v]);
        center = re_scale2(center, 1.0f / (float)sector->count);
        Vector2 label = screen_point(center, canvas, studio);
        DrawText(TextFormat("%u", (unsigned int)i), (int)label.x - 4, (int)label.y - 5, 10, line);
    }
    /* Overlays de autoría. Se dibujan después de las habitaciones para que
     * el volumen de activación y el radio de una luz sigan legibles. No forman
     * parte del mapa rasterizado ni alteran el playtest. */
    for (size_t i = 0; i < studio->project.interactions.trigger_count; i++) {
        const ReTriggerVolume *trigger = &studio->project.interactions.triggers[i];
        Color color = (Color){202, 92, 198, 210};
        if (trigger->shape == RE_TRIGGER_SECTOR) {
            if (trigger->sector < 0 || trigger->sector >= (int)studio->project.world.sector_count)
                continue;
            const ReSector *sector = &studio->project.world.sectors[trigger->sector];
            if (!on_floor(sector, studio->floor))
                continue;
            for (size_t edge = 0; edge < sector->count; edge++)
                DrawLineEx(
                    screen_point(sector->vertices[edge], canvas, studio),
                    screen_point(sector->vertices[(edge + 1u) % sector->count], canvas, studio), 2,
                    color);
            ReVec2 center = {0};
            for (size_t vertex = 0; vertex < sector->count; vertex++)
                center = re_add2(center, sector->vertices[vertex]);
            center = re_scale2(center, 1.0f / (float)sector->count);
            Vector2 label = screen_point(center, canvas, studio);
            DrawText(trigger->id, (int)label.x + 8, (int)label.y + 8, 9, color);
        } else {
            float bottom = trigger->center.z - trigger->half_size.z;
            float top = trigger->center.z + trigger->half_size.z;
            if (studio->floor < bottom - .13f || studio->floor > top + .13f)
                continue;
            Vector2 center =
                screen_point(re_v2(trigger->center.x, trigger->center.y), canvas, studio);
            if (trigger->shape == RE_TRIGGER_CYLINDER)
                DrawCircleLines((int)center.x, (int)center.y, trigger->radius * studio->zoom,
                                color);
            else {
                Rectangle box = {center.x - trigger->half_size.x * studio->zoom,
                                 center.y - trigger->half_size.y * studio->zoom,
                                 trigger->half_size.x * 2 * studio->zoom,
                                 trigger->half_size.y * 2 * studio->zoom};
                DrawRectangleLinesEx(box, 2, color);
            }
            DrawText(trigger->id, (int)center.x + 8, (int)center.y + 8, 9, color);
        }
    }
    for (size_t i = 0; i < studio->project.interactions.light_count; i++) {
        const ReLight *light = &studio->project.interactions.lights[i];
        int sector = re_world_sector_at(&studio->project.world, light->position, -1);
        if (sector < 0 || !on_floor(&studio->project.world.sectors[sector], studio->floor))
            continue;
        Vector2 center = screen_point(re_v2(light->position.x, light->position.y), canvas, studio);
        Color color = {(unsigned char)(light->color.x * 255), (unsigned char)(light->color.y * 255),
                       (unsigned char)(light->color.z * 255), 170};
        DrawCircleLines((int)center.x, (int)center.y, light->radius * studio->zoom, color);
        DrawCircleV(center, 5, color);
        DrawText(light->id, (int)center.x + 8, (int)center.y - 5, 9, color);
    }
    for (size_t i = 0; i < studio->project.world.marker_count; i++) {
        const ReMarker *marker = &studio->project.world.markers[i];
        if (marker->sector < 0 || marker->sector >= (int)studio->project.world.sector_count ||
            !on_floor(&studio->project.world.sectors[marker->sector], studio->floor))
            continue;
        Vector2 p = screen_point(re_v2(marker->position.x, marker->position.y), canvas, studio);
        DrawCircleV(p, 7, strcmp(marker->kind, "actor") == 0 ? RED : SKYBLUE);
        DrawText(marker->id, (int)p.x + 9, (int)p.y - 5, 10, RAYWHITE);
    }
    EndScissorMode();
}

static void edit_plan(Studio *studio, Rectangle canvas) {
    if (!CheckCollisionPointRec(GetMousePosition(), canvas) || studio->playing)
        return;
    float wheel = GetMouseWheelMove();
    if (wheel)
        studio->zoom = re_clamp(studio->zoom + wheel * 3, 12, 80);
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
        studio->dragging = true;
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        studio->pan.x += delta.x / studio->zoom;
        studio->pan.y -= delta.y / studio->zoom;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE))
        studio->dragging = false;
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        studio->selected_vertex = -1;
        if (studio->selected_sector >= 0) {
            ReSector *selected = &studio->project.world.sectors[studio->selected_sector];
            for (size_t v = 0; v < selected->count; v++) {
                Vector2 vertex = screen_point(selected->vertices[v], canvas, studio);
                float dx = vertex.x - mouse.x, dy = vertex.y - mouse.y;
                if (sqrtf(dx * dx + dy * dy) < 10)
                    studio->selected_vertex = (int)v;
            }
        }
        if (studio->selected_vertex < 0) {
            ReVec2 point = world_point(mouse, canvas, studio);
            studio->selected_sector = -1;
            for (size_t i = 0; i < studio->project.world.sector_count; i++)
                if (on_floor(&studio->project.world.sectors[i], studio->floor) &&
                    re_sector_contains(&studio->project.world.sectors[i], point))
                    studio->selected_sector = (int)i;
        }
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && studio->selected_sector >= 0 &&
        studio->selected_vertex >= 0) {
        ReVec2 p = world_point(mouse, canvas, studio);
        p.x = roundf(p.x * 4) * .25f;
        p.y = roundf(p.y * 4) * .25f;
        studio->project.world.sectors[studio->selected_sector].vertices[studio->selected_vertex] =
            p;
        studio->dragging = true;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && studio->dragging) {
        studio->dragging = false;
        history_push(studio);
    }
}

static void add_room(Studio *studio) {
    ReWorld *world = &studio->project.world;
    if (world->sector_count == RE_MAX_SECTORS) {
        status(studio, "Límite de sectores alcanzado");
        return;
    }
    ReSector *sector = &world->sectors[world->sector_count];
    *sector = (ReSector){.floor = studio->floor,
                         .ceiling = studio->floor + 3,
                         .light = .85f,
                         .wall_material = 0,
                         .floor_material = 1,
                         .ceiling_material = 2,
                         .count = 4};
    float x = (float)(world->sector_count % 6u) * 5;
    float y = (float)(world->sector_count / 6u) * 5;
    sector->vertices[0] = re_v2(x, y);
    sector->vertices[1] = re_v2(x + 4, y);
    sector->vertices[2] = re_v2(x + 4, y + 4);
    sector->vertices[3] = re_v2(x, y + 4);
    for (size_t i = 0; i < RE_MAX_VERTICES; i++) {
        sector->neighbor[i] = -1;
        sector->neighbor_edge[i] = -1;
        sector->portal_start[i] = 0;
        sector->portal_end[i] = 1;
    }
    studio->selected_sector = (int)world->sector_count++;
    history_push(studio);
    status(studio, "Habitación creada; arrastra sus vértices sobre la cuadrícula");
}

static void duplicate_room(Studio *studio) {
    ReWorld *world = &studio->project.world;
    if (studio->selected_sector < 0 || world->sector_count == RE_MAX_SECTORS)
        return;
    ReSector copy = world->sectors[studio->selected_sector];
    float min_x = INFINITY, max_x = -INFINITY;
    for (size_t i = 0; i < copy.count; i++) {
        min_x = fminf(min_x, copy.vertices[i].x);
        max_x = fmaxf(max_x, copy.vertices[i].x);
    }
    for (size_t i = 0; i < copy.count; i++) {
        copy.vertices[i].x += max_x - min_x + 1;
        copy.neighbor[i] = -1;
        copy.neighbor_edge[i] = -1;
        copy.portal_start[i] = 0;
        copy.portal_end[i] = 1;
    }
    world->sectors[world->sector_count] = copy;
    studio->selected_sector = (int)world->sector_count++;
    history_push(studio);
    status(studio, "Habitación duplicada sin conexiones");
}

static ReVec2 sector_center(const ReSector *sector) {
    ReVec2 center = {0};
    for (size_t i = 0; i < sector->count; i++)
        center = re_add2(center, sector->vertices[i]);
    return re_scale2(center, 1.0f / (float)sector->count);
}

static void stack_room(Studio *studio) {
    ReWorld *world = &studio->project.world;
    if (studio->selected_sector < 0 || world->sector_count == RE_MAX_SECTORS)
        return;
    ReSector copy = world->sectors[studio->selected_sector];
    float height = copy.ceiling - copy.floor;
    copy.floor = copy.ceiling;
    copy.ceiling += height;
    for (size_t i = 0; i < copy.count; i++) {
        copy.neighbor[i] = -1;
        copy.neighbor_edge[i] = -1;
        copy.portal_start[i] = 0;
        copy.portal_end[i] = 1;
    }
    world->sectors[world->sector_count] = copy;
    studio->selected_sector = (int)world->sector_count++;
    studio->floor = copy.floor;
    history_push(studio);
    status(studio, "Planta superior creada sobre la habitación seleccionada");
}

static void add_step(Studio *studio) {
    ReWorld *world = &studio->project.world;
    if (studio->selected_sector < 0 || studio->selected_vertex < 0 ||
        world->sector_count == RE_MAX_SECTORS)
        return;
    ReSector *source = &world->sectors[studio->selected_sector];
    size_t edge = (size_t)studio->selected_vertex;
    if (edge >= source->count || source->neighbor[edge] >= 0) {
        status(studio, "Elige una arista exterior para crear el peldaño");
        return;
    }
    ReVec2 a = source->vertices[edge], b = source->vertices[(edge + 1u) % source->count];
    ReVec2 direction = re_normalize2(re_sub2(b, a));
    ReVec2 outside = re_v2(direction.y, -direction.x);
    ReSector *step = &world->sectors[world->sector_count];
    *step = (ReSector){.floor = source->floor + .25f,
                       .ceiling = source->ceiling + .25f,
                       .light = source->light,
                       .wall_material = source->wall_material,
                       .floor_material = source->floor_material,
                       .ceiling_material = source->ceiling_material,
                       .count = 4};
    step->vertices[0] = b;
    step->vertices[1] = a;
    step->vertices[2] = re_add2(a, outside);
    step->vertices[3] = re_add2(b, outside);
    for (size_t i = 0; i < RE_MAX_VERTICES; i++) {
        step->neighbor[i] = -1;
        step->neighbor_edge[i] = -1;
        step->portal_start[i] = 0;
        step->portal_end[i] = 1;
    }
    step->neighbor[0] = studio->selected_sector;
    step->neighbor_edge[0] = (int)edge;
    source->neighbor[edge] = (int)world->sector_count;
    source->neighbor_edge[edge] = 0;
    /* Un peldaño ocupa el ancho completo de su arista: no debe crear jambas
     * entre escalones consecutivos. */
    source->portal_start[edge] = 0;
    source->portal_end[edge] = 1;
    step->portal_start[0] = 0;
    step->portal_end[0] = 1;
    studio->selected_sector = (int)world->sector_count++;
    studio->selected_vertex = 2; /* Arista exterior lista para el siguiente paso. */
    studio->floor = step->floor;
    history_push(studio);
    status(studio, "Peldaño de 25 cm creado y conectado");
}

static void link_edge(Studio *studio) {
    if (studio->selected_sector < 0 || studio->selected_vertex < 0)
        return;
    ReWorld *world = &studio->project.world;
    ReSector *current = &world->sectors[studio->selected_sector];
    int edge = studio->selected_vertex;
    if (edge >= (int)current->count || current->neighbor[edge] >= 0) {
        status(studio, "La arista ya está conectada o no es válida");
        return;
    }
    if (studio->link_sector < 0) {
        studio->link_sector = studio->selected_sector;
        studio->link_edge = edge;
        status(studio, "Primera arista guardada; selecciona la arista opuesta y pulsa Conectar");
        return;
    }
    ReSector *first = &world->sectors[studio->link_sector];
    ReVec2 a0 = first->vertices[studio->link_edge];
    ReVec2 a1 = first->vertices[((size_t)studio->link_edge + 1u) % first->count];
    ReVec2 b0 = current->vertices[edge];
    ReVec2 b1 = current->vertices[((size_t)edge + 1u) % current->count];
    if (re_length2(re_sub2(a0, b1)) > .001f || re_length2(re_sub2(a1, b0)) > .001f) {
        studio->link_sector = -1;
        status(studio, "Las aristas deben coincidir y tener sentidos opuestos");
        return;
    }
    first->neighbor[studio->link_edge] = studio->selected_sector;
    first->neighbor_edge[studio->link_edge] = edge;
    current->neighbor[edge] = studio->link_sector;
    current->neighbor_edge[edge] = studio->link_edge;
    /* Las habitaciones se conectan mediante una abertura central. El usuario
     * puede afinar este intervalo desde el inspector; ambas caras almacenan
     * valores reciprocos porque sus aristas tienen sentidos opuestos. */
    first->portal_start[studio->link_edge] = .2f;
    first->portal_end[studio->link_edge] = .8f;
    current->portal_start[edge] = .2f;
    current->portal_end[edge] = .8f;
    studio->link_sector = -1;
    history_push(studio);
    status(studio, "Habitaciones conectadas");
}

static void add_actor_marker(Studio *studio) {
    ReWorld *world = &studio->project.world;
    if (studio->selected_sector < 0 || studio->selected_actor < 0 ||
        world->marker_count == RE_MAX_MARKERS)
        return;
    ReMarker *marker = &world->markers[world->marker_count];
    const ReSector *sector = &world->sectors[studio->selected_sector];
    ReVec2 center = sector_center(sector);
    const char *definition = studio->project.characters[studio->selected_actor].id;
    *marker = (ReMarker){.sector = studio->selected_sector,
                         .position = {center.x, center.y, sector->floor}};
    (void)snprintf(marker->id, sizeof(marker->id), "actor-%zu", world->marker_count);
    (void)snprintf(marker->kind, sizeof(marker->kind), "actor");
    (void)snprintf(marker->definition, sizeof(marker->definition), "%s", definition);
    world->marker_count++;
    history_push(studio);
    status(studio, "Personaje colocado en el centro de la habitación");
}

static void add_barrier(Studio *studio, enum ReBarrierKind kind) {
    ReWorld *world = &studio->project.world;
    if (studio->selected_sector < 0 || studio->selected_vertex < 0 ||
        world->barrier_count == RE_MAX_BARRIERS)
        return;
    int edge = studio->selected_vertex;
    if (world->sectors[studio->selected_sector].neighbor[edge] < 0 ||
        re_world_barrier_at(world, studio->selected_sector, edge) >= 0) {
        status(studio, "Puertas y ventanas requieren una conexión sin otra barrera");
        return;
    }
    ReBarrier *barrier = &world->barriers[world->barrier_count];
    *barrier = (ReBarrier){.kind = kind,
                           .sector = studio->selected_sector,
                           .edge = edge,
                           .material = kind == RE_BARRIER_DOOR ? 4 : 5,
                           .blocks = kind == RE_BARRIER_DOOR
                                         ? RE_BLOCK_MOVEMENT | RE_BLOCK_SIGHT | RE_BLOCK_PROJECTILE
                                         : RE_BLOCK_MOVEMENT | RE_BLOCK_PROJECTILE,
                           .health = kind == RE_BARRIER_WINDOW ? 25 : 0};
    (void)snprintf(barrier->id, sizeof(barrier->id), "%s-%zu",
                   kind == RE_BARRIER_DOOR ? "door" : "window", world->barrier_count);
    world->barrier_count++;
    history_push(studio);
    status(studio, kind == RE_BARRIER_DOOR ? "Puerta creada" : "Ventana rompible creada");
}

static void save_project(Studio *studio) {
    ReError error = {0};
    if (!re_project_save(&studio->project, &error)) {
        (void)snprintf(studio->status, sizeof(studio->status), "Error %zu: %.220s", error.line,
                       error.message);
        return;
    }
    char path[RE_PROJECT_PATH * 2];
    for (size_t i = 0; i < studio->project.character_count; i++)
        if (re_project_path(&studio->project, studio->project.actor_files[i], path, sizeof(path)) &&
            !re_character_save(path, &studio->project.characters[i], &error)) {
            (void)snprintf(studio->status, sizeof(studio->status), "Actor: %.240s", error.message);
            return;
        }
    studio->dirty = false;
    status(studio, "Proyecto guardado y validado");
}

static void autosave_paths(const Studio *studio, char *directory, size_t directory_size, char *map,
                           size_t map_size, char *rules, size_t rules_size, char *dialogue,
                           size_t dialogue_size) {
    (void)snprintf(directory, directory_size, "%s/.retroforge/autosave", studio->project.root);
    /* Componer desde root conserva para el compilador la cota RE_PROJECT_PATH;
     * además evita encadenar una cadena potencialmente truncada. */
    (void)snprintf(map, map_size, "%s/.retroforge/autosave/world.map", studio->project.root);
    (void)snprintf(rules, rules_size, "%s/.retroforge/autosave/gameplay.rules",
                   studio->project.root);
    (void)snprintf(dialogue, dialogue_size, "%s/.retroforge/autosave/story.dialogue",
                   studio->project.root);
}

static void autosave_project(Studio *studio) {
    char base[1024], directory[1024], map[1024], rules[1024], dialogue[1024];
    (void)snprintf(base, sizeof(base), "%s/.retroforge", studio->project.root);
    autosave_paths(studio, directory, sizeof(directory), map, sizeof(map), rules, sizeof(rules),
                   dialogue, sizeof(dialogue));
    if (!DirectoryExists(base))
        (void)MakeDirectory(base);
    if (!DirectoryExists(directory))
        (void)MakeDirectory(directory);
    ReError error = {0};
    if (re_world_save_v4(map, &studio->project.world, &error) &&
        re_interaction_save_rules(rules, &studio->project.interactions, &error) &&
        re_interaction_save_dialogues(dialogue, &studio->project.interactions, &error)) {
        studio->autosave_available = true;
        studio->autosave_timer = 0;
        status(studio, "Copia automática segura actualizada");
    } else
        (void)snprintf(studio->status, sizeof(studio->status), "Autosave: %.220s", error.message);
}

static void recover_autosave(Studio *studio) {
    char directory[1024], map[1024], rules[1024], dialogue[1024];
    autosave_paths(studio, directory, sizeof(directory), map, sizeof(map), rules, sizeof(rules),
                   dialogue, sizeof(dialogue));
    ReWorld recovered_world = {0};
    ReInteractionDefinitions recovered_interactions = {0};
    ReError error = {0};
    if (!re_world_load(map, &recovered_world, &error) ||
        !re_interaction_load_rules(rules, &recovered_interactions, &error) ||
        !re_interaction_load_dialogues(dialogue, &recovered_interactions, &error)) {
        (void)snprintf(studio->status, sizeof(studio->status), "Recuperación: %.210s",
                       error.message);
        return;
    }
    studio->project.world = recovered_world;
    studio->project.interactions = recovered_interactions;
    history_push(studio);
    status(studio, "Copia automática recuperada; GUARDAR confirma los cambios");
}

static void duplicate_character(Studio *studio) {
    ReProject *project = &studio->project;
    if (project->character_count >= RE_MAX_CHARACTER_DEFS) {
        status(studio, "Límite de definiciones de personaje alcanzado");
        return;
    }

    size_t destination = project->character_count;
    if (studio->selected_actor >= 0 && studio->selected_actor < (int)project->character_count)
        project->characters[destination] = project->characters[studio->selected_actor];
    else
        project->characters[destination] = (ReCharacterDef){
            .cell_width = 32,
            .cell_height = 48,
            .radius = .3f,
            .height = 1.7f,
            .step_height = .3f,
            .speed = 2.2f,
            .turn_speed = 5,
            .sight_range = 12,
            .field_of_view = 1.4f,
            .hearing_range = 8,
            .memory_time = 4,
            .capture_range = .65f,
            .attack_range = 1,
            .max_health = 100,
            .attack_damage = 10,
            .can_be_stunned = true,
            .tracking = RE_TRACK_PERCEPTION,
        };

    ReCharacterDef *character = &project->characters[destination];
    char id[64];
    (void)snprintf(id, sizeof(id), "actor_%zu", destination + 1u);
    memcpy(character->id, id, strlen(id) + 1u);
    if (character->sprite[0] == '\0')
        (void)snprintf(character->sprite, sizeof(character->sprite), "none");
    (void)snprintf(project->actor_files[destination], RE_PROJECT_PATH, "actors/%s.actor", id);
    project->character_count++;
    studio->selected_actor = (int)destination;
    studio->selected_animation = 0;
    studio->selected_frame = 0;
    studio->dirty = true;
    status(studio, "Personaje duplicado: cambia sus propiedades y guarda");
}

static void import_dropped(Studio *studio) {
    if (!IsFileDropped())
        return;
    FilePathList files = LoadDroppedFiles();
    char directory[RE_PROJECT_PATH * 2];
    (void)snprintf(directory, sizeof(directory), "%s/art", studio->project.root);
    if (!DirectoryExists(directory))
        (void)MakeDirectory(directory);
    for (unsigned int i = 0; i < files.count; i++) {
        const char *extension = GetFileExtension(files.paths[i]);
        if (strcmp(extension, ".png") != 0 && strcmp(extension, ".wav") != 0) {
            status(studio, "Importación omitida: sólo PNG y WAV");
            continue;
        }
        char relative[RE_PROJECT_PATH];
        (void)snprintf(relative, sizeof(relative), "art/%s", GetFileName(files.paths[i]));
        ReError error = {0};
        if (re_project_import(&studio->project, files.paths[i], relative, &error)) {
            if (strcmp(extension, ".png") == 0 && studio->selected_actor >= 0 &&
                studio->selected_actor < (int)studio->project.character_count) {
                ReCharacterDef *actor = &studio->project.characters[studio->selected_actor];
                size_t length = strlen(relative);
                if (length >= sizeof(actor->sprite))
                    status(studio, "PNG copiado, pero su ruta es demasiado larga para asignarla");
                else {
                    memcpy(actor->sprite, relative, length + 1u);
                    studio->dirty = true;
                    status(studio, "PNG importado y asignado al personaje seleccionado");
                }
            } else
                status(studio, "Recurso copiado a art/; sus referencias son relativas");
        } else
            (void)snprintf(studio->status, sizeof(studio->status), "Importación: %.220s",
                           error.message);
    }
    UnloadDroppedFiles(files);
}

static void export_project(Studio *studio) {
    save_project(studio);
    if (studio->dirty)
        return;
#ifdef _WIN32
    char script[1024], source_script[1024], output[1024];
    (void)snprintf(script, sizeof(script), "%stools/export-project.ps1", GetApplicationDirectory());
    if (!FileExists(script)) {
        (void)snprintf(source_script, sizeof(source_script), "%s../../../tools/export-project.ps1",
                       GetApplicationDirectory());
        (void)snprintf(script, sizeof(script), "%s", source_script);
    }
    (void)snprintf(output, sizeof(output), "%s/../exports", studio->project.root);
    intptr_t process =
        _spawnlp(_P_NOWAIT, "powershell.exe", "powershell.exe", "-NoProfile", "-WindowStyle",
                 "Hidden", "-ExecutionPolicy", "Bypass", "-File", script, "-Project",
                 studio->project.manifest, "-Output", output, nullptr);
    if (process == -1) {
        status(studio, "No se pudo iniciar la exportación");
        return;
    }
    status(studio, "Exportación iniciada en la carpeta exports del proyecto");
#else
    status(studio, "La exportación gráfica está validada para Windows");
#endif
}

static void procedural_art(Studio *studio) {
    static const RePixel colors[] = {{84, 101, 109, 255}, {51, 61, 67, 255}, {37, 44, 50, 255},
                                     {194, 106, 52, 255}, {74, 92, 99, 255}, {79, 162, 154, 255}};
    for (int i = 0; i < RE_MAX_MATERIALS; i++) {
        (void)re_texture_init(&studio->materials[i], 8, 8);
        for (size_t p = 0; p < 64; p++)
            studio->materials[i].pixels[p] =
                colors[(size_t)i % (sizeof(colors) / sizeof(colors[0]))];
    }
    (void)re_texture_init(&studio->actor_sprite, 24, 40);
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 24; x++) {
            bool body = x > 4 && x < 20 && y > 3 && y < 38;
            studio->actor_sprite.pixels[(size_t)y * 24u + (size_t)x] =
                body ? re_rgba(210, 70, 67, 255) : re_rgba(0, 0, 0, 0);
        }
}

static void start_play(Studio *studio) {
    studio->runtime_world = studio->project.world;
    re_gameplay_init(&studio->gameplay, &studio->runtime_world, studio->project.characters,
                     studio->project.character_count, 0xC0FFEEu);
    memset(studio->actor_ids, 0, sizeof(studio->actor_ids));
    const ReMarker *player = nullptr;
    for (size_t i = 0; i < studio->runtime_world.marker_count; i++) {
        const ReMarker *marker = &studio->runtime_world.markers[i];
        if (strcmp(marker->kind, "player") == 0)
            player = marker;
        if (strcmp(marker->kind, "actor") == 0)
            for (size_t d = 0; d < studio->project.character_count; d++)
                if (strcmp(marker->definition, studio->project.characters[d].id) == 0) {
                    ReEntityId id = re_gameplay_spawn(&studio->gameplay, d, marker->position,
                                                      marker->yaw, marker->sector);
                    if (id.index != UINT16_MAX)
                        (void)snprintf(studio->actor_ids[id.index],
                                       sizeof(studio->actor_ids[id.index]), "%s", marker->id);
                }
    }
    if (player)
        studio->player = (ReBody){.position = player->position,
                                  .radius = .28f,
                                  .height = 1.72f,
                                  .step_height = .3f,
                                  .sector = player->sector,
                                  .grounded = true};
    else {
        const ReSector *first = &studio->runtime_world.sectors[0];
        ReVec2 center = {0};
        for (size_t i = 0; i < first->count; i++)
            center = re_add2(center, first->vertices[i]);
        center = re_scale2(center, 1.0f / (float)first->count);
        studio->player = (ReBody){.position = {center.x, center.y, first->floor},
                                  .radius = .28f,
                                  .height = 1.72f,
                                  .step_height = .3f,
                                  .sector = 0,
                                  .grounded = true};
    }
    studio->camera = (ReCamera){.position = re_add3(studio->player.position, re_v3(0, 0, 1.55f)),
                                .yaw = player ? player->yaw : 0,
                                .fov = 82 * RE_PI / 180.0f,
                                .near_plane = .05f,
                                .far_plane = 80};
    re_interaction_init(&studio->interaction, &studio->project.interactions, &studio->runtime_world,
                        &studio->gameplay, 100, 3);
    studio->playing = true;
    studio->preview = true;
    status(studio, "PROBANDO: WASD, ratón derecho para mirar, Esc para detener");
}

static void tick_play(Studio *studio) {
    float dt = fminf(GetFrameTime(), 1.0f / 30.0f);
    if (IsKeyPressed(KEY_ESCAPE)) {
        studio->playing = false;
        status(studio, "Prueba detenida: la edición permanece intacta");
        return;
    }
    const ReDialogueNode *dialogue = re_interaction_dialogue(&studio->interaction);
    if (dialogue && dialogue->pauses_world) {
        if (IsKeyPressed(KEY_ENTER))
            (void)re_interaction_choose(&studio->interaction, 0);
        re_interaction_tick(&studio->interaction, &studio->player, dt);
        studio->elapsed += dt;
        return;
    }
    Vector2 look = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? GetMouseDelta() : (Vector2){0};
    studio->camera.yaw += look.x * .003f;
    studio->camera.pitch =
        re_clamp(studio->camera.pitch - look.y * .003f, -85 * RE_PI / 180.0f, 85 * RE_PI / 180.0f);
    ReVec2 forward = re_v2(sinf(studio->camera.yaw), cosf(studio->camera.yaw));
    ReVec2 right = re_v2(cosf(studio->camera.yaw), -sinf(studio->camera.yaw));
    ReVec2 movement = {0};
    if (IsKeyDown(KEY_W))
        movement = re_add2(movement, forward);
    if (IsKeyDown(KEY_S))
        movement = re_sub2(movement, forward);
    if (IsKeyDown(KEY_D))
        movement = re_add2(movement, right);
    if (IsKeyDown(KEY_A))
        movement = re_sub2(movement, right);
    if (re_length2(movement) > 1)
        movement = re_normalize2(movement);
    re_body_move(&studio->runtime_world, &studio->player, re_scale2(movement, 3.4f * dt), dt, 18);
    studio->camera.position = re_add3(studio->player.position, re_v3(0, 0, 1.55f));
    if (IsKeyPressed(KEY_E))
        (void)re_interaction_interact(&studio->interaction, studio->camera.position,
                                      re_camera_forward(&studio->camera), 2.2f);
    if (IsKeyPressed(KEY_ENTER) && dialogue)
        (void)re_interaction_choose(&studio->interaction, 0);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ReVec3 direction = re_camera_forward(&studio->camera);
        ReTraceHit hit = re_world_trace(&studio->runtime_world, studio->camera.position, direction,
                                        30, RE_BLOCK_PROJECTILE);
        int selected = -1;
        float nearest = hit.distance;
        for (size_t i = 0; i < RE_MAX_ENTITIES; i++) {
            ReCharacter *actor = &studio->gameplay.characters[i];
            if (!actor->active || actor->state == RE_CHARACTER_DEAD)
                continue;
            const ReCharacterDef *definition = &studio->project.characters[actor->definition];
            ReVec3 center = re_add3(actor->body.position, re_v3(0, 0, definition->height * .5f));
            ReVec3 delta = re_sub3(center, studio->camera.position);
            float along = re_dot3(delta, direction);
            ReVec3 lateral = re_sub3(delta, re_scale3(direction, along));
            float radius = fmaxf(definition->radius, definition->height * .28f);
            if (along > 0 && along < nearest && re_dot3(lateral, lateral) <= radius * radius) {
                selected = (int)i;
                nearest = along;
            }
        }
        if (selected >= 0) {
            ReCharacter *actor = &studio->gameplay.characters[selected];
            bool was_alive = actor->state != RE_CHARACTER_DEAD;
            ReEntityId id = {.index = (uint16_t)selected, .generation = actor->generation};
            (void)re_gameplay_damage(&studio->gameplay, id, 40, .18f);
            ReLogicEvent logic = {.kind = actor->state == RE_CHARACTER_DEAD && was_alive
                                              ? RE_LOGIC_ENTITY_DIED
                                              : RE_LOGIC_ENTITY_DAMAGED,
                                  .position = actor->body.position,
                                  .value = {.kind = RE_VALUE_INT, .as.integer = 40}};
            (void)snprintf(logic.source, sizeof(logic.source), "%s", studio->actor_ids[selected]);
            (void)re_interaction_emit(&studio->interaction, logic);
        } else if (hit.barrier >= 0)
            (void)re_barrier_damage(&studio->runtime_world, (size_t)hit.barrier, 25);
    }
    re_gameplay_tick(&studio->gameplay,
                     (ReGameplayInput){.player = &studio->player,
                                       .player_eye = studio->camera.position,
                                       .player_noise = re_length2(movement) > .1f ? 7 : 0},
                     dt);
    ReGameplayEvent event;
    while (re_gameplay_event(&studio->gameplay, &event)) {
        if (event.kind == RE_EVENT_CAPTURED) {
            const char *source =
                event.source.index < RE_MAX_ENTITIES ? studio->actor_ids[event.source.index] : "*";
            ReLogicEvent captured = {.kind = RE_LOGIC_CAPTURED, .position = event.position};
            (void)snprintf(captured.source, sizeof(captured.source), "%s", source);
            (void)re_interaction_emit(&studio->interaction, captured);
        } else if (event.kind == RE_EVENT_PLAYER_DAMAGE) {
            studio->interaction.state.player_health =
                studio->interaction.state.player_health > event.value
                    ? studio->interaction.state.player_health - event.value
                    : 0;
            if (!studio->interaction.state.player_health)
                (void)re_interaction_emit(
                    &studio->interaction,
                    (ReLogicEvent){.kind = RE_LOGIC_PLAYER_DIED, .source = "player"});
        }
    }
    re_interaction_tick(&studio->interaction, &studio->player, dt);
    studio->elapsed += dt;
    if (studio->interaction.state.game_over) {
        studio->playing = false;
        status(studio, "GAME OVER: la prueba terminó; el documento sigue intacto");
    }
}

static void render_preview(Studio *studio, Rectangle canvas) {
    const ReWorld *world = studio->playing ? &studio->runtime_world : &studio->project.world;
    if (!studio->playing && studio->selected_sector >= 0) {
        const ReSector *sector = &world->sectors[studio->selected_sector];
        ReVec2 center = {0};
        for (size_t i = 0; i < sector->count; i++)
            center = re_add2(center, sector->vertices[i]);
        center = re_scale2(center, 1.0f / (float)sector->count);
        studio->camera.position = re_v3(center.x, center.y - 5, sector->floor + 1.55f);
        studio->camera.yaw = 0;
    }
    re_renderer_clear(&studio->renderer, re_rgba(15, 20, 28, 255));
    re_draw_world(&studio->renderer, &studio->camera, world, studio->materials);
    if (studio->playing)
        for (size_t i = 0; i < RE_MAX_ENTITIES; i++)
            if (studio->gameplay.characters[i].active)
                re_draw_billboard(&studio->renderer, &studio->camera,
                                  studio->gameplay.characters[i].body.position, .8f, 1.75f,
                                  &studio->actor_sprite, 1);
    if (studio->playing) {
        for (size_t i = 0; i < studio->interaction.state.pickup_count; i++)
            if (studio->interaction.state.pickups[i].active)
                re_draw_billboard(&studio->renderer, &studio->camera,
                                  studio->interaction.state.pickups[i].position, .4f, .4f,
                                  &studio->materials[3], 1);
        re_apply_lights(&studio->renderer, &studio->camera, studio->project.interactions.lights,
                        studio->interaction.state.light_enabled,
                        studio->project.interactions.light_count, studio->elapsed);
        re_rect(&studio->renderer, 0, 247, 480, 23, re_rgba(7, 11, 15, 235));
        char hud[96];
        (void)snprintf(hud, sizeof(hud), "PLAYTEST  VIDA %03d  E INTERACTUA  ESC DETIENE",
                       studio->interaction.state.player_health);
        re_text(&studio->renderer, 10, 255, hud, 1, re_rgba(220, 226, 218, 255));
        if (studio->interaction.state.message_time > 0) {
            re_rect(&studio->renderer, 8, 218, 464, 22, re_rgba(8, 12, 17, 225));
            re_text(&studio->renderer, 14, 226, studio->interaction.state.message, 1,
                    re_rgba(240, 178, 86, 255));
        }
        const ReDialogueNode *dialogue_node = re_interaction_dialogue(&studio->interaction);
        if (dialogue_node) {
            re_rect(&studio->renderer, 28, 154, 424, 86, re_rgba(7, 11, 16, 244));
            re_text(&studio->renderer, 42, 164, dialogue_node->speaker, 2,
                    re_rgba(235, 166, 75, 255));
            re_text(&studio->renderer, 42, 188, dialogue_node->text, 1,
                    re_rgba(225, 230, 224, 255));
            const char *prompt = dialogue_node->choice_count ? dialogue_node->choices[0].text
                                                             : "ENTER PARA CONTINUAR";
            re_text(&studio->renderer, 48, 220, prompt, 1, re_rgba(250, 188, 92, 255));
        }
    }
    UpdateTexture(studio->screen, studio->renderer.pixels);
    float scale = fminf(canvas.width / 480.0f, canvas.height / 270.0f);
    Rectangle destination = {canvas.x + (canvas.width - 480 * scale) * .5f,
                             canvas.y + (canvas.height - 270 * scale) * .5f, 480 * scale,
                             270 * scale};
    DrawRectangleRec(canvas, BLACK);
    DrawTexturePro(studio->screen, (Rectangle){0, 0, 480, 270}, destination, (Vector2){0}, 0,
                   WHITE);
}

static const char *event_label(enum ReLogicEventKind event) {
    static const char *const labels[] = {
        "INICIO DE NIVEL", "ENTRAR EN ZONA",  "PERMANECER EN ZONA",  "SALIR DE ZONA",
        "INTERACTUAR",     "ENTIDAD DAÑADA",  "ENTIDAD MUERTA",      "CAPTURA",
        "RECOGER OBJETO",  "USAR OBJETO",     "ELECCIÓN DE DIÁLOGO", "FIN DE DIÁLOGO",
        "OBJETIVO CAMBIA", "TEMPORIZADOR",    "EVENTO DE ANIMACIÓN", "BARRERA CAMBIA",
        "MUERTE JUGADOR",  "PARTIDA CARGADA", "PERSONALIZADO"};
    return event <= RE_LOGIC_CUSTOM ? labels[event] : "DESCONOCIDO";
}

static const char *action_label(enum ReActionKind action) {
    static const char *const labels[] = {"Asignar variable", "Sumar variable",   "Dar objeto",
                                         "Quitar objeto",    "Cambiar objetivo", "Mostrar mensaje",
                                         "Abrir barrera",    "Cerrar barrera",   "Dañar jugador",
                                         "Curar jugador",    "Asignar vidas",    "Crear checkpoint",
                                         "Iniciar diálogo",  "Cambiar luz",      "Crear pickup",
                                         "Emitir evento",    "Victoria",         "Game over"};
    return action <= RE_RULE_GAME_OVER ? labels[action] : "Acción desconocida";
}

static void draw_logic_workspace(Studio *studio, Rectangle canvas) {
    DrawRectangleRec(canvas, (Color){20, 25, 31, 255});
    DrawText("LÓGICA DEL PROYECTO", (int)canvas.x + 24, (int)canvas.y + 22, 20, RAYWHITE);
    DrawText("CUANDO ocurre un evento, SI se cumplen condiciones, HACER acciones en orden.",
             (int)canvas.x + 24, (int)canvas.y + 50, 12, GRAY);
    float y = canvas.y + 82;
    for (size_t i = 0;
         i < studio->project.interactions.rule_count && y < canvas.y + canvas.height - 52;
         i++, y += 42) {
        const ReRuleDefinition *rule = &studio->project.interactions.rules[i];
        Rectangle card = {canvas.x + 20, y, canvas.width - 40, 34};
        DrawRectangleRec(card, (int)i == studio->selected_rule ? (Color){91, 67, 39, 255}
                                                               : (Color){30, 38, 46, 255});
        DrawRectangleLinesEx(card, 1, (Color){70, 86, 95, 255});
        DrawText(rule->id, (int)card.x + 10, (int)card.y + 6, 13, RAYWHITE);
        DrawText(TextFormat("CUANDO %s  ·  ORIGEN %s", event_label(rule->event), rule->source),
                 (int)card.x + 185, (int)card.y + 7, 11, LIGHTGRAY);
        DrawText(TextFormat("%u SI  /  %u HACER", (unsigned int)rule->condition_count,
                            (unsigned int)rule->action_count),
                 (int)(card.x + card.width - 135), (int)card.y + 7, 10, ORANGE);
        if (CheckCollisionPointRec(GetMousePosition(), card) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            studio->selected_rule = (int)i;
    }
}

static void draw_dialogue_workspace(Studio *studio, Rectangle canvas) {
    DrawRectangleRec(canvas, (Color){20, 25, 31, 255});
    DrawText("DIÁLOGOS RAMIFICADOS", (int)canvas.x + 24, (int)canvas.y + 22, 20, RAYWHITE);
    DrawText("Cada tarjeta es un nodo; sus opciones apuntan al siguiente ID.", (int)canvas.x + 24,
             (int)canvas.y + 50, 12, GRAY);
    float y = canvas.y + 82;
    for (size_t i = 0; i < studio->project.interactions.dialogue_count; i++, y += 106) {
        const ReDialogueNode *node = &studio->project.interactions.dialogues[i];
        Rectangle card = {canvas.x + 20, y, canvas.width - 40, 96};
        DrawRectangleRec(card, (int)i == studio->selected_dialogue ? (Color){63, 56, 48, 255}
                                                                   : (Color){30, 38, 46, 255});
        DrawRectangleLinesEx(card, 1, (Color){79, 94, 101, 255});
        DrawText(node->id, (int)card.x + 10, (int)card.y + 8, 13, ORANGE);
        DrawText(node->speaker, (int)card.x + 170, (int)card.y + 8, 13, RAYWHITE);
        DrawText(node->text, (int)card.x + 10, (int)card.y + 32, 11, LIGHTGRAY);
        DrawText(TextFormat("SIGUIENTE %s  ·  %u OPCIONES  ·  %s", node->next,
                            (unsigned int)node->choice_count,
                            node->pauses_world ? "PAUSA" : "TIEMPO REAL"),
                 (int)card.x + 10, (int)card.y + 72, 10, GRAY);
        if (CheckCollisionPointRec(GetMousePosition(), card) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            studio->selected_dialogue = (int)i;
    }
}

static void draw_help(Studio *studio) {
    static const struct {
        const char *title;
        const char *lines[4];
    } pages[] = {
        {"1. HABITACION Y ESCALERA",
         {"En MAPA usa + SALA y arrastra vertices.", "CONECTAR une aristas opuestas.",
          "PELDAÑO crea un volumen 25 cm mas alto.", "Repite peldaños hasta la otra planta."}},
        {"2. SPRITE Y ANIMACION",
         {"Selecciona un personaje en ENTIDADES.", "Arrastra un PNG para copiarlo a art/.",
          "Ajusta celda, frame y duracion en Inspector.",
          "Las direcciones admitidas son 1, 4 u 8."}},
        {"3. CONVERSACION",
         {"En DIALOGO crea un nodo por parlamento.", "Cada opcion apunta al ID del siguiente nodo.",
          "Una variable puede ocultar una opcion.", "PROBAR ejecuta el mismo runtime del juego."}},
        {"4. ENEMIGO QUE SUELTA LLAVE",
         {"Crea una regla CUANDO ENTIDAD MUERTA.", "Usa el ID estable del actor como origen.",
          "Añade HACER Crear pickup y marca una vez.", "Otra regla reacciona a RECOGER OBJETO."}},
        {"5. LLAVE, PUERTA Y OBJETIVO",
         {"La regla de puerta comprueba el inventario.", "Luego quita la llave y abre la barrera.",
          "Completa el objetivo en la misma transaccion.",
          "Prioridad resuelve reglas simultaneas."}},
        {"6. PERSEGUIDORA, CHECKPOINT Y JEFE",
         {"Elige percepcion o seguimiento constante.", "Captura emite un solo evento bloqueable.",
          "Checkpoint guarda toda la simulacion.", "Las fases del jefe cambian por vida."}},
        {"7. GUARDAR, CARGAR Y EXPORTAR",
         {"Ctrl+S valida y guarda de forma segura.", "Autosave se recupera sin tocar el original.",
          "El Player ofrece ranuras, F5 y F9.", "EXPORTAR crea carpeta y ZIP portables."}},
    };
    if (!studio->help_open)
        return;
    int width = GetScreenWidth(), height = GetScreenHeight();
    DrawRectangle(0, 0, width, height, (Color){4, 7, 10, 190});
    Rectangle dialog = {(float)width * .5f - 260, (float)height * .5f - 170, 520, 340};
    DrawRectangleRec(dialog, (Color){25, 31, 38, 255});
    DrawRectangleLinesEx(dialog, 2, (Color){226, 145, 61, 255});
    const size_t page_count = sizeof(pages) / sizeof(pages[0]);
    size_t page = (size_t)studio->tutorial_step;
    DrawText("TUTORIAL INTERACTIVO", (int)dialog.x + 26, (int)dialog.y + 24, 14, ORANGE);
    DrawText(pages[page].title, (int)dialog.x + 26, (int)dialog.y + 62, 21, RAYWHITE);
    for (int i = 0; i < 4; i++) {
        DrawCircle((int)dialog.x + 34, (int)dialog.y + 113 + i * 36, 4, ORANGE);
        DrawText(pages[page].lines[i], (int)dialog.x + 50, (int)dialog.y + 105 + i * 36, 14,
                 LIGHTGRAY);
    }
    DrawText(TextFormat("PASO %u DE %u", (unsigned int)page + 1u, (unsigned int)page_count),
             (int)dialog.x + 26, (int)(dialog.y + dialog.height - 53), 11, GRAY);
    if (GuiButton((Rectangle){dialog.x + 210, dialog.y + dialog.height - 66, 84, 32}, "ANTERIOR") &&
        studio->tutorial_step > 0)
        studio->tutorial_step--;
    if (GuiButton((Rectangle){dialog.x + 300, dialog.y + dialog.height - 66, 84, 32},
                  "SIGUIENTE") &&
        studio->tutorial_step + 1 < (int)page_count)
        studio->tutorial_step++;
    if (GuiButton((Rectangle){dialog.x + 390, dialog.y + dialog.height - 66, 102, 32}, "CERRAR"))
        studio->help_open = false;
}

static void sidebar_left(Studio *studio, Rectangle panel) {
    GuiPanel(panel, "PROYECTO");
    DrawText(studio->project.name, (int)panel.x + 12, (int)panel.y + 30, 18, RAYWHITE);
    static const char *const section_names[] = {"HABITACIONES", "ENTIDADES", "REGLAS",
                                                "CONVERSACIONES", "DIAGNOSTICO"};
    DrawText(section_names[studio->workspace], (int)panel.x + 12, (int)panel.y + 64, 11, GRAY);
    float y = panel.y + 82;
    for (size_t i = 0; studio->workspace == 0 && i < studio->project.world.sector_count &&
                       y < panel.y + panel.height - 190;
         i++, y += 24) {
        Rectangle row = {panel.x + 8, y, panel.width - 16, 21};
        if ((int)i == studio->selected_sector)
            DrawRectangleRec(row, (Color){103, 77, 45, 255});
        if (GuiLabelButton(row, TextFormat("Sector %u  [z %.2f]", (unsigned int)i,
                                           (double)studio->project.world.sectors[i].floor))) {
            studio->selected_sector = (int)i;
            studio->floor = studio->project.world.sectors[i].floor;
        }
    }
    if (studio->workspace == 1) {
        for (size_t i = 0; i < studio->project.character_count && y < panel.y + panel.height - 100;
             i++, y += 25) {
            Rectangle row = {panel.x + 8, y, panel.width - 16, 22};
            if ((int)i == studio->selected_actor)
                DrawRectangleRec(row, (Color){103, 77, 45, 255});
            if (GuiLabelButton(row, studio->project.characters[i].id))
                studio->selected_actor = (int)i;
        }
        y += 10;
        DrawText("INSTANCIAS DEL MAPA", (int)panel.x + 12, (int)y, 10, GRAY);
        y += 20;
        for (size_t i = 0;
             i < studio->project.world.marker_count && y < panel.y + panel.height - 100; i++) {
            const ReMarker *marker = &studio->project.world.markers[i];
            if (strcmp(marker->kind, "actor") != 0 && strcmp(marker->kind, "npc") != 0)
                continue;
            DrawText(TextFormat("%s  |  %s", marker->id, marker->definition), (int)panel.x + 12,
                     (int)y, 10, LIGHTGRAY);
            y += 22;
        }
    } else if (studio->workspace == 2) {
        for (size_t i = 0;
             i < studio->project.interactions.rule_count && y < panel.y + panel.height - 100;
             i++, y += 25) {
            Rectangle row = {panel.x + 8, y, panel.width - 16, 22};
            if ((int)i == studio->selected_rule)
                DrawRectangleRec(row, (Color){103, 77, 45, 255});
            if (GuiLabelButton(row, studio->project.interactions.rules[i].id))
                studio->selected_rule = (int)i;
        }
    } else if (studio->workspace == 3) {
        for (size_t i = 0;
             i < studio->project.interactions.dialogue_count && y < panel.y + panel.height - 100;
             i++, y += 25) {
            Rectangle row = {panel.x + 8, y, panel.width - 16, 22};
            if ((int)i == studio->selected_dialogue)
                DrawRectangleRec(row, (Color){103, 77, 45, 255});
            if (GuiLabelButton(row, studio->project.interactions.dialogues[i].id))
                studio->selected_dialogue = (int)i;
        }
    } else if (studio->workspace == 4) {
        DrawText("PLAYTEST AISLADO", (int)panel.x + 12, (int)y, 13, RAYWHITE);
        DrawText("La partida usa una copia del documento.", (int)panel.x + 12, (int)y + 28, 10,
                 LIGHTGRAY);
        DrawText("Detener restaura el estado de edicion.", (int)panel.x + 12, (int)y + 46, 10,
                 LIGHTGRAY);
        DrawText("OVERLAYS", (int)panel.x + 12, (int)y + 82, 10, GRAY);
        DrawText("F2  colisiones", (int)panel.x + 12, (int)y + 104, 10, LIGHTGRAY);
        DrawText("F3  rutas y portales", (int)panel.x + 12, (int)y + 124, 10, LIGHTGRAY);
        DrawText("F4  variables y objetivos", (int)panel.x + 12, (int)y + 144, 10, LIGHTGRAY);
    }
    if (studio->workspace == 0) {
        if (GuiButton((Rectangle){panel.x + 8, panel.y + panel.height - 174, 68, 28}, "+ SALA"))
            add_room(studio);
        if (GuiButton((Rectangle){panel.x + 82, panel.y + panel.height - 174, 82, 28}, "DUPLICAR"))
            duplicate_room(studio);
        if (GuiButton((Rectangle){panel.x + 170, panel.y + panel.height - 174, 58, 28}, "APILAR"))
            stack_room(studio);
        if (GuiButton((Rectangle){panel.x + 8, panel.y + panel.height - 140, 68, 28}, "PELDAÑO"))
            add_step(studio);
        if (GuiButton((Rectangle){panel.x + 82, panel.y + panel.height - 140, 82, 28}, "CONECTAR"))
            link_edge(studio);
        if (GuiButton((Rectangle){panel.x + 170, panel.y + panel.height - 140, 58, 28}, "ACTOR"))
            add_actor_marker(studio);
        if (GuiButton((Rectangle){panel.x + 8, panel.y + panel.height - 106, 104, 28}, "PUERTA"))
            add_barrier(studio, RE_BARRIER_DOOR);
        if (GuiButton((Rectangle){panel.x + 118, panel.y + panel.height - 106, 110, 28}, "VENTANA"))
            add_barrier(studio, RE_BARRIER_WINDOW);
    }
    if (GuiButton((Rectangle){panel.x + 8, panel.y + panel.height - 58, 68, 30}, "GUARDAR"))
        save_project(studio);
    if (GuiButton((Rectangle){panel.x + 82, panel.y + panel.height - 58, 68, 30},
                  studio->playing ? "DETENER" : "PROBAR")) {
        if (studio->playing) {
            studio->playing = false;
            status(studio, "Prueba detenida");
        } else
            start_play(studio);
    }
    if (GuiButton((Rectangle){panel.x + 156, panel.y + panel.height - 58, 72, 30}, "EXPORTAR"))
        export_project(studio);
}

static void sidebar_right(Studio *studio, Rectangle panel) {
    GuiPanel(panel, "INSPECTOR");
    float x = panel.x + 12, y = panel.y + 34, width = panel.width - 24;
    if (studio->workspace == 2) {
        ReInteractionDefinitions *definitions = &studio->project.interactions;
        DrawText("REGLA SELECCIONADA", (int)x, (int)y, 12, GRAY);
        if (definitions->rule_count < RE_MAX_RULES &&
            GuiButton((Rectangle){x + width - 92, y - 6, 92, 24}, "+ REGLA")) {
            size_t index = definitions->rule_count++;
            ReRuleDefinition *created = &definitions->rules[index];
            *created = (ReRuleDefinition){
                .event = RE_LOGIC_CUSTOM, .priority = 50, .source = "*", .action_count = 1};
            (void)snprintf(created->id, sizeof(created->id), "rule_%zu", index + 1u);
            created->actions[0] =
                (ReRuleAction){.kind = RE_RULE_SHOW_MESSAGE,
                               .target = "-",
                               .value = {.kind = RE_VALUE_TEXT, .as.text = "Nueva regla"}};
            studio->selected_rule = (int)index;
            studio->dirty = true;
        }
        y += 30;
        if (studio->selected_rule >= 0 && studio->selected_rule < (int)definitions->rule_count) {
            ReRuleDefinition *rule = &definitions->rules[studio->selected_rule];
            DrawText(rule->id, (int)x, (int)y, 18, RAYWHITE);
            y += 34;
            DrawText("CUANDO", (int)x, (int)y, 10, GRAY);
            y += 15;
            if (GuiButton((Rectangle){x, y, width, 26}, event_label(rule->event))) {
                rule->event = (enum ReLogicEventKind)((rule->event + 1) % (RE_LOGIC_CUSTOM + 1));
                studio->dirty = true;
            }
            y += 36;
            DrawText(TextFormat("ORIGEN: %s", rule->source), (int)x, (int)y, 11, LIGHTGRAY);
            y += 25;
            if (GuiCheckBox((Rectangle){x, y, 18, 18}, "Sólo una vez", &rule->once))
                studio->dirty = true;
            y += 30;
            float priority = (float)rule->priority;
            DrawText(TextFormat("PRIORIDAD %d", rule->priority), (int)x, (int)y, 10, LIGHTGRAY);
            y += 13;
            GuiSlider((Rectangle){x, y, width, 15}, nullptr, nullptr, &priority, 0, 200);
            int new_priority = (int)roundf(priority);
            if (new_priority != rule->priority) {
                rule->priority = new_priority;
                studio->dirty = true;
            }
            y += 28;
            DrawText(TextFormat("COOLDOWN %.2fs", (double)rule->cooldown), (int)x, (int)y, 10,
                     LIGHTGRAY);
            y += 13;
            float previous = rule->cooldown;
            GuiSlider((Rectangle){x, y, width, 15}, nullptr, nullptr, &rule->cooldown, 0, 10);
            studio->dirty |= previous != rule->cooldown;
            y += 32;
            DrawText(TextFormat("SI  (%u)", (unsigned int)rule->condition_count), (int)x, (int)y,
                     11, GRAY);
            y += 20;
            for (size_t i = 0; i < rule->condition_count && i < 4; i++, y += 18)
                DrawText(TextFormat("• %s", rule->conditions[i].key), (int)x, (int)y, 11,
                         LIGHTGRAY);
            y += 8;
            DrawText(TextFormat("HACER  (%u)", (unsigned int)rule->action_count), (int)x, (int)y,
                     11, GRAY);
            y += 20;
            for (size_t i = 0; i < rule->action_count && i < 6; i++, y += 18)
                DrawText(TextFormat("%u. %s → %s", (unsigned int)i + 1u,
                                    action_label(rule->actions[i].kind), rule->actions[i].target),
                         (int)x, (int)y, 10, LIGHTGRAY);
            if (rule->action_count < RE_MAX_RULE_ACTIONS &&
                GuiButton((Rectangle){x, panel.y + panel.height - 42, width, 26}, "+ ACCIÓN")) {
                rule->actions[rule->action_count++] =
                    (ReRuleAction){.kind = RE_RULE_SHOW_MESSAGE,
                                   .target = "-",
                                   .value = {.kind = RE_VALUE_TEXT, .as.text = "Nueva acción"}};
                studio->dirty = true;
            }
        }
        return;
    }
    if (studio->workspace == 3) {
        ReInteractionDefinitions *definitions = &studio->project.interactions;
        DrawText("NODO DE DIÁLOGO", (int)x, (int)y, 12, GRAY);
        if (definitions->dialogue_count < RE_MAX_DIALOGUE_NODES &&
            GuiButton((Rectangle){x + width - 88, y - 6, 88, 24}, "+ NODO")) {
            size_t index = definitions->dialogue_count++;
            ReDialogueNode *node = &definitions->dialogues[index];
            *node = (ReDialogueNode){.pauses_world = true, .next = "end"};
            (void)snprintf(node->id, sizeof(node->id), "dialogue_%zu", index + 1u);
            (void)snprintf(node->speaker, sizeof(node->speaker), "Personaje");
            (void)snprintf(node->text, sizeof(node->text), "Nueva línea de diálogo");
            studio->selected_dialogue = (int)index;
            studio->dirty = true;
        }
        y += 34;
        if (studio->selected_dialogue >= 0 &&
            studio->selected_dialogue < (int)definitions->dialogue_count) {
            ReDialogueNode *node = &definitions->dialogues[studio->selected_dialogue];
            DrawText(node->id, (int)x, (int)y, 18, RAYWHITE);
            y += 34;
            DrawText("HABLANTE", (int)x, (int)y, 10, GRAY);
            y += 16;
            DrawText(node->speaker, (int)x, (int)y, 14, ORANGE);
            y += 30;
            DrawText("TEXTO", (int)x, (int)y, 10, GRAY);
            y += 17;
            DrawText(node->text, (int)x, (int)y, 11, LIGHTGRAY);
            y += 54;
            if (GuiCheckBox((Rectangle){x, y, 18, 18}, "Pausa el mundo", &node->pauses_world))
                studio->dirty = true;
            y += 34;
            DrawText(TextFormat("SIGUIENTE: %s", node->next), (int)x, (int)y, 11, LIGHTGRAY);
            y += 30;
            DrawText(TextFormat("OPCIONES (%u)", (unsigned int)node->choice_count), (int)x, (int)y,
                     11, GRAY);
            y += 22;
            for (size_t i = 0; i < node->choice_count; i++, y += 34) {
                DrawText(TextFormat("• %s", node->choices[i].text), (int)x, (int)y, 10, LIGHTGRAY);
                DrawText(TextFormat("  → %s", node->choices[i].next), (int)x, (int)y + 14, 9, GRAY);
            }
        }
        return;
    }
    if (studio->workspace == 4) {
        DrawText("SESION DE PRUEBA", (int)x, (int)y, 16, RAYWHITE);
        y += 34;
        DrawText(studio->playing ? "Estado: ejecutando copia aislada" : "Estado: detenida", (int)x,
                 (int)y, 11, studio->playing ? GREEN : LIGHTGRAY);
        y += 28;
        DrawText(TextFormat("Sectores        %u", (unsigned int)studio->project.world.sector_count),
                 (int)x, (int)y, 11, LIGHTGRAY);
        y += 20;
        DrawText(TextFormat("Entidades       %u", (unsigned int)studio->project.world.marker_count),
                 (int)x, (int)y, 11, LIGHTGRAY);
        y += 20;
        DrawText(
            TextFormat("Reglas          %u", (unsigned int)studio->project.interactions.rule_count),
            (int)x, (int)y, 11, LIGHTGRAY);
        y += 20;
        DrawText(TextFormat("Luces           %u",
                            (unsigned int)studio->project.interactions.light_count),
                 (int)x, (int)y, 11, LIGHTGRAY);
        y += 38;
        DrawText("La consola y los overlays usan el mismo", (int)x, (int)y, 10, GRAY);
        DrawText("estado determinista que el juego exportado.", (int)x, (int)y + 17, 10, GRAY);
        return;
    }
    if (studio->workspace == 0 && studio->selected_sector >= 0) {
        ReSector *sector = &studio->project.world.sectors[studio->selected_sector];
        DrawText(TextFormat("SECTOR %d", studio->selected_sector), (int)x, (int)y, 16, RAYWHITE);
        y += 34;
        float old_floor = sector->floor, old_ceiling = sector->ceiling, old_light = sector->light;
        DrawText(TextFormat("SUELO                         %.2f", (double)sector->floor), (int)x,
                 (int)y, 10, LIGHTGRAY);
        y += 14;
        GuiSlider((Rectangle){x, y, width, 16}, nullptr, nullptr, &sector->floor, -4, 12);
        y += 26;
        DrawText(TextFormat("TECHO                         %.2f", (double)sector->ceiling), (int)x,
                 (int)y, 10, LIGHTGRAY);
        y += 14;
        GuiSlider((Rectangle){x, y, width, 16}, nullptr, nullptr, &sector->ceiling, -1, 16);
        y += 26;
        DrawText(TextFormat("LUZ                           %.2f", (double)sector->light), (int)x,
                 (int)y, 10, LIGHTGRAY);
        y += 14;
        GuiSlider((Rectangle){x, y, width, 16}, nullptr, nullptr, &sector->light, 0, 1);
        if ((old_floor != sector->floor || old_ceiling != sector->ceiling ||
             old_light != sector->light) &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            studio->floor = sector->floor;
            history_push(studio);
        }
        y += 32;
        if (studio->selected_vertex >= 0 && studio->selected_vertex < (int)sector->count &&
            sector->neighbor[studio->selected_vertex] >= 0) {
            int edge = studio->selected_vertex;
            float old_start = sector->portal_start[edge], old_end = sector->portal_end[edge];
            DrawText(TextFormat("ABERTURA %.0f%% - %.0f%%", (double)(old_start * 100),
                                (double)(old_end * 100)),
                     (int)x, (int)y, 10, LIGHTGRAY);
            y += 14;
            GuiSlider((Rectangle){x, y, width, 14}, nullptr, nullptr, &sector->portal_start[edge],
                      0, .9f);
            y += 19;
            GuiSlider((Rectangle){x, y, width, 14}, nullptr, nullptr, &sector->portal_end[edge],
                      .1f, 1);
            if (sector->portal_end[edge] - sector->portal_start[edge] < .1f)
                sector->portal_end[edge] = fminf(1, sector->portal_start[edge] + .1f);
            if (old_start != sector->portal_start[edge] || old_end != sector->portal_end[edge]) {
                int neighbor = sector->neighbor[edge];
                int other_edge = sector->neighbor_edge[edge];
                ReSector *other = &studio->project.world.sectors[neighbor];
                other->portal_start[other_edge] = 1 - sector->portal_end[edge];
                other->portal_end[other_edge] = 1 - sector->portal_start[edge];
                studio->dirty = true;
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                    history_push(studio);
            }
            y += 26;
        } else
            y += 14;
        DrawText("VÉRTICES", (int)x, (int)y, 11, GRAY);
        y += 22;
        for (size_t i = 0; i < sector->count; i++, y += 20)
            DrawText(TextFormat("%u   %.2f, %.2f", (unsigned int)i, (double)sector->vertices[i].x,
                                (double)sector->vertices[i].y),
                     (int)x, (int)y, 12, LIGHTGRAY);
    } else if (studio->workspace == 0) {
        DrawText("Selecciona una habitación", (int)x, (int)y, 14, GRAY);
    }
    if (studio->workspace != 1)
        return;
    y = panel.y + 34;
    DrawText("PERSONAJES", (int)x, (int)y, 11, GRAY);
    if (GuiButton((Rectangle){panel.x + panel.width - 94, y - 5, 82, 22}, "+ PERSONAJE"))
        duplicate_character(studio);
    y += 20;
    for (size_t i = 0; i < studio->project.character_count && i < 3; i++, y += 24) {
        Rectangle row = {x, y, width, 21};
        if (GuiLabelButton(row, studio->project.characters[i].id))
            studio->selected_actor = (int)i;
    }
    if (studio->selected_actor >= 0 &&
        studio->selected_actor < (int)studio->project.character_count) {
        ReCharacterDef *actor = &studio->project.characters[studio->selected_actor];
        y += 6;
        if (GuiCheckBox((Rectangle){x, y, 18, 18}, "Captura = game over",
                        &actor->capture_game_over))
            studio->dirty = true;
        y += 28;
        bool omniscient = actor->tracking == RE_TRACK_OMNISCIENT;
        if (GuiCheckBox((Rectangle){x, y, 18, 18}, "Seguimiento constante", &omniscient)) {
            actor->tracking = omniscient ? RE_TRACK_OMNISCIENT : RE_TRACK_PERCEPTION;
            studio->dirty = true;
        }
        y += 30;
        float old_speed = actor->speed;
        DrawText(TextFormat("VELOCIDAD                     %.2f", (double)actor->speed), (int)x,
                 (int)y, 10, LIGHTGRAY);
        y += 13;
        GuiSlider((Rectangle){x, y, width, 15}, nullptr, nullptr, &actor->speed, 0, 8);
        if (old_speed != actor->speed)
            studio->dirty = true;
        y += 21;
        if (actor->animation_count) {
            studio->selected_animation = studio->selected_animation % (int)actor->animation_count;
            ReAnimationClip *clip = &actor->animations[studio->selected_animation];
            if (GuiButton((Rectangle){x, y, 22, 19}, "<")) {
                studio->selected_animation =
                    (studio->selected_animation + (int)actor->animation_count - 1) %
                    (int)actor->animation_count;
                studio->selected_frame = 0;
            }
            DrawText(TextFormat("ANIM %s  %u DIR", clip->name, clip->directions), (int)x + 28,
                     (int)y + 5, 10, LIGHTGRAY);
            if (GuiButton((Rectangle){x + width - 22, y, 22, 19}, ">")) {
                studio->selected_animation =
                    (studio->selected_animation + 1) % (int)actor->animation_count;
                studio->selected_frame = 0;
            }
            y += 24;
            clip = &actor->animations[studio->selected_animation];
            if (clip->frame_count) {
                studio->selected_frame %= (int)clip->frame_count;
                ReAnimationFrame *frame = &clip->frames[studio->selected_frame];
                if (GuiButton((Rectangle){x, y, 22, 19}, "<"))
                    studio->selected_frame = (studio->selected_frame + (int)clip->frame_count - 1) %
                                             (int)clip->frame_count;
                DrawText(TextFormat("FRAME %d/%u  CELDA %u", studio->selected_frame + 1,
                                    (unsigned int)clip->frame_count, frame->cell),
                         (int)x + 28, (int)y + 5, 10, LIGHTGRAY);
                if (GuiButton((Rectangle){x + width - 22, y, 22, 19}, ">"))
                    studio->selected_frame = (studio->selected_frame + 1) % (int)clip->frame_count;
                y += 23;
                frame = &clip->frames[studio->selected_frame];
                float old_duration = frame->duration;
                DrawText(TextFormat("DURACIÓN                      %.2fs", (double)frame->duration),
                         (int)x, (int)y, 10, LIGHTGRAY);
                y += 13;
                GuiSlider((Rectangle){x, y, width, 15}, nullptr, nullptr, &frame->duration, .03f,
                          1);
                if (old_duration != frame->duration)
                    studio->dirty = true;
                y += 20;
                if (GuiButton((Rectangle){x, y, 72, 20}, "CELDA -") && frame->cell > 0) {
                    frame->cell--;
                    studio->dirty = true;
                }
                if (GuiButton((Rectangle){x + 78, y, 72, 20}, "CELDA +")) {
                    frame->cell++;
                    studio->dirty = true;
                }
                if (clip->frame_count < RE_MAX_ANIMATION_FRAMES &&
                    GuiButton((Rectangle){x + 156, y, width - 156, 20}, "+ FRAME")) {
                    clip->frames[clip->frame_count] = *frame;
                    clip->frame_count++;
                    studio->selected_frame = (int)clip->frame_count - 1;
                    studio->dirty = true;
                }
            }
        }
    }
}

static bool studio_init(Studio *studio, const char *manifest) {
    *studio = (Studio){.selected_sector = 0,
                       .selected_vertex = -1,
                       .selected_actor = 0,
                       .selected_rule = 0,
                       .selected_dialogue = 0,
                       .link_sector = -1,
                       .link_edge = -1,
                       .zoom = 32,
                       .camera = {.fov = 82 * RE_PI / 180.0f, .near_plane = .05f, .far_plane = 80}};
    ReError error = {0};
    if (!re_project_load(manifest, &studio->project, &error)) {
        (void)fprintf(stderr, "%s:%zu: %s\n", manifest, error.line, error.message);
        return false;
    }
    studio->floor = studio->project.world.sectors[0].floor;
    if (!history_init(&studio->history, &studio->project.world) ||
        !re_renderer_init(&studio->renderer, 480, 270))
        return false;
    procedural_art(studio);
    Image image = {.data = studio->renderer.pixels,
                   .width = 480,
                   .height = 270,
                   .mipmaps = 1,
                   .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    studio->screen = LoadTextureFromImage(image);
    SetTextureFilter(studio->screen, TEXTURE_FILTER_POINT);
    status(studio, "Proyecto listo. Ctrl+S guarda; Ctrl+Z/Y deshace y rehace.");
    char directory[1024], map[1024], rules[1024], dialogue[1024];
    autosave_paths(studio, directory, sizeof(directory), map, sizeof(map), rules, sizeof(rules),
                   dialogue, sizeof(dialogue));
    studio->autosave_available = FileExists(map) && FileExists(rules) && FileExists(dialogue);
    if (studio->autosave_available)
        status(studio, "Existe una copia automática. Usa RECUPERAR para inspeccionarla.");
    return true;
}

static void studio_destroy(Studio *studio) {
    UnloadTexture(studio->screen);
    for (int i = 0; i < RE_MAX_MATERIALS; i++)
        re_texture_destroy(&studio->materials[i]);
    re_texture_destroy(&studio->actor_sprite);
    re_renderer_destroy(&studio->renderer);
    free(studio->history.states);
}

int main(int argc, char **argv) {
    const char *manifest_argument = nullptr;
    const char *capture_path = nullptr;
    bool play_on_start = false, tutorial_on_start = false;
    int requested_workspace = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
            capture_path = argv[++i];
        else if (strcmp(argv[i], "--play") == 0)
            play_on_start = true;
        else if (strcmp(argv[i], "--tutorial") == 0)
            tutorial_on_start = true;
        else if (strcmp(argv[i], "--workspace") == 0 && i + 1 < argc)
            requested_workspace = atoi(argv[++i]);
        else
            manifest_argument = argv[i];
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "RetroForge Studio");
    SetExitKey(KEY_NULL);
    char default_manifest[1024];
    if (manifest_argument)
        (void)snprintf(default_manifest, sizeof(default_manifest), "%s", manifest_argument);
    else
        (void)snprintf(default_manifest, sizeof(default_manifest), "%sassets/studio/haunted.retro",
                       GetApplicationDirectory());
    Studio *studio = calloc(1, sizeof(*studio));
    if (!studio || !studio_init(studio, default_manifest)) {
        free(studio);
        CloseWindow();
        return 1;
    }
    if (play_on_start)
        start_play(studio);
    studio->help_open = tutorial_on_start;
    studio->workspace =
        requested_workspace >= 0 && requested_workspace <= 4 ? requested_workspace : 0;
    GuiSetStyle(DEFAULT, TEXT_SIZE, 13);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt((Color){19, 24, 30, 255}));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt((Color){30, 38, 46, 255}));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt((Color){53, 67, 76, 255}));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt((Color){191, 121, 58, 255}));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt((Color){72, 88, 96, 255}));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt((Color){236, 158, 72, 255}));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt((Color){205, 215, 215, 255}));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(RAYWHITE));
    int capture_frames = 0;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F1))
            studio->help_open = !studio->help_open;
        import_dropped(studio);
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S))
            save_project(studio);
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z))
            history_move(studio, -1);
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y))
            history_move(studio, 1);
        if (studio->playing)
            tick_play(studio);
        else if (studio->dirty) {
            studio->autosave_timer += GetFrameTime();
            if (studio->autosave_timer >= 30)
                autosave_project(studio);
        }
        int width = GetScreenWidth(), height = GetScreenHeight();
        Rectangle left = {0, 42, 240, (float)height - 76};
        Rectangle right = {(float)width - 300, 42, 300, (float)height - 76};
        Rectangle canvas = {240, 42, (float)width - 540, (float)height - 76};
        if ((studio->workspace == 0 || studio->workspace == 1) && !studio->preview)
            edit_plan(studio, canvas);
        BeginDrawing();
        ClearBackground((Color){14, 18, 23, 255});
        DrawRectangle(0, 0, width, 42, (Color){27, 33, 40, 255});
        DrawText("RETROFORGE STUDIO", 14, 12, 18, RAYWHITE);
        if (GuiButton((Rectangle){(float)width - 86, 8, 74, 26}, "? AYUDA"))
            studio->help_open = true;
        DrawText(TextFormat("PISO %.2f", (double)studio->floor), 242, 14, 14, ORANGE);
        if (GuiButton((Rectangle){330, 8, 32, 26}, "-"))
            studio->floor -= .25f;
        if (GuiButton((Rectangle){366, 8, 32, 26}, "+"))
            studio->floor += .25f;
        static const char *const workspaces[] = {"MAPA", "ENTIDADES", "LÓGICA", "DIÁLOGO",
                                                 "PRUEBA"};
        float workspace_x = 404;
        for (int i = 0; i < 5; i++) {
            float button_width = i == 1 ? 86 : 70;
            if (GuiButton((Rectangle){workspace_x, 8, button_width, 26}, workspaces[i])) {
                studio->workspace = i;
                studio->preview = i == 4;
            }
            workspace_x += button_width + 4;
        }
        if ((studio->workspace == 0 || studio->workspace == 1) &&
            GuiButton((Rectangle){workspace_x + 4, 8, 58, 26}, studio->preview ? "2D" : "3D"))
            studio->preview = !studio->preview;
        if (studio->autosave_available &&
            GuiButton((Rectangle){workspace_x + 66, 8, 108, 26}, "RECUPERAR"))
            recover_autosave(studio);
        if (studio->dirty)
            DrawText("CAMBIOS SIN GUARDAR", width - 490, 14, 12, GOLD);
        sidebar_left(studio, left);
        sidebar_right(studio, right);
        if (studio->workspace == 2)
            draw_logic_workspace(studio, canvas);
        else if (studio->workspace == 3)
            draw_dialogue_workspace(studio, canvas);
        else if (studio->preview || studio->workspace == 4)
            render_preview(studio, canvas);
        else
            draw_plan(studio, canvas);
        DrawRectangle(0, height - 34, width, 34, (Color){27, 33, 40, 255});
        DrawText(studio->status, 12, height - 23, 13, LIGHTGRAY);
        draw_help(studio);
        EndDrawing();
        if (capture_path && ++capture_frames == 5) {
            TakeScreenshot(capture_path);
            break;
        }
    }
    studio_destroy(studio);
    free(studio);
    CloseWindow();
    return 0;
}
