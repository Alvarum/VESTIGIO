/* UI de píxeles: mismas coordenadas internas que el mundo, independiente de
 * ventana/GPU. Fuente monoespaciada 5x7 dibujada con máscaras originales. */
#include "retro/render.h"
#include <stdint.h>
#include <stdlib.h>

static void pixel(ReRenderer *r, int x, int y, RePixel color) {
    if (x < 0 || y < 0 || x >= r->width || y >= r->height)
        return;
    RePixel *dst = &r->pixels[(size_t)y * (size_t)r->width + (size_t)x];
    unsigned int a = color.a, inv = 255 - a;
    dst->r = (uint8_t)(((unsigned int)color.r * a + (unsigned int)dst->r * inv) / 255);
    dst->g = (uint8_t)(((unsigned int)color.g * a + (unsigned int)dst->g * inv) / 255);
    dst->b = (uint8_t)(((unsigned int)color.b * a + (unsigned int)dst->b * inv) / 255);
    dst->a = 255;
}
void re_rect(ReRenderer *r, int x, int y, int width, int height, RePixel color) {
    if (width <= 0 || height <= 0)
        return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + width > r->width ? r->width : x + width,
        y1 = y + height > r->height ? r->height : y + height;
    for (int py = y0; py < y1; py++)
        for (int px = x0; px < x1; px++)
            pixel(r, px, py, color);
}
void re_line(ReRenderer *r, int x0, int y0, int x1, int y1, RePixel color) {
    /* Bresenham usa error acumulado entero. Los llamantes dan coordenadas
     * acotadas de pantalla; pixel hace el recorte final. */
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1,
        error = dx + dy;
    for (;;) {
        pixel(r, x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int doubled = 2 * error;
        if (doubled >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

/* Cinco bits por fila. Se conserva una tabla explícita para que puedas cambiar
 * la tipografía sin herramientas externas ni dependencias de fuentes. */
static const uint8_t alphabet[36][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 25, 21, 19, 19, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
/* Decodifica un punto de código UTF-8 y avanza exactamente una unidad visual.
 * Ante una secuencia dañada consume un byte y dibuja '?': así el texto sigue
 * siendo legible y nunca interpretamos un byte de continuación como otro
 * carácter ni desplazamos todo lo que viene después. */
static uint32_t utf8_next(const char **cursor) {
    const unsigned char *s = (const unsigned char *)*cursor;
    uint32_t codepoint = '?';
    size_t length = 1;
    if (s[0] < 0x80u) {
        codepoint = s[0];
    } else if (s[0] >= 0xC2u && s[0] <= 0xDFu && (s[1] & 0xC0u) == 0x80u) {
        codepoint = ((uint32_t)(s[0] & 0x1Fu) << 6u) | (uint32_t)(s[1] & 0x3Fu);
        length = 2;
    } else if (s[0] >= 0xE0u && s[0] <= 0xEFu && s[1] && s[2] && (s[1] & 0xC0u) == 0x80u &&
               (s[2] & 0xC0u) == 0x80u) {
        uint32_t candidate = ((uint32_t)(s[0] & 0x0Fu) << 12u) | ((uint32_t)(s[1] & 0x3Fu) << 6u) |
                             (uint32_t)(s[2] & 0x3Fu);
        if (candidate >= 0x800u && !(candidate >= 0xD800u && candidate <= 0xDFFFu)) {
            codepoint = candidate;
            length = 3;
        }
    } else if (s[0] >= 0xF0u && s[0] <= 0xF4u && s[1] && s[2] && s[3] && (s[1] & 0xC0u) == 0x80u &&
               (s[2] & 0xC0u) == 0x80u && (s[3] & 0xC0u) == 0x80u) {
        uint32_t candidate = ((uint32_t)(s[0] & 0x07u) << 18u) | ((uint32_t)(s[1] & 0x3Fu) << 12u) |
                             ((uint32_t)(s[2] & 0x3Fu) << 6u) | (uint32_t)(s[3] & 0x3Fu);
        if (candidate >= 0x10000u && candidate <= 0x10FFFFu) {
            codepoint = candidate;
            length = 4;
        }
    }
    *cursor += length;
    return codepoint;
}

static uint8_t glyph(uint32_t codepoint, int row) {
    static const uint8_t spanish[][7] = {
        {4, 2, 14, 17, 31, 17, 17},  /* Á */
        {4, 2, 31, 16, 30, 16, 31},  /* É */
        {4, 2, 14, 4, 4, 4, 14},     /* Í */
        {4, 2, 14, 17, 17, 17, 14},  /* Ó */
        {4, 2, 17, 17, 17, 17, 14},  /* Ú */
        {10, 5, 17, 25, 21, 19, 17}, /* Ñ */
        {10, 0, 17, 17, 17, 17, 14}, /* Ü */
        {4, 0, 4, 2, 1, 17, 14},     /* ¿ */
        {4, 0, 4, 4, 4, 4, 4},       /* ¡ */
    };
    int c = (int)codepoint;
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    switch (codepoint) {
    case 0x00C1u:
    case 0x00E1u:
        return spanish[0][row];
    case 0x00C9u:
    case 0x00E9u:
        return spanish[1][row];
    case 0x00CDu:
    case 0x00EDu:
        return spanish[2][row];
    case 0x00D3u:
    case 0x00F3u:
        return spanish[3][row];
    case 0x00DAu:
    case 0x00FAu:
        return spanish[4][row];
    case 0x00D1u:
    case 0x00F1u:
        return spanish[5][row];
    case 0x00DCu:
    case 0x00FCu:
        return spanish[6][row];
    case 0x00BFu:
        return spanish[7][row];
    case 0x00A1u:
        return spanish[8][row];
    default:
        break;
    }
    if (c >= 'A' && c <= 'Z')
        return alphabet[c - 'A'][row];
    if (c >= '0' && c <= '9')
        return alphabet[26 + c - '0'][row];
    switch (c) {
    case ':':
        return row == 2 || row == 5 ? 4 : 0;
    case '.':
        return row == 6 ? 4 : 0;
    case '-':
        return row == 3 ? 14 : 0;
    case '+':
        return row == 3 ? 14 : (row >= 2 && row <= 4 ? 4 : 0);
    case '/':
        return (uint8_t)(1u << (unsigned int)(row * 4 / 6));
    case '>':
        return (uint8_t)(1u << (unsigned int)(row <= 3 ? 3 - row : row - 3));
    case '<':
        return (uint8_t)(1u << (unsigned int)(row <= 3 ? row : 6 - row));
    case '[':
        return row == 0 || row == 6 ? 14 : 8;
    case ']':
        return row == 0 || row == 6 ? 14 : 2;
    case '!':
        return row < 4 || row == 6 ? 4 : 0;
    case '?':
        return (const uint8_t[7]){14, 17, 1, 2, 4, 0, 4}[row];
    case ',':
        return row == 5 ? 4 : (row == 6 ? 8 : 0);
    case ';':
        return row == 2 ? 4 : (row == 5 ? 4 : (row == 6 ? 8 : 0));
    case '\'':
        return row <= 1 ? 4 : 0;
    case '"':
        return row <= 1 ? 10 : 0;
    case '%':
        return row == 0 || row == 1
                   ? 17
                   : (row == 5 || row == 6 ? 17 : (uint8_t)(1u << (unsigned int)(row - 1)));
    case '_':
        return row == 6 ? 31 : 0;
    default:
        return 0;
    }
}
void re_text(ReRenderer *r, int x, int y, const char *text, int scale, RePixel color) {
    if (!r || !text || scale < 1 || scale > 8)
        return;
    int start = x;
    const char *cursor = text;
    while (*cursor) {
        uint32_t codepoint = utf8_next(&cursor);
        if (codepoint == '\n') {
            x = start;
            y += 9 * scale;
            continue;
        }
        for (int row = 0; row < 7; row++) {
            uint8_t bits = glyph(codepoint, row);
            for (int col = 0; col < 5; col++)
                if (bits & (1u << (unsigned int)(4 - col)))
                    re_rect(r, x + col * scale, y + row * scale, scale, scale, color);
        }
        x += 6 * scale;
    }
}
void re_blit(ReRenderer *r, const ReTexture *t, int x, int y, int scale) {
    if (!t || !t->pixels || scale < 1 || scale > 8)
        return;
    for (int py = 0; py < t->height; py++)
        for (int px = 0; px < t->width; px++) {
            RePixel color = t->pixels[(size_t)py * (size_t)t->width + (size_t)px];
            if (color.a)
                re_rect(r, x + px * scale, y + py * scale, scale, scale, color);
        }
}
