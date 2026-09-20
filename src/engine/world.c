/* Colisión y consultas sobre el mismo mundo que se dibuja: una puerta modifica
 * el techo de su sector, afectando simultáneamente imagen, movimiento y rayos. */
#include "retro/world.h"

int re_world_sector(const ReWorld *w, ReVec2 p, int preferred) {
    if (preferred >= 0 && preferred < (int)w->sector_count &&
        re_sector_contains(&w->sectors[preferred], p))
        return preferred;
    for (size_t i = 0; i < w->sector_count; i++)
        if (re_sector_contains(&w->sectors[i], p))
            return (int)i;
    return -1;
}

int re_world_sector_at(const ReWorld *w, ReVec3 p, int preferred) {
    if (preferred >= 0 && preferred < (int)w->sector_count) {
        const ReSector *s = &w->sectors[preferred];
        if (p.z >= s->floor - RE_EPSILON && p.z < s->ceiling - RE_EPSILON &&
            re_sector_contains(s, re_v2(p.x, p.y)))
            return preferred;
    }
    for (size_t i = 0; i < w->sector_count; i++) {
        const ReSector *s = &w->sectors[i];
        if (p.z >= s->floor - RE_EPSILON && p.z < s->ceiling - RE_EPSILON &&
            re_sector_contains(s, re_v2(p.x, p.y)))
            return (int)i;
    }
    return -1;
}

int re_world_barrier_at(const ReWorld *w, int sector, int edge) {
    for (size_t i = 0; i < w->barrier_count; i++) {
        const ReBarrier *b = &w->barriers[i];
        if (b->sector == sector && b->edge == edge)
            return (int)i;
        if (b->sector >= 0 && b->sector < (int)w->sector_count &&
            w->sectors[b->sector].neighbor[b->edge] == sector &&
            w->sectors[b->sector].neighbor_edge[b->edge] == edge)
            return (int)i;
    }
    return -1;
}

static ReVec2 edge_point(ReVec2 a, ReVec2 b, float t) {
    return re_add2(a, re_scale2(re_sub2(b, a), t));
}

/* Devuelve la coordenada normalizada del punto sobre la arista. El clamp
 * absorbe el pequeno error numerico de una interseccion calculada por rayos. */
static float edge_parameter(ReVec2 a, ReVec2 b, ReVec2 point) {
    ReVec2 edge = re_sub2(b, a);
    float length_squared = re_dot2(edge, edge);
    if (length_squared <= RE_EPSILON)
        return 0;
    return re_clamp(re_dot2(re_sub2(point, a), edge) / length_squared, 0, 1);
}

static bool portal_clearance_blocks(const ReWorld *w, const ReSector *s, size_t edge,
                                    const ReBody *body) {
    int sector = (int)(s - w->sectors);
    int barrier = re_world_barrier_at(w, sector, (int)edge);
    if (barrier >= 0 && w->barriers[barrier].open_fraction < 0.95f &&
        (w->barriers[barrier].blocks & RE_BLOCK_MOVEMENT) != 0)
        return true;
    int adjacent = s->neighbor[edge];
    if (adjacent < 0)
        return true;
    const ReSector *other = &w->sectors[adjacent];
    float floor = fmaxf(s->floor, other->floor);
    float ceiling = fminf(s->ceiling, other->ceiling);
    float allowance = body->grounded ? body->step_height : 0;
    float feet = fmaxf(body->position.z, floor);
    return floor > body->position.z + allowance + RE_EPSILON ||
           ceiling < feet + body->height - RE_EPSILON;
}

void re_body_move(const ReWorld *w, ReBody *body, ReVec2 delta, float dt, float gravity) {
    ReVec2 p = re_v2(body->position.x, body->position.y);
    ReVec2 original = p;
    /* A lo sumo cuatro contactos por tick. Se avanza al primer impacto y se
     * elimina sólo la componente que entra en la pared (wall sliding).
     * Coste O(contactos * aristas); suficiente y medible para mapas pequeños. */
    for (int iteration = 0; iteration < 4 && re_length2(delta) > RE_EPSILON; iteration++) {
        float earliest = 1;
        ReVec2 normal = {0};
        bool hit = false;
        for (size_t si = 0; si < w->sector_count; si++) {
            const ReSector *s = &w->sectors[si];
            /* Dos plantas pueden ocupar el mismo polígono XY. Una pared sólo
             * puede tocar al cilindro si sus intervalos verticales se cruzan.
             * Antes se barrían todas las aristas del mapa; por eso las paredes
             * del ático, aunque empezaban tres metros más arriba, se convertían
             * en muros invisibles en la planta baja.
             *
             * La comparación es abierta: apoyar exactamente los pies sobre el
             * techo de un volumen inferior o la cabeza bajo otro superior no
             * constituye penetración. Los desniveles transitables continúan
             * resolviéndose en portal_clearance_blocks(). */
            float body_bottom = body->position.z;
            float body_top = body->position.z + body->height;
            if (s->ceiling <= body_bottom + RE_EPSILON || s->floor >= body_top - RE_EPSILON)
                continue;
            for (size_t e = 0; e < s->count; e++) {
                ReVec2 a = s->vertices[e], b = s->vertices[(e + 1) % s->count];
                ReVec2 segment_a[3] = {a, {0, 0}, {0, 0}};
                ReVec2 segment_b[3] = {b, {0, 0}, {0, 0}};
                size_t segment_count = 1;
                if (s->neighbor[e] >= 0 && !portal_clearance_blocks(w, s, e, body)) {
                    ReVec2 opening_a = edge_point(a, b, s->portal_start[e]);
                    ReVec2 opening_b = edge_point(a, b, s->portal_end[e]);
                    segment_count = 0;
                    if (s->portal_start[e] > RE_EPSILON) {
                        segment_a[segment_count] = a;
                        segment_b[segment_count++] = opening_a;
                    }
                    if (s->portal_end[e] < 1 - RE_EPSILON) {
                        segment_a[segment_count] = opening_b;
                        segment_b[segment_count++] = b;
                    }
                }
                for (size_t segment = 0; segment < segment_count; segment++) {
                    float t;
                    ReVec2 n;
                    if (re_sweep_circle(p, delta, body->radius, segment_a[segment],
                                        segment_b[segment], &t, &n) &&
                        t < earliest) {
                        earliest = t;
                        normal = n;
                        hit = true;
                    }
                }
            }
        }
        float travel = hit ? fmaxf(0, earliest - 0.0001f) : 1;
        p = re_add2(p, re_scale2(delta, travel));
        delta = re_scale2(delta, 1 - travel);
        if (hit) {
            float inward = re_dot2(delta, normal);
            if (inward < 0)
                delta = re_sub2(delta, re_scale2(normal, inward));
        } else
            break;
    }
    int sector = -1;
    /* El cuerpo puede subir un escalón antes de que Z haya sido ajustada. Por
     * eso la selección admite suelo hasta step_height sobre los pies actuales. */
    if (body->sector >= 0 && body->sector < (int)w->sector_count &&
        re_sector_contains(&w->sectors[body->sector], p))
        sector = body->sector;
    for (size_t i = 0; sector < 0 && i < w->sector_count; i++) {
        const ReSector *candidate = &w->sectors[i];
        if (re_sector_contains(candidate, p) &&
            candidate->floor <= body->position.z + body->step_height + RE_EPSILON &&
            candidate->ceiling >= body->position.z + body->height - RE_EPSILON)
            sector = (int)i;
    }
    if (sector < 0) {
        p = original;
        sector = body->sector;
    }
    if (sector < 0 || sector >= (int)w->sector_count)
        return;
    body->position.x = p.x;
    body->position.y = p.y;
    body->sector = sector;
    const ReSector *s = &w->sectors[sector];
    /* El círculo puede abarcar dos sectores antes de que su centro cruce el
     * portal. Un techo vecino limita el salto y evita atravesar su dintel. */
    float ceiling = s->ceiling;
    for (size_t e = 0; e < s->count; e++)
        if (s->neighbor[e] >= 0) {
            float distance = re_length2(
                re_sub2(p, re_closest_segment(p, s->vertices[e], s->vertices[(e + 1) % s->count])));
            ReVec2 closest = re_closest_segment(p, s->vertices[e], s->vertices[(e + 1) % s->count]);
            float along = edge_parameter(s->vertices[e], s->vertices[(e + 1) % s->count], closest);
            if (distance < body->radius && along >= s->portal_start[e] && along <= s->portal_end[e])
                ceiling = fminf(ceiling, w->sectors[s->neighbor[e]].ceiling);
        }
    if (body->grounded && s->floor > body->position.z)
        body->position.z = s->floor;
    body->vertical_speed -= gravity * dt;
    body->position.z += body->vertical_speed * dt;
    if (body->position.z + body->height > ceiling) {
        body->position.z = ceiling - body->height;
        body->vertical_speed = fminf(body->vertical_speed, 0);
    }
    if (body->position.z <= s->floor) {
        body->position.z = s->floor;
        body->vertical_speed = 0;
        body->grounded = true;
    } else
        body->grounded = false;
}

ReTraceHit re_world_trace(const ReWorld *w, ReVec3 origin, ReVec3 d, float maximum,
                          unsigned int mask) {
    ReTraceHit hit = {.distance = maximum, .sector = -1, .edge = -1, .barrier = -1};
    ReVec2 xy = re_v2(origin.x, origin.y), direction = re_v2(d.x, d.y);
    for (size_t si = 0; si < w->sector_count; si++) {
        const ReSector *s = &w->sectors[si];
        if (fabsf(d.z) > RE_EPSILON) {
            float planes[2] = {s->floor, s->ceiling};
            for (int i = 0; i < 2; i++) {
                float t = (planes[i] - origin.z) / d.z;
                if (t > RE_EPSILON && t < hit.distance &&
                    re_sector_contains(s, re_add2(xy, re_scale2(direction, t)))) {
                    hit.distance = t;
                    hit.sector = (int)si;
                    hit.edge = -1;
                    hit.barrier = -1;
                }
            }
        }
        for (size_t e = 0; e < s->count; e++) {
            float t;
            if (!re_ray_segment(xy, direction, s->vertices[e], s->vertices[(e + 1) % s->count],
                                &t) ||
                t < RE_EPSILON || t >= hit.distance)
                continue;
            float z = origin.z + d.z * t;
            if (z < s->floor - RE_EPSILON || z > s->ceiling + RE_EPSILON)
                continue;
            int n = s->neighbor[e];
            if (n >= 0) {
                float low = fmaxf(s->floor, w->sectors[n].floor);
                float high = fminf(s->ceiling, w->sectors[n].ceiling);
                ReVec2 impact = re_add2(xy, re_scale2(direction, t));
                float along =
                    edge_parameter(s->vertices[e], s->vertices[(e + 1) % s->count], impact);
                if (along >= s->portal_start[e] - RE_EPSILON &&
                    along <= s->portal_end[e] + RE_EPSILON && z > low + RE_EPSILON &&
                    z < high - RE_EPSILON) {
                    int barrier = re_world_barrier_at(w, (int)si, (int)e);
                    if (barrier < 0 || w->barriers[barrier].open_fraction >= 0.95f ||
                        (w->barriers[barrier].blocks & mask) == 0)
                        continue;
                    hit.barrier = barrier;
                }
            }
            hit.distance = t;
            hit.sector = (int)si;
            hit.edge = (int)e;
        }
    }
    return hit;
}

float re_world_raycast(const ReWorld *w, ReVec3 origin, ReVec3 d, float maximum) {
    return re_world_trace(w, origin, d, maximum,
                          RE_BLOCK_MOVEMENT | RE_BLOCK_SIGHT | RE_BLOCK_PROJECTILE)
        .distance;
}

int re_world_next_sector(const ReWorld *w, int start, int goal, float height, float step) {
    if (start < 0 || goal < 0 || start >= (int)w->sector_count || goal >= (int)w->sector_count)
        return -1;
    if (start == goal)
        return goal;
    int queue[RE_MAX_SECTORS], parent[RE_MAX_SECTORS];
    for (int i = 0; i < RE_MAX_SECTORS; i++)
        parent[i] = -1;
    size_t head = 0, tail = 0;
    queue[tail++] = start;
    parent[start] = start;
    /* Cada nodo entra una sola vez: O(V+E), memoria acotada por V. */
    while (head < tail) {
        int index = queue[head++];
        const ReSector *s = &w->sectors[index];
        for (size_t e = 0; e < s->count; e++) {
            int n = s->neighbor[e];
            if (n < 0 || parent[n] != -1)
                continue;
            int barrier = re_world_barrier_at(w, index, (int)e);
            if (barrier >= 0 && w->barriers[barrier].open_fraction < 0.95f &&
                (w->barriers[barrier].blocks & RE_BLOCK_MOVEMENT) != 0)
                continue;
            const ReSector *next = &w->sectors[n];
            float opening = fminf(s->ceiling, next->ceiling) - fmaxf(s->floor, next->floor);
            float width = re_length2(re_sub2(s->vertices[(e + 1) % s->count], s->vertices[e])) *
                          (s->portal_end[e] - s->portal_start[e]);
            if (opening < height || width < 0.1f || next->floor - s->floor > step)
                continue;
            parent[n] = index;
            if (n == goal) {
                int first = goal;
                while (parent[first] != start)
                    first = parent[first];
                return first;
            }
            queue[tail++] = n;
        }
    }
    return -1;
}
