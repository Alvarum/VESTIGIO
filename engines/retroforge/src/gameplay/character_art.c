/* Arte provisional original compartido por Player y Studio. La misma fórmula
 * evita que la biblioteca represente otro personaje distinto al de la partida.
 * Sólo se genera al cargar; no participa del bucle de simulación. */
#include "retro/character_art.h"
#include <stdlib.h>

bool re_character_placeholder(ReTexture *texture, const ReCharacterDef *definition, size_t seed) {
    if (!texture || !definition)
        return false;
    int cell_width = definition->cell_width > 0 ? definition->cell_width : 32;
    int cell_height = definition->cell_height > 0 ? definition->cell_height : 48;
    int columns = 8, rows = 8;
    if (cell_width > 512 || cell_height > 512 ||
        !re_texture_init(texture, cell_width * columns, cell_height * rows))
        return false;
    for (int cell = 0; cell < columns * rows; cell++)
        for (int y = 0; y < cell_height; y++)
            for (int x = 0; x < cell_width; x++) {
                int pose = cell % 4;
                int center = cell_width / 2 + pose - 2;
                int head_y = cell_height / 6, head_radius = cell_width / 6;
                int dx = x - center, dy = y - head_y;
                bool hair = dx * dx + dy * dy <= head_radius * head_radius;
                bool face =
                    dx * dx + dy * dy <= (head_radius - 2) * (head_radius - 2) && y >= head_y - 1;
                int torso_top = cell_height / 3, torso_bottom = cell_height * 2 / 3;
                int half_body = cell_width / 5 + (y - torso_top) / 10;
                bool coat = y >= torso_top && y <= torso_bottom && abs(x - center) <= half_body;
                bool left_arm =
                    y >= torso_top && y < torso_bottom && abs(x - (center - half_body - 2)) <= 2;
                bool right_arm =
                    y >= torso_top && y < torso_bottom && abs(x - (center + half_body + 2)) <= 2;
                bool left_leg = y > torso_bottom && y < cell_height - 2 &&
                                abs(x - (center - cell_width / 8 - pose % 2)) <= 2;
                bool right_leg = y > torso_bottom && y < cell_height - 2 &&
                                 abs(x - (center + cell_width / 8 + pose % 2)) <= 2;
                uint8_t red = (uint8_t)(150u + (unsigned int)(seed % 3u) * 28u +
                                        (unsigned int)(cell % 3) * 8u);
                RePixel pixel = re_rgba(0, 0, 0, 0);
                if (hair)
                    pixel = re_rgba(35, 25, 28, 255);
                if (face)
                    pixel = re_rgba(207, 158, 132, 255);
                if (coat || left_arm || right_arm)
                    pixel = re_rgba(red, (uint8_t)(48 + pose * 7), 62, 255);
                if (left_leg || right_leg)
                    pixel = re_rgba(38, 42, 49, 255);
                size_t px = (size_t)(cell % columns) * (size_t)cell_width + (size_t)x;
                size_t py = (size_t)(cell / columns) * (size_t)cell_height + (size_t)y;
                texture->pixels[py * (size_t)texture->width + px] = pixel;
            }
    return true;
}
