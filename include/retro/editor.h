/* API de autoría de RetroForge.
 *
 * Esta frontera deliberadamente pequeña permite que Studio use el motor C23
 * sin conocer la disposición binaria de ReProject ni depender de raylib. El
 * documento conserva una sola copia autorizada del proyecto y registra cada
 * edición como un comando deshacer/rehacer.
 *
 * Todas las cadenas entregadas por estas funciones son UTF-8. Los índices sólo
 * son válidos mientras no cambie la revisión del documento. */
#ifndef RETRO_EDITOR_H
#define RETRO_EDITOR_H

#include "retro/project.h"
#include <stdint.h>

#if defined(_WIN32) && defined(RETRO_EDITOR_BUILD)
#define RE_EDITOR_API __declspec(dllexport)
#elif defined(_WIN32)
#define RE_EDITOR_API __declspec(dllimport)
#else
#define RE_EDITOR_API
#endif

typedef struct ReEditorDocument ReEditorDocument;

enum ReEditorObjectKind {
    RE_EDITOR_PROJECT,
    RE_EDITOR_SECTOR,
    RE_EDITOR_MARKER,
    RE_EDITOR_BARRIER,
    RE_EDITOR_CHARACTER,
    RE_EDITOR_RULE,
    RE_EDITOR_DIALOGUE,
    RE_EDITOR_TRIGGER,
    RE_EDITOR_LIGHT
};

typedef struct ReEditorOverview {
    char name[64], id[64], manifest[RE_PROJECT_PATH], root[RE_PROJECT_PATH];
    uint64_t revision;
    uint32_t sector_count, marker_count, barrier_count, character_count;
    uint32_t rule_count, dialogue_count, trigger_count, light_count;
    int dirty, can_undo, can_redo;
} ReEditorOverview;

typedef struct ReEditorSectorView {
    float floor, ceiling, light;
    int wall_material, floor_material, ceiling_material;
    uint32_t vertex_count;
    float vertices[RE_MAX_VERTICES * 2];
} ReEditorSectorView;

typedef struct ReEditorMarkerView {
    char id[32], kind[24], definition[64];
    int sector;
    float x, y, z, yaw;
} ReEditorMarkerView;

typedef struct ReEditorBarrierView {
    char id[32];
    int kind, sector, edge, material;
    uint32_t blocks;
    float open_fraction, health;
} ReEditorBarrierView;

typedef struct ReEditorCharacterView {
    char id[64], sprite[160];
    int cell_width, cell_height, max_health, attack_damage, tracking;
    float radius, height, speed, sight_range, field_of_view, capture_range, attack_range;
    uint32_t animation_count, phase_count;
    int invulnerable, can_be_stunned, can_open_doors, capture_game_over;
} ReEditorCharacterView;

typedef struct ReEditorRuleView {
    char id[32], source[32];
    int event, priority, once;
    float cooldown;
    uint32_t condition_count, action_count;
} ReEditorRuleView;

typedef struct ReEditorDialogueView {
    char id[32], speaker[48], text[192], next[32];
    int pauses_world;
    uint32_t choice_count;
} ReEditorDialogueView;

typedef struct ReEditorTriggerView {
    char id[32];
    int shape, sector, once;
    float x, y, z, size_x, size_y, size_z, radius;
} ReEditorTriggerView;

typedef struct ReEditorLightView {
    char id[32];
    int kind, enabled;
    float x, y, z, red, green, blue, radius, intensity, yaw, cone, flicker;
} ReEditorLightView;

typedef struct ReEditorAnimationView {
    char name[24];
    uint32_t directions, frame_count;
    int loop;
} ReEditorAnimationView;

typedef struct ReEditorAnimationFrameView {
    uint32_t cell;
    float duration;
    int event;
} ReEditorAnimationFrameView;

typedef struct ReEditorBossPhaseView {
    char name[24];
    float health_threshold, speed_multiplier, cooldown;
    int tracking, action;
    uint32_t summon_limit;
} ReEditorBossPhaseView;

typedef struct ReEditorRuleConditionView {
    int kind, comparison;
    char key[32], value[96];
} ReEditorRuleConditionView;

typedef struct ReEditorRuleActionView {
    int kind;
    char target[32], value[96];
} ReEditorRuleActionView;

typedef struct ReEditorDialogueChoiceView {
    char id[24], text[96], next[32], condition_variable[32];
    int condition_value;
} ReEditorDialogueChoiceView;

RE_EDITOR_API int re_editor_open(const char *manifest, ReEditorDocument **out, ReError *error);
RE_EDITOR_API void re_editor_close(ReEditorDocument *document);
RE_EDITOR_API int re_editor_overview(const ReEditorDocument *document, ReEditorOverview *out);
RE_EDITOR_API int re_editor_sector(const ReEditorDocument *document, uint32_t index,
                                   ReEditorSectorView *out);
RE_EDITOR_API int re_editor_marker(const ReEditorDocument *document, uint32_t index,
                                   ReEditorMarkerView *out);
RE_EDITOR_API int re_editor_barrier(const ReEditorDocument *document, uint32_t index,
                                    ReEditorBarrierView *out);
RE_EDITOR_API int re_editor_character(const ReEditorDocument *document, uint32_t index,
                                      ReEditorCharacterView *out);
RE_EDITOR_API int re_editor_rule(const ReEditorDocument *document, uint32_t index,
                                 ReEditorRuleView *out);
RE_EDITOR_API int re_editor_dialogue(const ReEditorDocument *document, uint32_t index,
                                     ReEditorDialogueView *out);
RE_EDITOR_API int re_editor_trigger(const ReEditorDocument *document, uint32_t index,
                                    ReEditorTriggerView *out);
RE_EDITOR_API int re_editor_light(const ReEditorDocument *document, uint32_t index,
                                  ReEditorLightView *out);
RE_EDITOR_API int re_editor_animation(const ReEditorDocument *document, uint32_t character,
                                      uint32_t animation, ReEditorAnimationView *out);
RE_EDITOR_API int re_editor_animation_frame(const ReEditorDocument *document, uint32_t character,
                                            uint32_t animation, uint32_t frame,
                                            ReEditorAnimationFrameView *out);
RE_EDITOR_API int re_editor_boss_phase(const ReEditorDocument *document, uint32_t character,
                                       uint32_t phase, ReEditorBossPhaseView *out);
RE_EDITOR_API int re_editor_rule_condition(const ReEditorDocument *document, uint32_t rule,
                                           uint32_t condition, ReEditorRuleConditionView *out);
RE_EDITOR_API int re_editor_rule_action(const ReEditorDocument *document, uint32_t rule,
                                        uint32_t action, ReEditorRuleActionView *out);
RE_EDITOR_API int re_editor_dialogue_choice(const ReEditorDocument *document, uint32_t dialogue,
                                            uint32_t choice, ReEditorDialogueChoiceView *out);

/* Comandos de mapa usados por el flujo directo de Studio. Cada llamada válida
 * crea exactamente un paso de deshacer, aunque cambie varias propiedades. Las
 * coordenadas están en metros y deben caer dentro de un sector transitable. */
RE_EDITOR_API int re_editor_create_marker(ReEditorDocument *document, const char *kind,
                                          const char *definition, float x, float y,
                                          uint32_t *out_index, ReError *error);
RE_EDITOR_API int re_editor_move_marker(ReEditorDocument *document, uint32_t index, float x,
                                        float y, ReError *error);
RE_EDITOR_API int re_editor_duplicate_marker(ReEditorDocument *document, uint32_t index,
                                             uint32_t *out_index, ReError *error);
RE_EDITOR_API int re_editor_delete_marker(ReEditorDocument *document, uint32_t index,
                                          ReError *error);

/* Asistente equivalente a “Al morir, soltar objeto”. Crea una regla one-shot
 * completa y enlazada con la instancia, evitando identificadores manuales. */
RE_EDITOR_API int re_editor_add_drop_rule(ReEditorDocument *document, uint32_t marker,
                                          const char *item, uint32_t *out_rule, ReError *error);
RE_EDITOR_API int re_editor_add_interaction_rule(ReEditorDocument *document, uint32_t marker,
                                                 int action, const char *target, const char *value,
                                                 uint32_t *out_rule, ReError *error);

/* value usa la representación visible del inspector: números con punto como
 * separador y booleanos true/false. Una operación inválida no modifica ni el
 * documento ni su historial. */
RE_EDITOR_API int re_editor_set_property(ReEditorDocument *document, int kind, uint32_t index,
                                         const char *property, const char *value, ReError *error);
RE_EDITOR_API int re_editor_undo(ReEditorDocument *document);
RE_EDITOR_API int re_editor_redo(ReEditorDocument *document);
RE_EDITOR_API int re_editor_save(ReEditorDocument *document, ReError *error);
/* Disponible en builds con aplicaciones. Copia el documento no guardado a una
 * sesión aislada; el receptor libera el handle con re_session_destroy. */
struct ReGameSession;
RE_EDITOR_API int re_editor_start_session(const ReEditorDocument *document,
                                          struct ReGameSession **out, ReError *error);

#endif
