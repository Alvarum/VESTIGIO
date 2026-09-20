/* Juego de ejemplo: reglas específicas separadas del motor y de la plataforma.
 * Puede simularse entero en pruebas sin abrir una ventana ni un dispositivo. */
#ifndef FPS_GAME_H
#define FPS_GAME_H
#include "art.h"
#include "retro/input.h"
#include "retro/render.h"

enum FpsMode { FPS_TITLE, FPS_PLAYING, FPS_PAUSED, FPS_OPTIONS, FPS_WON, FPS_LOST };
enum FpsEnemyState { FPS_IDLE, FPS_CHASE, FPS_ATTACK, FPS_PAIN, FPS_CORPSE };
enum FpsPickupType { FPS_PICK_HEALTH, FPS_PICK_AMMO, FPS_PICK_KEY, FPS_PICK_EXIT };
enum FpsSound {
    FPS_SOUND_SHOT = 1u << 0,
    FPS_SOUND_PICKUP = 1u << 1,
    FPS_SOUND_HURT = 1u << 2,
    FPS_SOUND_DOOR = 1u << 3,
    FPS_SOUND_WIN = 1u << 4
};
enum { FPS_MAX_ENEMIES = 16, FPS_MAX_PICKUPS = 32 };
typedef struct FpsEnemy {
    ReBody body;
    int health;
    enum FpsEnemyState state;
    float timer;
} FpsEnemy;
typedef struct FpsPickup {
    enum FpsPickupType type;
    ReVec3 position;
    bool active;
} FpsPickup;
typedef struct FpsTuning {
    float move_speed, jump_speed, gravity, shot_interval, enemy_speed, attack_interval;
    int shot_damage, enemy_health, attack_damage;
} FpsTuning;
typedef struct FpsGame {
    ReWorld initial_world, world;
    ReBody player;
    ReCamera previous_camera, camera;
    FpsEnemy enemies[FPS_MAX_ENEMIES];
    FpsPickup pickups[FPS_MAX_PICKUPS];
    size_t enemy_count, pickup_count;
    enum FpsMode mode, options_parent;
    int health, ammo, kills, selection, door_sector;
    bool key, door_opening, quit, map_view, wire_view, depth_view, stats_view;
    float door_height, shot_timer, hurt_timer, hit_timer, elapsed, message_timer;
    float sensitivity, volume;
    char message[80];
    uint32_t sound_events;
    FpsTuning tuning;
} FpsGame;

/* game debe estar inicializado a cero. Valida tanto geometría como marcadores
 * del FPS. Error no inicia una partida parcialmente utilizable. */
[[nodiscard]] bool fps_game_init(FpsGame *game, const char *map, ReError *error);
void fps_game_restart(FpsGame *game);
/* frame atiende menús y foco una vez por frame; tick sólo simula PLAYING. */
void fps_game_frame(FpsGame *game, ReInput input);
void fps_game_tick(FpsGame *game, ReInput input, float dt);
void fps_game_draw(const FpsGame *game, const FpsArt *art, ReRenderer *renderer, float alpha);
#endif
