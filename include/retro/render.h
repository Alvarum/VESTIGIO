/* Renderer CPU sin ventana. Todos los recursos tienen un único propietario;
 * las llamadas draw sólo los toman prestados durante la llamada. */
#ifndef RETRO_RENDER_H
#define RETRO_RENDER_H
#include "retro/world.h"
#include <stdint.h>

typedef struct RePixel {
    uint8_t r, g, b, a;
} RePixel;
static_assert(sizeof(RePixel) == 4, "La presentación necesita cuatro bytes RGBA por pixel");
typedef struct ReTexture {
    int width, height;
    RePixel *pixels;
} ReTexture;
typedef struct ReCamera {
    ReVec3 position;
    float yaw, pitch, fov; /* Radianes; fov horizontal. */
    float near_plane, far_plane;
} ReCamera;
typedef struct ReVertex {
    ReVec3 position;
    float u, v;
} ReVertex;
typedef struct ReRenderStats {
    size_t submitted, rasterized, shaded;
} ReRenderStats;
typedef struct ReRenderer {
    int width, height;
    RePixel *pixels;
    float *depth; /* Distancia Z en espacio de cámara; INFINITY = fondo. */
    ReRenderStats stats;
    bool wireframe;
} ReRenderer;
struct ReLight;

static inline RePixel re_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (RePixel){r, g, b, a};
}
/* init sólo acepta objetos a cero o previamente destruidos. Destrucción tolera
 * nullptr y objetos vacíos. Dimensiones limitadas a 4096 por eje. */
[[nodiscard]] bool re_texture_init(ReTexture *texture, int width, int height);
void re_texture_destroy(ReTexture *texture);
/* Convierte una celda de atlas en UV normalizadas. Rechaza dimensiones que no
 * dividan exactamente la textura y cualquier indice fuera del atlas. */
[[nodiscard]] bool re_texture_cell_uv(const ReTexture *texture, int cell_width, int cell_height,
                                      int cell, float out_uv[4]);
[[nodiscard]] bool re_renderer_init(ReRenderer *renderer, int width, int height);
void re_renderer_destroy(ReRenderer *renderer);
void re_renderer_clear(ReRenderer *renderer, RePixel background);
ReVec3 re_camera_forward(const ReCamera *camera);
ReCamera re_camera_interpolate(ReCamera previous, ReCamera current, float alpha);
/* UV en repeticiones, no en píxeles. Textura nullptr = blanco. Alpha <128 no
 * escribe color NI profundidad; útil para sprites recortados. */
void re_draw_triangle(ReRenderer *r, const ReCamera *camera, const ReVertex vertices[3],
                      const ReTexture *texture, float light);
void re_draw_world(ReRenderer *r, const ReCamera *camera, const ReWorld *world,
                   const ReTexture materials[RE_MAX_MATERIALS]);
/* Postproceso de luces retro. Agrupa como máximo ocho luces por tile de 16×16;
 * enabled tiene un elemento por light. No modifica el depth buffer. */
void re_apply_lights(ReRenderer *r, const ReCamera *camera, const struct ReLight *lights,
                     const bool *enabled, size_t light_count, float time);
void re_draw_billboard(ReRenderer *r, const ReCamera *camera, ReVec3 feet, float width,
                       float height, const ReTexture *texture, float light);
/* Variante para hojas de sprites. UV están normalizadas en [0,1]. */
void re_draw_billboard_region(ReRenderer *r, const ReCamera *camera, ReVec3 feet, float width,
                              float height, const ReTexture *texture, float light, float u0,
                              float v0, float u1, float v1);
void re_draw_depth(ReRenderer *r, float distance);
/* Primitivas 2D recortadas al framebuffer. UI no escribe el depth buffer. */
void re_rect(ReRenderer *r, int x, int y, int width, int height, RePixel color);
void re_line(ReRenderer *r, int x0, int y0, int x1, int y1, RePixel color);
void re_text(ReRenderer *r, int x, int y, const char *text, int scale, RePixel color);
void re_blit(ReRenderer *r, const ReTexture *texture, int x, int y, int scale);
/* PPM binario para capturas headless; la plataforma también ofrece PNG. */
[[nodiscard]] bool re_capture_ppm(const ReRenderer *r, const char *path);
#endif
