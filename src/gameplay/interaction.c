/* Runtime determinista de interacciones, diálogos y persistencia.
 *
 * El parser sólo trabaja al cargar. El tick usa capacidades fijas y una cola
 * FIFO. Una regla jamás llama recursivamente a otra: cualquier evento nuevo se
 * coloca al final de la cola. Esta propiedad hace reproducibles los resultados
 * y permite cortar ciclos de reglas con un presupuesto explícito. */
#include "retro/interaction.h"
#include <ctype.h>
#include <errno.h>
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
static bool replace_file(const char *temporary, const char *destination);

/* Divide una línea conservando espacios dentro de comillas. No interpreta
 * escapes: los formatos usan UTF-8 literal y comillas sólo como delimitador. */
static size_t tokens(char *line, char **out, size_t capacity) {
    size_t count = 0;
    char *cursor = line;
    while (*cursor) {
        while (isspace((unsigned char)*cursor))
            cursor++;
        if (!*cursor || *cursor == '#')
            break;
        if (count == capacity)
            return capacity + 1u;
        if (*cursor == '"') {
            cursor++;
            out[count++] = cursor;
            while (*cursor && *cursor != '"')
                cursor++;
            if (*cursor)
                *cursor++ = '\0';
        } else {
            out[count++] = cursor;
            while (*cursor && !isspace((unsigned char)*cursor) && *cursor != '#')
                cursor++;
            if (*cursor == '#') {
                *cursor = '\0';
                break;
            }
            if (*cursor)
                *cursor++ = '\0';
        }
    }
    return count;
}

static bool boolean(const char *text, bool *out) {
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0)
        *out = true;
    else if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0)
        *out = false;
    else
        return false;
    return true;
}
static bool integer(const char *text, int *out) {
    char *end = nullptr;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < INT32_MIN || value > INT32_MAX)
        return false;
    *out = (int)value;
    return true;
}
static bool real(const char *text, float *out) {
    char *end = nullptr;
    errno = 0;
    float value = strtof(text, &end);
    if (errno || end == text || *end || !isfinite(value))
        return false;
    *out = value;
    return true;
}
static bool copy_text(char *destination, size_t capacity, const char *source) {
    size_t length = strlen(source);
    if (length >= capacity)
        return false;
    memcpy(destination, source, length + 1u);
    return true;
}

static bool parse_value(const char *kind, const char *text, ReValue *out) {
    ReValue value = {0};
    if (strcmp(kind, "bool") == 0) {
        value.kind = RE_VALUE_BOOL;
        if (!boolean(text, &value.as.boolean))
            return false;
    } else if (strcmp(kind, "int") == 0) {
        value.kind = RE_VALUE_INT;
        if (!integer(text, &value.as.integer))
            return false;
    } else if (strcmp(kind, "float") == 0) {
        value.kind = RE_VALUE_FLOAT;
        if (!real(text, &value.as.real))
            return false;
    } else if (strcmp(kind, "text") == 0) {
        value.kind = RE_VALUE_TEXT;
        if (!copy_text(value.as.text, sizeof(value.as.text), text))
            return false;
    } else if (strcmp(kind, "none") != 0)
        return false;
    *out = value;
    return true;
}

static int event_kind(const char *name) {
    static const char *const names[] = {
        "level_start",     "trigger_enter",     "trigger_stay",      "trigger_exit", "interact",
        "entity_damaged",  "entity_died",       "captured",          "item_picked",  "item_used",
        "dialogue_choice", "dialogue_finished", "objective_changed", "timer",        "animation",
        "barrier_changed", "player_died",       "game_loaded",       "custom"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(name, names[i]) == 0)
            return (int)i;
    return -1;
}
static int condition_kind(const char *name) {
    static const char *const names[] = {"variable", "item", "objective", "health", "lives"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(name, names[i]) == 0)
            return (int)i;
    return -1;
}
static int comparison_kind(const char *name) {
    static const char *const names[] = {"eq", "ne", "lt", "le", "gt", "ge"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(name, names[i]) == 0)
            return (int)i;
    return -1;
}
static int action_kind(const char *name) {
    static const char *const names[] = {
        "set_variable", "add_variable", "give_item",      "take_item", "set_objective",
        "message",      "open_barrier", "close_barrier",  "damage",    "heal",
        "set_lives",    "checkpoint",   "start_dialogue", "light",     "spawn_pickup",
        "emit",         "win",          "game_over"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (strcmp(name, names[i]) == 0)
            return (int)i;
    return -1;
}

static bool parse_rule_line(ReInteractionDefinitions *definitions, ReRuleDefinition **current,
                            char **part, size_t count, ReError *error, size_t line) {
    if (count == 7 && strcmp(part[0], "rule") == 0 && definitions->rule_count < RE_MAX_RULES) {
        int event = event_kind(part[2]);
        ReRuleDefinition rule = {0};
        if (event < 0 || !copy_text(rule.id, sizeof(rule.id), part[1]) ||
            !copy_text(rule.source, sizeof(rule.source), part[3]) ||
            !integer(part[4], &rule.priority) || !boolean(part[5], &rule.once) ||
            !real(part[6], &rule.cooldown) || rule.cooldown < 0)
            return fail(error, line, "Regla inválida");
        rule.event = (enum ReLogicEventKind)event;
        definitions->rules[definitions->rule_count] = rule;
        *current = &definitions->rules[definitions->rule_count++];
        return true;
    }
    if (count == 1 && strcmp(part[0], "end") == 0 && *current) {
        *current = nullptr;
        return true;
    }
    if (count == 6 && strcmp(part[0], "condition") == 0 && *current &&
        (*current)->condition_count < RE_MAX_RULE_CONDITIONS) {
        int kind = condition_kind(part[1]), comparison = comparison_kind(part[3]);
        ReRuleCondition condition = {0};
        if (kind < 0 || comparison < 0 ||
            !copy_text(condition.key, sizeof(condition.key), part[2]) ||
            !parse_value(part[4], part[5], &condition.value))
            return fail(error, line, "Condición inválida");
        condition.kind = (enum ReConditionKind)kind;
        condition.comparison = (enum ReCompareOp)comparison;
        (*current)->conditions[(*current)->condition_count++] = condition;
        return true;
    }
    if (count == 5 && strcmp(part[0], "action") == 0 && *current &&
        (*current)->action_count < RE_MAX_RULE_ACTIONS) {
        int kind = action_kind(part[1]);
        ReRuleAction action = {0};
        if (kind < 0 || !copy_text(action.target, sizeof(action.target), part[2]) ||
            !parse_value(part[3], part[4], &action.value))
            return fail(error, line, "Acción inválida");
        action.kind = (enum ReActionKind)kind;
        (*current)->actions[(*current)->action_count++] = action;
        return true;
    }
    return false;
}

bool re_interaction_load_rules(const char *path, ReInteractionDefinitions *out, ReError *error) {
    if (!path || !out || !error)
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return fail(error, 0, "No se pudo abrir retro_rules");
    ReInteractionDefinitions *candidate = malloc(sizeof(*candidate));
    if (!candidate) {
        (void)fclose(file);
        return fail(error, 0, "Memoria insuficiente para reglas");
    }
    *candidate = *out;
    bool header = false, ok = true;
    ReRuleDefinition *current = nullptr;
    size_t line_number = 0;
    char line[1024];
    while (ok && fgets(line, sizeof(line), file)) {
        line_number++;
        char *part[16];
        size_t count = tokens(line, part, 16);
        if (!count)
            continue;
        if (!header) {
            header = count == 2 && strcmp(part[0], "retro_rules") == 0 && strcmp(part[1], "1") == 0;
            if (!header)
                ok = fail(error, line_number, "Se esperaba retro_rules 1");
            continue;
        }
        if (parse_rule_line(candidate, &current, part, count, error, line_number))
            continue;
        if (current) {
            ok = fail(error, line_number, "Directiva inválida dentro de rule");
            continue;
        }
        if (count == 4 && strcmp(part[0], "variable") == 0 &&
            candidate->variable_count < RE_MAX_VARIABLES) {
            ReVariableDefinition definition = {0};
            ok = copy_text(definition.id, sizeof(definition.id), part[1]) &&
                 parse_value(part[2], part[3], &definition.initial);
            if (ok)
                candidate->variables[candidate->variable_count++] = definition;
        } else if (count == 4 && strcmp(part[0], "item") == 0 &&
                   candidate->item_count < RE_MAX_ITEMS) {
            ReItemDefinition definition = {0};
            int maximum = 0;
            ok = copy_text(definition.id, sizeof(definition.id), part[1]) &&
                 copy_text(definition.name, sizeof(definition.name), part[2]) &&
                 integer(part[3], &maximum) && maximum > 0;
            definition.max_stack = (unsigned int)maximum;
            if (ok)
                candidate->items[candidate->item_count++] = definition;
        } else if (count == 3 && strcmp(part[0], "objective") == 0 &&
                   candidate->objective_count < RE_MAX_OBJECTIVES) {
            ReObjectiveDefinition definition = {0};
            ok = copy_text(definition.id, sizeof(definition.id), part[1]) &&
                 copy_text(definition.title, sizeof(definition.title), part[2]);
            if (ok)
                candidate->objectives[candidate->objective_count++] = definition;
        } else if (count == 5 && strcmp(part[0], "trigger") == 0 &&
                   strcmp(part[2], "sector") == 0 && candidate->trigger_count < RE_MAX_TRIGGERS) {
            ReTriggerVolume trigger = {.shape = RE_TRIGGER_SECTOR};
            ok = copy_text(trigger.id, sizeof(trigger.id), part[1]) &&
                 integer(part[3], &trigger.sector) && boolean(part[4], &trigger.once);
            if (ok)
                candidate->triggers[candidate->trigger_count++] = trigger;
        } else if (count == 10 && strcmp(part[0], "trigger") == 0 && strcmp(part[2], "box") == 0 &&
                   candidate->trigger_count < RE_MAX_TRIGGERS) {
            ReTriggerVolume trigger = {.shape = RE_TRIGGER_BOX, .sector = -1};
            ok = copy_text(trigger.id, sizeof(trigger.id), part[1]) &&
                 real(part[3], &trigger.center.x) && real(part[4], &trigger.center.y) &&
                 real(part[5], &trigger.center.z) && real(part[6], &trigger.half_size.x) &&
                 real(part[7], &trigger.half_size.y) && real(part[8], &trigger.half_size.z) &&
                 boolean(part[9], &trigger.once);
            if (ok)
                candidate->triggers[candidate->trigger_count++] = trigger;
        } else if (count == 9 && strcmp(part[0], "trigger") == 0 &&
                   strcmp(part[2], "cylinder") == 0 && candidate->trigger_count < RE_MAX_TRIGGERS) {
            ReTriggerVolume trigger = {.shape = RE_TRIGGER_CYLINDER, .sector = -1};
            ok = copy_text(trigger.id, sizeof(trigger.id), part[1]) &&
                 real(part[3], &trigger.center.x) && real(part[4], &trigger.center.y) &&
                 real(part[5], &trigger.center.z) && real(part[6], &trigger.radius) &&
                 real(part[7], &trigger.half_size.z) && boolean(part[8], &trigger.once);
            if (ok)
                candidate->triggers[candidate->trigger_count++] = trigger;
        } else if ((count == 12 || count == 15) && strcmp(part[0], "light") == 0 &&
                   candidate->light_count < RE_MAX_LIGHTS) {
            ReLight light = {.kind = strcmp(part[2], "spot") == 0 ? RE_LIGHT_SPOT : RE_LIGHT_POINT};
            ok = (strcmp(part[2], "point") == 0 || strcmp(part[2], "spot") == 0) &&
                 copy_text(light.id, sizeof(light.id), part[1]) &&
                 real(part[3], &light.position.x) && real(part[4], &light.position.y) &&
                 real(part[5], &light.position.z) && real(part[6], &light.color.x) &&
                 real(part[7], &light.color.y) && real(part[8], &light.color.z) &&
                 real(part[9], &light.radius) && real(part[10], &light.intensity) &&
                 boolean(part[11], &light.initially_enabled);
            if (ok && count == 15)
                ok = real(part[12], &light.yaw) && real(part[13], &light.cone) &&
                     real(part[14], &light.flicker);
            if (ok)
                candidate->lights[candidate->light_count++] = light;
        } else
            ok = false;
        if (!ok && !error->message[0])
            (void)fail(error, line_number, "Directiva de reglas desconocida o inválida");
    }
    if (current && ok)
        ok = fail(error, line_number, "Falta end al final de rule");
    if (ferror(file) || fclose(file) != 0)
        ok = fail(error, line_number, "Error leyendo retro_rules");
    if (ok && !header)
        ok = fail(error, 0, "Archivo de reglas vacío");
    if (ok && !re_interaction_validate(candidate, error))
        ok = false;
    if (ok) {
        *out = *candidate;
        *error = (ReError){0};
    }
    free(candidate);
    return ok;
}

bool re_interaction_load_dialogues(const char *path, ReInteractionDefinitions *out,
                                   ReError *error) {
    if (!path || !out || !error)
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return fail(error, 0, "No se pudo abrir retro_dialogue");
    ReInteractionDefinitions *candidate = malloc(sizeof(*candidate));
    if (!candidate) {
        (void)fclose(file);
        return fail(error, 0, "Memoria insuficiente para diálogos");
    }
    *candidate = *out;
    bool header = false, ok = true;
    ReDialogueNode *current = nullptr;
    size_t line_number = 0;
    char line[1024];
    while (ok && fgets(line, sizeof(line), file)) {
        line_number++;
        char *part[10];
        size_t count = tokens(line, part, 10);
        if (!count)
            continue;
        if (!header) {
            header =
                count == 2 && strcmp(part[0], "retro_dialogue") == 0 && strcmp(part[1], "1") == 0;
            if (!header)
                ok = fail(error, line_number, "Se esperaba retro_dialogue 1");
        } else if (count == 7 && strcmp(part[0], "node") == 0 &&
                   candidate->dialogue_count < RE_MAX_DIALOGUE_NODES) {
            ReDialogueNode node = {0};
            ok = copy_text(node.id, sizeof(node.id), part[1]) &&
                 copy_text(node.speaker, sizeof(node.speaker), part[2]) &&
                 boolean(part[3], &node.pauses_world) &&
                 copy_text(node.text, sizeof(node.text), part[4]) && strcmp(part[5], "next") == 0 &&
                 copy_text(node.next, sizeof(node.next), part[6]);
            if (ok) {
                candidate->dialogues[candidate->dialogue_count] = node;
                current = &candidate->dialogues[candidate->dialogue_count++];
            }
        } else if (count == 7 && strcmp(part[0], "choice") == 0 && current &&
                   current->choice_count < RE_MAX_DIALOGUE_CHOICES) {
            ReDialogueChoice choice = {0};
            ok = copy_text(choice.id, sizeof(choice.id), part[1]) &&
                 copy_text(choice.text, sizeof(choice.text), part[2]) &&
                 copy_text(choice.next, sizeof(choice.next), part[3]) &&
                 copy_text(choice.condition_variable, sizeof(choice.condition_variable), part[4]) &&
                 strcmp(part[5], "bool") == 0 && boolean(part[6], &choice.condition_value);
            if (ok)
                current->choices[current->choice_count++] = choice;
        } else if (count == 1 && strcmp(part[0], "end") == 0)
            current = nullptr;
        else
            ok = fail(error, line_number, "Directiva de diálogo desconocida o inválida");
    }
    if (ferror(file) || fclose(file) != 0)
        ok = fail(error, line_number, "Error leyendo retro_dialogue");
    if (ok && !header)
        ok = fail(error, 0, "Archivo de diálogo vacío");
    if (ok && !re_interaction_validate(candidate, error))
        ok = false;
    if (ok) {
        *out = *candidate;
        *error = (ReError){0};
    }
    free(candidate);
    return ok;
}

static bool write_value(FILE *file, ReValue value) {
    switch (value.kind) {
    case RE_VALUE_BOOL:
        return fprintf(file, "bool %s", value.as.boolean ? "true" : "false") > 0;
    case RE_VALUE_INT:
        return fprintf(file, "int %d", value.as.integer) > 0;
    case RE_VALUE_FLOAT:
        return fprintf(file, "float %.9g", (double)value.as.real) > 0;
    case RE_VALUE_TEXT:
        return fprintf(file, "text \"%s\"", value.as.text) > 0;
    default:
        return fprintf(file, "none -") > 0;
    }
}
static const char *event_name(enum ReLogicEventKind kind) {
    static const char *const names[] = {
        "level_start",     "trigger_enter",     "trigger_stay",      "trigger_exit", "interact",
        "entity_damaged",  "entity_died",       "captured",          "item_picked",  "item_used",
        "dialogue_choice", "dialogue_finished", "objective_changed", "timer",        "animation",
        "barrier_changed", "player_died",       "game_loaded",       "custom"};
    return kind <= RE_LOGIC_CUSTOM ? names[kind] : "custom";
}
static const char *condition_name(enum ReConditionKind kind) {
    static const char *const names[] = {"variable", "item", "objective", "health", "lives"};
    return kind <= RE_CONDITION_PLAYER_LIVES ? names[kind] : "variable";
}
static const char *comparison_name(enum ReCompareOp value) {
    static const char *const names[] = {"eq", "ne", "lt", "le", "gt", "ge"};
    return value <= RE_COMPARE_GREATER_EQUAL ? names[value] : "eq";
}
static const char *action_name(enum ReActionKind kind) {
    static const char *const names[] = {
        "set_variable", "add_variable", "give_item",      "take_item", "set_objective",
        "message",      "open_barrier", "close_barrier",  "damage",    "heal",
        "set_lives",    "checkpoint",   "start_dialogue", "light",     "spawn_pickup",
        "emit",         "win",          "game_over"};
    return kind <= RE_RULE_GAME_OVER ? names[kind] : "message";
}

bool re_interaction_save_rules(const char *path, const ReInteractionDefinitions *definitions,
                               ReError *error) {
    if (!path || !definitions || !error)
        return false;
    char temporary[1024];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return fail(error, 0, "Ruta de reglas demasiado larga");
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return fail(error, 0, "No se pudo crear retro_rules temporal");
    bool ok = fprintf(file, "# Generado por RetroForge Studio.\nretro_rules 1\n") > 0;
    for (size_t i = 0; ok && i < definitions->variable_count; i++)
        ok = fprintf(file, "variable %s ", definitions->variables[i].id) > 0 &&
             write_value(file, definitions->variables[i].initial) && fputc('\n', file) != EOF;
    for (size_t i = 0; ok && i < definitions->item_count; i++)
        ok = fprintf(file, "item %s \"%s\" %u\n", definitions->items[i].id,
                     definitions->items[i].name, definitions->items[i].max_stack) > 0;
    for (size_t i = 0; ok && i < definitions->objective_count; i++)
        ok = fprintf(file, "objective %s \"%s\"\n", definitions->objectives[i].id,
                     definitions->objectives[i].title) > 0;
    for (size_t i = 0; ok && i < definitions->trigger_count; i++) {
        const ReTriggerVolume *trigger = &definitions->triggers[i];
        if (trigger->shape == RE_TRIGGER_SECTOR)
            ok = fprintf(file, "trigger %s sector %d %s\n", trigger->id, trigger->sector,
                         trigger->once ? "true" : "false") > 0;
        else if (trigger->shape == RE_TRIGGER_BOX)
            ok = fprintf(file, "trigger %s box %.9g %.9g %.9g %.9g %.9g %.9g %s\n", trigger->id,
                         (double)trigger->center.x, (double)trigger->center.y,
                         (double)trigger->center.z, (double)trigger->half_size.x,
                         (double)trigger->half_size.y, (double)trigger->half_size.z,
                         trigger->once ? "true" : "false") > 0;
        else
            ok = fprintf(file, "trigger %s cylinder %.9g %.9g %.9g %.9g %.9g %s\n", trigger->id,
                         (double)trigger->center.x, (double)trigger->center.y,
                         (double)trigger->center.z, (double)trigger->radius,
                         (double)trigger->half_size.z, trigger->once ? "true" : "false") > 0;
    }
    for (size_t i = 0; ok && i < definitions->light_count; i++) {
        const ReLight *light = &definitions->lights[i];
        ok =
            fprintf(file, "light %s %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %s %.9g %.9g %.9g\n",
                    light->id, light->kind == RE_LIGHT_SPOT ? "spot" : "point",
                    (double)light->position.x, (double)light->position.y, (double)light->position.z,
                    (double)light->color.x, (double)light->color.y, (double)light->color.z,
                    (double)light->radius, (double)light->intensity,
                    light->initially_enabled ? "true" : "false", (double)light->yaw,
                    (double)light->cone, (double)light->flicker) > 0;
    }
    for (size_t i = 0; ok && i < definitions->rule_count; i++) {
        const ReRuleDefinition *rule = &definitions->rules[i];
        ok = fprintf(file, "\nrule %s %s %s %d %s %.9g\n", rule->id, event_name(rule->event),
                     rule->source, rule->priority, rule->once ? "true" : "false",
                     (double)rule->cooldown) > 0;
        for (size_t c = 0; ok && c < rule->condition_count; c++) {
            const ReRuleCondition *condition = &rule->conditions[c];
            ok = fprintf(file, "condition %s %s %s ", condition_name(condition->kind),
                         condition->key, comparison_name(condition->comparison)) > 0 &&
                 write_value(file, condition->value) && fputc('\n', file) != EOF;
        }
        for (size_t a = 0; ok && a < rule->action_count; a++) {
            const ReRuleAction *action = &rule->actions[a];
            ok = fprintf(file, "action %s %s ", action_name(action->kind), action->target) > 0 &&
                 write_value(file, action->value) && fputc('\n', file) != EOF;
        }
        ok = ok && fprintf(file, "end\n") > 0;
    }
    if (fclose(file) != 0)
        ok = false;
    if (!ok || !replace_file(temporary, path)) {
        (void)remove(temporary);
        return fail(error, 0, "No se pudo reemplazar retro_rules");
    }
    *error = (ReError){0};
    return true;
}

bool re_interaction_save_dialogues(const char *path, const ReInteractionDefinitions *definitions,
                                   ReError *error) {
    if (!path || !definitions || !error)
        return false;
    char temporary[1024];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return fail(error, 0, "Ruta de diálogo demasiado larga");
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return fail(error, 0, "No se pudo crear retro_dialogue temporal");
    bool ok = fprintf(file, "# Generado por RetroForge Studio.\nretro_dialogue 1\n") > 0;
    for (size_t i = 0; ok && i < definitions->dialogue_count; i++) {
        const ReDialogueNode *node = &definitions->dialogues[i];
        ok = fprintf(file, "\nnode %s \"%s\" %s \"%s\" next %s\n", node->id, node->speaker,
                     node->pauses_world ? "true" : "false", node->text,
                     node->next[0] ? node->next : "end") > 0;
        for (size_t c = 0; ok && c < node->choice_count; c++) {
            const ReDialogueChoice *choice = &node->choices[c];
            ok = fprintf(file, "choice %s \"%s\" %s %s bool %s\n", choice->id, choice->text,
                         choice->next,
                         choice->condition_variable[0] ? choice->condition_variable : "-",
                         choice->condition_value ? "true" : "false") > 0;
        }
        ok = ok && fprintf(file, "end\n") > 0;
    }
    if (fclose(file) != 0)
        ok = false;
    if (!ok || !replace_file(temporary, path)) {
        (void)remove(temporary);
        return fail(error, 0, "No se pudo reemplazar retro_dialogue");
    }
    *error = (ReError){0};
    return true;
}

static int variable_index(const ReInteractionDefinitions *definitions, const char *id) {
    for (size_t i = 0; i < definitions->variable_count; i++)
        if (strcmp(definitions->variables[i].id, id) == 0)
            return (int)i;
    return -1;
}
static int item_index(const ReInteractionDefinitions *definitions, const char *id) {
    for (size_t i = 0; i < definitions->item_count; i++)
        if (strcmp(definitions->items[i].id, id) == 0)
            return (int)i;
    return -1;
}
static int objective_index(const ReInteractionDefinitions *definitions, const char *id) {
    for (size_t i = 0; i < definitions->objective_count; i++)
        if (strcmp(definitions->objectives[i].id, id) == 0)
            return (int)i;
    return -1;
}
static int light_index(const ReInteractionDefinitions *definitions, const char *id) {
    for (size_t i = 0; i < definitions->light_count; i++)
        if (strcmp(definitions->lights[i].id, id) == 0)
            return (int)i;
    return -1;
}
static int dialogue_index(const ReInteractionDefinitions *definitions, const char *id) {
    for (size_t i = 0; i < definitions->dialogue_count; i++)
        if (strcmp(definitions->dialogues[i].id, id) == 0)
            return (int)i;
    return -1;
}

bool re_interaction_validate(const ReInteractionDefinitions *d, ReError *error) {
    if (!d || !error)
        return false;
    for (size_t i = 0; i < d->variable_count; i++)
        for (size_t j = 0; j < i; j++)
            if (strcmp(d->variables[i].id, d->variables[j].id) == 0)
                return fail(error, 0, "Variable duplicada");
    for (size_t i = 0; i < d->item_count; i++)
        for (size_t j = 0; j < i; j++)
            if (strcmp(d->items[i].id, d->items[j].id) == 0)
                return fail(error, 0, "Ítem duplicado");
    for (size_t i = 0; i < d->rule_count; i++) {
        if (!d->rules[i].action_count)
            return fail(error, 0, "Cada regla necesita al menos una acción");
        for (size_t j = 0; j < i; j++)
            if (strcmp(d->rules[i].id, d->rules[j].id) == 0)
                return fail(error, 0, "Regla duplicada");
    }
    for (size_t i = 0; i < d->dialogue_count; i++) {
        if (d->dialogues[i].next[0] && strcmp(d->dialogues[i].next, "end") != 0 &&
            dialogue_index(d, d->dialogues[i].next) < 0)
            return fail(error, 0, "Diálogo referencia un nodo inexistente");
        for (size_t c = 0; c < d->dialogues[i].choice_count; c++)
            if (strcmp(d->dialogues[i].choices[c].next, "end") != 0 &&
                dialogue_index(d, d->dialogues[i].choices[c].next) < 0)
                return fail(error, 0, "Opción referencia un nodo inexistente");
    }
    *error = (ReError){0};
    return true;
}

void re_interaction_init(ReInteractionRuntime *runtime, const ReInteractionDefinitions *definitions,
                         ReWorld *world, ReGameplay *gameplay, int health, int lives) {
    *runtime =
        (ReInteractionRuntime){.definitions = definitions, .world = world, .gameplay = gameplay};
    runtime->state.player_health = health;
    runtime->state.player_max_health = health;
    runtime->state.player_lives = lives;
    for (size_t i = 0; i < definitions->variable_count; i++)
        runtime->state.variables[i] = definitions->variables[i].initial;
    for (size_t i = 0; i < definitions->light_count; i++)
        runtime->state.light_enabled[i] = definitions->lights[i].initially_enabled;
    for (size_t i = 0; i < definitions->rule_count; i++)
        runtime->rule_order[i] = i;
    for (size_t i = 1; i < definitions->rule_count; i++) {
        size_t value = runtime->rule_order[i], j = i;
        while (j && definitions->rules[runtime->rule_order[j - 1u]].priority <
                        definitions->rules[value].priority) {
            runtime->rule_order[j] = runtime->rule_order[j - 1u];
            j--;
        }
        runtime->rule_order[j] = value;
    }
    if (world)
        for (size_t i = 0;
             i < world->marker_count && runtime->state.pickup_count < RE_MAX_RUNTIME_PICKUPS; i++)
            if (strcmp(world->markers[i].kind, "pickup") == 0) {
                ReRuntimePickup *pickup = &runtime->state.pickups[runtime->state.pickup_count++];
                (void)copy_text(pickup->id, sizeof(pickup->id), world->markers[i].id);
                (void)copy_text(pickup->item, sizeof(pickup->item), world->markers[i].definition);
                pickup->position = world->markers[i].position;
                pickup->sector = world->markers[i].sector;
                pickup->count = 1;
                pickup->active = true;
            }
    (void)re_interaction_emit(runtime,
                              (ReLogicEvent){.kind = RE_LOGIC_LEVEL_START, .source = "level"});
}

bool re_interaction_emit(ReInteractionRuntime *runtime, ReLogicEvent event) {
    if (!runtime || runtime->state.event_count == RE_MAX_LOGIC_EVENTS)
        return false;
    size_t index = (runtime->state.event_head + runtime->state.event_count) % RE_MAX_LOGIC_EVENTS;
    runtime->state.events[index] = event;
    runtime->state.event_count++;
    return true;
}

bool re_interaction_set_variable(ReInteractionRuntime *runtime, const char *id, ReValue value) {
    int index = variable_index(runtime->definitions, id);
    if (index < 0 || runtime->definitions->variables[index].initial.kind != value.kind)
        return false;
    runtime->state.variables[index] = value;
    return true;
}
const ReValue *re_interaction_variable(const ReInteractionRuntime *runtime, const char *id) {
    int index = variable_index(runtime->definitions, id);
    return index < 0 ? nullptr : &runtime->state.variables[index];
}
unsigned int re_interaction_item_count(const ReInteractionRuntime *runtime, const char *id) {
    int index = item_index(runtime->definitions, id);
    return index < 0 ? 0 : runtime->state.inventory[index];
}
enum ReObjectiveStatus re_interaction_objective(const ReInteractionRuntime *runtime,
                                                const char *id) {
    int index = objective_index(runtime->definitions, id);
    return index < 0 ? RE_OBJECTIVE_INACTIVE : runtime->state.objectives[index];
}

static int compare_values(ReValue a, ReValue b) {
    if (a.kind != b.kind)
        return 2;
    switch (a.kind) {
    case RE_VALUE_BOOL:
        return (int)a.as.boolean - (int)b.as.boolean;
    case RE_VALUE_INT:
        return (a.as.integer > b.as.integer) - (a.as.integer < b.as.integer);
    case RE_VALUE_FLOAT:
        return (a.as.real > b.as.real) - (a.as.real < b.as.real);
    case RE_VALUE_TEXT:
        return strcmp(a.as.text, b.as.text);
    default:
        return 0;
    }
}
static bool comparison(int order, enum ReCompareOp operation) {
    switch (operation) {
    case RE_COMPARE_EQUAL:
        return order == 0;
    case RE_COMPARE_NOT_EQUAL:
        return order != 0;
    case RE_COMPARE_LESS:
        return order < 0;
    case RE_COMPARE_LESS_EQUAL:
        return order <= 0;
    case RE_COMPARE_GREATER:
        return order > 0;
    case RE_COMPARE_GREATER_EQUAL:
        return order >= 0;
    }
    return false;
}
static bool condition_passes(const ReInteractionRuntime *runtime,
                             const ReRuleCondition *condition) {
    ReValue actual = {0};
    int index = -1;
    switch (condition->kind) {
    case RE_CONDITION_VARIABLE:
        index = variable_index(runtime->definitions, condition->key);
        if (index >= 0)
            actual = runtime->state.variables[index];
        break;
    case RE_CONDITION_ITEM_COUNT:
        actual = (ReValue){.kind = RE_VALUE_INT,
                           .as.integer = (int)re_interaction_item_count(runtime, condition->key)};
        break;
    case RE_CONDITION_OBJECTIVE:
        actual = (ReValue){.kind = RE_VALUE_INT,
                           .as.integer = (int)re_interaction_objective(runtime, condition->key)};
        break;
    case RE_CONDITION_PLAYER_HEALTH:
        actual = (ReValue){.kind = RE_VALUE_INT, .as.integer = runtime->state.player_health};
        break;
    case RE_CONDITION_PLAYER_LIVES:
        actual = (ReValue){.kind = RE_VALUE_INT, .as.integer = runtime->state.player_lives};
        break;
    }
    return comparison(compare_values(actual, condition->value), condition->comparison);
}

static int barrier_index(const ReWorld *world, const char *id) {
    if (!world)
        return -1;
    for (size_t i = 0; i < world->barrier_count; i++)
        if (strcmp(world->barriers[i].id, id) == 0)
            return (int)i;
    return -1;
}
static void checkpoint_capture(ReInteractionRuntime *runtime, const ReBody *player) {
    ReInteractionState *state = &runtime->state;
    state->checkpoint_player = *player;
    state->checkpoint_health = state->player_health;
    state->checkpoint_lives = state->player_lives;
    memcpy(state->checkpoint_variables, state->variables, sizeof(state->variables));
    memcpy(state->checkpoint_inventory, state->inventory, sizeof(state->inventory));
    memcpy(state->checkpoint_objectives, state->objectives, sizeof(state->objectives));
    memcpy(state->checkpoint_trigger_inside, state->trigger_inside, sizeof(state->trigger_inside));
    memcpy(state->checkpoint_trigger_consumed, state->trigger_consumed,
           sizeof(state->trigger_consumed));
    memcpy(state->checkpoint_rule_fired, state->rule_fired, sizeof(state->rule_fired));
    memcpy(state->checkpoint_light_enabled, state->light_enabled, sizeof(state->light_enabled));
    memcpy(state->checkpoint_rule_cooldown, state->rule_cooldown, sizeof(state->rule_cooldown));
    memcpy(state->checkpoint_trigger_stay, state->trigger_stay, sizeof(state->trigger_stay));
    memcpy(state->checkpoint_pickups, state->pickups, sizeof(state->pickups));
    state->checkpoint_pickup_count = state->pickup_count;
    if (runtime->world)
        memcpy(state->checkpoint_barriers, runtime->world->barriers,
               sizeof(runtime->world->barriers));
    if (runtime->gameplay) {
        memcpy(state->checkpoint_characters, runtime->gameplay->characters,
               sizeof(runtime->gameplay->characters));
        state->checkpoint_random_state = runtime->gameplay->random_state;
    }
    state->checkpoint_valid = true;
}
void re_interaction_checkpoint_restore(ReInteractionRuntime *runtime, ReBody *player) {
    ReInteractionState *state = &runtime->state;
    if (!state->checkpoint_valid)
        return;
    *player = state->checkpoint_player;
    state->player_health = state->checkpoint_health;
    state->player_lives = state->checkpoint_lives;
    memcpy(state->variables, state->checkpoint_variables, sizeof(state->variables));
    memcpy(state->inventory, state->checkpoint_inventory, sizeof(state->inventory));
    memcpy(state->objectives, state->checkpoint_objectives, sizeof(state->objectives));
    memcpy(state->trigger_inside, state->checkpoint_trigger_inside, sizeof(state->trigger_inside));
    memcpy(state->trigger_consumed, state->checkpoint_trigger_consumed,
           sizeof(state->trigger_consumed));
    memcpy(state->rule_fired, state->checkpoint_rule_fired, sizeof(state->rule_fired));
    memcpy(state->light_enabled, state->checkpoint_light_enabled, sizeof(state->light_enabled));
    memcpy(state->rule_cooldown, state->checkpoint_rule_cooldown, sizeof(state->rule_cooldown));
    memcpy(state->trigger_stay, state->checkpoint_trigger_stay, sizeof(state->trigger_stay));
    memcpy(state->pickups, state->checkpoint_pickups, sizeof(state->pickups));
    state->pickup_count = state->checkpoint_pickup_count;
    if (runtime->world)
        memcpy(runtime->world->barriers, state->checkpoint_barriers,
               sizeof(runtime->world->barriers));
    if (runtime->gameplay) {
        memcpy(runtime->gameplay->characters, state->checkpoint_characters,
               sizeof(runtime->gameplay->characters));
        runtime->gameplay->random_state = state->checkpoint_random_state;
        runtime->gameplay->event_head = 0;
        runtime->gameplay->event_count = 0;
    }
    /* Los eventos posteriores al checkpoint pertenecen a la línea temporal
     * descartada. Restaurarlos podría repetir drops o finales. */
    state->event_head = 0;
    state->event_count = 0;
    state->active_dialogue[0] = '\0';
    state->dialogue_pauses = false;
    state->won = false;
    state->game_over = false;
}

static void execute_action(ReInteractionRuntime *runtime, const ReRuleAction *action,
                           const ReLogicEvent *event, ReBody *player) {
    ReInteractionState *state = &runtime->state;
    int index;
    switch (action->kind) {
    case RE_RULE_SET_VARIABLE:
        (void)re_interaction_set_variable(runtime, action->target, action->value);
        break;
    case RE_RULE_ADD_VARIABLE:
        index = variable_index(runtime->definitions, action->target);
        if (index >= 0 && action->value.kind == RE_VALUE_INT &&
            state->variables[index].kind == RE_VALUE_INT)
            state->variables[index].as.integer += action->value.as.integer;
        else if (index >= 0 && action->value.kind == RE_VALUE_FLOAT &&
                 state->variables[index].kind == RE_VALUE_FLOAT)
            state->variables[index].as.real += action->value.as.real;
        break;
    case RE_RULE_GIVE_ITEM:
    case RE_RULE_TAKE_ITEM:
        index = item_index(runtime->definitions, action->target);
        if (index >= 0 && action->value.kind == RE_VALUE_INT && action->value.as.integer >= 0) {
            unsigned int amount = (unsigned int)action->value.as.integer;
            if (action->kind == RE_RULE_GIVE_ITEM) {
                unsigned int maximum = runtime->definitions->items[index].max_stack;
                state->inventory[index] = state->inventory[index] + amount > maximum
                                              ? maximum
                                              : state->inventory[index] + amount;
            } else
                state->inventory[index] =
                    state->inventory[index] > amount ? state->inventory[index] - amount : 0;
        }
        break;
    case RE_RULE_SET_OBJECTIVE:
        index = objective_index(runtime->definitions, action->target);
        if (index >= 0 && action->value.kind == RE_VALUE_INT && action->value.as.integer >= 0 &&
            action->value.as.integer <= RE_OBJECTIVE_FAILED) {
            state->objectives[index] = (enum ReObjectiveStatus)action->value.as.integer;
            (void)re_interaction_emit(runtime, (ReLogicEvent){.kind = RE_LOGIC_OBJECTIVE_CHANGED,
                                                              .source = "objective",
                                                              .value = action->value});
        }
        break;
    case RE_RULE_SHOW_MESSAGE:
        if (action->value.kind == RE_VALUE_TEXT) {
            (void)copy_text(state->message, sizeof(state->message), action->value.as.text);
            state->message_time = 4;
        }
        break;
    case RE_RULE_OPEN_BARRIER:
    case RE_RULE_CLOSE_BARRIER:
        index = barrier_index(runtime->world, action->target);
        if (index >= 0) {
            runtime->world->barriers[index].open_fraction =
                action->kind == RE_RULE_OPEN_BARRIER ? 1 : 0;
            (void)re_interaction_emit(
                runtime, (ReLogicEvent){.kind = RE_LOGIC_BARRIER_CHANGED, .source = "barrier"});
        }
        break;
    case RE_RULE_DAMAGE_PLAYER:
        if (action->value.kind == RE_VALUE_INT) {
            state->player_health = state->player_health > action->value.as.integer
                                       ? state->player_health - action->value.as.integer
                                       : 0;
            if (!state->player_health)
                (void)re_interaction_emit(
                    runtime, (ReLogicEvent){.kind = RE_LOGIC_PLAYER_DIED, .source = "player"});
        }
        break;
    case RE_RULE_HEAL_PLAYER:
        if (action->value.kind == RE_VALUE_INT)
            state->player_health =
                state->player_health + action->value.as.integer > state->player_max_health
                    ? state->player_max_health
                    : state->player_health + action->value.as.integer;
        break;
    case RE_RULE_SET_LIVES:
        if (action->value.kind == RE_VALUE_INT)
            state->player_lives = action->value.as.integer;
        break;
    case RE_RULE_CHECKPOINT:
        checkpoint_capture(runtime, player);
        break;
    case RE_RULE_START_DIALOGUE:
        index = dialogue_index(runtime->definitions, action->target);
        if (index >= 0) {
            (void)copy_text(state->active_dialogue, sizeof(state->active_dialogue), action->target);
            state->dialogue_pauses = runtime->definitions->dialogues[index].pauses_world;
        }
        break;
    case RE_RULE_TOGGLE_LIGHT:
        index = light_index(runtime->definitions, action->target);
        if (index >= 0 && action->value.kind == RE_VALUE_BOOL)
            state->light_enabled[index] = action->value.as.boolean;
        break;
    case RE_RULE_SPAWN_PICKUP:
        if (state->pickup_count < RE_MAX_RUNTIME_PICKUPS &&
            item_index(runtime->definitions, action->target) >= 0) {
            ReRuntimePickup *pickup = &state->pickups[state->pickup_count++];
            (void)snprintf(pickup->id, sizeof(pickup->id), "drop-%zu", state->pickup_count);
            (void)copy_text(pickup->item, sizeof(pickup->item), action->target);
            pickup->position = event->position;
            pickup->sector =
                runtime->world ? re_world_sector_at(runtime->world, event->position, -1) : -1;
            pickup->count = action->value.kind == RE_VALUE_INT && action->value.as.integer > 0
                                ? (unsigned int)action->value.as.integer
                                : 1u;
            pickup->active = true;
        }
        break;
    case RE_RULE_EMIT_EVENT:
        if (action->value.kind == RE_VALUE_INT && action->value.as.integer >= 0 &&
            action->value.as.integer <= RE_LOGIC_CUSTOM)
            (void)re_interaction_emit(
                runtime, (ReLogicEvent){.kind = (enum ReLogicEventKind)action->value.as.integer,
                                        .source = "rule",
                                        .target = "custom",
                                        .position = event->position});
        break;
    case RE_RULE_WIN:
        state->won = true;
        break;
    case RE_RULE_GAME_OVER:
        state->game_over = true;
        break;
    }
}

static bool trigger_contains(const ReTriggerVolume *trigger, const ReBody *player) {
    if (trigger->shape == RE_TRIGGER_SECTOR)
        return player->sector == trigger->sector;
    ReVec3 delta = re_sub3(player->position, trigger->center);
    if (trigger->shape == RE_TRIGGER_BOX)
        return fabsf(delta.x) <= trigger->half_size.x && fabsf(delta.y) <= trigger->half_size.y &&
               player->position.z + player->height >= trigger->center.z - trigger->half_size.z &&
               player->position.z <= trigger->center.z + trigger->half_size.z;
    return delta.x * delta.x + delta.y * delta.y <= trigger->radius * trigger->radius &&
           fabsf(delta.z) <= trigger->half_size.z;
}

static void update_spatial_events(ReInteractionRuntime *runtime, ReBody *player, float dt) {
    ReInteractionState *state = &runtime->state;
    const ReInteractionDefinitions *definitions = runtime->definitions;
    for (size_t i = 0; i < definitions->trigger_count; i++) {
        const ReTriggerVolume *trigger = &definitions->triggers[i];
        bool inside = trigger_contains(trigger, player);
        if (inside && !state->trigger_inside[i] && !state->trigger_consumed[i]) {
            ReLogicEvent event = {.kind = RE_LOGIC_TRIGGER_ENTER, .position = player->position};
            (void)copy_text(event.source, sizeof(event.source), trigger->id);
            (void)re_interaction_emit(runtime, event);
            state->trigger_consumed[i] = trigger->once;
        } else if (!inside && state->trigger_inside[i]) {
            ReLogicEvent event = {.kind = RE_LOGIC_TRIGGER_EXIT, .position = player->position};
            (void)copy_text(event.source, sizeof(event.source), trigger->id);
            (void)re_interaction_emit(runtime, event);
        }
        state->trigger_stay[i] += inside ? dt : -state->trigger_stay[i];
        if (inside && state->trigger_stay[i] >= 1) {
            ReLogicEvent event = {.kind = RE_LOGIC_TRIGGER_STAY, .position = player->position};
            (void)copy_text(event.source, sizeof(event.source), trigger->id);
            (void)re_interaction_emit(runtime, event);
            state->trigger_stay[i] = 0;
        }
        state->trigger_inside[i] = inside;
    }
    for (size_t i = 0; i < state->pickup_count; i++) {
        ReRuntimePickup *pickup = &state->pickups[i];
        if (!pickup->active || pickup->sector != player->sector)
            continue;
        ReVec3 delta = re_sub3(pickup->position, player->position);
        if (delta.x * delta.x + delta.y * delta.y > .75f * .75f || fabsf(delta.z) > 1.8f)
            continue;
        int item = item_index(definitions, pickup->item);
        if (item < 0)
            continue;
        unsigned int maximum = definitions->items[item].max_stack;
        if (state->inventory[item] >= maximum)
            continue;
        state->inventory[item] = state->inventory[item] + pickup->count > maximum
                                     ? maximum
                                     : state->inventory[item] + pickup->count;
        pickup->active = false;
        ReLogicEvent event = {.kind = RE_LOGIC_ITEM_PICKED, .position = pickup->position};
        (void)copy_text(event.source, sizeof(event.source), pickup->id);
        (void)copy_text(event.target, sizeof(event.target), pickup->item);
        (void)re_interaction_emit(runtime, event);
    }
}

void re_interaction_tick(ReInteractionRuntime *runtime, ReBody *player, float dt) {
    if (!runtime || !runtime->definitions || !player || dt <= 0)
        return;
    ReInteractionState *state = &runtime->state;
    state->message_time = fmaxf(0, state->message_time - dt);
    for (size_t i = 0; i < runtime->definitions->rule_count; i++)
        state->rule_cooldown[i] = fmaxf(0, state->rule_cooldown[i] - dt);
    update_spatial_events(runtime, player, dt);
    unsigned int processed = 0;
    while (state->event_count && processed++ < RE_LOGIC_EVENT_BUDGET) {
        ReLogicEvent event = state->events[state->event_head];
        state->event_head = (state->event_head + 1u) % RE_MAX_LOGIC_EVENTS;
        state->event_count--;
        for (size_t order = 0; order < runtime->definitions->rule_count; order++) {
            size_t index = runtime->rule_order[order];
            const ReRuleDefinition *rule = &runtime->definitions->rules[index];
            if (rule->event != event.kind ||
                (strcmp(rule->source, "*") != 0 && strcmp(rule->source, event.source) != 0) ||
                (rule->once && state->rule_fired[index]) || state->rule_cooldown[index] > 0)
                continue;
            bool pass = true;
            for (size_t condition = 0; condition < rule->condition_count; condition++)
                pass &= condition_passes(runtime, &rule->conditions[condition]);
            if (!pass)
                continue;
            for (size_t action = 0; action < rule->action_count; action++)
                execute_action(runtime, &rule->actions[action], &event, player);
            state->rule_fired[index] = true;
            state->rule_cooldown[index] = rule->cooldown;
        }
    }
    if (state->event_count) {
        state->cycle_limited = true;
        state->event_head = state->event_count = 0;
        (void)copy_text(state->message, sizeof(state->message),
                        "Ciclo de reglas detenido: revisa la consola de lógica");
        state->message_time = 6;
    }
}

bool re_interaction_interact(ReInteractionRuntime *runtime, ReVec3 origin, ReVec3 direction,
                             float distance) {
    if (!runtime || !runtime->world || distance <= 0)
        return false;
    int selected = -1;
    float nearest = distance;
    for (size_t i = 0; i < runtime->world->marker_count; i++) {
        const ReMarker *marker = &runtime->world->markers[i];
        if (strcmp(marker->kind, "interact") != 0 && strcmp(marker->kind, "npc") != 0)
            continue;
        ReVec3 center = re_add3(marker->position, re_v3(0, 0, .8f));
        ReVec3 delta = re_sub3(center, origin);
        float along = re_dot3(delta, direction);
        if (along < 0 || along > nearest)
            continue;
        ReVec3 lateral = re_sub3(delta, re_scale3(direction, along));
        if (re_dot3(lateral, lateral) > .65f * .65f)
            continue;
        ReTraceHit hit = re_world_trace(runtime->world, origin, direction, along, RE_BLOCK_SIGHT);
        if (hit.distance + .02f < along)
            continue;
        selected = (int)i;
        nearest = along;
    }
    if (selected < 0)
        return false;
    ReLogicEvent event = {.kind = RE_LOGIC_INTERACT,
                          .position = runtime->world->markers[selected].position};
    (void)copy_text(event.source, sizeof(event.source), runtime->world->markers[selected].id);
    (void)copy_text(event.target, sizeof(event.target),
                    runtime->world->markers[selected].definition);
    return re_interaction_emit(runtime, event);
}

const ReDialogueNode *re_interaction_dialogue(const ReInteractionRuntime *runtime) {
    if (!runtime || !runtime->state.active_dialogue[0])
        return nullptr;
    int index = dialogue_index(runtime->definitions, runtime->state.active_dialogue);
    return index < 0 ? nullptr : &runtime->definitions->dialogues[index];
}
bool re_interaction_choose(ReInteractionRuntime *runtime, size_t visible_choice) {
    const ReDialogueNode *node = re_interaction_dialogue(runtime);
    if (!node)
        return false;
    const char *next = node->next;
    char choice_id[24] = {0};
    if (node->choice_count) {
        size_t visible = 0;
        const ReDialogueChoice *selected = nullptr;
        for (size_t i = 0; i < node->choice_count; i++) {
            const ReDialogueChoice *choice = &node->choices[i];
            bool allowed = true;
            if (choice->condition_variable[0] && strcmp(choice->condition_variable, "-") != 0) {
                const ReValue *value = re_interaction_variable(runtime, choice->condition_variable);
                allowed = value && value->kind == RE_VALUE_BOOL &&
                          value->as.boolean == choice->condition_value;
            }
            if (allowed && visible++ == visible_choice) {
                selected = choice;
                break;
            }
        }
        if (!selected)
            return false;
        next = selected->next;
        (void)copy_text(choice_id, sizeof(choice_id), selected->id);
        ReLogicEvent event = {.kind = RE_LOGIC_DIALOGUE_CHOICE};
        (void)copy_text(event.source, sizeof(event.source), node->id);
        (void)copy_text(event.target, sizeof(event.target), choice_id);
        (void)re_interaction_emit(runtime, event);
    }
    if (!next[0] || strcmp(next, "end") == 0) {
        ReLogicEvent event = {.kind = RE_LOGIC_DIALOGUE_FINISHED};
        (void)copy_text(event.source, sizeof(event.source), node->id);
        runtime->state.active_dialogue[0] = '\0';
        runtime->state.dialogue_pauses = false;
        return re_interaction_emit(runtime, event);
    }
    int next_index = dialogue_index(runtime->definitions, next);
    if (next_index < 0)
        return false;
    (void)copy_text(runtime->state.active_dialogue, sizeof(runtime->state.active_dialogue), next);
    runtime->state.dialogue_pauses = runtime->definitions->dialogues[next_index].pauses_world;
    return true;
}

typedef struct ReSaveHeader {
    char magic[8];
    uint32_t version, payload_size, crc32;
    char project_id[64];
} ReSaveHeader;
typedef struct ReSavePayload {
    ReInteractionState interaction;
    ReBody player;
    size_t barrier_count;
    ReBarrier barriers[RE_MAX_BARRIERS];
    size_t character_count;
    ReCharacter characters[RE_MAX_ENTITIES];
    uint32_t random_state;
} ReSavePayload;

static uint32_t crc32(const void *data, size_t size) {
    const unsigned char *bytes = data;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1u) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}
static bool replace_file(const char *temporary, const char *destination) {
#ifdef _WIN32
    return MoveFileExA(temporary, destination,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, destination) == 0;
#endif
}

bool re_save_write(const char *path, const char *project_id, const ReInteractionRuntime *runtime,
                   const ReBody *player, ReError *error) {
    if (!path || !project_id || !runtime || !player || !error || strlen(project_id) >= 64)
        return false;
    ReSavePayload *payload = calloc(1, sizeof(*payload));
    if (!payload)
        return fail(error, 0, "Memoria insuficiente para snapshot");
    payload->interaction = runtime->state;
    payload->player = *player;
    if (runtime->world) {
        payload->barrier_count = runtime->world->barrier_count;
        memcpy(payload->barriers, runtime->world->barriers,
               payload->barrier_count * sizeof(payload->barriers[0]));
    }
    if (runtime->gameplay) {
        payload->character_count = RE_MAX_ENTITIES;
        memcpy(payload->characters, runtime->gameplay->characters, sizeof(payload->characters));
        payload->random_state = runtime->gameplay->random_state;
    }
    ReSaveHeader header = {.magic = {'R', 'F', 'S', 'A', 'V', 'E', '2', '\0'},
                           .version = 2,
                           .payload_size = (uint32_t)sizeof(*payload),
                           .crc32 = crc32(payload, sizeof(*payload))};
    (void)copy_text(header.project_id, sizeof(header.project_id), project_id);
    size_t path_length = strlen(path);
    char *temporary = malloc(path_length + 5u);
    if (!temporary) {
        free(payload);
        return fail(error, 0, "Memoria insuficiente para ruta temporal");
    }
    memcpy(temporary, path, path_length);
    memcpy(temporary + path_length, ".tmp", 5);
    FILE *file = fopen(temporary, "wb");
    bool ok = file && fwrite(&header, sizeof(header), 1, file) == 1 &&
              fwrite(payload, sizeof(*payload), 1, file) == 1;
    if (file && fclose(file) != 0)
        ok = false;
    if (ok)
        ok = replace_file(temporary, path);
    if (!ok) {
        (void)remove(temporary);
        (void)fail(error, 0, "No se pudo guardar la ranura de forma atómica");
    } else
        *error = (ReError){0};
    free(temporary);
    free(payload);
    return ok;
}

bool re_save_read(const char *path, const char *project_id, ReInteractionRuntime *runtime,
                  ReBody *player, ReError *error) {
    if (!path || !project_id || !runtime || !player || !error)
        return false;
    FILE *file = fopen(path, "rb");
    if (!file)
        return fail(error, 0, "La ranura no existe");
    ReSaveHeader header = {0};
    ReSavePayload *payload = malloc(sizeof(*payload));
    bool ok = payload && fread(&header, sizeof(header), 1, file) == 1 &&
              fread(payload, sizeof(*payload), 1, file) == 1;
    int extra = fgetc(file);
    if (fclose(file) != 0)
        ok = false;
    if (!ok || extra != EOF || memcmp(header.magic, "RFSAVE2", 7) != 0 || header.version != 2 ||
        header.payload_size != sizeof(*payload) || strcmp(header.project_id, project_id) != 0 ||
        crc32(payload, sizeof(*payload)) != header.crc32 ||
        payload->barrier_count > RE_MAX_BARRIERS || payload->character_count > RE_MAX_ENTITIES) {
        free(payload);
        return fail(error, 0, "Ranura corrupta, incompatible o de otro proyecto");
    }
    runtime->state = payload->interaction;
    *player = payload->player;
    if (runtime->world) {
        runtime->world->barrier_count = payload->barrier_count;
        memcpy(runtime->world->barriers, payload->barriers,
               payload->barrier_count * sizeof(payload->barriers[0]));
    }
    if (runtime->gameplay) {
        memcpy(runtime->gameplay->characters, payload->characters, sizeof(payload->characters));
        runtime->gameplay->random_state = payload->random_state;
    }
    free(payload);
    *error = (ReError){0};
    (void)re_interaction_emit(runtime,
                              (ReLogicEvent){.kind = RE_LOGIC_GAME_LOADED, .source = "save"});
    return true;
}
