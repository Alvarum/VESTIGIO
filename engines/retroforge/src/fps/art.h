/* Recursos originales del FPS. El motor sólo recibe texturas y no conoce su arte. */
#ifndef FPS_ART_H
#define FPS_ART_H
#include "retro/render.h"
enum FpsSprite {
    FPS_GUARD,
    FPS_HURT,
    FPS_DEAD,
    FPS_HEALTH,
    FPS_AMMO,
    FPS_KEY,
    FPS_EXIT,
    FPS_SPRITE_COUNT
};
typedef struct FpsArt {
    ReTexture materials[RE_MAX_MATERIALS];
    ReTexture sprites[FPS_SPRITE_COUNT];
    ReTexture weapon;
} FpsArt;
[[nodiscard]] bool fps_art_init(FpsArt *art);
void fps_art_destroy(FpsArt *art);
/* Generador PCM reproducible; output debe tener count muestras. */
void fps_sound_generate(int effect, int16_t *output, size_t count, unsigned int rate);
#endif
