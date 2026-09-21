#include "world/transform_internal.h"

#include <math.h>
#include <string.h>

#define VG_TRANSFORM_EPSILON 1.0e-5f
#define VG_NO_PARENT UINT16_MAX
#define VG_NO_OVERRIDE UINT32_MAX

static float vg_vec_length(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}

bool vg_transform_sanitize(const VgTransform *input, VgTransform *output) {
    if (input == NULL || output == NULL)
        return false;
    const float values[] = {input->position.x, input->position.y, input->position.z,
                            input->rotation.x, input->rotation.y, input->rotation.z,
                            input->rotation.w, input->scale.x,    input->scale.y,
                            input->scale.z};
    for (size_t index = 0u; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!isfinite(values[index]))
            return false;
    }
    if (input->scale.x <= VG_TRANSFORM_EPSILON || input->scale.y <= VG_TRANSFORM_EPSILON ||
        input->scale.z <= VG_TRANSFORM_EPSILON)
        return false;
    float length = vg_vec_length(input->rotation.x, input->rotation.y, input->rotation.z);
    length = sqrtf(length * length + input->rotation.w * input->rotation.w);
    if (!isfinite(length) || length <= VG_TRANSFORM_EPSILON)
        return false;
    *output = *input;
    output->rotation.x /= length;
    output->rotation.y /= length;
    output->rotation.z /= length;
    output->rotation.w /= length;
    return true;
}

VgMatrix vg_transform_matrix(VgTransform transform) {
    float x = transform.rotation.x;
    float y = transform.rotation.y;
    float z = transform.rotation.z;
    float w = transform.rotation.w;
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;
    VgMatrix result = {
        {(1.0f - 2.0f * (yy + zz)) * transform.scale.x, (2.0f * (xy - wz)) * transform.scale.y,
         (2.0f * (xz + wy)) * transform.scale.z, transform.position.x,
         (2.0f * (xy + wz)) * transform.scale.x, (1.0f - 2.0f * (xx + zz)) * transform.scale.y,
         (2.0f * (yz - wx)) * transform.scale.z, transform.position.y,
         (2.0f * (xz - wy)) * transform.scale.x, (2.0f * (yz + wx)) * transform.scale.y,
         (1.0f - 2.0f * (xx + yy)) * transform.scale.z, transform.position.z, 0.0f, 0.0f, 0.0f,
         1.0f}};
    return result;
}

VgMatrix vg_matrix_multiply(VgMatrix left, VgMatrix right) {
    VgMatrix result = {{0}};
    for (uint32_t row = 0u; row < 4u; ++row) {
        for (uint32_t column = 0u; column < 4u; ++column) {
            for (uint32_t k = 0u; k < 4u; ++k)
                result.m[row * 4u + column] += left.m[row * 4u + k] * right.m[k * 4u + column];
        }
    }
    return result;
}

bool vg_matrix_inverse_affine(VgMatrix matrix, VgMatrix *out_inverse) {
    float a = matrix.m[0], b = matrix.m[1], c = matrix.m[2];
    float d = matrix.m[4], e = matrix.m[5], f = matrix.m[6];
    float g = matrix.m[8], h = matrix.m[9], i = matrix.m[10];
    float determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (!isfinite(determinant) || fabsf(determinant) <= VG_TRANSFORM_EPSILON)
        return false;
    float inverse_determinant = 1.0f / determinant;
    VgMatrix inverse = {
        {(e * i - f * h) * inverse_determinant, (c * h - b * i) * inverse_determinant,
         (b * f - c * e) * inverse_determinant, 0.0f, (f * g - d * i) * inverse_determinant,
         (a * i - c * g) * inverse_determinant, (c * d - a * f) * inverse_determinant, 0.0f,
         (d * h - e * g) * inverse_determinant, (b * g - a * h) * inverse_determinant,
         (a * e - b * d) * inverse_determinant, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
    float tx = matrix.m[3], ty = matrix.m[7], tz = matrix.m[11];
    inverse.m[3] = -(inverse.m[0] * tx + inverse.m[1] * ty + inverse.m[2] * tz);
    inverse.m[7] = -(inverse.m[4] * tx + inverse.m[5] * ty + inverse.m[6] * tz);
    inverse.m[11] = -(inverse.m[8] * tx + inverse.m[9] * ty + inverse.m[10] * tz);
    *out_inverse = inverse;
    return true;
}

static VgQuat vg_quaternion_from_rotation(const VgMatrix *matrix) {
    VgQuat quaternion = {0.0f, 0.0f, 0.0f, 1.0f};
    float trace = matrix->m[0] + matrix->m[5] + matrix->m[10];
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        quaternion.w = 0.25f * s;
        quaternion.x = (matrix->m[9] - matrix->m[6]) / s;
        quaternion.y = (matrix->m[2] - matrix->m[8]) / s;
        quaternion.z = (matrix->m[4] - matrix->m[1]) / s;
    } else if (matrix->m[0] > matrix->m[5] && matrix->m[0] > matrix->m[10]) {
        float s = sqrtf(1.0f + matrix->m[0] - matrix->m[5] - matrix->m[10]) * 2.0f;
        quaternion.w = (matrix->m[9] - matrix->m[6]) / s;
        quaternion.x = 0.25f * s;
        quaternion.y = (matrix->m[1] + matrix->m[4]) / s;
        quaternion.z = (matrix->m[2] + matrix->m[8]) / s;
    } else if (matrix->m[5] > matrix->m[10]) {
        float s = sqrtf(1.0f + matrix->m[5] - matrix->m[0] - matrix->m[10]) * 2.0f;
        quaternion.w = (matrix->m[2] - matrix->m[8]) / s;
        quaternion.x = (matrix->m[1] + matrix->m[4]) / s;
        quaternion.y = 0.25f * s;
        quaternion.z = (matrix->m[6] + matrix->m[9]) / s;
    } else {
        float s = sqrtf(1.0f + matrix->m[10] - matrix->m[0] - matrix->m[5]) * 2.0f;
        quaternion.w = (matrix->m[4] - matrix->m[1]) / s;
        quaternion.x = (matrix->m[2] + matrix->m[8]) / s;
        quaternion.y = (matrix->m[6] + matrix->m[9]) / s;
        quaternion.z = 0.25f * s;
    }
    return quaternion;
}

bool vg_matrix_to_transform(VgMatrix matrix, VgTransform *out_transform) {
    if (out_transform == NULL)
        return false;
    float sx = vg_vec_length(matrix.m[0], matrix.m[4], matrix.m[8]);
    float sy = vg_vec_length(matrix.m[1], matrix.m[5], matrix.m[9]);
    float sz = vg_vec_length(matrix.m[2], matrix.m[6], matrix.m[10]);
    if (!isfinite(sx) || !isfinite(sy) || !isfinite(sz) || sx <= VG_TRANSFORM_EPSILON ||
        sy <= VG_TRANSFORM_EPSILON || sz <= VG_TRANSFORM_EPSILON)
        return false;
    float dot01 =
        (matrix.m[0] * matrix.m[1] + matrix.m[4] * matrix.m[5] + matrix.m[8] * matrix.m[9]) /
        (sx * sy);
    float dot02 =
        (matrix.m[0] * matrix.m[2] + matrix.m[4] * matrix.m[6] + matrix.m[8] * matrix.m[10]) /
        (sx * sz);
    float dot12 =
        (matrix.m[1] * matrix.m[2] + matrix.m[5] * matrix.m[6] + matrix.m[9] * matrix.m[10]) /
        (sy * sz);
    if (fabsf(dot01) > 1.0e-4f || fabsf(dot02) > 1.0e-4f || fabsf(dot12) > 1.0e-4f)
        return false;
    VgMatrix rotation = matrix;
    rotation.m[0] /= sx;
    rotation.m[4] /= sx;
    rotation.m[8] /= sx;
    rotation.m[1] /= sy;
    rotation.m[5] /= sy;
    rotation.m[9] /= sy;
    rotation.m[2] /= sz;
    rotation.m[6] /= sz;
    rotation.m[10] /= sz;
    float determinant =
        rotation.m[0] * (rotation.m[5] * rotation.m[10] - rotation.m[6] * rotation.m[9]) -
        rotation.m[1] * (rotation.m[4] * rotation.m[10] - rotation.m[6] * rotation.m[8]) +
        rotation.m[2] * (rotation.m[4] * rotation.m[9] - rotation.m[5] * rotation.m[8]);
    if (!isfinite(determinant) || determinant < 1.0f - 1.0e-3f || determinant > 1.0f + 1.0e-3f)
        return false;
    VgTransform transform = {{matrix.m[3], matrix.m[7], matrix.m[11]},
                             vg_quaternion_from_rotation(&rotation),
                             {sx, sy, sz}};
    return vg_transform_sanitize(&transform, out_transform);
}

VgResult vg_world_entity_matrix(const VgWorldState *world, uint32_t entity_index,
                                uint32_t override_index, const VgTransform *override_local,
                                uint16_t override_parent, uint16_t override_parent_generation,
                                VgMatrix *out_matrix) {
    if (world == NULL || out_matrix == NULL || entity_index >= world->entity_capacity)
        return VG_ERROR_INVALID_ARGUMENT;
    uint16_t chain[VG_HANDLE_ENTITY_MAX];
    uint32_t depth = 0u;
    uint32_t current = entity_index;
    uint16_t required_generation = 0u;
    bool validate_generation = false;
    while (current != VG_NO_PARENT) {
        if (current >= world->entity_capacity || depth >= VG_HANDLE_ENTITY_MAX)
            return VG_ERROR_CONFLICT;
        const VgEntitySlot *slot = &world->entities[current];
        if (slot->state == VG_ENTITY_FREE || slot->state == VG_ENTITY_RETIRED)
            return VG_ERROR_INVALID_HANDLE;
        if (validate_generation && slot->generation != required_generation)
            return VG_ERROR_INVALID_HANDLE;
        chain[depth++] = (uint16_t)current;
        if (current == override_index) {
            current = override_parent;
            required_generation = override_parent_generation;
        } else {
            current = slot->parent_index;
            required_generation = slot->parent_generation;
        }
        validate_generation = current != VG_NO_PARENT;
    }
    VgMatrix matrix = {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f}};
    while (depth > 0u) {
        uint32_t index = chain[--depth];
        const VgTransform *local =
            index == override_index ? override_local : &world->entities[index].local;
        matrix = vg_matrix_multiply(matrix, vg_transform_matrix(*local));
    }
    *out_matrix = matrix;
    return VG_OK;
}

VgResult vg_world_validate_transform_change(const VgWorldState *world, uint32_t entity_index,
                                            VgTransform local, uint16_t parent_index,
                                            uint16_t parent_generation) {
    for (uint32_t index = 0u; index < world->entity_capacity; ++index) {
        uint8_t state = world->entities[index].state;
        if (state == VG_ENTITY_FREE || state == VG_ENTITY_RETIRED)
            continue;
        VgMatrix matrix;
        VgTransform decomposed;
        VgResult result = vg_world_entity_matrix(world, index, entity_index, &local, parent_index,
                                                 parent_generation, &matrix);
        if (result != VG_OK)
            return result;
        if (!vg_matrix_to_transform(matrix, &decomposed))
            return VG_ERROR_UNSUPPORTED;
    }
    return VG_OK;
}
