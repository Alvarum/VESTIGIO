#include "content/room_recipe.h"
#include "assets/import/model_ir.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool room_fail(char *error, size_t capacity, const char *message) {
    if (error != NULL && capacity != 0u)
        (void)snprintf(error, capacity, "%s", message);
    return false;
}

static bool room_number(const VgJsonNode *node, float *out) {
    if (node == NULL || node->type != VG_JSON_NUMBER || !isfinite(node->as.number.value) ||
        fabs(node->as.number.value) > 10000.0)
        return false;
    *out = (float)node->as.number.value;
    return true;
}

static float room_cross(const float a[2], const float b[2], const float c[2]) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

static bool room_intersect(const float a[2], const float b[2], const float c[2], const float d[2]) {
    float ac = room_cross(a, b, c), ad = room_cross(a, b, d);
    float ca = room_cross(c, d, a), cb = room_cross(c, d, b);
    const float epsilon = 0.0001f;
    if (((ac > epsilon && ad < -epsilon) || (ac < -epsilon && ad > epsilon)) &&
        ((ca > epsilon && cb < -epsilon) || (ca < -epsilon && cb > epsilon)))
        return true;
    const float *points[4] = {c, d, a, b};
    const float *starts[4] = {a, a, c, c};
    const float *ends[4] = {b, b, d, d};
    float crosses[4] = {ac, ad, ca, cb};
    for (uint32_t i = 0u; i < 4u; ++i)
        if (fabsf(crosses[i]) <= epsilon &&
            points[i][0] >= fminf(starts[i][0], ends[i][0]) - epsilon &&
            points[i][0] <= fmaxf(starts[i][0], ends[i][0]) + epsilon &&
            points[i][1] >= fminf(starts[i][1], ends[i][1]) - epsilon &&
            points[i][1] <= fmaxf(starts[i][1], ends[i][1]) + epsilon)
            return true;
    return false;
}

bool vg_room_recipe_parse(const VgJsonNode *node, VgRoomRecipe *out, char *error,
                          size_t error_capacity) {
    if (node == NULL || node->type != VG_JSON_OBJECT || out == NULL)
        return room_fail(error, error_capacity, "La receta de sala debe ser un objeto");
    memset(out, 0, sizeof(*out));
    const VgJsonNode *version = vg_json_object_get(node, "version");
    const VgJsonNode *vertices = vg_json_object_get(node, "vertices");
    if (version == NULL || version->type != VG_JSON_NUMBER || version->as.number.value != 1.0 ||
        vertices == NULL || vertices->type != VG_JSON_ARRAY || vertices->as.array.count < 3u ||
        vertices->as.array.count > VG_ROOM_MAX_VERTICES ||
        !room_number(vg_json_object_get(node, "floor_z"), &out->floor_z) ||
        !room_number(vg_json_object_get(node, "wall_height"), &out->wall_height) ||
        !room_number(vg_json_object_get(node, "wall_thickness"), &out->wall_thickness) ||
        fabsf(out->floor_z) > 1000.0f || out->wall_height < 0.5f || out->wall_height > 20.0f ||
        out->wall_thickness < 0.05f ||
        out->wall_thickness > 1.0f)
        return room_fail(error, error_capacity,
                         "Receta requiere version 1, 3-24 vertices, piso y muros validos");
    out->vertex_count = (uint32_t)vertices->as.array.count;
    for (uint32_t i = 0u; i < out->vertex_count; ++i) {
        const VgJsonNode *point = vertices->as.array.items[i];
        if (point == NULL || point->type != VG_JSON_ARRAY || point->as.array.count != 2u ||
            !room_number(point->as.array.items[0], &out->vertices[i][0]) ||
            !room_number(point->as.array.items[1], &out->vertices[i][1]) ||
            fabsf(out->vertices[i][0]) > 1000.0f || fabsf(out->vertices[i][1]) > 1000.0f)
            return room_fail(error, error_capacity, "Vertice de planta invalido (XY en metros)");
    }
    double signed_area = 0.0;
    for (uint32_t i = 0u; i < out->vertex_count; ++i) {
        uint32_t next = (i + 1u) % out->vertex_count;
        float dx = out->vertices[next][0] - out->vertices[i][0];
        float dy = out->vertices[next][1] - out->vertices[i][1];
        if (hypotf(dx, dy) <= out->wall_thickness * 1.01f)
            return room_fail(error, error_capacity,
                             "Arista demasiado corta para el grosor del muro");
        uint32_t after = (next + 1u) % out->vertex_count;
        if (fabsf(room_cross(out->vertices[i], out->vertices[next], out->vertices[after])) <=
            0.0001f)
            return room_fail(error, error_capacity,
                             "Vertices consecutivos colineales no soportados");
        signed_area += (double)out->vertices[i][0] * out->vertices[next][1] -
                       (double)out->vertices[next][0] * out->vertices[i][1];
        for (uint32_t j = i + 1u; j < out->vertex_count; ++j) {
            uint32_t jnext = (j + 1u) % out->vertex_count;
            if (j == next || jnext == i)
                continue;
            if (room_intersect(out->vertices[i], out->vertices[next], out->vertices[j],
                               out->vertices[jnext]))
                return room_fail(error, error_capacity, "La planta no puede autointersectarse");
        }
    }
    if (signed_area <= 0.01)
        return room_fail(error, error_capacity,
                         "Vertices deben formar planta CCW de area positiva");
    const VgJsonNode *openings = vg_json_object_get(node, "openings");
    if (openings != NULL &&
        (openings->type != VG_JSON_ARRAY || openings->as.array.count > VG_ROOM_MAX_OPENINGS))
        return room_fail(error, error_capacity, "Se admiten como maximo 48 aberturas");
    out->opening_count = openings != NULL ? (uint32_t)openings->as.array.count : 0u;
    for (uint32_t i = 0u; i < out->opening_count; ++i) {
        const VgJsonNode *item = openings->as.array.items[i];
        const VgJsonNode *edge = vg_json_object_get(item, "edge");
        const VgJsonNode *kind = vg_json_object_get(item, "kind");
        VgRoomOpening *opening = &out->openings[i];
        if (item == NULL || item->type != VG_JSON_OBJECT || edge == NULL ||
            edge->type != VG_JSON_NUMBER || edge->as.number.value < 0.0 ||
            edge->as.number.value >= out->vertex_count ||
            edge->as.number.value != floor(edge->as.number.value) || kind == NULL ||
            kind->type != VG_JSON_STRING ||
            !room_number(vg_json_object_get(item, "offset"), &opening->offset) ||
            !room_number(vg_json_object_get(item, "width"), &opening->width) ||
            !room_number(vg_json_object_get(item, "height"), &opening->height) ||
            !room_number(vg_json_object_get(item, "sill"), &opening->sill))
            return room_fail(error, error_capacity,
                             "Abertura requiere edge, kind, offset, width, height y sill");
        opening->edge = (uint32_t)edge->as.number.value;
        if (strcmp(kind->as.string.data, "door") == 0)
            opening->kind = VG_ROOM_DOOR;
        else if (strcmp(kind->as.string.data, "window") == 0)
            opening->kind = VG_ROOM_WINDOW;
        else if (strcmp(kind->as.string.data, "gap") == 0)
            opening->kind = VG_ROOM_GAP;
        else
            return room_fail(error, error_capacity, "kind debe ser door, window o gap");
        uint32_t next = (opening->edge + 1u) % out->vertex_count;
        float length = hypotf(out->vertices[next][0] - out->vertices[opening->edge][0],
                              out->vertices[next][1] - out->vertices[opening->edge][1]);
        if (opening->width < 0.2f || opening->height < 0.2f ||
            opening->offset < out->wall_thickness * 0.5f ||
            opening->offset + opening->width > length - out->wall_thickness * 0.5f ||
            opening->sill < 0.0f || opening->sill + opening->height > out->wall_height + 0.0001f ||
            (opening->kind != VG_ROOM_WINDOW && opening->sill != 0.0f) ||
            (opening->kind == VG_ROOM_WINDOW && opening->sill < 0.2f))
            return room_fail(error, error_capacity,
                             "Abertura fuera de muro o dimensiones invalidas");
        for (uint32_t j = 0u; j < i; ++j) {
            const VgRoomOpening *other = &out->openings[j];
            if (other->edge == opening->edge &&
                opening->offset < other->offset + other->width + out->wall_thickness * 0.5f &&
                other->offset < opening->offset + opening->width + out->wall_thickness * 0.5f)
                return room_fail(error, error_capacity, "Aberturas de un muro no pueden solaparse");
        }
    }
    if (error != NULL && error_capacity != 0u)
        error[0] = '\0';
    return true;
}

static bool room_triangle(VgRoomMesh *mesh, const float a[3], const float b[3], const float c[3]) {
    if (mesh->vertex_count / 3u >= VG_ROOM_MAX_TRIANGLES)
        return false;
    const float *points[3] = {a, b, c};
    for (uint32_t i = 0u; i < 3u; ++i) {
        VgRoomVertex *v = &mesh->vertices[mesh->vertex_count++];
        memcpy(v->position, points[i], sizeof(v->position));
        v->uv[0] = points[i][0];
        v->uv[1] = points[i][2] + points[i][1] * 0.25f;
    }
    return true;
}

static bool room_quad(VgRoomMesh *mesh, const float a[3], const float b[3], const float c[3],
                      const float d[3]) {
    return room_triangle(mesh, a, b, c) && room_triangle(mesh, a, c, d);
}

static bool room_box(VgRoomMesh *mesh, const float a[2], const float b[2], float thickness,
                     float bottom, float top) {
    if (top - bottom < 0.001f || hypotf(b[0] - a[0], b[1] - a[1]) < 0.001f)
        return true;
    float dx = b[0] - a[0], dy = b[1] - a[1];
    float length = hypotf(dx, dy), nx = -dy / length * thickness * 0.5f;
    float ny = dx / length * thickness * 0.5f;
    float p[8][3] = {{a[0] - nx, a[1] - ny, bottom}, {b[0] - nx, b[1] - ny, bottom},
                     {b[0] + nx, b[1] + ny, bottom}, {a[0] + nx, a[1] + ny, bottom},
                     {a[0] - nx, a[1] - ny, top},    {b[0] - nx, b[1] - ny, top},
                     {b[0] + nx, b[1] + ny, top},    {a[0] + nx, a[1] + ny, top}};
    return room_quad(mesh, p[3], p[2], p[1], p[0]) && room_quad(mesh, p[4], p[5], p[6], p[7]) &&
           room_quad(mesh, p[0], p[1], p[5], p[4]) && room_quad(mesh, p[2], p[3], p[7], p[6]) &&
           room_quad(mesh, p[1], p[2], p[6], p[5]) && room_quad(mesh, p[3], p[0], p[4], p[7]);
}

static bool room_floor(VgRoomMesh *mesh, const VgRoomRecipe *recipe) {
    uint32_t remaining[VG_ROOM_MAX_VERTICES];
    uint32_t count = recipe->vertex_count;
    for (uint32_t i = 0u; i < count; ++i)
        remaining[i] = i;
    float z = recipe->floor_z;
    while (count > 2u) {
        bool clipped = false;
        for (uint32_t i = 0u; i < count; ++i) {
            uint32_t ai = remaining[(i + count - 1u) % count];
            uint32_t bi = remaining[i];
            uint32_t ci = remaining[(i + 1u) % count];
            const float *a = recipe->vertices[ai], *b = recipe->vertices[bi],
                        *c = recipe->vertices[ci];
            if (room_cross(a, b, c) <= 0.0001f)
                continue;
            bool contains = false;
            for (uint32_t j = 0u; j < count; ++j) {
                uint32_t pi = remaining[j];
                if (pi == ai || pi == bi || pi == ci)
                    continue;
                const float *p = recipe->vertices[pi];
                if (room_cross(a, b, p) >= -0.0001f && room_cross(b, c, p) >= -0.0001f &&
                    room_cross(c, a, p) >= -0.0001f) {
                    contains = true;
                    break;
                }
            }
            if (contains)
                continue;
            float top_a[3] = {a[0], a[1], z}, top_b[3] = {b[0], b[1], z};
            float top_c[3] = {c[0], c[1], z};
            float bot_a[3] = {a[0], a[1], z - 0.12f};
            float bot_b[3] = {b[0], b[1], z - 0.12f};
            float bot_c[3] = {c[0], c[1], z - 0.12f};
            if (!room_triangle(mesh, top_a, top_b, top_c) ||
                !room_triangle(mesh, bot_c, bot_b, bot_a))
                return false;
            memmove(&remaining[i], &remaining[i + 1u], (count - i - 1u) * sizeof(remaining[0]));
            --count;
            clipped = true;
            break;
        }
        if (!clipped)
            return false;
    }
    for (uint32_t i = 0u; i < recipe->vertex_count; ++i) {
        const float *a = recipe->vertices[i];
        const float *b = recipe->vertices[(i + 1u) % recipe->vertex_count];
        float p0[3] = {a[0], a[1], z - 0.12f};
        float p1[3] = {b[0], b[1], z - 0.12f};
        float p2[3] = {b[0], b[1], z};
        float p3[3] = {a[0], a[1], z};
        if (!room_quad(mesh, p1, p0, p3, p2))
            return false;
    }
    return true;
}

bool vg_room_recipe_build(const VgRoomRecipe *recipe, VgRoomMesh *out, char *error,
                          size_t error_capacity) {
    if (recipe == NULL || out == NULL || recipe->vertex_count < 3u)
        return room_fail(error, error_capacity, "Receta no validada");
    *out = (VgRoomMesh){0};
    out->vertices = calloc(VG_ROOM_MAX_TRIANGLES * 3u, sizeof(*out->vertices));
    if (out->vertices == NULL)
        return room_fail(error, error_capacity, "Sin memoria para la malla de sala");
    bool ok = room_floor(out, recipe);
    for (uint32_t i = 0u; ok && i < recipe->vertex_count; ++i) {
        const float *a = recipe->vertices[i];
        const float *b = recipe->vertices[(i + 1u) % recipe->vertex_count];
        float dx = b[0] - a[0], dy = b[1] - a[1], length = hypotf(dx, dy);
        float breaks[VG_ROOM_MAX_OPENINGS * 2u + 2u] = {0.0f};
        uint32_t break_count = 1u;
        for (uint32_t j = 0u; j < recipe->opening_count; ++j) {
            const VgRoomOpening *opening = &recipe->openings[j];
            if (opening->edge == i) {
                breaks[break_count++] = opening->offset;
                breaks[break_count++] = opening->offset + opening->width;
            }
        }
        breaks[break_count++] = length;
        for (uint32_t j = 1u; j < break_count; ++j) {
            float value = breaks[j];
            uint32_t k = j;
            while (k > 0u && breaks[k - 1u] > value) {
                breaks[k] = breaks[k - 1u];
                --k;
            }
            breaks[k] = value;
        }
        for (uint32_t j = 0u; ok && j + 1u < break_count; ++j) {
            float start = breaks[j], end = breaks[j + 1u];
            if (end - start < 0.001f)
                continue;
            float p[2] = {a[0] + dx * start / length, a[1] + dy * start / length};
            float q[2] = {a[0] + dx * end / length, a[1] + dy * end / length};
            const VgRoomOpening *active = NULL;
            float midpoint = (start + end) * 0.5f;
            for (uint32_t k = 0u; k < recipe->opening_count; ++k) {
                const VgRoomOpening *opening = &recipe->openings[k];
                if (opening->edge == i && midpoint > opening->offset &&
                    midpoint < opening->offset + opening->width) {
                    active = opening;
                    break;
                }
            }
            if (active == NULL) {
                ok = room_box(out, p, q, recipe->wall_thickness, recipe->floor_z,
                              recipe->floor_z + recipe->wall_height);
            } else {
                if (active->sill > 0.001f)
                    ok = room_box(out, p, q, recipe->wall_thickness, recipe->floor_z,
                                  recipe->floor_z + active->sill);
                if (ok && active->sill + active->height < recipe->wall_height - 0.001f)
                    ok = room_box(out, p, q, recipe->wall_thickness,
                                  recipe->floor_z + active->sill + active->height,
                                  recipe->floor_z + recipe->wall_height);
            }
        }
    }
    if (!ok) {
        vg_room_mesh_destroy(out);
        return room_fail(error, error_capacity,
                         "No se pudo triangular sala o se excedio el limite");
    }
    if (error != NULL && error_capacity != 0u)
        error[0] = '\0';
    return true;
}

void vg_room_mesh_destroy(VgRoomMesh *mesh) {
    if (mesh != NULL) {
        free(mesh->vertices);
        *mesh = (VgRoomMesh){0};
    }
}

static size_t room_align(size_t value, size_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

bool vg_room_mesh_model_ir(const VgRoomMesh *mesh, void **out_data, size_t *out_bytes) {
    if (mesh == NULL || mesh->vertices == NULL || mesh->vertex_count < 3u ||
        mesh->vertex_count > VG_ROOM_MAX_TRIANGLES * 3u || out_data == NULL || out_bytes == NULL)
        return false;
    size_t cursor = sizeof(VgStaticModelIr);
#define ROOM_LAYOUT(field, type, count)                                                            \
    do {                                                                                           \
        cursor = room_align(cursor, _Alignof(type));                                               \
        offsets.field##_offset = (uint32_t)cursor;                                                 \
        cursor += sizeof(type) * (count);                                                          \
    } while (0)
    VgStaticModelIr offsets = {0};
    ROOM_LAYOUT(nodes, VgModelNodeIr, 1u);
    ROOM_LAYOUT(meshes, VgModelMeshIr, 1u);
    ROOM_LAYOUT(primitives, VgModelPrimitiveIr, 1u);
    ROOM_LAYOUT(vertices, VgModelVertexIr, mesh->vertex_count);
    ROOM_LAYOUT(indices, uint32_t, mesh->vertex_count);
    ROOM_LAYOUT(materials, VgModelMaterialIr, 1u);
    ROOM_LAYOUT(strings, char, 1u);
#undef ROOM_LAYOUT
    VgStaticModelIr *model = calloc(1u, cursor);
    if (model == NULL)
        return false;
    *model = offsets;
    model->magic = VG_MODEL_IR_MAGIC;
    model->version = VG_MODEL_IR_VERSION;
    model->importer_version = VG_ROOM_MODEL_IMPORTER_VERSION;
    model->total_bytes = cursor;
    model->node_count = model->mesh_count = model->primitive_count = model->material_count = 1u;
    model->vertex_count = model->index_count = mesh->vertex_count;
    model->strings_size = 1u;
    VgModelNodeIr *node = VG_MODEL_IR_ARRAY(model, VgModelNodeIr, nodes);
    node->parent = VG_MODEL_NO_INDEX;
    node->mesh = 0u;
    node->scene_root = 1u;
    node->determinant_sign = 1;
    node->local_transform[0] = node->local_transform[5] = node->local_transform[10] =
        node->local_transform[15] = 1.0f;
    VgModelMeshIr *built_mesh = VG_MODEL_IR_ARRAY(model, VgModelMeshIr, meshes);
    built_mesh->primitive_count = 1u;
    VgModelPrimitiveIr *primitive = VG_MODEL_IR_ARRAY(model, VgModelPrimitiveIr, primitives);
    primitive->vertex_count = primitive->index_count = mesh->vertex_count;
    primitive->material = 0u;
    for (uint32_t axis = 0u; axis < 3u; ++axis)
        primitive->bounds_min[axis] = model->geometry_bounds_min[axis] = FLT_MAX;
    for (uint32_t axis = 0u; axis < 3u; ++axis)
        primitive->bounds_max[axis] = model->geometry_bounds_max[axis] = -FLT_MAX;
    VgModelVertexIr *vertices = VG_MODEL_IR_ARRAY(model, VgModelVertexIr, vertices);
    uint32_t *indices = VG_MODEL_IR_ARRAY(model, uint32_t, indices);
    for (uint32_t i = 0u; i < mesh->vertex_count; i += 3u) {
        const float *a = mesh->vertices[i].position;
        const float *b = mesh->vertices[i + 1u].position;
        const float *c = mesh->vertices[i + 2u].position;
        float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        float ac[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        float normal[3] = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
                           ab[0] * ac[1] - ab[1] * ac[0]};
        float length = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        if (length <= 1.0e-7f) {
            free(model);
            return false;
        }
        for (uint32_t axis = 0u; axis < 3u; ++axis)
            normal[axis] /= length;
        for (uint32_t k = 0u; k < 3u; ++k) {
            uint32_t index = i + k;
            VgModelVertexIr *vertex = &vertices[index];
            memcpy(vertex->position, mesh->vertices[index].position, sizeof(vertex->position));
            memcpy(vertex->normal, normal, sizeof(normal));
            memcpy(vertex->texcoord, mesh->vertices[index].uv, sizeof(vertex->texcoord));
            for (uint32_t axis = 0u; axis < 4u; ++axis)
                vertex->color[axis] = 1.0f;
            for (uint32_t axis = 0u; axis < 3u; ++axis) {
                if (vertex->position[axis] < primitive->bounds_min[axis])
                    primitive->bounds_min[axis] = vertex->position[axis];
                if (vertex->position[axis] > primitive->bounds_max[axis])
                    primitive->bounds_max[axis] = vertex->position[axis];
            }
            indices[index] = index;
        }
    }
    memcpy(model->geometry_bounds_min, primitive->bounds_min, sizeof(primitive->bounds_min));
    memcpy(model->geometry_bounds_max, primitive->bounds_max, sizeof(primitive->bounds_max));
    VgModelMaterialIr *material = VG_MODEL_IR_ARRAY(model, VgModelMaterialIr, materials);
    material->base_color_texture = VG_MODEL_NO_INDEX;
    material->double_sided = 1u;
    material->base_color[0] = 0.32f;
    material->base_color[1] = 0.48f;
    material->base_color[2] = 0.56f;
    material->base_color[3] = 1.0f;
    material->alpha_cutoff = 0.5f;
    *out_data = model;
    *out_bytes = cursor;
    return true;
}
