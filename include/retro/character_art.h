/* Generador del atlas provisional (8 x 8 celdas), independiente de raylib.
 * texture debe estar vacío; el llamador libera con re_texture_destroy.
 * Devuelve false si las dimensiones exceden el límite o falla la reserva. */
#ifndef RETRO_CHARACTER_ART_H
#define RETRO_CHARACTER_ART_H
#include "retro/gameplay.h"
#include "retro/render.h"
bool re_character_placeholder(ReTexture *texture, const ReCharacterDef *definition, size_t seed);
#endif
