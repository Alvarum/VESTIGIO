/* Vectores propios: el núcleo no depende de los tipos de raylib.
 * Convención: XY = suelo, Z = altura; metros, segundos y radianes.
 * Lee docs/03-matematicas-renderer.md antes de modificar las convenciones. */
#ifndef RETRO_MATH_H
#define RETRO_MATH_H

#include <math.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "RetroForge requiere C23 publicado; selecciona GCC 15+ y -std=c23"
#endif

typedef struct ReVec2 {
    float x, y;
} ReVec2;
typedef struct ReVec3 {
    float x, y, z;
} ReVec3;

#define RE_PI 3.14159265358979323846f
#define RE_EPSILON 0.00001f

/* static inline permite compartir operaciones pequeñas sin símbolos globales
 * duplicados. Se pasan por valor: no hay memoria prestada ni heap. */
static inline ReVec2 re_v2(float x, float y) {
    return (ReVec2){x, y};
}
static inline ReVec3 re_v3(float x, float y, float z) {
    return (ReVec3){x, y, z};
}
static inline ReVec2 re_add2(ReVec2 a, ReVec2 b) {
    return re_v2(a.x + b.x, a.y + b.y);
}
static inline ReVec2 re_sub2(ReVec2 a, ReVec2 b) {
    return re_v2(a.x - b.x, a.y - b.y);
}
static inline ReVec2 re_scale2(ReVec2 a, float s) {
    return re_v2(a.x * s, a.y * s);
}
static inline float re_dot2(ReVec2 a, ReVec2 b) {
    return a.x * b.x + a.y * b.y;
}
static inline float re_cross2(ReVec2 a, ReVec2 b) {
    return a.x * b.y - a.y * b.x;
}
static inline float re_length2(ReVec2 a) {
    return sqrtf(re_dot2(a, a));
}
static inline float re_clamp(float x, float lo, float hi) {
    return fminf(hi, fmaxf(lo, x));
}
static inline float re_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}
static inline ReVec2 re_normalize2(ReVec2 a) {
    float length = re_length2(a);
    return length > RE_EPSILON ? re_scale2(a, 1.0f / length) : re_v2(0, 0);
}
static inline ReVec3 re_add3(ReVec3 a, ReVec3 b) {
    return re_v3(a.x + b.x, a.y + b.y, a.z + b.z);
}
static inline ReVec3 re_sub3(ReVec3 a, ReVec3 b) {
    return re_v3(a.x - b.x, a.y - b.y, a.z - b.z);
}
static inline ReVec3 re_scale3(ReVec3 a, float s) {
    return re_v3(a.x * s, a.y * s, a.z * s);
}
static inline float re_dot3(ReVec3 a, ReVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* Devuelve el punto del segmento más cercano. Segmentos degenerados devuelven a. */
ReVec2 re_closest_segment(ReVec2 p, ReVec2 a, ReVec2 b);
/* Rayo p+t*d contra segmento [a,b]. t>=0, u dentro de [0,1].
 * d no necesita estar normalizado. Los paralelos no producen impacto único. */
[[nodiscard]] bool re_ray_segment(ReVec2 p, ReVec2 d, ReVec2 a, ReVec2 b, float *t);
/* Círculo barrido p+t*delta contra una cápsula de radio radius alrededor del
 * segmento. Devuelve el primer t en [0,1] y la normal hacia el círculo. */
[[nodiscard]] bool re_sweep_circle(ReVec2 p, ReVec2 delta, float radius, ReVec2 a, ReVec2 b,
                                   float *time, ReVec2 *normal);
#endif
