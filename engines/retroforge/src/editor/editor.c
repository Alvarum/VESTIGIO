/* Implementación del documento transaccional usado por RetroForge Studio.
 *
 * El historial almacena proyectos completos. Los límites fijos del motor hacen
 * que esta estrategia sea predecible, fácil de auditar y suficientemente rápida
 * para un editor personal. Si los límites crecen, esta implementación puede
 * sustituirse por deltas sin cambiar la ABI pública. */
#include "retro/editor.h"
#include "retro/character_art.h"
#ifdef RETRO_EDITOR_SESSIONS
#include "retro/session.h"
#endif
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RE_EDITOR_HISTORY = 48 };

struct ReEditorDocument {
    ReProject *states;
    size_t count, cursor, saved_cursor;
    bool editing; /* El candidato se conserva fuera del historial. */
    uint64_t revision;
};

static ReProject *current(ReEditorDocument *document) {
    return document ? &document->states[document->cursor] : nullptr;
}

static const ReProject *current_const(const ReEditorDocument *document) {
    return document ? &document->states[document->cursor] : nullptr;
}

#ifdef RETRO_EDITOR_SESSIONS
int re_editor_start_session(const ReEditorDocument *document, ReGameSession **out, ReError *error) {
    const ReProject *project = current_const(document);
    return project ? re_session_create(project, 1, 0, out, error) : 0;
}
#endif

static int fail(ReError *error, const char *message) {
    if (error) {
        error->line = 0;
        (void)snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return 0;
}

static bool copy_text(char *destination, size_t capacity, const char *source) {
    if (!destination || !capacity || !source || strlen(source) >= capacity)
        return false;
    (void)memcpy(destination, source, strlen(source) + 1u);
    return true;
}

static bool parse_float(const char *text, float *out) {
    if (!text || !out)
        return false;
    errno = 0;
    char *end = nullptr;
    float value = strtof(text, &end);
    if (errno || end == text || *end != '\0' || !isfinite(value))
        return false;
    *out = value;
    return true;
}

static bool parse_int(const char *text, int *out) {
    if (!text || !out)
        return false;
    errno = 0;
    char *end = nullptr;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX)
        return false;
    *out = (int)value;
    return true;
}

static bool parse_bool(const char *text, bool *out) {
    if (!text || !out)
        return false;
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

static ReProject *begin_command(ReEditorDocument *document) {
    if (!document || document->editing)
        return nullptr;
    document->states[RE_EDITOR_HISTORY] = document->states[document->cursor];
    document->editing = true;
    return &document->states[RE_EDITOR_HISTORY];
}

/* Sólo un comando validado puede eliminar la rama de rehacer. El slot
 * provisional también impide perder el estado más antiguo al rechazar
 * operaciones cuando el historial está lleno. */
static void commit_command(ReEditorDocument *document) {
    if (!document || !document->editing)
        return;
    if (document->saved_cursor > document->cursor)
        document->saved_cursor = SIZE_MAX;
    document->count = document->cursor + 1u;
    if (document->count == RE_EDITOR_HISTORY) {
        (void)memmove(&document->states[0], &document->states[1],
                      (RE_EDITOR_HISTORY - 1u) * sizeof(*document->states));
        document->count--;
        document->cursor--;
        if (document->saved_cursor != SIZE_MAX && document->saved_cursor > 0)
            document->saved_cursor--;
        else
            document->saved_cursor = SIZE_MAX;
    }
    document->states[document->count] = document->states[RE_EDITOR_HISTORY];
    document->cursor = document->count++;
    document->editing = false;
    document->revision++;
}

static void cancel_command(ReEditorDocument *document) {
    if (document)
        document->editing = false;
}

int re_editor_open(const char *manifest, ReEditorDocument **out, ReError *error) {
    if (!manifest || !out || !error)
        return fail(error, "Argumentos inválidos al abrir el proyecto");
    ReEditorDocument *document = calloc(1, sizeof(*document));
    if (!document)
        return fail(error, "No hay memoria para el documento");
    document->states = calloc(RE_EDITOR_HISTORY + 1u, sizeof(*document->states));
    if (!document->states) {
        free(document);
        return fail(error, "No hay memoria para el historial");
    }
    if (!re_project_load(manifest, &document->states[0], error)) {
        free(document->states);
        free(document);
        return 0;
    }
    document->count = 1;
    document->saved_cursor = 0;
    document->revision = 1;
    *out = document;
    return 1;
}

void re_editor_close(ReEditorDocument *document) {
    if (!document)
        return;
    free(document->states);
    free(document);
}

int re_editor_new(const char *manifest, ReEditorDocument **out, ReError *error) {
    if (!manifest || !out || !error)
        return fail(error, "Ruta de proyecto inválida");
    FILE *existing = fopen(manifest, "rb");
    if (existing) {
        (void)fclose(existing);
        return fail(error, "El proyecto ya existe");
    }
    ReEditorDocument *document = calloc(1, sizeof(*document));
    if (!document)
        return fail(error, "Memoria insuficiente");
    document->states = calloc(RE_EDITOR_HISTORY + 1u, sizeof(*document->states));
    if (!document->states) {
        free(document);
        return fail(error, "Memoria insuficiente");
    }
    ReProject *project = &document->states[0];
    const char *slash = strrchr(manifest, '/'), *backslash = strrchr(manifest, '\\');
    if (backslash && (!slash || backslash > slash))
        slash = backslash;
    size_t root_length = slash ? (size_t)(slash - manifest) : 0;
    if (!slash || root_length >= sizeof(project->root) ||
        !copy_text(project->manifest, sizeof(project->manifest), manifest)) {
        re_editor_close(document);
        return fail(error, "Usa una ruta absoluta más corta");
    }
    memcpy(project->root, manifest, root_length);
    (void)snprintf(project->name, sizeof(project->name), "Mi_juego");
    /* La identidad del proyecto no se deriva del nombre visible. */
    uint32_t hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)manifest; *p; p++)
        hash = (hash ^ *p) * 16777619u;
    (void)snprintf(project->id, sizeof(project->id), "project_%08x", hash);
    (void)snprintf(project->initial_level, sizeof(project->initial_level), "level_%08x.rmap", hash);
    (void)snprintf(project->rules, sizeof(project->rules), "fps");
    project->death_policy = RE_DEATH_LAST_CHECKPOINT;
    project->format_version = 2;
    ReWorld *world = &project->world;
    world->sector_count = 1;
    world->marker_count = 1;
    world->format_version = 4;
    world->sectors[0] = (ReSector){.floor = 0,
                                   .ceiling = 3,
                                   .light = .75f,
                                   .floor_material = 1,
                                   .ceiling_material = 2,
                                   .count = 4,
                                   .vertices = {{0, 0}, {6, 0}, {6, 6}, {0, 6}}};
    for (size_t i = 0; i < 4; i++) {
        world->sectors[0].neighbor[i] = -1;
        world->sectors[0].neighbor_edge[i] = -1;
        world->sectors[0].portal_end[i] = 1;
    }
    world->markers[0] = (ReMarker){.kind = "player",
                                   .id = "player",
                                   .definition = "player",
                                   .position = {3, 3, 0},
                                   .sector = 0};
    document->count = 1;
    document->saved_cursor = SIZE_MAX;
    document->revision = 1;
    *out = document;
    *error = (ReError){0};
    return 1;
}

int re_editor_overview(const ReEditorDocument *document, ReEditorOverview *out) {
    const ReProject *project = current_const(document);
    if (!project || !out)
        return 0;
    *out = (ReEditorOverview){0};
    (void)copy_text(out->name, sizeof(out->name), project->name);
    (void)copy_text(out->id, sizeof(out->id), project->id);
    (void)copy_text(out->manifest, sizeof(out->manifest), project->manifest);
    (void)copy_text(out->root, sizeof(out->root), project->root);
    out->revision = document->revision;
    out->sector_count = (uint32_t)project->world.sector_count;
    out->marker_count = (uint32_t)project->world.marker_count;
    out->barrier_count = (uint32_t)project->world.barrier_count;
    out->character_count = (uint32_t)project->character_count;
    out->rule_count = (uint32_t)project->interactions.rule_count;
    out->dialogue_count = (uint32_t)project->interactions.dialogue_count;
    out->trigger_count = (uint32_t)project->interactions.trigger_count;
    out->light_count = (uint32_t)project->interactions.light_count;
    out->dirty = document->cursor != document->saved_cursor;
    out->can_undo = document->cursor > 0;
    out->can_redo = document->cursor + 1u < document->count;
    return 1;
}

int re_editor_item(const ReEditorDocument *document, uint32_t index, ReItemDefinition *out) {
    const ReProject *project = current_const(document);
    if (!project || !out || index >= project->interactions.item_count)
        return 0;
    *out = project->interactions.items[index];
    return 1;
}

int re_editor_placeholder(const ReEditorDocument *document, uint32_t character, uint32_t cell,
                          void *rgba, uint32_t capacity) {
    const ReProject *p = current_const(document);
    if (!p || !rgba || character >= p->character_count || cell >= 64)
        return 0;
    const ReCharacterDef *definition = &p->characters[character];
    if (definition->cell_width <= 0 || definition->cell_width > 512 ||
        definition->cell_height <= 0 || definition->cell_height > 512)
        return 0;
    size_t width = (size_t)definition->cell_width, height = (size_t)definition->cell_height;
    if (capacity < width * height * sizeof(RePixel))
        return 0;
    ReTexture atlas = {0};
    if (!re_character_placeholder(&atlas, definition, character))
        return 0;
    for (size_t y = 0; y < height; y++)
        memcpy((unsigned char *)rgba + y * width * sizeof(RePixel),
               atlas.pixels + ((cell / 8u) * height + y) * (size_t)atlas.width +
                   (cell % 8u) * width,
               width * sizeof(RePixel));
    re_texture_destroy(&atlas);
    return 1;
}

int re_editor_portal(const ReEditorDocument *document, uint32_t sector, uint32_t edge, float *start,
                     float *end) {
    const ReProject *p = current_const(document);
    if (!p || !start || !end || sector >= p->world.sector_count ||
        edge >= p->world.sectors[sector].count)
        return 0;
    const ReSector *room = &p->world.sectors[sector];
    if (room->neighbor[edge] < 0)
        return 0;
    *start = room->portal_start[edge];
    *end = room->portal_end[edge];
    return 1;
}

int re_editor_sector(const ReEditorDocument *document, uint32_t index, ReEditorSectorView *out) {
    const ReProject *project = current_const(document);
    if (!project || !out || index >= project->world.sector_count)
        return 0;
    const ReSector *sector = &project->world.sectors[index];
    *out = (ReEditorSectorView){.floor = sector->floor,
                                .ceiling = sector->ceiling,
                                .light = sector->light,
                                .wall_material = sector->wall_material,
                                .floor_material = sector->floor_material,
                                .ceiling_material = sector->ceiling_material,
                                .vertex_count = (uint32_t)sector->count};
    for (size_t i = 0; i < sector->count; i++) {
        out->vertices[i * 2u] = sector->vertices[i].x;
        out->vertices[i * 2u + 1u] = sector->vertices[i].y;
    }
    return 1;
}

int re_editor_marker(const ReEditorDocument *document, uint32_t index, ReEditorMarkerView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || index >= p->world.marker_count)
        return 0;
    const ReMarker *m = &p->world.markers[index];
    *out = (ReEditorMarkerView){.sector = m->sector,
                                .x = m->position.x,
                                .y = m->position.y,
                                .z = m->position.z,
                                .yaw = m->yaw};
    (void)copy_text(out->id, sizeof(out->id), m->id);
    (void)copy_text(out->kind, sizeof(out->kind), m->kind);
    (void)copy_text(out->definition, sizeof(out->definition), m->definition);
    return 1;
}

int re_editor_barrier(const ReEditorDocument *document, uint32_t index, ReEditorBarrierView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || index >= p->world.barrier_count)
        return 0;
    const ReBarrier *b = &p->world.barriers[index];
    *out = (ReEditorBarrierView){.kind = (int)b->kind,
                                 .sector = b->sector,
                                 .edge = b->edge,
                                 .material = b->material,
                                 .blocks = b->blocks,
                                 .open_fraction = b->open_fraction,
                                 .health = b->health};
    (void)copy_text(out->id, sizeof(out->id), b->id);
    return 1;
}

int re_editor_character(const ReEditorDocument *document, uint32_t index,
                        ReEditorCharacterView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || index >= p->character_count)
        return 0;
    const ReCharacterDef *c = &p->characters[index];
    *out = (ReEditorCharacterView){.cell_width = c->cell_width,
                                   .cell_height = c->cell_height,
                                   .max_health = c->max_health,
                                   .attack_damage = c->attack_damage,
                                   .tracking = (int)c->tracking,
                                   .radius = c->radius,
                                   .height = c->height,
                                   .speed = c->speed,
                                   .sight_range = c->sight_range,
                                   .field_of_view = c->field_of_view,
                                   .capture_range = c->capture_range,
                                   .attack_range = c->attack_range,
                                   .animation_count = (uint32_t)c->animation_count,
                                   .phase_count = (uint32_t)c->phase_count,
                                   .invulnerable = c->invulnerable,
                                   .can_be_stunned = c->can_be_stunned,
                                   .can_open_doors = c->can_open_doors,
                                   .capture_game_over = c->capture_game_over};
    (void)copy_text(out->id, sizeof(out->id), c->id);
    (void)copy_text(out->sprite, sizeof(out->sprite), c->sprite);
    return 1;
}

int re_editor_rule(const ReEditorDocument *document, uint32_t index, ReEditorRuleView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || index >= p->interactions.rule_count)
        return 0;
    const ReRuleDefinition *r = &p->interactions.rules[index];
    *out = (ReEditorRuleView){.event = (int)r->event,
                              .priority = r->priority,
                              .once = r->once,
                              .cooldown = r->cooldown,
                              .condition_count = (uint32_t)r->condition_count,
                              .action_count = (uint32_t)r->action_count};
    (void)copy_text(out->id, sizeof(out->id), r->id);
    (void)copy_text(out->source, sizeof(out->source), r->source);
    return 1;
}

int re_editor_dialogue(const ReEditorDocument *document, uint32_t index,
                       ReEditorDialogueView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || index >= p->interactions.dialogue_count)
        return 0;
    const ReDialogueNode *d = &p->interactions.dialogues[index];
    *out = (ReEditorDialogueView){.pauses_world = d->pauses_world,
                                  .choice_count = (uint32_t)d->choice_count};
    (void)copy_text(out->id, sizeof(out->id), d->id);
    (void)copy_text(out->speaker, sizeof(out->speaker), d->speaker);
    (void)copy_text(out->text, sizeof(out->text), d->text);
    (void)copy_text(out->next, sizeof(out->next), d->next);
    return 1;
}

int re_editor_trigger(const ReEditorDocument *document, uint32_t index, ReEditorTriggerView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || index >= p->interactions.trigger_count)
        return 0;
    const ReTriggerVolume *t = &p->interactions.triggers[index];
    *out = (ReEditorTriggerView){.shape = (int)t->shape,
                                 .sector = t->sector,
                                 .once = t->once,
                                 .x = t->center.x,
                                 .y = t->center.y,
                                 .z = t->center.z,
                                 .size_x = t->half_size.x,
                                 .size_y = t->half_size.y,
                                 .size_z = t->half_size.z,
                                 .radius = t->radius};
    (void)copy_text(out->id, sizeof(out->id), t->id);
    return 1;
}

int re_editor_light(const ReEditorDocument *document, uint32_t index, ReEditorLightView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || index >= p->interactions.light_count)
        return 0;
    const ReLight *l = &p->interactions.lights[index];
    *out = (ReEditorLightView){.kind = (int)l->kind,
                               .enabled = l->initially_enabled,
                               .x = l->position.x,
                               .y = l->position.y,
                               .z = l->position.z,
                               .red = l->color.x,
                               .green = l->color.y,
                               .blue = l->color.z,
                               .radius = l->radius,
                               .intensity = l->intensity,
                               .yaw = l->yaw,
                               .cone = l->cone,
                               .flicker = l->flicker};
    (void)copy_text(out->id, sizeof(out->id), l->id);
    return 1;
}

static void value_text(ReValue value, char *out, size_t capacity) {
    switch (value.kind) {
    case RE_VALUE_BOOL:
        (void)snprintf(out, capacity, "%s", value.as.boolean ? "true" : "false");
        break;
    case RE_VALUE_INT:
        (void)snprintf(out, capacity, "%d", value.as.integer);
        break;
    case RE_VALUE_FLOAT:
        (void)snprintf(out, capacity, "%.3g", (double)value.as.real);
        break;
    case RE_VALUE_TEXT:
        (void)snprintf(out, capacity, "%s", value.as.text);
        break;
    default:
        (void)snprintf(out, capacity, "-");
        break;
    }
}

int re_editor_animation(const ReEditorDocument *document, uint32_t character, uint32_t animation,
                        ReEditorAnimationView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || character >= p->character_count ||
        animation >= p->characters[character].animation_count)
        return 0;
    const ReAnimationClip *clip = &p->characters[character].animations[animation];
    *out = (ReEditorAnimationView){.directions = clip->directions,
                                   .frame_count = (uint32_t)clip->frame_count,
                                   .loop = clip->loop};
    (void)copy_text(out->name, sizeof(out->name), clip->name);
    return 1;
}

int re_editor_animation_frame(const ReEditorDocument *document, uint32_t character,
                              uint32_t animation, uint32_t frame, ReEditorAnimationFrameView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || character >= p->character_count ||
        animation >= p->characters[character].animation_count ||
        frame >= p->characters[character].animations[animation].frame_count)
        return 0;
    const ReAnimationFrame *value = &p->characters[character].animations[animation].frames[frame];
    *out = (ReEditorAnimationFrameView){
        .cell = value->cell, .duration = value->duration, .event = (int)value->event};
    return 1;
}

int re_editor_boss_phase(const ReEditorDocument *document, uint32_t character, uint32_t phase,
                         ReEditorBossPhaseView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || character >= p->character_count ||
        phase >= p->characters[character].phase_count)
        return 0;
    const ReBossPhase *value = &p->characters[character].phases[phase];
    *out = (ReEditorBossPhaseView){.health_threshold = value->health_threshold,
                                   .speed_multiplier = value->speed_multiplier,
                                   .cooldown = value->cooldown,
                                   .tracking = (int)value->tracking,
                                   .action = (int)value->action,
                                   .summon_limit = value->summon_limit};
    (void)copy_text(out->name, sizeof(out->name), value->name);
    return 1;
}

int re_editor_rule_condition(const ReEditorDocument *document, uint32_t rule, uint32_t condition,
                             ReEditorRuleConditionView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || rule >= p->interactions.rule_count ||
        condition >= p->interactions.rules[rule].condition_count)
        return 0;
    const ReRuleCondition *value = &p->interactions.rules[rule].conditions[condition];
    *out =
        (ReEditorRuleConditionView){.kind = (int)value->kind, .comparison = (int)value->comparison};
    (void)copy_text(out->key, sizeof(out->key), value->key);
    value_text(value->value, out->value, sizeof(out->value));
    return 1;
}

int re_editor_rule_action(const ReEditorDocument *document, uint32_t rule, uint32_t action,
                          ReEditorRuleActionView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || rule >= p->interactions.rule_count ||
        action >= p->interactions.rules[rule].action_count)
        return 0;
    const ReRuleAction *value = &p->interactions.rules[rule].actions[action];
    *out = (ReEditorRuleActionView){.kind = (int)value->kind};
    (void)copy_text(out->target, sizeof(out->target), value->target);
    value_text(value->value, out->value, sizeof(out->value));
    return 1;
}

int re_editor_dialogue_choice(const ReEditorDocument *document, uint32_t dialogue, uint32_t choice,
                              ReEditorDialogueChoiceView *out) {
    const ReProject *p = current_const(document);
    if (!p || !out || dialogue >= p->interactions.dialogue_count ||
        choice >= p->interactions.dialogues[dialogue].choice_count)
        return 0;
    const ReDialogueChoice *value = &p->interactions.dialogues[dialogue].choices[choice];
    *out = (ReEditorDialogueChoiceView){.condition_value = value->condition_value};
    (void)copy_text(out->id, sizeof(out->id), value->id);
    (void)copy_text(out->text, sizeof(out->text), value->text);
    (void)copy_text(out->next, sizeof(out->next), value->next);
    (void)copy_text(out->condition_variable, sizeof(out->condition_variable),
                    value->condition_variable);
    return 1;
}

static bool set_project(ReProject *p, const char *key, const char *value) {
    if (strcmp(key, "name") == 0)
        return copy_text(p->name, sizeof(p->name), value);
    return false;
}

static bool set_sector(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->world.sector_count)
        return false;
    ReSector *s = &p->world.sectors[index];
    float number;
    int integer;
    if (strcmp(key, "floor") == 0 && parse_float(value, &number) && number < s->ceiling) {
        s->floor = number;
        return true;
    }
    if (strcmp(key, "ceiling") == 0 && parse_float(value, &number) && number > s->floor) {
        s->ceiling = number;
        return true;
    }
    if (strcmp(key, "light") == 0 && parse_float(value, &number) && number >= 0 && number <= 1) {
        s->light = number;
        return true;
    }
    if (parse_int(value, &integer) && integer >= 0 && integer < RE_MAX_MATERIALS) {
        if (strcmp(key, "wall_material") == 0)
            s->wall_material = integer;
        else if (strcmp(key, "floor_material") == 0)
            s->floor_material = integer;
        else if (strcmp(key, "ceiling_material") == 0)
            s->ceiling_material = integer;
        else
            return false;
        return true;
    }
    return false;
}

static bool set_marker(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->world.marker_count)
        return false;
    ReMarker *m = &p->world.markers[index];
    float number;
    if (strcmp(key, "id") == 0)
        return copy_text(m->id, sizeof(m->id), value);
    if (strcmp(key, "definition") == 0)
        return copy_text(m->definition, sizeof(m->definition), value);
    if (!parse_float(value, &number))
        return false;
    if (strcmp(key, "x") == 0)
        m->position.x = number;
    else if (strcmp(key, "y") == 0)
        m->position.y = number;
    else if (strcmp(key, "z") == 0)
        m->position.z = number;
    else if (strcmp(key, "yaw") == 0)
        m->yaw = number;
    else
        return false;
    return true;
}

static bool set_barrier(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->world.barrier_count)
        return false;
    ReBarrier *b = &p->world.barriers[index];
    float number;
    if (strcmp(key, "id") == 0)
        return copy_text(b->id, sizeof(b->id), value);
    if (!parse_float(value, &number))
        return false;
    if (strcmp(key, "open_fraction") == 0 && number >= 0 && number <= 1)
        b->open_fraction = number;
    else if (strcmp(key, "health") == 0 && number >= 0)
        b->health = number;
    else
        return false;
    return true;
}

static bool set_character(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->character_count)
        return false;
    ReCharacterDef *c = &p->characters[index];
    float number;
    int integer;
    bool boolean;
    if (strcmp(key, "id") == 0)
        return copy_text(c->id, sizeof(c->id), value);
    if (strcmp(key, "sprite") == 0)
        return copy_text(c->sprite, sizeof(c->sprite), value);
    if (parse_bool(value, &boolean)) {
        if (strcmp(key, "invulnerable") == 0)
            c->invulnerable = boolean;
        else if (strcmp(key, "can_be_stunned") == 0)
            c->can_be_stunned = boolean;
        else if (strcmp(key, "can_open_doors") == 0)
            c->can_open_doors = boolean;
        else if (strcmp(key, "capture_game_over") == 0)
            c->capture_game_over = boolean;
        else
            return false;
        return true;
    }
    if (parse_int(value, &integer)) {
        if (strcmp(key, "max_health") == 0 && integer > 0)
            c->max_health = integer;
        else if (strcmp(key, "attack_damage") == 0 && integer >= 0)
            c->attack_damage = integer;
        else if (strcmp(key, "cell_width") == 0 && integer > 0)
            c->cell_width = integer;
        else if (strcmp(key, "cell_height") == 0 && integer > 0)
            c->cell_height = integer;
        else
            integer = INT_MIN;
        if (integer != INT_MIN)
            return true;
    }
    if (!parse_float(value, &number) || number < 0)
        return false;
    if (strcmp(key, "speed") == 0)
        c->speed = number;
    else if (strcmp(key, "sight_range") == 0)
        c->sight_range = number;
    else if (strcmp(key, "capture_range") == 0)
        c->capture_range = number;
    else if (strcmp(key, "attack_range") == 0)
        c->attack_range = number;
    else
        return false;
    return true;
}

static bool set_rule(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->interactions.rule_count)
        return false;
    ReRuleDefinition *r = &p->interactions.rules[index];
    int integer;
    float number;
    bool boolean;
    if (strcmp(key, "id") == 0)
        return copy_text(r->id, sizeof(r->id), value);
    if (strcmp(key, "source") == 0)
        return copy_text(r->source, sizeof(r->source), value);
    if (strcmp(key, "priority") == 0 && parse_int(value, &integer)) {
        r->priority = integer;
        return true;
    }
    if (strcmp(key, "cooldown") == 0 && parse_float(value, &number) && number >= 0) {
        r->cooldown = number;
        return true;
    }
    if (strcmp(key, "once") == 0 && parse_bool(value, &boolean)) {
        r->once = boolean;
        return true;
    }
    return false;
}

static bool set_dialogue(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->interactions.dialogue_count)
        return false;
    ReDialogueNode *d = &p->interactions.dialogues[index];
    bool boolean;
    if (strcmp(key, "id") == 0)
        return copy_text(d->id, sizeof(d->id), value);
    if (strcmp(key, "speaker") == 0)
        return copy_text(d->speaker, sizeof(d->speaker), value);
    if (strcmp(key, "text") == 0)
        return copy_text(d->text, sizeof(d->text), value);
    if (strcmp(key, "next") == 0)
        return copy_text(d->next, sizeof(d->next), value);
    if (strcmp(key, "pauses_world") == 0 && parse_bool(value, &boolean)) {
        d->pauses_world = boolean;
        return true;
    }
    return false;
}

static bool set_trigger(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->interactions.trigger_count)
        return false;
    ReTriggerVolume *trigger = &p->interactions.triggers[index];
    float number;
    bool boolean;
    if (strcmp(key, "id") == 0)
        return copy_text(trigger->id, sizeof(trigger->id), value);
    if (strcmp(key, "once") == 0 && parse_bool(value, &boolean)) {
        trigger->once = boolean;
        return true;
    }
    if (!parse_float(value, &number))
        return false;
    if (strcmp(key, "x") == 0)
        trigger->center.x = number;
    else if (strcmp(key, "y") == 0)
        trigger->center.y = number;
    else if (strcmp(key, "z") == 0)
        trigger->center.z = number;
    else if (strcmp(key, "size_x") == 0 && number > 0)
        trigger->half_size.x = number;
    else if (strcmp(key, "size_y") == 0 && number > 0)
        trigger->half_size.y = number;
    else if (strcmp(key, "size_z") == 0 && number > 0)
        trigger->half_size.z = number;
    else if (strcmp(key, "radius") == 0 && number > 0)
        trigger->radius = number;
    else
        return false;
    return true;
}

static bool set_light(ReProject *p, uint32_t index, const char *key, const char *value) {
    if (index >= p->interactions.light_count)
        return false;
    ReLight *light = &p->interactions.lights[index];
    float number;
    bool boolean;
    if (strcmp(key, "id") == 0)
        return copy_text(light->id, sizeof(light->id), value);
    if (strcmp(key, "enabled") == 0 && parse_bool(value, &boolean)) {
        light->initially_enabled = boolean;
        return true;
    }
    if (!parse_float(value, &number))
        return false;
    if (strcmp(key, "x") == 0)
        light->position.x = number;
    else if (strcmp(key, "y") == 0)
        light->position.y = number;
    else if (strcmp(key, "z") == 0)
        light->position.z = number;
    else if (strcmp(key, "red") == 0 && number >= 0 && number <= 1)
        light->color.x = number;
    else if (strcmp(key, "green") == 0 && number >= 0 && number <= 1)
        light->color.y = number;
    else if (strcmp(key, "blue") == 0 && number >= 0 && number <= 1)
        light->color.z = number;
    else if (strcmp(key, "radius") == 0 && number > 0)
        light->radius = number;
    else if (strcmp(key, "intensity") == 0 && number >= 0)
        light->intensity = number;
    else if (strcmp(key, "yaw") == 0)
        light->yaw = number;
    else if (strcmp(key, "cone") == 0 && number > 0 && number <= RE_PI * 2)
        light->cone = number;
    else if (strcmp(key, "flicker") == 0 && number >= 0 && number <= 1)
        light->flicker = number;
    else
        return false;
    return true;
}

int re_editor_set_property(ReEditorDocument *document, int kind, uint32_t index,
                           const char *property, const char *value, ReError *error) {
    if (!document || !property || !value || !error)
        return fail(error, "Edición inválida");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    bool changed = false;
    switch (kind) {
    case RE_EDITOR_PROJECT:
        changed = set_project(project, property, value);
        break;
    case RE_EDITOR_SECTOR:
        changed = set_sector(project, index, property, value);
        break;
    case RE_EDITOR_MARKER:
        changed = set_marker(project, index, property, value);
        break;
    case RE_EDITOR_BARRIER:
        changed = set_barrier(project, index, property, value);
        break;
    case RE_EDITOR_CHARACTER:
        changed = set_character(project, index, property, value);
        break;
    case RE_EDITOR_RULE:
        changed = set_rule(project, index, property, value);
        break;
    case RE_EDITOR_DIALOGUE:
        changed = set_dialogue(project, index, property, value);
        break;
    case RE_EDITOR_TRIGGER:
        changed = set_trigger(project, index, property, value);
        break;
    case RE_EDITOR_LIGHT:
        changed = set_light(project, index, property, value);
        break;
    default:
        break;
    }
    if (!changed) {
        cancel_command(document);
        return fail(error, "La propiedad o el valor no son válidos");
    }
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

int re_editor_undo(ReEditorDocument *document) {
    if (!document || document->cursor == 0)
        return 0;
    document->cursor--;
    document->revision++;
    return 1;
}

int re_editor_redo(ReEditorDocument *document) {
    if (!document || document->cursor + 1u >= document->count)
        return 0;
    document->cursor++;
    document->revision++;
    return 1;
}

static int cancel_fail(ReEditorDocument *document, ReError *error, const char *message) {
    char preserved[sizeof(error->message)];
    (void)snprintf(preserved, sizeof(preserved), "%s", message);
    cancel_command(document);
    return fail(error, preserved);
}

static bool marker_id_exists(const ReWorld *world, const char *id) {
    for (size_t i = 0; i < world->marker_count; i++)
        if (strcmp(world->markers[i].id, id) == 0)
            return true;
    return false;
}

static bool make_marker_id(const ReWorld *world, const char *definition, char *out,
                           size_t capacity) {
    for (unsigned int suffix = 1; suffix < 10000; suffix++) {
        int written = snprintf(out, capacity, "%.20s-%u", definition, suffix);
        if (written > 0 && (size_t)written < capacity && !marker_id_exists(world, out))
            return true;
    }
    return false;
}

int re_editor_create_marker(ReEditorDocument *document, const char *kind, const char *definition,
                            float x, float y, uint32_t *out_index, ReError *error) {
    if (!document || !kind || !definition || !out_index || !error || !isfinite(x) || !isfinite(y) ||
        kind[0] == '\0' || definition[0] == '\0')
        return fail(error, "Datos inválidos para colocar la entidad");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    ReWorld *world = &project->world;
    if (world->marker_count >= RE_MAX_MARKERS)
        return cancel_fail(document, error, "El nivel alcanzó el límite de entidades");
    int sector = re_world_sector(world, re_v2(x, y), -1);
    if (sector < 0)
        return cancel_fail(document, error, "Suelta la entidad dentro de una habitación");
    ReMarker marker = {.sector = sector,
                       .position = re_v3(x, y, world->sectors[sector].floor),
                       .yaw = 0,
                       .source_line = 0};
    if (!copy_text(marker.kind, sizeof(marker.kind), kind) ||
        !copy_text(marker.definition, sizeof(marker.definition), definition) ||
        !make_marker_id(world, definition, marker.id, sizeof(marker.id)))
        return cancel_fail(document, error, "Nombre de entidad demasiado largo");
    size_t index = world->marker_count;
    world->markers[index] = marker;
    world->marker_count++;
    if (!re_world_validate(world, error))
        return cancel_fail(document, error, error->message);
    *out_index = (uint32_t)index;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

/* Detectar un tramo común mediante proyección sobre la primera pared. Al
 * enlazar, almacenamos ambos intervalos: evita asumir habitaciones iguales. */
static void link_room(ReWorld *world, size_t added) {
    ReSector *room = &world->sectors[added];
    for (size_t i = 0; i < added; i++) {
        ReSector *other = &world->sectors[i];
        if (fminf(room->ceiling, other->ceiling) - fmaxf(room->floor, other->floor) < 1.8f)
            continue;
        for (size_t e = 0; e < room->count; e++) {
            ReVec2 a = room->vertices[e], ab = re_sub2(room->vertices[(e + 1) % room->count], a);
            float length2 = re_dot2(ab, ab);
            for (size_t f = 0; f < other->count; f++) {
                if (room->neighbor[e] >= 0 || other->neighbor[f] >= 0)
                    continue;
                ReVec2 c = other->vertices[f],
                       cd = re_sub2(other->vertices[(f + 1) % other->count], c);
                if (re_dot2(ab, cd) >= 0 || fabsf(re_cross2(ab, re_sub2(c, a))) > RE_EPSILON ||
                    fabsf(re_cross2(ab, cd)) > RE_EPSILON)
                    continue;
                float t0 = re_dot2(re_sub2(c, a), ab) / length2;
                float t1 = re_dot2(re_sub2(re_add2(c, cd), a), ab) / length2;
                float lo = fmaxf(0, fminf(t0, t1)), hi = fminf(1, fmaxf(t0, t1));
                if ((hi - lo) * sqrtf(length2) < .7f)
                    continue;
                room->neighbor[e] = (int)i;
                room->neighbor_edge[e] = (int)f;
                room->portal_start[e] = lo;
                room->portal_end[e] = hi;
                other->neighbor[f] = (int)added;
                other->neighbor_edge[f] = (int)e;
                float c2 = re_dot2(cd, cd);
                other->portal_start[f] =
                    re_clamp(re_dot2(re_sub2(re_add2(a, re_scale2(ab, hi)), c), cd) / c2, 0, 1);
                other->portal_end[f] =
                    re_clamp(re_dot2(re_sub2(re_add2(a, re_scale2(ab, lo)), c), cd) / c2, 0, 1);
            }
        }
    }
}

int re_editor_create_room(ReEditorDocument *document, float x0, float y0, float x1, float y1,
                          float floor, float ceiling, uint32_t *out_index, ReError *error) {
    if (!document || !out_index || !error || !isfinite(x0) || !isfinite(y0) || !isfinite(x1) ||
        !isfinite(y1) || !isfinite(floor) || !isfinite(ceiling) || x1 - x0 < 1 || y1 - y0 < 1 ||
        ceiling - floor < 1.8f)
        return fail(error, "La habitación necesita al menos 1 × 1 m y 1,8 m de altura");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    ReWorld *world = &project->world;
    if (world->sector_count == RE_MAX_SECTORS)
        return cancel_fail(document, error, "Se alcanzó el límite de sectores");
    size_t index = world->sector_count++;
    ReSector *room = &world->sectors[index];
    *room = (ReSector){.floor = floor,
                       .ceiling = ceiling,
                       .light = .75f,
                       .floor_material = 1,
                       .ceiling_material = 2,
                       .count = 4,
                       .vertices = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}};
    for (size_t e = 0; e < 4; e++) {
        room->neighbor[e] = -1;
        room->neighbor_edge[e] = -1;
        room->portal_end[e] = 1;
    }
    link_room(world, index);
    if (!re_world_validate(world, error))
        return cancel_fail(document, error, error->message);
    *out_index = (uint32_t)index;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

int re_editor_add_barrier(ReEditorDocument *document, uint32_t sector, uint32_t edge, int kind,
                          float width, uint32_t *out_index, ReError *error) {
    if (!document || !out_index || !error || kind < 0 || kind > 1 || !isfinite(width) ||
        width < .7f)
        return fail(error, "Puerta o ventana inválida");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    ReWorld *world = &project->world;
    if (sector >= world->sector_count || edge >= world->sectors[sector].count ||
        world->barrier_count == RE_MAX_BARRIERS)
        return cancel_fail(document, error, "Pared o capacidad inválida");
    ReSector *room = &world->sectors[sector];
    if (room->neighbor[edge] < 0 || re_world_barrier_at(world, (int)sector, (int)edge) >= 0)
        return cancel_fail(document, error, "Elige una abertura libre entre dos habitaciones");
    size_t other_edge = (size_t)room->neighbor_edge[edge];
    ReSector *other = &world->sectors[room->neighbor[edge]];
    ReVec2 a = room->vertices[edge], ab = re_sub2(room->vertices[(edge + 1) % room->count], a);
    float length = re_length2(ab);
    float available = (room->portal_end[edge] - room->portal_start[edge]) * length;
    if (width > available + RE_EPSILON)
        return cancel_fail(document, error, "La abertura es más estrecha que la puerta");
    float middle = (room->portal_start[edge] + room->portal_end[edge]) * .5f;
    room->portal_start[edge] = middle - width / (2 * length);
    room->portal_end[edge] = middle + width / (2 * length);
    ReVec2 c = other->vertices[other_edge];
    ReVec2 cd = re_sub2(other->vertices[(other_edge + 1) % other->count], c);
    other->portal_start[other_edge] =
        re_dot2(re_sub2(re_add2(a, re_scale2(ab, room->portal_end[edge])), c), cd) /
        re_dot2(cd, cd);
    other->portal_end[other_edge] =
        re_dot2(re_sub2(re_add2(a, re_scale2(ab, room->portal_start[edge])), c), cd) /
        re_dot2(cd, cd);
    size_t index = world->barrier_count++;
    ReBarrier *barrier = &world->barriers[index];
    *barrier = (ReBarrier){.kind = (enum ReBarrierKind)kind,
                           .sector = (int)sector,
                           .edge = (int)edge,
                           .material = kind ? 5 : 3,
                           .blocks = kind ? 5u : 7u,
                           .health = kind ? 25 : 0};
    /* ID generado sin depender de un índice que pueda cambiar al borrar. */
    for (unsigned int suffix = 1; suffix < 10000; suffix++) {
        (void)snprintf(barrier->id, sizeof(barrier->id), "barrier_%u", suffix);
        bool used = false;
        for (size_t i = 0; i < index; i++)
            if (strcmp(world->barriers[i].id, barrier->id) == 0)
                used = true;
        if (!used)
            break;
    }
    if (!re_world_validate(world, error))
        return cancel_fail(document, error, error->message);
    *out_index = (uint32_t)index;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

/* La cota elegida por el editor elimina la ambigüedad de una planta superior.
 * La selección XY histórica se conserva sólo para llamantes antiguos. */
int re_editor_create_marker_at(ReEditorDocument *document, const char *kind, const char *definition,
                               float x, float y, float floor, uint32_t *out_index, ReError *error) {
    if (!document || !error || !out_index || !kind || !definition || !isfinite(x) || !isfinite(y) ||
        !isfinite(floor))
        return fail(error, "Posición inválida");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    ReWorld *world = &project->world;
    int sector = re_world_sector_at(world, re_v3(x, y, floor), -1);
    if (sector < 0 || world->marker_count >= RE_MAX_MARKERS)
        return cancel_fail(document, error, "Suelta el objeto dentro de la planta activa");
    ReMarker marker = {.sector = sector, .position = {x, y, world->sectors[sector].floor}};
    if (!copy_text(marker.kind, sizeof(marker.kind), kind) ||
        !copy_text(marker.definition, sizeof(marker.definition), definition) ||
        !make_marker_id(world, definition, marker.id, sizeof(marker.id)))
        return cancel_fail(document, error, "Nombre demasiado largo");
    size_t index = world->marker_count++;
    world->markers[index] = marker;
    if (!re_world_validate(world, error))
        return cancel_fail(document, error, error->message);
    *out_index = (uint32_t)index;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

int re_editor_move_marker(ReEditorDocument *document, uint32_t index, float x, float y,
                          ReError *error) {
    if (!document || !error || !isfinite(x) || !isfinite(y))
        return fail(error, "Movimiento inválido");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    ReWorld *world = &project->world;
    if (index >= world->marker_count)
        return cancel_fail(document, error, "La entidad ya no existe");
    ReMarker *marker = &world->markers[index];
    /* La altura pertenece a la instancia: arrastrar en planta no permite
     * saltar accidentalmente a una habitación superpuesta. */
    int sector = re_world_sector_at(world, re_v3(x, y, marker->position.z + .01f), marker->sector);
    if (sector < 0)
        return cancel_fail(document, error, "La entidad debe permanecer dentro de una habitación");
    marker->sector = sector;
    marker->position = re_v3(x, y, world->sectors[sector].floor);
    if (!re_world_validate(world, error))
        return cancel_fail(document, error, error->message);
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

int re_editor_duplicate_marker(ReEditorDocument *document, uint32_t index, uint32_t *out_index,
                               ReError *error) {
    if (!document || !out_index || !error)
        return fail(error, "Duplicación inválida");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    ReWorld *world = &project->world;
    if (index >= world->marker_count || world->marker_count >= RE_MAX_MARKERS)
        return cancel_fail(document, error, "No se puede duplicar esta entidad");
    ReMarker marker = world->markers[index];
    static const ReVec2 offsets[] = {{.5f, .5f}, {.5f, -.5f}, {-.5f, .5f}, {-.5f, -.5f}};
    bool placed = false;
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        ReVec2 candidate = re_add2(re_v2(marker.position.x, marker.position.y), offsets[i]);
        int sector = re_world_sector_at(
            world, re_v3(candidate.x, candidate.y, marker.position.z + .01f), marker.sector);
        if (sector >= 0) {
            marker.sector = sector;
            marker.position = re_v3(candidate.x, candidate.y, world->sectors[sector].floor);
            placed = true;
            break;
        }
    }
    if (!placed || !make_marker_id(world, marker.definition, marker.id, sizeof(marker.id)))
        return cancel_fail(document, error, "No hay espacio cercano para la copia");
    size_t duplicate = world->marker_count;
    world->markers[duplicate] = marker;
    world->marker_count++;
    if (!re_world_validate(world, error))
        return cancel_fail(document, error, error->message);
    *out_index = (uint32_t)duplicate;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

int re_editor_delete_marker(ReEditorDocument *document, uint32_t index, ReError *error) {
    if (!document || !error)
        return fail(error, "Eliminación inválida");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    ReWorld *world = &project->world;
    if (index >= world->marker_count)
        return cancel_fail(document, error, "La entidad ya no existe");
    const char *id = world->markers[index].id;
    for (size_t rule_index = 0; rule_index < project->interactions.rule_count; rule_index++) {
        const ReRuleDefinition *rule = &project->interactions.rules[rule_index];
        if (strcmp(rule->source, id) == 0)
            return cancel_fail(
                document, error,
                "La entidad se usa como origen de una regla; elimina esa conexión primero");
        for (size_t action = 0; action < rule->action_count; action++)
            if (strcmp(rule->actions[action].target, id) == 0)
                return cancel_fail(
                    document, error,
                    "Una regla actúa sobre esta entidad; elimina esa conexión primero");
    }
    size_t remaining = world->marker_count - (size_t)index - 1u;
    if (remaining > 0)
        (void)memmove(&world->markers[index], &world->markers[index + 1u],
                      remaining * sizeof(world->markers[0]));
    world->marker_count--;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

static bool rule_id_exists(const ReInteractionDefinitions *definitions, const char *id) {
    for (size_t i = 0; i < definitions->rule_count; i++)
        if (strcmp(definitions->rules[i].id, id) == 0)
            return true;
    return false;
}

int re_editor_add_drop_rule(ReEditorDocument *document, uint32_t marker_index, const char *item,
                            uint32_t *out_rule, ReError *error) {
    if (!document || !item || !out_rule || !error || item[0] == '\0')
        return fail(error, "Objeto de recompensa inválido");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    if (marker_index >= project->world.marker_count ||
        project->interactions.rule_count >= RE_MAX_RULES)
        return cancel_fail(document, error, "No se puede crear la regla para esta entidad");
    const ReMarker *marker = &project->world.markers[marker_index];
    if (strcmp(marker->kind, "actor") != 0)
        return cancel_fail(document, error,
                           "Sólo un enemigo o personaje puede soltar un objeto al morir");
    ReRuleDefinition rule = {
        .event = RE_LOGIC_ENTITY_DIED, .priority = 100, .once = true, .action_count = 1};
    if (!copy_text(rule.source, sizeof(rule.source), marker->id) ||
        !copy_text(rule.actions[0].target, sizeof(rule.actions[0].target), item))
        return cancel_fail(document, error, "El identificador no cabe en la regla");
    rule.actions[0].kind = RE_RULE_SPAWN_PICKUP;
    rule.actions[0].value = (ReValue){.kind = RE_VALUE_INT, .as.integer = 1};
    for (unsigned int suffix = 1; suffix < 10000; suffix++) {
        (void)snprintf(rule.id, sizeof(rule.id), "drop_%u", suffix);
        if (!rule_id_exists(&project->interactions, rule.id))
            break;
    }
    if (rule_id_exists(&project->interactions, rule.id))
        return cancel_fail(document, error, "No se pudo generar un identificador para la regla");
    size_t index = project->interactions.rule_count;
    project->interactions.rules[index] = rule;
    project->interactions.rule_count++;
    *out_rule = (uint32_t)index;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

static bool barrier_exists(const ReWorld *world, const char *id) {
    for (size_t i = 0; i < world->barrier_count; i++)
        if (strcmp(world->barriers[i].id, id) == 0)
            return true;
    return false;
}

static bool dialogue_exists(const ReInteractionDefinitions *definitions, const char *id) {
    for (size_t i = 0; i < definitions->dialogue_count; i++)
        if (strcmp(definitions->dialogues[i].id, id) == 0)
            return true;
    return false;
}

int re_editor_add_interaction_rule(ReEditorDocument *document, uint32_t marker_index, int action,
                                   const char *target, const char *value, uint32_t *out_rule,
                                   ReError *error) {
    if (!document || !target || !value || !out_rule || !error)
        return fail(error, "Evento de interacción inválido");
    ReProject *project = begin_command(document);
    if (!project)
        return fail(error, "No se pudo iniciar el comando");
    if (marker_index >= project->world.marker_count ||
        project->interactions.rule_count >= RE_MAX_RULES)
        return cancel_fail(document, error, "No se puede crear la interacción");

    bool supported = action == RE_RULE_SHOW_MESSAGE || action == RE_RULE_OPEN_BARRIER ||
                     action == RE_RULE_START_DIALOGUE || action == RE_RULE_GIVE_ITEM;
    if (!supported)
        return cancel_fail(document, error, "La acción no está disponible en el asistente rápido");
    if (action == RE_RULE_SHOW_MESSAGE && value[0] == '\0')
        return cancel_fail(document, error, "Escribe el mensaje que verá el jugador");
    if (action == RE_RULE_OPEN_BARRIER && !barrier_exists(&project->world, target))
        return cancel_fail(document, error, "La puerta o ventana seleccionada no existe");
    if (action == RE_RULE_START_DIALOGUE && !dialogue_exists(&project->interactions, target))
        return cancel_fail(document, error, "La conversación seleccionada no existe");
    if (action == RE_RULE_GIVE_ITEM && target[0] == '\0')
        return cancel_fail(document, error, "Selecciona el objeto que recibirá el jugador");

    const ReMarker *marker = &project->world.markers[marker_index];
    ReRuleDefinition rule = {.event = RE_LOGIC_INTERACT, .priority = 100, .action_count = 1};
    if (!copy_text(rule.source, sizeof(rule.source), marker->id))
        return cancel_fail(document, error, "El identificador del origen es demasiado largo");
    rule.actions[0].kind = (enum ReActionKind)action;
    const char *action_target = action == RE_RULE_SHOW_MESSAGE ? "-" : target;
    if (!copy_text(rule.actions[0].target, sizeof(rule.actions[0].target), action_target))
        return cancel_fail(document, error, "El identificador del destino es demasiado largo");
    if (action == RE_RULE_SHOW_MESSAGE) {
        rule.actions[0].value.kind = RE_VALUE_TEXT;
        if (!copy_text(rule.actions[0].value.as.text, sizeof(rule.actions[0].value.as.text), value))
            return cancel_fail(document, error, "El mensaje admite hasta 63 caracteres");
    } else if (action == RE_RULE_GIVE_ITEM) {
        rule.actions[0].value = (ReValue){.kind = RE_VALUE_INT, .as.integer = 1};
    }
    for (unsigned int suffix = 1; suffix < 10000; suffix++) {
        (void)snprintf(rule.id, sizeof(rule.id), "interact_%u", suffix);
        if (!rule_id_exists(&project->interactions, rule.id))
            break;
    }
    if (rule_id_exists(&project->interactions, rule.id))
        return cancel_fail(document, error, "No se pudo generar un identificador para la regla");
    size_t index = project->interactions.rule_count;
    project->interactions.rules[index] = rule;
    project->interactions.rule_count++;
    *out_rule = (uint32_t)index;
    commit_command(document);
    *error = (ReError){0};
    return 1;
}

int re_editor_save(ReEditorDocument *document, ReError *error) {
    ReProject *project = current(document);
    if (!project || !error)
        return fail(error, "No hay proyecto para guardar");
    if (!re_project_save(project, error))
        return 0;
    document->saved_cursor = document->cursor;
    document->revision++;
    *error = (ReError){0};
    return 1;
}
