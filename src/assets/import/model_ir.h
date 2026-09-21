#ifndef VESTIGIO_MODEL_IR_H
#define VESTIGIO_MODEL_IR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { VG_MODEL_IR_MAGIC = 0x52494756u, VG_MODEL_IR_VERSION = 1u, VG_MODEL_NO_INDEX = UINT32_MAX };

typedef enum VgModelAlphaMode {
    VG_MODEL_ALPHA_OPAQUE = 0,
    VG_MODEL_ALPHA_MASK = 1,
    VG_MODEL_ALPHA_BLEND = 2
} VgModelAlphaMode;

typedef enum VgModelImageMime {
    VG_MODEL_IMAGE_UNKNOWN = 0,
    VG_MODEL_IMAGE_PNG = 1,
    VG_MODEL_IMAGE_JPEG = 2
} VgModelImageMime;

typedef struct VgModelVertexIr {
    float position[3];
    float normal[3];
    float texcoord[2];
    float color[4];
} VgModelVertexIr;

typedef struct VgModelPrimitiveIr {
    uint32_t first_vertex;
    uint32_t vertex_count;
    uint32_t first_index;
    uint32_t index_count;
    uint32_t material;
    uint32_t reserved;
    float bounds_min[3];
    float bounds_max[3];
} VgModelPrimitiveIr;

typedef struct VgModelMeshIr {
    uint32_t name_offset;
    uint32_t first_primitive;
    uint32_t primitive_count;
    uint32_t reserved;
} VgModelMeshIr;

typedef struct VgModelNodeIr {
    uint32_t name_offset;
    uint32_t parent;
    uint32_t mesh;
    uint32_t reserved;
    int32_t determinant_sign;
    /* True only for a root of the document's default scene. */
    uint32_t scene_root;
    /* Column-major local matrix in the engine basis. */
    float local_transform[16];
} VgModelNodeIr;

typedef struct VgModelMaterialIr {
    uint32_t name_offset;
    uint32_t base_color_texture;
    uint32_t alpha_mode;
    uint32_t double_sided;
    uint32_t unlit;
    uint32_t reserved;
    float base_color[4];
    float emissive[3];
    float alpha_cutoff;
} VgModelMaterialIr;

typedef struct VgModelTextureIr {
    uint32_t image;
    uint32_t mag_filter;
    uint32_t min_filter;
    uint32_t wrap_s;
    uint32_t wrap_t;
} VgModelTextureIr;

typedef struct VgModelImageIr {
    uint32_t name_offset;
    uint32_t mime_type;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t dependency;
    uint32_t reserved;
} VgModelImageIr;

typedef struct VgModelDependencyIr {
    uint32_t path_offset;
    uint32_t kind;
    uint64_t byte_size;
    uint8_t sha256[32];
} VgModelDependencyIr;

typedef struct VgStaticModelIr {
    uint32_t magic;
    uint32_t version;
    uint32_t importer_version;
    uint32_t reserved;
    uint64_t total_bytes;
    uint8_t source_fingerprint[32];
    uint32_t node_count;
    uint32_t mesh_count;
    uint32_t primitive_count;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t material_count;
    uint32_t texture_count;
    uint32_t image_count;
    uint32_t dependency_count;
    uint32_t nodes_offset;
    uint32_t meshes_offset;
    uint32_t primitives_offset;
    uint32_t vertices_offset;
    uint32_t indices_offset;
    uint32_t materials_offset;
    uint32_t textures_offset;
    uint32_t images_offset;
    uint32_t dependencies_offset;
    uint32_t strings_offset;
    uint32_t strings_size;
    uint32_t image_data_offset;
    uint32_t image_data_size;
    /* Bounds of canonical mesh geometry before node transforms. */
    float geometry_bounds_min[3];
    float geometry_bounds_max[3];
} VgStaticModelIr;

#define VG_MODEL_IR_ARRAY(model, type, field)                                                      \
    ((type *)((uint8_t *)(model) + (model)->field##_offset))
#define VG_MODEL_IR_ARRAY_CONST(model, type, field)                                                \
    ((const type *)((const uint8_t *)(model) + (model)->field##_offset))

static inline const char *vg_model_ir_string(const VgStaticModelIr *model, uint32_t offset) {
    if (model == NULL || offset >= model->strings_size) {
        return "";
    }
    return (const char *)((const uint8_t *)model + model->strings_offset + offset);
}

#ifdef __cplusplus
}
#endif

#endif
