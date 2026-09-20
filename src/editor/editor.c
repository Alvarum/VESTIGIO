/* Implementación del documento transaccional usado por RetroForge Studio.
 *
 * El historial almacena proyectos completos. Los límites fijos del motor hacen
 * que esta estrategia sea predecible, fácil de auditar y suficientemente rápida
 * para un editor personal. Si los límites crecen, esta implementación puede
 * sustituirse por deltas sin cambiar la ABI pública. */
#include "retro/editor.h"
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
    uint64_t revision;
};

static ReProject *current(ReEditorDocument *document) {
    return document ? &document->states[document->cursor] : nullptr;
}

static const ReProject *current_const(const ReEditorDocument *document) {
    return document ? &document->states[document->cursor] : nullptr;
}

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
    if (!document)
        return nullptr;
    if (document->cursor + 1u < document->count)
        document->count = document->cursor + 1u;
    if (document->count == RE_EDITOR_HISTORY) {
        (void)memmove(&document->states[0], &document->states[1],
                      (RE_EDITOR_HISTORY - 1u) * sizeof(*document->states));
        document->count--;
        document->cursor--;
        if (document->saved_cursor > 0)
            document->saved_cursor--;
        else
            document->saved_cursor = SIZE_MAX;
    }
    document->states[document->count] = document->states[document->cursor];
    document->cursor = document->count++;
    return current(document);
}

static void cancel_command(ReEditorDocument *document) {
    if (!document || document->cursor == 0)
        return;
    document->count--;
    document->cursor--;
}

int re_editor_open(const char *manifest, ReEditorDocument **out, ReError *error) {
    if (!manifest || !out || !error)
        return fail(error, "Argumentos inválidos al abrir el proyecto");
    ReEditorDocument *document = calloc(1, sizeof(*document));
    if (!document)
        return fail(error, "No hay memoria para el documento");
    document->states = calloc(RE_EDITOR_HISTORY, sizeof(*document->states));
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
    *out = (ReEditorBarrierView){.kind = b->kind,
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
                                   .tracking = c->tracking,
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
    *out = (ReEditorRuleView){.event = r->event,
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
    *out = (ReEditorTriggerView){.shape = t->shape,
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
    *out = (ReEditorLightView){.kind = l->kind,
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
    default:
        break;
    }
    if (!changed) {
        cancel_command(document);
        return fail(error, "La propiedad o el valor no son válidos");
    }
    document->revision++;
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

int re_editor_save(ReEditorDocument *document, ReError *error) {
    ReProject *project = current(document);
    if (!project || !error)
        return fail(error, "No hay proyecto para guardar");
    if (!re_project_save(project, error))
        return 0;
    char path[RE_PROJECT_PATH * 2];
    for (size_t i = 0; i < project->character_count; i++) {
        if (!re_project_path(project, project->actor_files[i], path, sizeof(path)) ||
            !re_character_save(path, &project->characters[i], error))
            return 0;
    }
    document->saved_cursor = document->cursor;
    document->revision++;
    *error = (ReError){0};
    return 1;
}
