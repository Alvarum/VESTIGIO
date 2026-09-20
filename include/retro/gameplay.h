/* Gameplay reutilizable para FPS y terror.
 *
 * El núcleo geométrico no sabe qué es un enemigo. Esta capa interpreta
 * definiciones inmutables y conserva el estado mutable de cada instancia.
 * No expone raylib, no reserva memoria durante tick() y puede probarse sin
 * ventana. Todas las distancias están en metros y los tiempos en segundos. */
#ifndef RETRO_GAMEPLAY_H
#define RETRO_GAMEPLAY_H

#include "retro/world.h"
#include <stdint.h>

enum {
    RE_MAX_CHARACTER_DEFS = 32,
    RE_MAX_ENTITIES = 128,
    RE_MAX_ANIMATIONS = 24,
    RE_MAX_ANIMATION_FRAMES = 32,
    RE_MAX_BOSS_PHASES = 8,
    RE_MAX_GAMEPLAY_EVENTS = 128,
    RE_MAX_PATH_SECTORS = RE_MAX_SECTORS
};

typedef struct ReEntityId {
    uint16_t index;
    uint16_t generation;
} ReEntityId;

enum ReTrackingMode { RE_TRACK_PERCEPTION, RE_TRACK_OMNISCIENT };
enum ReCharacterState {
    RE_CHARACTER_IDLE,
    RE_CHARACTER_PATROL,
    RE_CHARACTER_CHASE,
    RE_CHARACTER_SEARCH,
    RE_CHARACTER_WINDUP,
    RE_CHARACTER_RECOVERY,
    RE_CHARACTER_STUNNED,
    RE_CHARACTER_DEAD
};
enum ReGameplayAction {
    RE_ACTION_WAIT,
    RE_ACTION_MELEE,
    RE_ACTION_PROJECTILE,
    RE_ACTION_CAPTURE,
    RE_ACTION_SUMMON,
    RE_ACTION_ACTIVATE
};
enum ReAnimationEvent { RE_ANIM_EVENT_NONE, RE_ANIM_EVENT_SOUND, RE_ANIM_EVENT_ATTACK };

typedef struct ReAnimationFrame {
    uint16_t cell;
    float duration;
    enum ReAnimationEvent event;
} ReAnimationFrame;
typedef struct ReAnimationClip {
    char name[24];
    unsigned int directions; /* 1, 4 u 8. */
    bool loop;
    size_t frame_count;
    ReAnimationFrame frames[RE_MAX_ANIMATION_FRAMES];
} ReAnimationClip;
typedef struct ReBossPhase {
    char name[24];
    float health_threshold; /* Fracción en [0,1]. */
    enum ReTrackingMode tracking;
    enum ReGameplayAction action;
    float speed_multiplier, cooldown;
    unsigned int summon_limit;
} ReBossPhase;
typedef struct ReCharacterDef {
    char id[64];
    char sprite[160];
    int cell_width, cell_height;
    float radius, height, step_height;
    float speed, turn_speed;
    float sight_range, field_of_view, hearing_range, memory_time;
    float capture_range, attack_range;
    int max_health, attack_damage;
    bool invulnerable, can_be_stunned, can_open_doors, capture_game_over;
    enum ReTrackingMode tracking;
    size_t animation_count, phase_count;
    ReAnimationClip animations[RE_MAX_ANIMATIONS];
    ReBossPhase phases[RE_MAX_BOSS_PHASES];
} ReCharacterDef;

typedef struct ReCharacter {
    bool active;
    uint16_t generation;
    uint16_t definition;
    ReBody body;
    int health;
    enum ReCharacterState state;
    float yaw, state_time, cooldown, memory_left, stuck_time, animation_time;
    ReVec3 last_known_position, previous_position;
    unsigned int phase, animation_frame, summoned;
    bool capture_emitted;
} ReCharacter;

enum ReGameplayEventKind {
    RE_EVENT_NONE,
    RE_EVENT_CAPTURED,
    RE_EVENT_PLAYER_DAMAGE,
    RE_EVENT_PROJECTILE,
    RE_EVENT_SUMMON,
    RE_EVENT_ACTIVATE,
    RE_EVENT_PHASE_CHANGED,
    RE_EVENT_ANIMATION
};
typedef struct ReGameplayEvent {
    enum ReGameplayEventKind kind;
    ReEntityId source;
    int value;
    ReVec3 position;
    char target[32];
} ReGameplayEvent;
typedef struct ReGameplayInput {
    const ReBody *player;
    ReVec3 player_eye;
    float player_noise; /* Radio audible de la acción más ruidosa del tick. */
} ReGameplayInput;
typedef struct ReGameplay {
    ReWorld *world; /* Préstamo mutable: puertas y vidrios viven en el mundo. */
    const ReCharacterDef *definitions;
    size_t definition_count;
    ReCharacter characters[RE_MAX_ENTITIES];
    ReGameplayEvent events[RE_MAX_GAMEPLAY_EVENTS];
    size_t event_head, event_count;
    uint32_t random_state;
} ReGameplay;

/* Carga transaccional de un archivo retro_actor 1. */
[[nodiscard]] bool re_character_load(const char *path, ReCharacterDef *out, ReError *error);
[[nodiscard]] bool re_character_save(const char *path, const ReCharacterDef *definition,
                                     ReError *error);
[[nodiscard]] bool re_character_validate(const ReCharacterDef *definition, ReError *error);
void re_gameplay_init(ReGameplay *gameplay, ReWorld *world, const ReCharacterDef *definitions,
                      size_t definition_count, uint32_t deterministic_seed);
[[nodiscard]] ReEntityId re_gameplay_spawn(ReGameplay *gameplay, size_t definition, ReVec3 position,
                                           float yaw, int sector);
ReCharacter *re_gameplay_get(ReGameplay *gameplay, ReEntityId id);
void re_gameplay_tick(ReGameplay *gameplay, ReGameplayInput input, float dt);
bool re_gameplay_damage(ReGameplay *gameplay, ReEntityId id, int damage, float stun_time);
bool re_gameplay_event(ReGameplay *gameplay, ReGameplayEvent *out);
/* Devuelve la celda de la hoja y la dirección ya cuantizada para renderizar. */
int re_character_animation_cell(const ReCharacterDef *definition, const ReCharacter *character,
                                const char *clip, float camera_yaw);
/* Abre/cierra con detección de cuerpos. Cerrar sobre un cuerpo invierte la
 * dirección y devuelve false; nunca aplasta ni teletransporta actores. */
bool re_barrier_tick(ReWorld *world, size_t barrier, float target, float speed, float dt,
                     const ReBody *bodies, size_t body_count);
bool re_barrier_damage(ReWorld *world, size_t barrier, float damage);

#endif
