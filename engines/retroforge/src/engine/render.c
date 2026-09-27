/* Rasterización completa en CPU. raylib no participa en estas matemáticas.
 * Ver docs/03-matematicas-renderer.md para derivación y convenciones. */
#include "retro/render.h"
#include "retro/interaction.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static_assert(CHAR_BIT == 8, "El formato RGBA8 requiere bytes de 8 bits");

bool re_texture_init(ReTexture *t, int width, int height) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return false;
    size_t count = (size_t)width * (size_t)height;
    RePixel *pixels = calloc(count, sizeof(*pixels));
    if (!pixels)
        return false;
    *t = (ReTexture){width, height, pixels};
    return true;
}
void re_texture_destroy(ReTexture *t) {
    if (!t)
        return;
    free(t->pixels);
    *t = (ReTexture){0};
}

bool re_texture_cell_uv(const ReTexture *texture, int cell_width, int cell_height, int cell,
                        float out_uv[4]) {
    if (!texture || !texture->pixels || !out_uv || cell_width <= 0 || cell_height <= 0 ||
        cell < 0 || texture->width < cell_width || texture->height < cell_height)
        return false;
    /* Los importadores y algunos generadores dejan uno o dos píxeles
     * transparentes al final de una hoja. Las celdas completas siguen siendo
     * válidas: la división entera ignora solamente ese margen final. Nunca
     * ampliamos una celda ni usamos la hoja completa como recuperación. */
    int columns = texture->width / cell_width;
    int rows = texture->height / cell_height;
    if (columns <= 0 || rows <= 0 || cell >= columns * rows)
        return false;
    int column = cell % columns, row = cell / columns;
    out_uv[0] = (float)(column * cell_width) / (float)texture->width;
    out_uv[1] = (float)(row * cell_height) / (float)texture->height;
    out_uv[2] = (float)((column + 1) * cell_width) / (float)texture->width;
    out_uv[3] = (float)((row + 1) * cell_height) / (float)texture->height;
    return true;
}
bool re_renderer_init(ReRenderer *r, int width, int height) {
    ReTexture pixels = {0};
    if (!re_texture_init(&pixels, width, height))
        return false;
    float *depth = malloc((size_t)width * (size_t)height * sizeof(*depth));
    if (!depth) {
        re_texture_destroy(&pixels);
        return false;
    }
    *r = (ReRenderer){.width = width, .height = height, .pixels = pixels.pixels, .depth = depth};
    re_renderer_clear(r, re_rgba(0, 0, 0, 255));
    return true;
}
void re_renderer_destroy(ReRenderer *r) {
    if (!r)
        return;
    free(r->pixels);
    free(r->depth);
    *r = (ReRenderer){0};
}
void re_renderer_clear(ReRenderer *r, RePixel color) {
    size_t count = (size_t)r->width * (size_t)r->height;
    for (size_t i = 0; i < count; i++) {
        r->pixels[i] = color;
        r->depth[i] = INFINITY;
    }
    r->stats = (ReRenderStats){0};
}
ReVec3 re_camera_forward(const ReCamera *c) {
    return re_v3(sinf(c->yaw) * cosf(c->pitch), cosf(c->yaw) * cosf(c->pitch), sinf(c->pitch));
}
ReCamera re_camera_interpolate(ReCamera a, ReCamera b, float alpha) {
    ReCamera result = b;
    result.position = re_add3(a.position, re_scale3(re_sub3(b.position, a.position), alpha));
    /* remainder devuelve el arco corto al cruzar +/-PI. */
    result.yaw = a.yaw + remainderf(b.yaw - a.yaw, 2 * RE_PI) * alpha;
    result.pitch = re_lerp(a.pitch, b.pitch, alpha);
    return result;
}

typedef struct ClipVertex {
    float x, y, z, u, v;
} ClipVertex;
typedef struct ScreenVertex {
    float x, y, inv_z, u_over_z, v_over_z;
} ScreenVertex;

static ClipVertex camera_vertex(ReVertex v, const ReCamera *c) {
    ReVec3 d = re_sub3(v.position, c->position);
    float sy = sinf(c->yaw), cy = cosf(c->yaw), sp = sinf(c->pitch), cp = cosf(c->pitch);
    float forward = d.x * sy + d.y * cy;
    return (ClipVertex){d.x * cy - d.y * sy, d.z * cp - forward * sp, forward * cp + d.z * sp, v.u,
                        v.v};
}
static float plane_distance(ClipVertex p, int plane, const ReCamera *c, float tx, float ty) {
    switch (plane) {
    case 0:
        return p.z - c->near_plane;
    case 1:
        return c->far_plane - p.z;
    case 2:
        return p.z * tx + p.x;
    case 3:
        return p.z * tx - p.x;
    case 4:
        return p.z * ty + p.y;
    default:
        return p.z * ty - p.y;
    }
}
static ClipVertex clip_lerp(ClipVertex a, ClipVertex b, float t) {
    return (ClipVertex){re_lerp(a.x, b.x, t), re_lerp(a.y, b.y, t), re_lerp(a.z, b.z, t),
                        re_lerp(a.u, b.u, t), re_lerp(a.v, b.v, t)};
}
static float edge(ScreenVertex a, ScreenVertex b, float x, float y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
static bool top_left(ScreenVertex a, ScreenVertex b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    return dy < 0 || (dy == 0 && dx > 0);
}
static RePixel sample(const ReTexture *t, float u, float v) {
    if (!t || !t->pixels)
        return re_rgba(255, 255, 255, 255);
    /* floorf, no conversión truncada: u=-0.2 debe envolver a 0.8. */
    int x = (int)((u - floorf(u)) * (float)t->width);
    int y = (int)((v - floorf(v)) * (float)t->height);
    x = x < t->width ? x : t->width - 1;
    y = y < t->height ? y : t->height - 1;
    return t->pixels[(size_t)y * (size_t)t->width + (size_t)x];
}

static void rasterize(ReRenderer *r, ScreenVertex a, ScreenVertex b, ScreenVertex c,
                      const ReTexture *texture, float light) {
    float area = edge(a, b, c.x, c.y);
    if (fabsf(area) < 0.00001f)
        return;
    if (area < 0) {
        ScreenVertex swap = b;
        b = c;
        c = swap;
        area = -area;
    }
    r->stats.rasterized++;
    int x0 = (int)fmaxf(0, floorf(fminf(a.x, fminf(b.x, c.x))));
    int y0 = (int)fmaxf(0, floorf(fminf(a.y, fminf(b.y, c.y))));
    int x1 = (int)fminf((float)(r->width - 1), ceilf(fmaxf(a.x, fmaxf(b.x, c.x))));
    int y1 = (int)fminf((float)(r->height - 1), ceilf(fmaxf(a.y, fmaxf(b.y, c.y))));
    bool tl0 = top_left(b, c), tl1 = top_left(c, a), tl2 = top_left(a, b);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float e0 = edge(b, c, px, py), e1 = edge(c, a, px, py), e2 = edge(a, b, px, py);
            if (e0 < 0 || (e0 == 0 && !tl0) || e1 < 0 || (e1 == 0 && !tl1) || e2 < 0 ||
                (e2 == 0 && !tl2))
                continue;
            float wa = e0 / area, wb = e1 / area, wc = e2 / area;
            float inv_z = wa * a.inv_z + wb * b.inv_z + wc * c.inv_z;
            float z = 1 / inv_z;
            size_t offset = (size_t)y * (size_t)r->width + (size_t)x;
            if (z >= r->depth[offset])
                continue;
            /* UV no es lineal en pantalla. u/z, v/z y 1/z sí lo son. */
            float u = (wa * a.u_over_z + wb * b.u_over_z + wc * c.u_over_z) * z;
            float v = (wa * a.v_over_z + wb * b.v_over_z + wc * c.v_over_z) * z;
            RePixel color = sample(texture, u, v);
            if (color.a < 128)
                continue;
            float shade = re_clamp(light / (1 + z * 0.045f), 0.08f, 1);
            if (r->wireframe) {
                float margin = fminf(wa, fminf(wb, wc));
                color = margin < 0.025f ? re_rgba(100, 245, 205, 255) : re_rgba(13, 26, 34, 255);
                shade = 1;
            }
            color.r = (uint8_t)((float)color.r * shade);
            color.g = (uint8_t)((float)color.g * shade);
            color.b = (uint8_t)((float)color.b * shade);
            color.a = 255;
            r->pixels[offset] = color;
            r->depth[offset] = z;
            r->stats.shaded++;
        }
}

void re_draw_triangle(ReRenderer *r, const ReCamera *c, const ReVertex vertices[3],
                      const ReTexture *texture, float light) {
    if (c->near_plane <= 0 || c->far_plane <= c->near_plane || c->fov <= 0 || c->fov >= RE_PI)
        return;
    r->stats.submitted++;
    ClipVertex buffers[2][16];
    for (int i = 0; i < 3; i++)
        buffers[0][i] = camera_vertex(vertices[i], c);
    size_t count = 3;
    int source = 0;
    float tx = tanf(c->fov * 0.5f), ty = tx * (float)r->height / (float)r->width;
    /* Sutherland-Hodgman recorta contra cada semiespacio. Un triángulo contra
     * seis planos produce a lo sumo nueve vértices; los 16 son capacidad fija. */
    for (int plane = 0; plane < 6 && count > 0; plane++) {
        size_t output = 0;
        ClipVertex *in = buffers[source], *out = buffers[1 - source];
        ClipVertex previous = in[count - 1];
        float pd = plane_distance(previous, plane, c, tx, ty);
        for (size_t i = 0; i < count; i++) {
            ClipVertex next = in[i];
            float nd = plane_distance(next, plane, c, tx, ty);
            if ((pd >= 0) != (nd >= 0))
                out[output++] = clip_lerp(previous, next, pd / (pd - nd));
            if (nd >= 0)
                out[output++] = next;
            previous = next;
            pd = nd;
        }
        source = 1 - source;
        count = output;
    }
    ScreenVertex projected[16];
    float focal = (float)r->width / (2 * tx);
    for (size_t i = 0; i < count; i++) {
        ClipVertex v = buffers[source][i];
        float inv = 1 / v.z;
        projected[i] =
            (ScreenVertex){(float)r->width * 0.5f + v.x * focal * inv,
                           (float)r->height * 0.5f - v.y * focal * inv, inv, v.u * inv, v.v * inv};
    }
    for (size_t i = 1; i + 1 < count; i++)
        rasterize(r, projected[0], projected[i], projected[i + 1], texture, light);
}

static void quad(ReRenderer *r, const ReCamera *c, ReVec2 a, ReVec2 b, float low, float high,
                 const ReTexture *texture, float light) {
    if (high - low < RE_EPSILON)
        return;
    float width = re_length2(re_sub2(b, a));
    ReVertex vertices[4] = {{{a.x, a.y, low}, 0, -low},
                            {{b.x, b.y, low}, width, -low},
                            {{b.x, b.y, high}, width, -high},
                            {{a.x, a.y, high}, 0, -high}};
    ReVertex first[3] = {vertices[0], vertices[1], vertices[2]};
    ReVertex second[3] = {vertices[0], vertices[2], vertices[3]};
    re_draw_triangle(r, c, first, texture, light);
    re_draw_triangle(r, c, second, texture, light);
}

static ReVec2 edge_point(ReVec2 a, ReVec2 b, float t) {
    return re_add2(a, re_scale2(re_sub2(b, a), t));
}

void re_draw_world(ReRenderer *r, const ReCamera *c, const ReWorld *w,
                   const ReTexture materials[RE_MAX_MATERIALS]) {
    for (size_t si = 0; si < w->sector_count; si++) {
        const ReSector *s = &w->sectors[si];
        /* Convexidad permite triangulación en abanico, O(n), sin ear clipping. */
        for (int plane = 0; plane < 2; plane++) {
            float z = plane ? s->ceiling : s->floor;
            int material = plane ? s->ceiling_material : s->floor_material;
            for (size_t i = 1; i + 1 < s->count; i++) {
                ReVec2 points[3] = {s->vertices[0], s->vertices[i], s->vertices[i + 1]};
                ReVertex triangle[3];
                for (int j = 0; j < 3; j++)
                    triangle[j] =
                        (ReVertex){{points[j].x, points[j].y, z}, points[j].x, points[j].y};
                re_draw_triangle(r, c, triangle, &materials[material],
                                 s->light * (plane ? 0.78f : 0.92f));
            }
        }
        for (size_t e = 0; e < s->count; e++) {
            ReVec2 a = s->vertices[e], b = s->vertices[(e + 1) % s->count];
            /* Sólo la cara que mira al interior de su sector: evita que una
             * pared vista desde otra sala oculte la conexión por su reverso. */
            if (re_cross2(re_sub2(b, a), re_sub2(re_v2(c->position.x, c->position.y), a)) < 0)
                continue;
            int n = s->neighbor[e];
            if (n < 0)
                quad(r, c, a, b, s->floor, s->ceiling, &materials[s->wall_material], s->light);
            else {
                /* Una conexion ya no elimina la arista completa. Primero se
                 * dibujan las jambas laterales y despues los paneles inferior
                 * y superior dentro del intervalo de la abertura. Asi una
                 * puerta de un metro no convierte un muro de seis metros en
                 * un tunel abierto. */
                ReVec2 opening_a = edge_point(a, b, s->portal_start[e]);
                ReVec2 opening_b = edge_point(a, b, s->portal_end[e]);
                quad(r, c, a, opening_a, s->floor, s->ceiling, &materials[s->wall_material],
                     s->light);
                quad(r, c, opening_b, b, s->floor, s->ceiling, &materials[s->wall_material],
                     s->light);
                float low = fmaxf(s->floor, w->sectors[n].floor);
                float high = fminf(s->ceiling, w->sectors[n].ceiling);
                if (high <= low)
                    quad(r, c, a, b, s->floor, s->ceiling, &materials[s->wall_material], s->light);
                else {
                    quad(r, c, opening_a, opening_b, s->floor, low, &materials[s->wall_material],
                         s->light);
                    quad(r, c, opening_a, opening_b, high, s->ceiling, &materials[s->wall_material],
                         s->light);
                }
            }
        }
    }
    /* Barreras dinámicas se dibujan una sola vez, después de la arquitectura.
     * La puerta sube verticalmente; un vidrio desaparece al romperse. El mismo
     * open_fraction decide imagen, colisión y trazado de rayos. */
    for (size_t i = 0; i < w->barrier_count; i++) {
        const ReBarrier *barrier = &w->barriers[i];
        if (barrier->open_fraction >= 0.999f || barrier->sector < 0 ||
            barrier->sector >= (int)w->sector_count)
            continue;
        const ReSector *s = &w->sectors[barrier->sector];
        if (barrier->edge < 0 || barrier->edge >= (int)s->count)
            continue;
        int neighbor = s->neighbor[barrier->edge];
        if (neighbor < 0)
            continue;
        float low = fmaxf(s->floor, w->sectors[neighbor].floor);
        float high = fminf(s->ceiling, w->sectors[neighbor].ceiling);
        if (barrier->kind == RE_BARRIER_DOOR)
            low += (high - low) * barrier->open_fraction;
        ReVec2 edge_a = s->vertices[barrier->edge];
        ReVec2 edge_b = s->vertices[((size_t)barrier->edge + 1u) % s->count];
        ReVec2 a = edge_point(edge_a, edge_b, s->portal_start[barrier->edge]);
        ReVec2 b = edge_point(edge_a, edge_b, s->portal_end[barrier->edge]);
        /* Puertas y vidrios son affordances de juego. Un mínimo de luz evita
         * que un panel cerrado en un sector oscuro parezca un portal vacío y,
         * por tanto, una pared invisible. */
        float barrier_light = fmaxf(s->light, barrier->kind == RE_BARRIER_DOOR ? .82f : .68f);
        quad(r, c, a, b, low, high, &materials[barrier->material], barrier_light);
    }
}

void re_apply_lights(ReRenderer *r, const ReCamera *c, const struct ReLight *lights,
                     const bool *enabled, size_t light_count, float time) {
    enum { TILE_SIZE = 16, MAX_TILES = 64 * 64, LIGHTS_PER_TILE = 8 };
    if (!r || !c || !lights || !enabled || r->width <= 0 || r->height <= 0 || r->width > 1024 ||
        r->height > 1024)
        return;
    int tiles_x = (r->width + TILE_SIZE - 1) / TILE_SIZE;
    int tiles_y = (r->height + TILE_SIZE - 1) / TILE_SIZE;
    uint8_t counts[MAX_TILES] = {0};
    uint8_t indices[MAX_TILES][LIGHTS_PER_TILE] = {{0}};
    /* Estos términos sólo dependen de la luz y del tiempo del frame. Antes se
     * recalculaban dentro del bucle de píxeles (incluidos varios sin/cos), lo
     * que multiplicaba trabajo idéntico por hasta 129 600 píxeles. Mantenerlos
     * indexados por la luz original conserva las listas compactas de tiles. */
    float radius_squared[RE_MAX_LIGHTS] = {0};
    float inverse_radius[RE_MAX_LIGHTS] = {0};
    float frame_intensity[RE_MAX_LIGHTS] = {0};
    float cone_cosine[RE_MAX_LIGHTS] = {0};
    ReVec2 spot_direction[RE_MAX_LIGHTS] = {0};
    float tangent = tanf(c->fov * .5f);
    float focal = (float)r->width / (2 * tangent);
    float sy = sinf(c->yaw), cy = cosf(c->yaw), sp = sinf(c->pitch), cp = cosf(c->pitch);
    for (size_t i = 0; i < light_count && i < RE_MAX_LIGHTS; i++) {
        if (!enabled[i] || lights[i].radius <= 0 || lights[i].intensity <= 0)
            continue;
        radius_squared[i] = lights[i].radius * lights[i].radius;
        inverse_radius[i] = 1 / lights[i].radius;
        float flicker =
            lights[i].flicker > 0
                ? 1 - lights[i].flicker * (.5f + .5f * sinf(time * 17 + (float)i * 2.7f))
                : 1;
        frame_intensity[i] = lights[i].intensity * flicker;
        if (lights[i].kind == RE_LIGHT_SPOT) {
            spot_direction[i] = re_v2(sinf(lights[i].yaw), cosf(lights[i].yaw));
            cone_cosine[i] = cosf(lights[i].cone * .5f);
        }
        ReVec3 delta = re_sub3(lights[i].position, c->position);
        float forward = delta.x * sy + delta.y * cy;
        float camera_x = delta.x * cy - delta.y * sy;
        float camera_y = delta.z * cp - forward * sp;
        float camera_z = forward * cp + delta.z * sp;
        if (camera_z + lights[i].radius <= c->near_plane)
            continue;
        float safe_z = fmaxf(c->near_plane, camera_z);
        float center_x = (float)r->width * .5f + camera_x * focal / safe_z;
        float center_y = (float)r->height * .5f - camera_y * focal / safe_z;
        float radius = focal * lights[i].radius / fmaxf(c->near_plane, safe_z - lights[i].radius);
        int x0 = (int)floorf((center_x - radius) / (float)TILE_SIZE);
        int x1 = (int)floorf((center_x + radius) / (float)TILE_SIZE);
        int y0 = (int)floorf((center_y - radius) / (float)TILE_SIZE);
        int y1 = (int)floorf((center_y + radius) / (float)TILE_SIZE);
        x0 = x0 < 0 ? 0 : x0;
        y0 = y0 < 0 ? 0 : y0;
        x1 = x1 >= tiles_x ? tiles_x - 1 : x1;
        y1 = y1 >= tiles_y ? tiles_y - 1 : y1;
        for (int ty = y0; ty <= y1; ty++)
            for (int tx = x0; tx <= x1; tx++) {
                size_t tile = (size_t)ty * (size_t)tiles_x + (size_t)tx;
                if (counts[tile] < LIGHTS_PER_TILE)
                    indices[tile][counts[tile]++] = (uint8_t)i;
            }
    }
    ReVec3 right = re_v3(cy, -sy, 0);
    ReVec3 forward = re_v3(sy * cp, cy * cp, sp);
    ReVec3 up = re_v3(-sy * sp, -cy * sp, cp);
    for (int y = 0; y < r->height; y++)
        for (int x = 0; x < r->width; x++) {
            size_t pixel = (size_t)y * (size_t)r->width + (size_t)x;
            float depth = r->depth[pixel];
            if (!isfinite(depth))
                continue;
            size_t tile = (size_t)(y / TILE_SIZE) * (size_t)tiles_x + (size_t)(x / TILE_SIZE);
            if (!counts[tile])
                continue;
            float camera_x = ((float)x + .5f - (float)r->width * .5f) * depth / focal;
            float camera_y = -((float)y + .5f - (float)r->height * .5f) * depth / focal;
            ReVec3 world = re_add3(
                c->position, re_add3(re_scale3(right, camera_x),
                                     re_add3(re_scale3(up, camera_y), re_scale3(forward, depth))));
            ReVec3 illumination = {0};
            for (uint8_t entry = 0; entry < counts[tile]; entry++) {
                size_t index = indices[tile][entry];
                ReVec3 to_point = re_sub3(world, lights[index].position);
                float distance_squared = re_dot3(to_point, to_point);
                if (distance_squared >= radius_squared[index])
                    continue;
                float distance = sqrtf(distance_squared);
                if (lights[index].kind == RE_LIGHT_SPOT && distance > RE_EPSILON) {
                    float facing = re_dot2(spot_direction[index],
                                           re_scale2(re_v2(to_point.x, to_point.y), 1 / distance));
                    if (facing < cone_cosine[index])
                        continue;
                }
                float attenuation = 1 - distance * inverse_radius[index];
                float strength = attenuation * attenuation * frame_intensity[index];
                illumination = re_add3(illumination, re_scale3(lights[index].color, strength));
            }
            RePixel color = r->pixels[pixel];
            float red = (float)color.r * (1 + illumination.x);
            float green = (float)color.g * (1 + illumination.y);
            float blue = (float)color.b * (1 + illumination.z);
            color.r = (uint8_t)((int)fminf(255, red) / 16 * 16);
            color.g = (uint8_t)((int)fminf(255, green) / 16 * 16);
            color.b = (uint8_t)((int)fminf(255, blue) / 16 * 16);
            r->pixels[pixel] = color;
        }
}
void re_draw_billboard_region(ReRenderer *r, const ReCamera *c, ReVec3 feet, float width,
                              float height, const ReTexture *texture, float light, float u0,
                              float v0, float u1, float v1) {
    ReVec2 right = re_v2(cosf(c->yaw) * width * 0.5f, -sinf(c->yaw) * width * 0.5f);
    ReVertex v[4] = {{{feet.x - right.x, feet.y - right.y, feet.z}, u0, v1},
                     {{feet.x + right.x, feet.y + right.y, feet.z}, u1, v1},
                     {{feet.x + right.x, feet.y + right.y, feet.z + height}, u1, v0},
                     {{feet.x - right.x, feet.y - right.y, feet.z + height}, u0, v0}};
    /* 1-epsilon mantiene la última fila/columna dentro de una textura envuelta. */
    for (int i = 0; i < 4; i++) {
        v[i].u = fminf(v[i].u, u1 - .00001f);
        v[i].v = fminf(v[i].v, v1 - .00001f);
    }
    ReVertex a[3] = {v[0], v[1], v[2]}, b[3] = {v[0], v[2], v[3]};
    re_draw_triangle(r, c, a, texture, light);
    re_draw_triangle(r, c, b, texture, light);
}
void re_draw_billboard(ReRenderer *r, const ReCamera *c, ReVec3 feet, float width, float height,
                       const ReTexture *texture, float light) {
    re_draw_billboard_region(r, c, feet, width, height, texture, light, 0, 0, 1, 1);
}
void re_draw_depth(ReRenderer *r, float distance) {
    for (size_t i = 0; i < (size_t)r->width * (size_t)r->height; i++) {
        uint8_t value = (uint8_t)(255 * re_clamp(1 - r->depth[i] / distance, 0, 1));
        r->pixels[i] = re_rgba(value, value, value, 255);
    }
}
bool re_capture_ppm(const ReRenderer *r, const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file)
        return false;
    bool ok = fprintf(file, "P6\n%d %d\n255\n", r->width, r->height) > 0;
    for (size_t i = 0; ok && i < (size_t)r->width * (size_t)r->height; i++) {
        unsigned char rgb[3] = {r->pixels[i].r, r->pixels[i].g, r->pixels[i].b};
        ok = fwrite(rgb, 1, 3, file) == 3;
    }
    if (fclose(file) != 0)
        ok = false;
    return ok;
}
