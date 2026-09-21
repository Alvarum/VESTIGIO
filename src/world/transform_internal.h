#ifndef VESTIGIO_TRANSFORM_INTERNAL_H
#define VESTIGIO_TRANSFORM_INTERNAL_H

#include "runtime/runtime_internal.h"

typedef struct VgMatrix {
    float m[16];
} VgMatrix;

bool vg_transform_sanitize(const VgTransform *input, VgTransform *output);
VgMatrix vg_transform_matrix(VgTransform transform);
VgMatrix vg_matrix_multiply(VgMatrix left, VgMatrix right);
bool vg_matrix_inverse_affine(VgMatrix matrix, VgMatrix *out_inverse);
bool vg_matrix_to_transform(VgMatrix matrix, VgTransform *out_transform);
VgResult vg_world_entity_matrix(const VgWorldState *world, uint32_t entity_index,
                                uint32_t override_index, const VgTransform *override_local,
                                uint16_t override_parent, VgMatrix *out_matrix);
VgResult vg_world_validate_transform_change(const VgWorldState *world, uint32_t entity_index,
                                            VgTransform local, uint16_t parent_index);

#endif
