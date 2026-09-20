/* Implementación de personajes dirigida por datos.
 *
 * Las decisiones costosas (búsqueda de ruta, percepción) usan arreglos de
 * capacidad fija. Así el perfil de memoria de una partida es visible y no hay
 * malloc oculto en el bucle de 60 Hz. */
#include "retro/gameplay.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static bool fail(ReError *error, size_t line, const char *message) {
    if (error) {
        error->line = line;
        (void)snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return false;
}

static bool replace_file(const char *temporary, const char *destination) {
#ifdef _WIN32
    return MoveFileExA(temporary, destination,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, destination) == 0;
#endif
}

static size_t tokens(char *line, char **out, size_t capacity) {
    size_t count = 0;
    for (char *p = line; *p;) {
        while (isspace((unsigned char)*p))
            p++;
        if (!*p || *p == '#')
            break;
        if (count == capacity)
            return capacity + 1u;
        out[count++] = p;
        while (*p && !isspace((unsigned char)*p) && *p != '#')
            p++;
        if (*p == '#') {
            *p = '\0';
            break;
        }
        if (*p)
            *p++ = '\0';
    }
    return count;
}

static bool real(const char *text, float *out) {
    char *end = nullptr;
    errno = 0;
    float value = strtof(text, &end);
    if (errno || end == text || *end || !isfinite(value) || fabsf(value) > 100000)
        return false;
    *out = value;
    return true;
}

static bool integer(const char *text, int *out) {
    char *end = nullptr;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < INT_MIN || value > INT_MAX)
        return false;
    *out = (int)value;
    return true;
}

static bool boolean(const char *text, bool *out) {
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool tracking(const char *text, enum ReTrackingMode *out) {
    if (strcmp(text, "perception") == 0)
        *out = RE_TRACK_PERCEPTION;
    else if (strcmp(text, "omniscient") == 0)
        *out = RE_TRACK_OMNISCIENT;
    else
        return false;
    return true;
}

static bool action(const char *text, enum ReGameplayAction *out) {
    static const char *const names[] = {"wait",    "melee",  "projectile",
                                        "capture", "summon", "activate"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(text, names[i]) == 0) {
            *out = (enum ReGameplayAction)i;
            return true;
        }
    return false;
}

static enum ReAnimationEvent animation_event(const char *text) {
    if (strcmp(text, "sound") == 0)
        return RE_ANIM_EVENT_SOUND;
    if (strcmp(text, "attack") == 0)
        return RE_ANIM_EVENT_ATTACK;
    return RE_ANIM_EVENT_NONE;
}

bool re_character_load(const char *path, ReCharacterDef *out, ReError *error) {
    if (!path || !out || !error)
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return fail(error, 0, "No se pudo abrir la definición de personaje");
    ReCharacterDef candidate = {.radius = .3f,
                                .height = 1.7f,
                                .step_height = .3f,
                                .speed = 2.5f,
                                .turn_speed = 8,
                                .sight_range = 16,
                                .field_of_view = 100 * RE_PI / 180.0f,
                                .hearing_range = 8,
                                .memory_time = 4,
                                .capture_range = .65f,
                                .attack_range = 1.2f,
                                .max_health = 100,
                                .attack_damage = 10};
    bool header = false, ok = true;
    ReAnimationClip *clip = nullptr;
    char line[1024];
    size_t line_number = 0;
    while (ok && fgets(line, sizeof(line), file)) {
        line_number++;
        if (strlen(line) == sizeof(line) - 1u && line[sizeof(line) - 2u] != '\n') {
            ok = fail(error, line_number, "Línea demasiado larga");
            break;
        }
        char *part[12];
        size_t count = tokens(line, part, 12);
        if (!count)
            continue;
        if (!header) {
            header = count == 2 && strcmp(part[0], "retro_actor") == 0 && strcmp(part[1], "1") == 0;
            if (!header)
                ok = fail(error, line_number, "Se esperaba retro_actor 1");
        } else if (count == 2 && strcmp(part[0], "id") == 0 && strlen(part[1]) < 64) {
            (void)snprintf(candidate.id, sizeof(candidate.id), "%s", part[1]);
        } else if (count == 4 && strcmp(part[0], "sprite") == 0 &&
                   strlen(part[1]) < sizeof(candidate.sprite) &&
                   integer(part[2], &candidate.cell_width) &&
                   integer(part[3], &candidate.cell_height)) {
            (void)snprintf(candidate.sprite, sizeof(candidate.sprite), "%s", part[1]);
        } else if (count == 4 && strcmp(part[0], "body") == 0 && real(part[1], &candidate.radius) &&
                   real(part[2], &candidate.height) && real(part[3], &candidate.step_height)) {
        } else if (count == 3 && strcmp(part[0], "movement") == 0 &&
                   real(part[1], &candidate.speed) && real(part[2], &candidate.turn_speed)) {
        } else if (count == 3 && strcmp(part[0], "health") == 0 &&
                   integer(part[1], &candidate.max_health) &&
                   integer(part[2], &candidate.attack_damage)) {
        } else if (count == 5 && strcmp(part[0], "flags") == 0 &&
                   boolean(part[1], &candidate.invulnerable) &&
                   boolean(part[2], &candidate.can_be_stunned) &&
                   boolean(part[3], &candidate.can_open_doors) &&
                   boolean(part[4], &candidate.capture_game_over)) {
        } else if (count == 2 && strcmp(part[0], "tracking") == 0 &&
                   tracking(part[1], &candidate.tracking)) {
        } else if (count == 5 && strcmp(part[0], "perception") == 0 &&
                   real(part[1], &candidate.sight_range) &&
                   real(part[2], &candidate.field_of_view) &&
                   real(part[3], &candidate.hearing_range) &&
                   real(part[4], &candidate.memory_time)) {
            candidate.field_of_view *= RE_PI / 180.0f;
        } else if (count == 3 && strcmp(part[0], "ranges") == 0 &&
                   real(part[1], &candidate.capture_range) &&
                   real(part[2], &candidate.attack_range)) {
        } else if (count == 4 && strcmp(part[0], "animation") == 0 &&
                   candidate.animation_count < RE_MAX_ANIMATIONS && strlen(part[1]) < 24) {
            int directions = 0;
            bool loop = false;
            if (!integer(part[2], &directions) || !boolean(part[3], &loop) ||
                (directions != 1 && directions != 4 && directions != 8)) {
                ok = fail(error, line_number, "Animación inválida");
                continue;
            }
            clip = &candidate.animations[candidate.animation_count++];
            (void)snprintf(clip->name, sizeof(clip->name), "%s", part[1]);
            clip->directions = (unsigned int)directions;
            clip->loop = loop;
        } else if (count == 4 && strcmp(part[0], "frame") == 0 && clip &&
                   clip->frame_count < RE_MAX_ANIMATION_FRAMES) {
            int cell = 0;
            ReAnimationFrame *frame = &clip->frames[clip->frame_count];
            if (!integer(part[1], &cell) || cell < 0 || cell > UINT16_MAX ||
                !real(part[2], &frame->duration) || frame->duration <= 0) {
                ok = fail(error, line_number, "Fotograma inválido");
                continue;
            }
            frame->cell = (uint16_t)cell;
            frame->event = animation_event(part[3]);
            clip->frame_count++;
        } else if (count == 8 && strcmp(part[0], "phase") == 0 &&
                   candidate.phase_count < RE_MAX_BOSS_PHASES && strlen(part[1]) < 24) {
            ReBossPhase *phase = &candidate.phases[candidate.phase_count];
            int summon_limit = 0;
            if (!real(part[2], &phase->health_threshold) || !tracking(part[3], &phase->tracking) ||
                !action(part[4], &phase->action) || !real(part[5], &phase->speed_multiplier) ||
                !real(part[6], &phase->cooldown) || !integer(part[7], &summon_limit) ||
                summon_limit < 0) {
                ok = fail(error, line_number, "Fase inválida");
                continue;
            }
            (void)snprintf(phase->name, sizeof(phase->name), "%s", part[1]);
            phase->summon_limit = (unsigned int)summon_limit;
            candidate.phase_count++;
        } else {
            ok = fail(error, line_number, "Directiva de personaje desconocida o inválida");
        }
    }
    bool read_error = ferror(file) != 0;
    int close_result = fclose(file);
    if (read_error || close_result != 0)
        ok = fail(error, line_number, "Error leyendo la definición");
    if (ok && !header)
        ok = fail(error, 1, "Definición vacía");
    if (ok)
        ok = re_character_validate(&candidate, error);
    if (ok) {
        *out = candidate;
        *error = (ReError){0};
    }
    return ok;
}

bool re_character_validate(const ReCharacterDef *d, ReError *error) {
    if (!d || d->id[0] == '\0' || d->cell_width <= 0 || d->cell_height <= 0 || d->radius <= 0 ||
        d->height <= 0 || d->step_height < 0 || d->speed < 0 || d->turn_speed < 0 ||
        d->max_health <= 0 || d->attack_damage < 0 || d->sight_range < 0 || d->field_of_view <= 0 ||
        d->field_of_view > 2 * RE_PI || d->hearing_range < 0 || d->memory_time < 0 ||
        d->capture_range < 0 || d->attack_range < 0 || d->animation_count > RE_MAX_ANIMATIONS ||
        d->phase_count > RE_MAX_BOSS_PHASES)
        return fail(error, 0, "Contrato de personaje inválido");
    for (size_t i = 0; i < d->animation_count; i++) {
        const ReAnimationClip *clip = &d->animations[i];
        if (!clip->frame_count || clip->frame_count > RE_MAX_ANIMATION_FRAMES ||
            (clip->directions != 1 && clip->directions != 4 && clip->directions != 8))
            return fail(error, 0, "Animación sin fotogramas o direcciones inválidas");
    }
    float previous = 1.001f;
    for (size_t i = 0; i < d->phase_count; i++) {
        const ReBossPhase *phase = &d->phases[i];
        if (phase->health_threshold < 0 || phase->health_threshold > 1 ||
            phase->health_threshold >= previous || phase->speed_multiplier < 0 ||
            phase->cooldown < 0)
            return fail(error, 0, "Fases deben ir de mayor a menor umbral de vida");
        previous = phase->health_threshold;
    }
    if (error)
        *error = (ReError){0};
    return true;
}

static const char *tracking_name(enum ReTrackingMode mode) {
    return mode == RE_TRACK_OMNISCIENT ? "omniscient" : "perception";
}

static const char *action_name(enum ReGameplayAction value) {
    static const char *const names[] = {"wait",    "melee",  "projectile",
                                        "capture", "summon", "activate"};
    return value >= RE_ACTION_WAIT && value <= RE_ACTION_ACTIVATE ? names[value] : "wait";
}

bool re_character_save(const char *path, const ReCharacterDef *d, ReError *error) {
    if (!path || !d || !error || !re_character_validate(d, error))
        return false;
    char temporary[1024];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return fail(error, 0, "Ruta de personaje demasiado larga");
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return fail(error, 0, "No se pudo crear la definición temporal");
    bool ok =
        fprintf(file,
                "retro_actor 1\nid %s\nsprite %s %d %d\n"
                "body %.9g %.9g %.9g\nmovement %.9g %.9g\nhealth %d %d\n"
                "flags %s %s %s %s\ntracking %s\n"
                "perception %.9g %.9g %.9g %.9g\nranges %.9g %.9g\n",
                d->id, d->sprite[0] ? d->sprite : "none", d->cell_width, d->cell_height,
                (double)d->radius, (double)d->height, (double)d->step_height, (double)d->speed,
                (double)d->turn_speed, d->max_health, d->attack_damage,
                d->invulnerable ? "true" : "false", d->can_be_stunned ? "true" : "false",
                d->can_open_doors ? "true" : "false", d->capture_game_over ? "true" : "false",
                tracking_name(d->tracking), (double)d->sight_range,
                (double)(d->field_of_view * 180.0f / RE_PI), (double)d->hearing_range,
                (double)d->memory_time, (double)d->capture_range, (double)d->attack_range) > 0;
    for (size_t i = 0; ok && i < d->animation_count; i++) {
        const ReAnimationClip *clip = &d->animations[i];
        ok = fprintf(file, "\nanimation %s %u %s\n", clip->name, clip->directions,
                     clip->loop ? "true" : "false") > 0;
        for (size_t f = 0; ok && f < clip->frame_count; f++) {
            const char *event =
                clip->frames[f].event == RE_ANIM_EVENT_SOUND
                    ? "sound"
                    : (clip->frames[f].event == RE_ANIM_EVENT_ATTACK ? "attack" : "none");
            ok = fprintf(file, "frame %u %.9g %s\n", (unsigned int)clip->frames[f].cell,
                         (double)clip->frames[f].duration, event) > 0;
        }
    }
    for (size_t i = 0; ok && i < d->phase_count; i++) {
        const ReBossPhase *phase = &d->phases[i];
        ok = fprintf(file, "phase %s %.9g %s %s %.9g %.9g %u\n", phase->name,
                     (double)phase->health_threshold, tracking_name(phase->tracking),
                     action_name(phase->action), (double)phase->speed_multiplier,
                     (double)phase->cooldown, phase->summon_limit) > 0;
    }
    if (fclose(file) != 0)
        ok = false;
    if (!ok) {
        (void)remove(temporary);
        return fail(error, 0, "Error escribiendo la definición temporal");
    }
    if (!replace_file(temporary, path)) {
        (void)remove(temporary);
        return fail(error, 0, "No se pudo reemplazar la definición");
    }
    *error = (ReError){0};
    return true;
}

void re_gameplay_init(ReGameplay *g, ReWorld *world, const ReCharacterDef *definitions,
                      size_t definition_count, uint32_t seed) {
    *g = (ReGameplay){.world = world,
                      .definitions = definitions,
                      .definition_count = definition_count,
                      .random_state = seed ? seed : 1u};
}

ReEntityId re_gameplay_spawn(ReGameplay *g, size_t definition, ReVec3 position, float yaw,
                             int sector) {
    if (!g || definition >= g->definition_count)
        return (ReEntityId){UINT16_MAX, 0};
    for (size_t i = 0; i < RE_MAX_ENTITIES; i++)
        if (!g->characters[i].active) {
            uint16_t generation = (uint16_t)(g->characters[i].generation + 1u);
            if (!generation)
                generation = 1;
            const ReCharacterDef *d = &g->definitions[definition];
            g->characters[i] = (ReCharacter){.active = true,
                                             .generation = generation,
                                             .definition = (uint16_t)definition,
                                             .body = {.position = position,
                                                      .radius = d->radius,
                                                      .height = d->height,
                                                      .step_height = d->step_height,
                                                      .sector = sector,
                                                      .grounded = true},
                                             .health = d->max_health,
                                             .state = RE_CHARACTER_IDLE,
                                             .yaw = yaw,
                                             .previous_position = position};
            return (ReEntityId){(uint16_t)i, generation};
        }
    return (ReEntityId){UINT16_MAX, 0};
}

ReCharacter *re_gameplay_get(ReGameplay *g, ReEntityId id) {
    if (!g || id.index >= RE_MAX_ENTITIES)
        return nullptr;
    ReCharacter *character = &g->characters[id.index];
    return character->active && character->generation == id.generation ? character : nullptr;
}

static void emit(ReGameplay *g, ReGameplayEvent event) {
    if (g->event_count == RE_MAX_GAMEPLAY_EVENTS)
        return; /* Capacidad visible: se conserva el orden de los primeros eventos. */
    size_t index = (g->event_head + g->event_count) % RE_MAX_GAMEPLAY_EVENTS;
    g->events[index] = event;
    g->event_count++;
}

bool re_gameplay_event(ReGameplay *g, ReGameplayEvent *out) {
    if (!g || !out || !g->event_count)
        return false;
    *out = g->events[g->event_head];
    g->event_head = (g->event_head + 1u) % RE_MAX_GAMEPLAY_EVENTS;
    g->event_count--;
    return true;
}

static ReVec2 centroid(const ReSector *sector) {
    ReVec2 center = {0};
    for (size_t i = 0; i < sector->count; i++)
        center = re_add2(center, sector->vertices[i]);
    return re_scale2(center, 1.0f / (float)sector->count);
}

/* A* devuelve sólo el primer salto. El coste de una puerta cerrada desalienta
 * usarla cuando existe una ruta abierta, pero no la vuelve invisible para un
 * personaje que sabe abrir puertas. */
static int path_next(const ReWorld *world, int start, int goal, const ReCharacterDef *d) {
    if (start < 0 || goal < 0 || start >= (int)world->sector_count ||
        goal >= (int)world->sector_count || start == goal)
        return start == goal ? goal : -1;
    float cost[RE_MAX_SECTORS], score[RE_MAX_SECTORS];
    int parent[RE_MAX_SECTORS];
    bool closed[RE_MAX_SECTORS] = {0};
    for (size_t i = 0; i < RE_MAX_SECTORS; i++) {
        cost[i] = INFINITY;
        score[i] = INFINITY;
        parent[i] = -1;
    }
    cost[start] = 0;
    score[start] =
        re_length2(re_sub2(centroid(&world->sectors[start]), centroid(&world->sectors[goal])));
    for (size_t visit = 0; visit < world->sector_count; visit++) {
        int current = -1;
        for (size_t i = 0; i < world->sector_count; i++)
            if (!closed[i] && isfinite(score[i]) && (current < 0 || score[i] < score[current]))
                current = (int)i;
        if (current < 0)
            break;
        if (current == goal)
            break;
        closed[current] = true;
        const ReSector *sector = &world->sectors[current];
        for (size_t edge = 0; edge < sector->count; edge++) {
            int next = sector->neighbor[edge];
            if (next < 0 || closed[next])
                continue;
            const ReSector *other = &world->sectors[next];
            float opening =
                fminf(sector->ceiling, other->ceiling) - fmaxf(sector->floor, other->floor);
            float width = re_length2(
                re_sub2(sector->vertices[(edge + 1u) % sector->count], sector->vertices[edge]));
            if (opening + RE_EPSILON < d->height || width + RE_EPSILON < d->radius * 2 ||
                other->floor - sector->floor > d->step_height + RE_EPSILON)
                continue;
            int barrier = re_world_barrier_at(world, current, (int)edge);
            float extra = 0;
            if (barrier >= 0 && world->barriers[barrier].open_fraction < .95f &&
                (world->barriers[barrier].blocks & RE_BLOCK_MOVEMENT) != 0) {
                if (world->barriers[barrier].kind != RE_BARRIER_DOOR || !d->can_open_doors)
                    continue;
                extra = 2;
            }
            float tentative =
                cost[current] + re_length2(re_sub2(centroid(sector), centroid(other))) + extra;
            if (tentative < cost[next]) {
                parent[next] = current;
                cost[next] = tentative;
                score[next] = tentative +
                              re_length2(re_sub2(centroid(other), centroid(&world->sectors[goal])));
            }
        }
    }
    if (parent[goal] < 0)
        return -1;
    int first = goal;
    while (parent[first] != start) {
        first = parent[first];
        if (first < 0)
            return -1;
    }
    return first;
}

static bool visible(const ReGameplay *g, const ReCharacter *c, const ReCharacterDef *d,
                    ReVec3 eye) {
    ReVec3 origin = re_add3(c->body.position, re_v3(0, 0, c->body.height * .82f));
    ReVec3 delta = re_sub3(eye, origin);
    float distance = sqrtf(re_dot3(delta, delta));
    if (distance < RE_EPSILON || distance > d->sight_range)
        return false;
    ReVec2 horizontal = re_normalize2(re_v2(delta.x, delta.y));
    ReVec2 facing = re_v2(sinf(c->yaw), cosf(c->yaw));
    if (re_dot2(horizontal, facing) < cosf(d->field_of_view * .5f))
        return false;
    ReVec3 direction = re_scale3(delta, 1.0f / distance);
    return re_world_trace(g->world, origin, direction, distance, RE_BLOCK_SIGHT).distance >=
           distance - .01f;
}

static int edge_to(const ReWorld *world, int sector, int next) {
    if (sector < 0 || sector >= (int)world->sector_count)
        return -1;
    for (size_t edge = 0; edge < world->sectors[sector].count; edge++)
        if (world->sectors[sector].neighbor[edge] == next)
            return (int)edge;
    return -1;
}

static void face_and_move(ReGameplay *g, ReCharacter *c, const ReCharacterDef *d, ReVec3 target,
                          float speed, float dt) {
    int target_sector = re_world_sector_at(g->world, target, -1);
    ReVec2 destination = re_v2(target.x, target.y);
    int next = path_next(g->world, c->body.sector, target_sector, d);
    if (next >= 0 && next != c->body.sector) {
        int edge = edge_to(g->world, c->body.sector, next);
        if (edge >= 0) {
            const ReSector *sector = &g->world->sectors[c->body.sector];
            ReVec2 a = sector->vertices[edge];
            ReVec2 b = sector->vertices[((size_t)edge + 1u) % sector->count];
            destination = re_scale2(re_add2(a, b), .5f);
            /* El centro exacto del portal pertenece a ambos polígonos. Avanzar
             * un radio hacia el volumen siguiente evita oscilar en esa frontera
             * y convierte el salto de A* en un objetivo inequívoco. */
            ReVec2 into_next =
                re_normalize2(re_sub2(centroid(&g->world->sectors[next]), centroid(sector)));
            destination = re_add2(destination, re_scale2(into_next, d->radius + .05f));
            int barrier = re_world_barrier_at(g->world, c->body.sector, edge);
            if (barrier >= 0 && d->can_open_doors &&
                g->world->barriers[barrier].kind == RE_BARRIER_DOOR &&
                re_length2(re_sub2(destination, re_v2(c->body.position.x, c->body.position.y))) <
                    1.2f)
                g->world->barriers[barrier].open_fraction =
                    fminf(1, g->world->barriers[barrier].open_fraction + dt * 1.8f);
        }
    }
    ReVec2 position = re_v2(c->body.position.x, c->body.position.y);
    ReVec2 offset = re_sub2(destination, position);
    float distance = re_length2(offset);
    if (distance < .04f)
        return;
    float desired = atan2f(offset.x, offset.y);
    float difference = remainderf(desired - c->yaw, 2 * RE_PI);
    c->yaw += re_clamp(difference, -d->turn_speed * dt, d->turn_speed * dt);
    ReVec2 step = re_scale2(offset, fminf(distance, speed * dt) / distance);
    re_body_move(g->world, &c->body, step, dt, 18);
}

static const ReBossPhase *phase_for(const ReCharacterDef *d, ReCharacter *c, ReEntityId id,
                                    ReGameplay *g) {
    if (!d->phase_count)
        return nullptr;
    float health = (float)c->health / (float)d->max_health;
    unsigned int selected = 0;
    for (size_t i = 0; i < d->phase_count; i++)
        if (health <= d->phases[i].health_threshold + RE_EPSILON)
            selected = (unsigned int)i;
    if (selected != c->phase) {
        c->phase = selected;
        c->cooldown = 0;
        emit(g, (ReGameplayEvent){.kind = RE_EVENT_PHASE_CHANGED,
                                  .source = id,
                                  .value = (int)selected,
                                  .position = c->body.position});
    }
    return &d->phases[selected];
}

static void tick_one(ReGameplay *g, size_t index, ReGameplayInput input, float dt) {
    ReCharacter *c = &g->characters[index];
    if (!c->active || !input.player || c->state == RE_CHARACTER_DEAD)
        return;
    const ReCharacterDef *d = &g->definitions[c->definition];
    ReEntityId id = {(uint16_t)index, c->generation};
    c->state_time += dt;
    c->cooldown = fmaxf(0, c->cooldown - dt);
    c->animation_time += dt;
    if (c->state == RE_CHARACTER_STUNNED) {
        if (c->cooldown <= 0) {
            c->state = RE_CHARACTER_SEARCH;
            c->state_time = 0;
        }
        return;
    }
    const ReBossPhase *phase = phase_for(d, c, id, g);
    enum ReTrackingMode mode = phase ? phase->tracking : d->tracking;
    float speed = d->speed * (phase ? phase->speed_multiplier : 1);
    bool sees = visible(g, c, d, input.player_eye);
    ReVec2 player_xy = re_v2(input.player->position.x, input.player->position.y);
    ReVec2 actor_xy = re_v2(c->body.position.x, c->body.position.y);
    float horizontal = re_length2(re_sub2(player_xy, actor_xy));
    bool hears =
        input.player_noise > 0 && horizontal <= fminf(input.player_noise, d->hearing_range);
    if (mode == RE_TRACK_OMNISCIENT || sees || hears) {
        c->last_known_position = input.player->position;
        c->memory_left = d->memory_time;
        c->state = RE_CHARACTER_CHASE;
    } else if (c->memory_left > 0) {
        c->memory_left = fmaxf(0, c->memory_left - dt);
        c->state = c->memory_left > 0 ? RE_CHARACTER_SEARCH : RE_CHARACTER_PATROL;
    } else if (c->state != RE_CHARACTER_STUNNED)
        c->state = RE_CHARACTER_PATROL;
    ReVec3 delta3 = re_sub3(input.player->position, c->body.position);
    bool vertical_overlap = c->body.position.z < input.player->position.z + input.player->height &&
                            input.player->position.z < c->body.position.z + c->body.height;
    float direct = sqrtf(re_dot3(delta3, delta3));
    bool unobstructed =
        direct < RE_EPSILON ||
        re_world_trace(g->world, re_add3(c->body.position, re_v3(0, 0, c->body.height * .5f)),
                       re_scale3(delta3, 1.0f / fmaxf(direct, RE_EPSILON)), direct, RE_BLOCK_SIGHT)
                .distance >= direct - .01f;
    enum ReGameplayAction selected =
        phase ? phase->action : (d->capture_game_over ? RE_ACTION_CAPTURE : RE_ACTION_MELEE);
    if (selected == RE_ACTION_CAPTURE && horizontal <= d->capture_range && vertical_overlap &&
        unobstructed) {
        if (!c->capture_emitted) {
            emit(g, (ReGameplayEvent){
                        .kind = RE_EVENT_CAPTURED, .source = id, .position = c->body.position});
            c->capture_emitted = true;
        }
        return;
    }
    if ((selected == RE_ACTION_MELEE || selected == RE_ACTION_PROJECTILE) &&
        horizontal <= d->attack_range && vertical_overlap && unobstructed && c->cooldown <= 0) {
        enum ReGameplayEventKind kind =
            selected == RE_ACTION_MELEE ? RE_EVENT_PLAYER_DAMAGE : RE_EVENT_PROJECTILE;
        emit(g, (ReGameplayEvent){.kind = kind,
                                  .source = id,
                                  .value = d->attack_damage,
                                  .position = c->body.position});
        c->cooldown = phase ? phase->cooldown : 1;
        c->state = RE_CHARACTER_RECOVERY;
    } else if (selected == RE_ACTION_SUMMON && phase && c->state == RE_CHARACTER_CHASE &&
               horizontal <= d->attack_range && vertical_overlap && unobstructed &&
               c->summoned < phase->summon_limit && c->cooldown <= 0) {
        emit(g, (ReGameplayEvent){
                    .kind = RE_EVENT_SUMMON, .source = id, .position = c->body.position});
        c->summoned++;
        c->cooldown = phase->cooldown;
    } else if (selected == RE_ACTION_ACTIVATE && c->state == RE_CHARACTER_CHASE &&
               horizontal <= d->attack_range && vertical_overlap && unobstructed &&
               c->cooldown <= 0) {
        emit(g, (ReGameplayEvent){
                    .kind = RE_EVENT_ACTIVATE, .source = id, .position = c->body.position});
        c->cooldown = phase ? phase->cooldown : 1;
    }
    if (c->state == RE_CHARACTER_CHASE)
        face_and_move(g, c, d, input.player->position, speed, dt);
    else if (c->state == RE_CHARACTER_SEARCH)
        face_and_move(g, c, d, c->last_known_position, speed * .7f, dt);
    float moved = re_length2(re_sub2(re_v2(c->body.position.x, c->body.position.y),
                                     re_v2(c->previous_position.x, c->previous_position.y)));
    c->stuck_time = moved < .001f && c->state == RE_CHARACTER_CHASE ? c->stuck_time + dt : 0;
    c->previous_position = c->body.position;
    if (c->stuck_time > 1) {
        c->memory_left = 0; /* Fuerza una reevaluación limpia del objetivo. */
        c->stuck_time = 0;
    }
}

void re_gameplay_tick(ReGameplay *g, ReGameplayInput input, float dt) {
    if (!g || !g->world || !input.player || dt <= 0 || dt > 1.0f / 30.0f)
        return;
    for (size_t i = 0; i < RE_MAX_ENTITIES; i++)
        tick_one(g, i, input, dt);
}

bool re_gameplay_damage(ReGameplay *g, ReEntityId id, int damage, float stun_time) {
    ReCharacter *c = re_gameplay_get(g, id);
    if (!c || damage <= 0)
        return false;
    const ReCharacterDef *d = &g->definitions[c->definition];
    if (!d->invulnerable) {
        c->health = c->health > damage ? c->health - damage : 0;
        if (!c->health) {
            c->state = RE_CHARACTER_DEAD;
            return true;
        }
    }
    if (d->can_be_stunned && stun_time > 0) {
        c->state = RE_CHARACTER_STUNNED;
        c->state_time = 0;
        c->cooldown = stun_time;
    }
    return true;
}

int re_character_animation_cell(const ReCharacterDef *d, const ReCharacter *c, const char *name,
                                float camera_yaw) {
    if (!d || !c || !name)
        return -1;
    const ReAnimationClip *clip = nullptr;
    for (size_t i = 0; i < d->animation_count; i++)
        if (strcmp(d->animations[i].name, name) == 0) {
            clip = &d->animations[i];
            break;
        }
    if (!clip || !clip->frame_count)
        return -1;
    float total = 0;
    for (size_t i = 0; i < clip->frame_count; i++)
        total += clip->frames[i].duration;
    float time = clip->loop ? fmodf(c->animation_time, total) : fminf(c->animation_time, total);
    size_t frame = 0;
    while (frame + 1u < clip->frame_count && time >= clip->frames[frame].duration) {
        time -= clip->frames[frame].duration;
        frame++;
    }
    float angle = remainderf(camera_yaw - c->yaw, 2 * RE_PI);
    float normalized = angle < 0 ? angle + 2 * RE_PI : angle;
    unsigned int direction =
        (unsigned int)lroundf(normalized * (float)clip->directions / (2 * RE_PI)) %
        clip->directions;
    return (int)clip->frames[frame].cell + (int)(direction * (unsigned int)clip->frame_count);
}

static bool body_touches_barrier(const ReWorld *world, const ReBarrier *barrier,
                                 const ReBody *body) {
    const ReSector *sector = &world->sectors[barrier->sector];
    ReVec2 a = sector->vertices[barrier->edge];
    ReVec2 b = sector->vertices[((size_t)barrier->edge + 1u) % sector->count];
    ReVec2 p = re_v2(body->position.x, body->position.y);
    return re_length2(re_sub2(p, re_closest_segment(p, a, b))) <= body->radius + .02f;
}

bool re_barrier_tick(ReWorld *world, size_t index, float target, float speed, float dt,
                     const ReBody *bodies, size_t count) {
    if (!world || index >= world->barrier_count || speed < 0 || dt < 0)
        return false;
    ReBarrier *barrier = &world->barriers[index];
    target = re_clamp(target, 0, 1);
    if (target < barrier->open_fraction)
        for (size_t i = 0; i < count; i++)
            if (body_touches_barrier(world, barrier, &bodies[i])) {
                barrier->open_fraction = fminf(1, barrier->open_fraction + speed * dt);
                return false;
            }
    float delta = target - barrier->open_fraction;
    barrier->open_fraction += re_clamp(delta, -speed * dt, speed * dt);
    return fabsf(target - barrier->open_fraction) <= RE_EPSILON;
}

bool re_barrier_damage(ReWorld *world, size_t index, float damage) {
    if (!world || index >= world->barrier_count || damage <= 0)
        return false;
    ReBarrier *barrier = &world->barriers[index];
    if (barrier->kind != RE_BARRIER_WINDOW || barrier->health <= 0)
        return false;
    barrier->health = fmaxf(0, barrier->health - damage);
    if (barrier->health <= 0) {
        barrier->open_fraction = 1;
        barrier->blocks = 0;
    }
    return true;
}
