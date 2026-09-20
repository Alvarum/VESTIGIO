/* Intersecciones geométricas puras. No asignan memoria ni leen estado global. */
#include "retro/math.h"

ReVec2 re_closest_segment(ReVec2 p, ReVec2 a, ReVec2 b) {
    ReVec2 ab = re_sub2(b, a);
    float length_squared = re_dot2(ab, ab);
    if (length_squared < RE_EPSILON * RE_EPSILON)
        return a;
    float t = re_clamp(re_dot2(re_sub2(p, a), ab) / length_squared, 0, 1);
    return re_add2(a, re_scale2(ab, t));
}

bool re_ray_segment(ReVec2 p, ReVec2 d, ReVec2 a, ReVec2 b, float *t) {
    ReVec2 edge = re_sub2(b, a);
    float denominator = re_cross2(d, edge);
    if (fabsf(denominator) < RE_EPSILON)
        return false;
    ReVec2 offset = re_sub2(a, p);
    float hit = re_cross2(offset, edge) / denominator;
    float u = re_cross2(offset, d) / denominator;
    if (hit < 0 || u < -RE_EPSILON || u > 1 + RE_EPSILON)
        return false;
    *t = hit;
    return true;
}

bool re_sweep_circle(ReVec2 p, ReVec2 delta, float radius, ReVec2 a, ReVec2 b, float *time,
                     ReVec2 *normal) {
    float best = 2;
    ReVec2 best_normal = {0};
    ReVec2 edge = re_sub2(b, a);
    float length = re_length2(edge);
    /* Una cápsula es un rectángulo más dos semicírculos. Primero resolvemos
     * las dos rectas paralelas; después las circunferencias de los extremos. */
    if (length > RE_EPSILON) {
        ReVec2 tangent = re_scale2(edge, 1 / length);
        ReVec2 n = re_v2(-tangent.y, tangent.x);
        float distance = re_dot2(re_sub2(p, a), n);
        float speed = re_dot2(delta, n);
        for (int side = -1; side <= 1; side += 2) {
            ReVec2 outward = re_scale2(n, (float)side);
            if (re_dot2(delta, outward) >= -RE_EPSILON)
                continue;
            float t = ((float)side * radius - distance) / speed;
            ReVec2 contact = re_add2(p, re_scale2(delta, t));
            float along = re_dot2(re_sub2(contact, a), tangent);
            if (t >= -RE_EPSILON && t <= 1 && along >= 0 && along <= length && t < best) {
                best = fmaxf(t, 0);
                best_normal = outward;
            }
        }
    }
    float aa = re_dot2(delta, delta);
    if (aa > RE_EPSILON * RE_EPSILON) {
        ReVec2 ends[2] = {a, b};
        for (int i = 0; i < 2; i++) {
            ReVec2 offset = re_sub2(p, ends[i]);
            float bb = re_dot2(offset, delta);
            float cc = re_dot2(offset, offset) - radius * radius;
            float discriminant = bb * bb - aa * cc;
            if (discriminant < 0)
                continue;
            float t = (-bb - sqrtf(discriminant)) / aa;
            if (t >= -RE_EPSILON && t <= 1 && t < best) {
                ReVec2 n = re_normalize2(re_add2(offset, re_scale2(delta, t)));
                if (re_dot2(delta, n) < 0) {
                    best = fmaxf(t, 0);
                    best_normal = n;
                }
            }
        }
    }
    if (best > 1)
        return false;
    *time = best;
    *normal = best_normal;
    return true;
}
