/* Arte procedural original, sin assets de Doom. Texturas de metal, paneles,
 * luminarias y sprites a mano en coordenadas de píxeles. Generados una vez.
 * Los recursos son datos editables: no afectan a física ni a reglas. */
#include "art.h"

static void fill(ReTexture *t, int x, int y, int w, int h, RePixel color) {
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            if (px >= 0 && py >= 0 && px < t->width && py < t->height)
                t->pixels[(size_t)py * (size_t)t->width + (size_t)px] = color;
}
static uint32_t noise(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
bool fps_art_init(FpsArt *art) {
    *art = (FpsArt){0};
    RePixel dark = re_rgba(18, 25, 31, 255), steel = re_rgba(91, 108, 115, 255);
    RePixel pale = re_rgba(192, 209, 209, 255), orange = re_rgba(237, 143, 58, 255);
    RePixel teal = re_rgba(75, 207, 193, 255), red = re_rgba(206, 66, 54, 255);
    for (int m = 0; m < RE_MAX_MATERIALS; m++) {
        ReTexture *t = &art->materials[m];
        if (!re_texture_init(t, 64, 64))
            goto fail;
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++) {
                int grain = (int)(noise((uint32_t)(x + y * 64 + m * 4096)) % 13u) - 6;
                int base = 70;
                if (m == 1)
                    base = ((x / 16 + y / 16) % 2) ? 63 : 72; /* Suelo: losetas. */
                if (m == 2)
                    base = 48; /* Techo. */
                if (m == 3)
                    base = 91; /* Panel naranja. */
                if (m == 4)
                    base = 52; /* Puerta acanalada. */
                RePixel p = re_rgba((uint8_t)(base + grain), (uint8_t)(base + grain + 10),
                                    (uint8_t)(base + grain + 15), 255);
                if (m == 3)
                    p = re_rgba((uint8_t)(base + grain + 40), (uint8_t)(base + grain - 15),
                                (uint8_t)(base + grain - 45), 255);
                t->pixels[(size_t)y * 64u + (size_t)x] = p;
            }
        if (m == 1) {
            for (int v = 0; v < 64; v += 16) {
                fill(t, v, 0, 1, 64, dark);
                fill(t, 0, v, 64, 1, dark);
            }
            fill(t, 3, 3, 2, 2, steel);
            fill(t, 59, 59, 2, 2, steel);
        } else if (m == 2) {
            fill(t, 0, 0, 64, 3, dark);
            fill(t, 0, 0, 3, 64, dark);
            fill(t, 10, 25, 44, 14, dark);
            fill(t, 12, 28, 40, 8, re_rgba(166, 183, 174, 255));
            fill(t, 14, 30, 36, 3, re_rgba(229, 236, 209, 255));
        } else if (m == 4) {
            for (int v = 0; v < 64; v += 8) {
                fill(t, 0, v, 64, 2, dark);
                fill(t, 0, v + 2, 64, 1, steel);
            }
            fill(t, 28, 0, 8, 64, dark);
            fill(t, 30, 0, 3, 64, orange);
        } else {
            fill(t, 0, 0, 3, 64, dark);
            fill(t, 0, 0, 64, 3, dark);
            fill(t, 3, 3, 1, 58, steel);
            fill(t, 0, 51, 64, 13, dark);
            fill(t, 0, 50, 64, 1, steel);
            fill(t, 8, 10, 48, 2, steel);
            fill(t, 8, 13, 48, 1, dark);
            for (int k = 0; k < 4; k++)
                fill(t, 10, 20 + k * 5, 28, 2, dark);
            fill(t, 46, 22, 6, 15, dark);
            fill(t, 48, 24, 2, 6, m == 3 ? orange : teal);
            fill(t, 6, 5, 2, 2, pale);
            fill(t, 57, 45, 2, 2, steel);
            if (m == 5) {
                fill(t, 4, 4, 56, 42, teal);
                fill(t, 7, 8, 50, 34, dark);
                fill(t, 11, 12, 42, 2, teal);
            }
        }
    }
    for (int sprite = 0; sprite < FPS_SPRITE_COUNT; sprite++) {
        ReTexture *t = &art->sprites[sprite];
        if (!re_texture_init(t, 32, 48))
            goto fail;
        if (sprite <= FPS_DEAD) {
            if (sprite == FPS_DEAD) {
                fill(t, 3, 42, 26, 4, dark);
                fill(t, 8, 39, 16, 6, red);
                fill(t, 20, 38, 8, 6, steel);
                continue;
            }
            RePixel armor = sprite == FPS_HURT ? red : re_rgba(111, 127, 104, 255);
            fill(t, 7, 43, 8, 4, dark);
            fill(t, 19, 43, 8, 4, dark);
            fill(t, 9, 29, 6, 15, steel);
            fill(t, 19, 29, 6, 15, steel);
            fill(t, 7, 13, 20, 19, dark);
            fill(t, 9, 14, 16, 15, armor);
            fill(t, 4, 16, 5, 17, steel);
            fill(t, 26, 16, 4, 17, steel);
            fill(t, 10, 1, 15, 14, dark);
            fill(t, 11, 2, 13, 11, steel);
            fill(t, 12, 6, 11, 4, dark);
            fill(t, 13, 7, 9, 2, orange);
            fill(t, 12, 17, 10, 5, dark);
            fill(t, 13, 18, 8, 2, orange);
            fill(t, 14, 25, 7, 9, dark);
            fill(t, 16, 26, 3, 12, steel);
            fill(t, 10, 31, 15, 2, orange);
        } else if (sprite == FPS_HEALTH) {
            fill(t, 5, 27, 23, 17, dark);
            fill(t, 6, 28, 21, 14, pale);
            fill(t, 14, 29, 5, 12, red);
            fill(t, 10, 33, 13, 4, red);
        } else if (sprite == FPS_AMMO) {
            fill(t, 6, 30, 21, 14, dark);
            fill(t, 8, 31, 17, 11, re_rgba(87, 114, 77, 255));
            for (int i = 0; i < 3; i++) {
                fill(t, 10 + i * 5, 25, 3, 12, orange);
                fill(t, 10 + i * 5, 25, 3, 3, pale);
            }
        } else if (sprite == FPS_KEY) {
            fill(t, 5, 29, 13, 13, orange);
            fill(t, 8, 32, 7, 7, dark);
            fill(t, 17, 33, 12, 4, orange);
            fill(t, 23, 36, 3, 6, orange);
            fill(t, 27, 36, 2, 4, orange);
        } else {
            fill(t, 4, 5, 24, 40, dark);
            fill(t, 6, 7, 20, 36, teal);
            fill(t, 9, 10, 14, 30, dark);
            fill(t, 14, 16, 4, 17, teal);
            fill(t, 10, 22, 12, 4, teal);
        }
    }
    if (!re_texture_init(&art->weapon, 96, 80))
        goto fail;
    ReTexture *gun = &art->weapon;
    fill(gun, 26, 53, 44, 27, re_rgba(61, 63, 61, 255));
    fill(gun, 21, 66, 54, 14, re_rgba(106, 112, 99, 255));
    fill(gun, 34, 19, 29, 56, dark);
    fill(gun, 37, 17, 23, 43, steel);
    fill(gun, 40, 13, 17, 31, pale);
    fill(gun, 42, 9, 13, 11, dark);
    fill(gun, 44, 12, 9, 6, re_rgba(6, 10, 13, 255));
    fill(gun, 37, 42, 23, 5, orange);
    fill(gun, 40, 50, 16, 20, re_rgba(43, 56, 64, 255));
    fill(gun, 44, 22, 9, 13, re_rgba(138, 155, 163, 255));
    for (int i = 0; i < 4; i++)
        fill(gun, 40, 53 + i * 4, 16, 2, dark);
    return true;
fail:
    fps_art_destroy(art);
    return false;
}
void fps_art_destroy(FpsArt *art) {
    for (int i = 0; i < RE_MAX_MATERIALS; i++)
        re_texture_destroy(&art->materials[i]);
    for (int i = 0; i < FPS_SPRITE_COUNT; i++)
        re_texture_destroy(&art->sprites[i]);
    re_texture_destroy(&art->weapon);
}
void fps_sound_generate(int effect, int16_t *output, size_t count, unsigned int rate) {
    const float frequency[5] = {150, 650, 95, 70, 880};
    for (size_t i = 0; i < count; i++) {
        float t = (float)i / (float)rate;
        float progress = (float)i / (float)count;
        float envelope = (1 - progress) * (1 - progress);
        float tone = sinf(2 * RE_PI * frequency[effect % 5] * t * (1 - progress * 0.4f));
        float hiss = (float)(noise((uint32_t)i + 23u) % 65536u) / 32768.0f - 1;
        float mix = effect == 0 ? 0.7f : 0.12f;
        output[i] = (int16_t)(envelope * (tone * (1 - mix) + hiss * mix) * 14000);
    }
}
