/* Lógica dirigida por datos para juegos RetroForge.
 *
 * Este módulo conecta hechos de la simulación (entrar en una zona, interactuar,
 * morir, recoger un objeto) con condiciones y acciones. Las definiciones son
 * inmutables durante una partida; ReInteractionState contiene todo el estado
 * que debe reiniciarse, incluirse en un checkpoint o guardarse en disco.
 *
 * No depende de raylib. Las capacidades fijas hacen visibles los límites y
 * evitan asignaciones dinámicas dentro del tick de 60 Hz. */
#ifndef RETRO_INTERACTION_H
#define RETRO_INTERACTION_H

#include "retro/gameplay.h"

enum {
    RE_MAX_VARIABLES = 64,
    RE_MAX_ITEMS = 32,
    RE_MAX_OBJECTIVES = 32,
    RE_MAX_TRIGGERS = 64,
    RE_MAX_LIGHTS = 32,
    RE_MAX_RULES = 128,
    RE_MAX_RULE_CONDITIONS = 8,
    RE_MAX_RULE_ACTIONS = 12,
    RE_MAX_LOGIC_EVENTS = 128,
    RE_MAX_DIALOGUE_NODES = 96,
    RE_MAX_DIALOGUE_CHOICES = 4,
    RE_MAX_RUNTIME_PICKUPS = 64,
    RE_LOGIC_EVENT_BUDGET = 256
};

enum ReValueKind { RE_VALUE_NONE, RE_VALUE_BOOL, RE_VALUE_INT, RE_VALUE_FLOAT, RE_VALUE_TEXT };
typedef struct ReValue {
    enum ReValueKind kind;
    union {
        bool boolean;
        int integer;
        float real;
        char text[64];
    } as;
} ReValue;

enum ReLogicEventKind {
    RE_LOGIC_LEVEL_START,
    RE_LOGIC_TRIGGER_ENTER,
    RE_LOGIC_TRIGGER_STAY,
    RE_LOGIC_TRIGGER_EXIT,
    RE_LOGIC_INTERACT,
    RE_LOGIC_ENTITY_DAMAGED,
    RE_LOGIC_ENTITY_DIED,
    RE_LOGIC_CAPTURED,
    RE_LOGIC_ITEM_PICKED,
    RE_LOGIC_ITEM_USED,
    RE_LOGIC_DIALOGUE_CHOICE,
    RE_LOGIC_DIALOGUE_FINISHED,
    RE_LOGIC_OBJECTIVE_CHANGED,
    RE_LOGIC_TIMER,
    RE_LOGIC_ANIMATION,
    RE_LOGIC_BARRIER_CHANGED,
    RE_LOGIC_PLAYER_DIED,
    RE_LOGIC_GAME_LOADED,
    RE_LOGIC_CUSTOM
};
typedef struct ReLogicEvent {
    enum ReLogicEventKind kind;
    char source[32];
    char target[32];
    ReValue value;
    ReVec3 position;
} ReLogicEvent;

enum ReCompareOp {
    RE_COMPARE_EQUAL,
    RE_COMPARE_NOT_EQUAL,
    RE_COMPARE_LESS,
    RE_COMPARE_LESS_EQUAL,
    RE_COMPARE_GREATER,
    RE_COMPARE_GREATER_EQUAL
};
enum ReConditionKind {
    RE_CONDITION_VARIABLE,
    RE_CONDITION_ITEM_COUNT,
    RE_CONDITION_OBJECTIVE,
    RE_CONDITION_PLAYER_HEALTH,
    RE_CONDITION_PLAYER_LIVES
};
typedef struct ReRuleCondition {
    enum ReConditionKind kind;
    enum ReCompareOp comparison;
    char key[32];
    ReValue value;
} ReRuleCondition;

enum ReActionKind {
    RE_RULE_SET_VARIABLE,
    RE_RULE_ADD_VARIABLE,
    RE_RULE_GIVE_ITEM,
    RE_RULE_TAKE_ITEM,
    RE_RULE_SET_OBJECTIVE,
    RE_RULE_SHOW_MESSAGE,
    RE_RULE_OPEN_BARRIER,
    RE_RULE_CLOSE_BARRIER,
    RE_RULE_DAMAGE_PLAYER,
    RE_RULE_HEAL_PLAYER,
    RE_RULE_SET_LIVES,
    RE_RULE_CHECKPOINT,
    RE_RULE_START_DIALOGUE,
    RE_RULE_TOGGLE_LIGHT,
    RE_RULE_SPAWN_PICKUP,
    RE_RULE_EMIT_EVENT,
    RE_RULE_WIN,
    RE_RULE_GAME_OVER
};
typedef struct ReRuleAction {
    enum ReActionKind kind;
    char target[32];
    ReValue value;
} ReRuleAction;
typedef struct ReRuleDefinition {
    char id[32];
    enum ReLogicEventKind event;
    char source[32]; /* "*" acepta cualquier origen. */
    int priority;
    bool once;
    float cooldown;
    size_t condition_count, action_count;
    ReRuleCondition conditions[RE_MAX_RULE_CONDITIONS];
    ReRuleAction actions[RE_MAX_RULE_ACTIONS];
} ReRuleDefinition;

typedef struct ReVariableDefinition {
    char id[32];
    ReValue initial;
} ReVariableDefinition;
typedef struct ReItemDefinition {
    char id[32];
    char name[48];
    unsigned int max_stack;
} ReItemDefinition;
enum ReObjectiveStatus {
    RE_OBJECTIVE_INACTIVE,
    RE_OBJECTIVE_ACTIVE,
    RE_OBJECTIVE_COMPLETE,
    RE_OBJECTIVE_FAILED
};
typedef struct ReObjectiveDefinition {
    char id[32];
    char title[80];
} ReObjectiveDefinition;

enum ReTriggerShape { RE_TRIGGER_BOX, RE_TRIGGER_CYLINDER, RE_TRIGGER_SECTOR };
typedef struct ReTriggerVolume {
    char id[32];
    enum ReTriggerShape shape;
    ReVec3 center;
    ReVec3 half_size;
    float radius;
    int sector;
    bool once;
} ReTriggerVolume;

enum ReLightKind { RE_LIGHT_POINT, RE_LIGHT_SPOT };
typedef struct ReLight {
    char id[32];
    enum ReLightKind kind;
    ReVec3 position;
    ReVec3 color; /* Componentes lineales en [0,1]. */
    float radius, intensity, yaw, cone, flicker;
    bool initially_enabled;
} ReLight;

typedef struct ReDialogueChoice {
    char id[24];
    char text[96];
    char next[32]; /* "end" termina la conversación. */
    char condition_variable[32];
    bool condition_value;
} ReDialogueChoice;
typedef struct ReDialogueNode {
    char id[32];
    char speaker[48];
    char text[192];
    char next[32];
    bool pauses_world;
    size_t choice_count;
    ReDialogueChoice choices[RE_MAX_DIALOGUE_CHOICES];
} ReDialogueNode;

typedef struct ReInteractionDefinitions {
    ReVariableDefinition variables[RE_MAX_VARIABLES];
    ReItemDefinition items[RE_MAX_ITEMS];
    ReObjectiveDefinition objectives[RE_MAX_OBJECTIVES];
    ReTriggerVolume triggers[RE_MAX_TRIGGERS];
    ReLight lights[RE_MAX_LIGHTS];
    ReRuleDefinition rules[RE_MAX_RULES];
    ReDialogueNode dialogues[RE_MAX_DIALOGUE_NODES];
    size_t variable_count, item_count, objective_count, trigger_count, light_count, rule_count,
        dialogue_count;
} ReInteractionDefinitions;

typedef struct ReRuntimePickup {
    char id[32];
    char item[32];
    ReVec3 position;
    int sector;
    unsigned int count;
    bool active;
} ReRuntimePickup;

typedef struct ReInteractionState {
    ReValue variables[RE_MAX_VARIABLES];
    unsigned int inventory[RE_MAX_ITEMS];
    enum ReObjectiveStatus objectives[RE_MAX_OBJECTIVES];
    bool trigger_inside[RE_MAX_TRIGGERS], trigger_consumed[RE_MAX_TRIGGERS];
    bool rule_fired[RE_MAX_RULES], light_enabled[RE_MAX_LIGHTS];
    float rule_cooldown[RE_MAX_RULES], trigger_stay[RE_MAX_TRIGGERS];
    ReRuntimePickup pickups[RE_MAX_RUNTIME_PICKUPS];
    size_t pickup_count;
    ReLogicEvent events[RE_MAX_LOGIC_EVENTS];
    size_t event_head, event_count;
    int player_health, player_max_health, player_lives;
    char active_dialogue[32], message[192];
    float message_time;
    bool dialogue_pauses, won, game_over, cycle_limited, checkpoint_valid;
    ReBody checkpoint_player;
    int checkpoint_health, checkpoint_lives;
    ReValue checkpoint_variables[RE_MAX_VARIABLES];
    unsigned int checkpoint_inventory[RE_MAX_ITEMS];
    enum ReObjectiveStatus checkpoint_objectives[RE_MAX_OBJECTIVES];
    /* Un checkpoint es una fotografía de la simulación, no sólo una
     * coordenada de respawn. Así una llave no reaparece, una puerta conserva
     * su estado y un enemigo muerto no revive accidentalmente al reintentar. */
    bool checkpoint_trigger_inside[RE_MAX_TRIGGERS];
    bool checkpoint_trigger_consumed[RE_MAX_TRIGGERS];
    bool checkpoint_rule_fired[RE_MAX_RULES];
    bool checkpoint_light_enabled[RE_MAX_LIGHTS];
    float checkpoint_rule_cooldown[RE_MAX_RULES];
    float checkpoint_trigger_stay[RE_MAX_TRIGGERS];
    ReRuntimePickup checkpoint_pickups[RE_MAX_RUNTIME_PICKUPS];
    size_t checkpoint_pickup_count;
    ReBarrier checkpoint_barriers[RE_MAX_BARRIERS];
    ReCharacter checkpoint_characters[RE_MAX_ENTITIES];
    uint32_t checkpoint_random_state;
} ReInteractionState;

typedef struct ReInteractionRuntime {
    const ReInteractionDefinitions *definitions; /* Préstamo inmutable. */
    ReWorld *world;                              /* Préstamo mutable. */
    ReGameplay *gameplay;                        /* Puede ser nullptr. */
    ReInteractionState state;
    size_t rule_order[RE_MAX_RULES];
} ReInteractionRuntime;

[[nodiscard]] bool re_interaction_load_rules(const char *path, ReInteractionDefinitions *out,
                                             ReError *error);
[[nodiscard]] bool re_interaction_load_dialogues(const char *path, ReInteractionDefinitions *out,
                                                 ReError *error);
[[nodiscard]] bool re_interaction_save_rules(const char *path,
                                             const ReInteractionDefinitions *definitions,
                                             ReError *error);
[[nodiscard]] bool re_interaction_save_dialogues(const char *path,
                                                 const ReInteractionDefinitions *definitions,
                                                 ReError *error);
[[nodiscard]] bool re_interaction_validate(const ReInteractionDefinitions *definitions,
                                           ReError *error);
void re_interaction_init(ReInteractionRuntime *runtime, const ReInteractionDefinitions *definitions,
                         ReWorld *world, ReGameplay *gameplay, int health, int lives);
bool re_interaction_emit(ReInteractionRuntime *runtime, ReLogicEvent event);
void re_interaction_tick(ReInteractionRuntime *runtime, ReBody *player, float dt);
bool re_interaction_interact(ReInteractionRuntime *runtime, ReVec3 origin, ReVec3 direction,
                             float distance);
bool re_interaction_choose(ReInteractionRuntime *runtime, size_t visible_choice);
bool re_interaction_set_variable(ReInteractionRuntime *runtime, const char *id, ReValue value);
const ReValue *re_interaction_variable(const ReInteractionRuntime *runtime, const char *id);
unsigned int re_interaction_item_count(const ReInteractionRuntime *runtime, const char *id);
enum ReObjectiveStatus re_interaction_objective(const ReInteractionRuntime *runtime,
                                                const char *id);
const ReDialogueNode *re_interaction_dialogue(const ReInteractionRuntime *runtime);
void re_interaction_checkpoint_restore(ReInteractionRuntime *runtime, ReBody *player);

/* El snapshot incluye estado de reglas, jugador, barreras, enemigos y RNG.
 * Se escribe mediante temporal + reemplazo atómico y lleva CRC32. */
[[nodiscard]] bool re_save_write(const char *path, const char *project_id,
                                 const ReInteractionRuntime *runtime, const ReBody *player,
                                 ReError *error);
[[nodiscard]] bool re_save_read(const char *path, const char *project_id,
                                ReInteractionRuntime *runtime, ReBody *player, ReError *error);

#endif
