#include "render/gpu_raylib/gpu_renderer.h"

#include "assets/import/model_ir.h"
#include "runtime/runtime_internal.h"
#include "world/transform_internal.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
typedef unsigned long VgGpuThreadId;
static VgGpuThreadId gpu_thread_current(void) {
    return GetCurrentThreadId();
}
static bool gpu_thread_equal(VgGpuThreadId left, VgGpuThreadId right) {
    return left == right;
}
#else
#include <pthread.h>
typedef pthread_t VgGpuThreadId;
static VgGpuThreadId gpu_thread_current(void) {
    return pthread_self();
}
static bool gpu_thread_equal(VgGpuThreadId left, VgGpuThreadId right) {
    return pthread_equal(left, right) != 0;
}
#endif

enum { VG_GL_VENDOR = 0x1F00u, VG_GL_RENDERER = 0x1F01u, VG_GL_VERSION = 0x1F02u };
typedef const unsigned char *(*VgGlGetString)(unsigned int name);

enum { VG_GPU_RESOURCE_MAGIC = 0x55504756u, VG_MATERIAL_MAP_COUNT = 12u };

typedef struct VgGpuPrimitive {
    Mesh mesh;
    uint32_t material;
} VgGpuPrimitive;

typedef struct VgGpuMeshRange {
    uint32_t first_primitive;
    uint32_t primitive_count;
} VgGpuMeshRange;

typedef struct VgGpuMaterialData {
    uint32_t texture;
    uint32_t alpha_mode;
    uint32_t double_sided;
    float base_color[4];
    float emissive[3];
    float alpha_cutoff;
} VgGpuMaterialData;

typedef struct VgGpuResource {
    uint32_t magic;
    uint32_t primitive_count;
    uint32_t mesh_count;
    uint32_t node_count;
    uint32_t material_count;
    uint32_t texture_count;
    uint64_t estimated_bytes;
    uint64_t token;
    VgGpuPrimitive *primitives;
    VgGpuMeshRange *meshes;
    Matrix *node_world;
    uint32_t *node_mesh;
    int32_t *node_determinant_sign;
    VgGpuMaterialData *materials;
    Texture2D *textures;
    bool *texture_owned;
    struct VgGpuResource *next;
} VgGpuResource;

struct VgGpuRenderer {
    RenderTexture2D target;
    Mesh cube;
    Material material;
    Texture2D sprite;
    Camera3D camera;
    VgFrameStats stats;
    uint32_t width, height;
    VgGpuThreadId owner_thread;
    VgGpuResource *resources;
    int alpha_mode_location;
    int alpha_cutoff_location;
    int emissive_location;
    int error_material_location;
    uint64_t next_resource_token;
    _Atomic uint32_t wrong_thread_calls;
};

static bool renderer_is_owner(const VgGpuRenderer *renderer) {
    return renderer != NULL && gpu_thread_equal(renderer->owner_thread, gpu_thread_current());
}

static bool renderer_require_owner(const VgGpuRenderer *renderer) {
    if (renderer_is_owner(renderer))
        return true;
    if (renderer != NULL)
        (void)atomic_fetch_add_explicit(&((VgGpuRenderer *)renderer)->wrong_thread_calls, 1u,
                                        memory_order_relaxed);
    return false;
}
static const char *vertex_shader = "#version 330\n"
                                   "in vec3 vertexPosition;\n"
                                   "in vec2 vertexTexCoord;\n"
                                   "in vec4 vertexColor;\n"
                                   "uniform mat4 mvp;\n"
                                   "out vec2 fragTexCoord;\n"
                                   "out vec4 fragColor;\n"
                                   "void main(){fragTexCoord=vertexTexCoord;fragColor=vertexColor;"
                                   "gl_Position=mvp*vec4(vertexPosition,1.0);}\n";

static const char *fragment_shader =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform int alphaMode;\n"
    "uniform float alphaCutoff;\n"
    "uniform vec3 emissiveColor;\n"
    "uniform int errorMaterial;\n"
    "out vec4 finalColor;\n"
    "void main(){vec4 texel=texture(texture0,fragTexCoord);"
    "vec4 color=texel*colDiffuse*fragColor;"
    "if(alphaMode==1 && color.a<alphaCutoff) discard;"
    "if(errorMaterial!=0){float p=mod(floor(gl_FragCoord.x/8.0)+floor(gl_FragCoord.y/8.0),2.0);"
    "color=vec4(mix(vec3(0.05),vec3(1.0,0.0,1.0),p),1.0);}"
    "color.rgb+=emissiveColor;finalColor=color;}\n";

static void message(char *out, size_t capacity, const char *value) {
    if (out && capacity)
        (void)snprintf(out, capacity, "%s", value);
}

static unsigned char color_byte(float value) {
    if (value <= 0.0f)
        return 0u;
    if (value >= 1.0f)
        return 255u;
    return (unsigned char)(value * 255.0f + 0.5f);
}

static Matrix matrix_from_ir(const float value[16]) {
    return (Matrix){value[0], value[4],  value[8],  value[12], value[1],  value[5],
                    value[9], value[13], value[2],  value[6],  value[10], value[14],
                    value[3], value[7],  value[11], value[15]};
}

static Matrix matrix_from_transform(VgTransform transform) {
    Matrix scale = MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z);
    Matrix rotation = QuaternionToMatrix((Quaternion){transform.rotation.x, transform.rotation.y,
                                                      transform.rotation.z, transform.rotation.w});
    Matrix translation =
        MatrixTranslate(transform.position.x, transform.position.y, transform.position.z);
    return MatrixMultiply(MatrixMultiply(scale, rotation), translation);
}

static VgGpuResource *resource_find(const VgGpuRenderer *renderer, uint64_t token) {
    VgGpuResource *candidate = renderer != NULL ? renderer->resources : NULL;
    while (candidate != NULL) {
        if (candidate->token == token && candidate->magic == VG_GPU_RESOURCE_MAGIC)
            return candidate;
        candidate = candidate->next;
    }
    return NULL;
}

static void resource_unload(VgGpuResource *resource) {
    if (resource == NULL)
        return;
    for (uint32_t index = 0u; resource->primitives != NULL && index < resource->primitive_count;
         ++index) {
        if (resource->primitives[index].mesh.vaoId != 0u)
            UnloadMesh(resource->primitives[index].mesh);
        else {
            MemFree(resource->primitives[index].mesh.indices);
            MemFree(resource->primitives[index].mesh.colors);
            MemFree(resource->primitives[index].mesh.texcoords);
            MemFree(resource->primitives[index].mesh.normals);
            MemFree(resource->primitives[index].mesh.vertices);
        }
    }
    for (uint32_t index = 0u; resource->texture_owned != NULL && resource->textures != NULL &&
                              index < resource->texture_count;
         ++index)
        if (resource->texture_owned[index] && resource->textures[index].id != 0u)
            UnloadTexture(resource->textures[index]);
    resource->magic = 0u;
    free(resource->texture_owned);
    free(resource->textures);
    free(resource->materials);
    free(resource->node_determinant_sign);
    free(resource->node_mesh);
    free(resource->node_world);
    free(resource->meshes);
    free(resource->primitives);
    free(resource);
}

typedef struct VgIrRegion {
    uint64_t begin;
    uint64_t end;
} VgIrRegion;

static VgResult ir_region_add(VgIrRegion *regions, uint32_t *region_count, uint32_t offset,
                              uint32_t count, size_t element_size, size_t alignment, uint64_t bytes,
                              const char *label, char *error, uint32_t error_capacity) {
    if (count == 0u)
        return VG_OK;
    uint64_t size = (uint64_t)count * (uint64_t)element_size;
    uint64_t begin = offset;
    if (alignment == 0u || begin % alignment != 0u || begin < sizeof(VgStaticModelIr) ||
        begin > bytes || size > bytes - begin) {
        char detail[160];
        (void)snprintf(detail, sizeof(detail), "Static model IR region invalid: %s", label);
        message(error, error_capacity, detail);
        return VG_ERROR_FORMAT_VERSION;
    }
    uint64_t end = begin + size;
    for (uint32_t index = 0u; index < *region_count; ++index) {
        if (begin < regions[index].end && regions[index].begin < end) {
            char detail[160];
            (void)snprintf(detail, sizeof(detail), "Static model IR region overlaps: %s", label);
            message(error, error_capacity, detail);
            return VG_ERROR_FORMAT_VERSION;
        }
    }
    regions[*region_count] = (VgIrRegion){begin, end};
    ++*region_count;
    return VG_OK;
}

static bool finite_values(const float *values, uint32_t count) {
    for (uint32_t index = 0u; index < count; ++index)
        if (!isfinite(values[index]))
            return false;
    return true;
}

static int32_t matrix_determinant_sign(const float matrix[16]) {
    float determinant = matrix[0] * (matrix[5] * matrix[10] - matrix[9] * matrix[6]) -
                        matrix[4] * (matrix[1] * matrix[10] - matrix[9] * matrix[2]) +
                        matrix[8] * (matrix[1] * matrix[6] - matrix[5] * matrix[2]);
    return determinant < 0.0f ? -1 : (determinant > 0.0f ? 1 : 0);
}
static VgResult model_ir_validate(const VgStaticModelIr *model, uint64_t bytes, char *error,
                                  uint32_t error_capacity) {
    if (model == NULL || bytes < sizeof(*model)) {
        message(error, error_capacity, "Static model IR payload is truncated");
        return VG_ERROR_FORMAT_VERSION;
    }
    if (model->magic != VG_MODEL_IR_MAGIC || model->version != VG_MODEL_IR_VERSION) {
        message(error, error_capacity, "Static model IR magic or version is unsupported");
        return VG_ERROR_FORMAT_VERSION;
    }
    if (model->total_bytes != bytes) {
        message(error, error_capacity, "Static model IR byte size does not match its header");
        return VG_ERROR_FORMAT_VERSION;
    }
    if (bytes > UINT32_MAX) {
        message(error, error_capacity, "Static model IR exceeds 32-bit packed-offset capacity");
        return VG_ERROR_UNSUPPORTED;
    }

    VgIrRegion regions[12] = {0};
    uint32_t region_count = 0u;
    VgResult result = VG_OK;
#define ADD_REGION(field, count, type)                                                             \
    do {                                                                                           \
        result =                                                                                   \
            ir_region_add(regions, &region_count, model->field##_offset, model->count,             \
                          sizeof(type), _Alignof(type), bytes, #field, error, error_capacity);     \
        if (result != VG_OK)                                                                       \
            return result;                                                                         \
    } while (0)
    ADD_REGION(nodes, node_count, VgModelNodeIr);
    ADD_REGION(meshes, mesh_count, VgModelMeshIr);
    ADD_REGION(primitives, primitive_count, VgModelPrimitiveIr);
    ADD_REGION(vertices, vertex_count, VgModelVertexIr);
    ADD_REGION(indices, index_count, uint32_t);
    ADD_REGION(materials, material_count, VgModelMaterialIr);
    ADD_REGION(textures, texture_count, VgModelTextureIr);
    ADD_REGION(images, image_count, VgModelImageIr);
    ADD_REGION(dependencies, dependency_count, VgModelDependencyIr);
#undef ADD_REGION
    result = ir_region_add(regions, &region_count, model->strings_offset, model->strings_size, 1u,
                           1u, bytes, "strings", error, error_capacity);
    if (result != VG_OK)
        return result;
    result = ir_region_add(regions, &region_count, model->image_data_offset, model->image_data_size,
                           1u, 1u, bytes, "image_data", error, error_capacity);
    if (result != VG_OK)
        return result;

    const VgModelNodeIr *nodes = VG_MODEL_IR_ARRAY_CONST(model, VgModelNodeIr, nodes);
    const VgModelMeshIr *meshes = VG_MODEL_IR_ARRAY_CONST(model, VgModelMeshIr, meshes);
    const VgModelPrimitiveIr *primitives =
        VG_MODEL_IR_ARRAY_CONST(model, VgModelPrimitiveIr, primitives);
    const VgModelVertexIr *vertices = VG_MODEL_IR_ARRAY_CONST(model, VgModelVertexIr, vertices);
    const uint32_t *indices = VG_MODEL_IR_ARRAY_CONST(model, uint32_t, indices);
    const VgModelMaterialIr *materials =
        VG_MODEL_IR_ARRAY_CONST(model, VgModelMaterialIr, materials);
    const VgModelTextureIr *textures = VG_MODEL_IR_ARRAY_CONST(model, VgModelTextureIr, textures);
    const VgModelImageIr *images = VG_MODEL_IR_ARRAY_CONST(model, VgModelImageIr, images);

    for (uint32_t index = 0u; index < model->node_count; ++index) {
        if ((nodes[index].parent != VG_MODEL_NO_INDEX &&
             nodes[index].parent >= model->node_count) ||
            nodes[index].parent == index ||
            (nodes[index].mesh != VG_MODEL_NO_INDEX && nodes[index].mesh >= model->mesh_count) ||
            nodes[index].scene_root > 1u || nodes[index].determinant_sign < -1 ||
            nodes[index].determinant_sign > 1 ||
            !finite_values(nodes[index].local_transform, 16u) ||
            nodes[index].determinant_sign !=
                matrix_determinant_sign(nodes[index].local_transform)) {
            message(error, error_capacity, "Static model IR node is invalid or non-finite");
            return VG_ERROR_FORMAT_VERSION;
        }
    }
    for (uint32_t index = 0u; index < model->mesh_count; ++index) {
        if (meshes[index].first_primitive > model->primitive_count ||
            meshes[index].primitive_count >
                model->primitive_count - meshes[index].first_primitive) {
            message(error, error_capacity, "Static model IR mesh primitive range is invalid");
            return VG_ERROR_FORMAT_VERSION;
        }
    }
    for (uint32_t index = 0u; index < model->primitive_count; ++index) {
        const VgModelPrimitiveIr *primitive = &primitives[index];
        if (primitive->vertex_count == 0u || primitive->index_count == 0u ||
            primitive->index_count % 3u != 0u || primitive->first_vertex > model->vertex_count ||
            primitive->vertex_count > model->vertex_count - primitive->first_vertex ||
            primitive->first_index > model->index_count ||
            primitive->index_count > model->index_count - primitive->first_index ||
            (primitive->material != VG_MODEL_NO_INDEX &&
             primitive->material >= model->material_count) ||
            !finite_values(primitive->bounds_min, 3u) ||
            !finite_values(primitive->bounds_max, 3u)) {
            message(error, error_capacity, "Static model IR primitive is invalid");
            return VG_ERROR_FORMAT_VERSION;
        }
        for (uint32_t axis = 0u; axis < 3u; ++axis)
            if (primitive->bounds_min[axis] > primitive->bounds_max[axis]) {
                message(error, error_capacity, "Static model IR primitive bounds are inverted");
                return VG_ERROR_FORMAT_VERSION;
            }
        for (uint32_t element = 0u; element < primitive->index_count; ++element)
            if (indices[primitive->first_index + element] >= primitive->vertex_count) {
                message(error, error_capacity, "Static model IR primitive index is out of range");
                return VG_ERROR_FORMAT_VERSION;
            }
    }
    for (uint32_t index = 0u; index < model->vertex_count; ++index) {
        if (!finite_values(vertices[index].position, 3u) ||
            !finite_values(vertices[index].normal, 3u) ||
            !finite_values(vertices[index].texcoord, 2u) ||
            !finite_values(vertices[index].color, 4u)) {
            message(error, error_capacity, "Static model IR vertex contains non-finite data");
            return VG_ERROR_FORMAT_VERSION;
        }
    }
    for (uint32_t index = 0u; index < model->material_count; ++index) {
        const VgModelMaterialIr *material = &materials[index];
        if (material->alpha_mode > VG_MODEL_ALPHA_BLEND || material->double_sided > 1u ||
            material->unlit > 1u ||
            (material->base_color_texture != VG_MODEL_NO_INDEX &&
             material->base_color_texture >= model->texture_count) ||
            !finite_values(material->base_color, 4u) || !finite_values(material->emissive, 3u) ||
            !isfinite(material->alpha_cutoff) || material->alpha_cutoff < 0.0f ||
            material->alpha_cutoff > 1.0f) {
            message(error, error_capacity, "Static model IR material is invalid or non-finite");
            return VG_ERROR_FORMAT_VERSION;
        }
    }
    for (uint32_t index = 0u; index < model->texture_count; ++index)
        if (textures[index].image >= model->image_count) {
            message(error, error_capacity, "Static model IR texture references an invalid image");
            return VG_ERROR_FORMAT_VERSION;
        }
    for (uint32_t index = 0u; index < model->image_count; ++index) {
        if ((images[index].mime_type != VG_MODEL_IMAGE_PNG &&
             images[index].mime_type != VG_MODEL_IMAGE_JPEG) ||
            images[index].data_size == 0u || images[index].data_offset > model->image_data_size ||
            images[index].data_size > model->image_data_size - images[index].data_offset) {
            message(error, error_capacity, "Static model IR image payload or MIME type is invalid");
            return VG_ERROR_FORMAT_VERSION;
        }
    }
    if (model->vertex_count != 0u && (!finite_values(model->geometry_bounds_min, 3u) ||
                                      !finite_values(model->geometry_bounds_max, 3u))) {
        message(error, error_capacity, "Static model IR geometry bounds are non-finite");
        return VG_ERROR_FORMAT_VERSION;
    }
    return VG_OK;
}

static VgResult resource_build_nodes(VgGpuResource *resource, const VgStaticModelIr *model,
                                     char *error, uint32_t error_capacity) {
    const VgModelNodeIr *nodes = VG_MODEL_IR_ARRAY_CONST(model, VgModelNodeIr, nodes);
    bool *ready = calloc(model->node_count, sizeof(*ready));
    uint32_t *path = malloc((size_t)model->node_count * sizeof(*path));
    if (model->node_count != 0u && (ready == NULL || path == NULL)) {
        free(path);
        free(ready);
        message(error, error_capacity, "Out of memory resolving model node hierarchy");
        return VG_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t index = 0u; index < model->node_count; ++index) {
        resource->node_world[index] = matrix_from_ir(nodes[index].local_transform);
        resource->node_mesh[index] = nodes[index].mesh;
        resource->node_determinant_sign[index] = nodes[index].determinant_sign;
    }
    for (uint32_t index = 0u; index < model->node_count; ++index) {
        uint32_t count = 0u;
        uint32_t cursor = index;
        while (cursor != VG_MODEL_NO_INDEX) {
            if (cursor >= model->node_count || count >= model->node_count) {
                free(path);
                free(ready);
                message(error, error_capacity, "Static model IR node hierarchy contains a cycle");
                return VG_ERROR_FORMAT_VERSION;
            }
            if (ready[cursor])
                break;
            path[count++] = cursor;
            cursor = nodes[cursor].parent;
        }
        while (count != 0u) {
            uint32_t node_index = path[--count];
            uint32_t parent = nodes[node_index].parent;
            if (parent != VG_MODEL_NO_INDEX) {
                resource->node_world[node_index] =
                    MatrixMultiply(resource->node_world[node_index], resource->node_world[parent]);
                resource->node_determinant_sign[node_index] *=
                    resource->node_determinant_sign[parent];
            }
            ready[node_index] = true;
        }
    }
    free(path);
    free(ready);
    return VG_OK;
}

static VgResult resource_upload_meshes(VgGpuResource *resource, const VgStaticModelIr *model,
                                       char *error, uint32_t error_capacity) {
    const VgModelMeshIr *meshes = VG_MODEL_IR_ARRAY_CONST(model, VgModelMeshIr, meshes);
    const VgModelPrimitiveIr *primitives =
        VG_MODEL_IR_ARRAY_CONST(model, VgModelPrimitiveIr, primitives);
    const VgModelVertexIr *vertices = VG_MODEL_IR_ARRAY_CONST(model, VgModelVertexIr, vertices);
    const uint32_t *indices = VG_MODEL_IR_ARRAY_CONST(model, uint32_t, indices);
    for (uint32_t index = 0u; index < model->mesh_count; ++index)
        resource->meshes[index] =
            (VgGpuMeshRange){meshes[index].first_primitive, meshes[index].primitive_count};
    for (uint32_t index = 0u; index < model->primitive_count; ++index) {
        const VgModelPrimitiveIr *source = &primitives[index];
        VgGpuPrimitive *target = &resource->primitives[index];
        if (source->vertex_count > UINT16_MAX || source->vertex_count > INT_MAX ||
            source->index_count / 3u > INT_MAX) {
            message(error, error_capacity, "Mesh primitive exceeds raylib 16-bit index limits");
            return VG_ERROR_UNSUPPORTED;
        }
        target->mesh.vertexCount = (int)source->vertex_count;
        target->mesh.triangleCount = (int)(source->index_count / 3u);
        target->mesh.vertices = MemAlloc((unsigned int)source->vertex_count * 3u * sizeof(float));
        target->mesh.normals = MemAlloc((unsigned int)source->vertex_count * 3u * sizeof(float));
        target->mesh.texcoords = MemAlloc((unsigned int)source->vertex_count * 2u * sizeof(float));
        target->mesh.colors = MemAlloc((unsigned int)source->vertex_count * 4u);
        target->mesh.indices = MemAlloc((unsigned int)source->index_count * sizeof(unsigned short));
        if (target->mesh.vertices == NULL || target->mesh.normals == NULL ||
            target->mesh.texcoords == NULL || target->mesh.colors == NULL ||
            target->mesh.indices == NULL) {
            message(error, error_capacity, "Out of memory staging mesh upload");
            return VG_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t vertex = 0u; vertex < source->vertex_count; ++vertex) {
            const VgModelVertexIr *input = &vertices[source->first_vertex + vertex];
            memcpy(target->mesh.vertices + vertex * 3u, input->position, sizeof(input->position));
            memcpy(target->mesh.normals + vertex * 3u, input->normal, sizeof(input->normal));
            memcpy(target->mesh.texcoords + vertex * 2u, input->texcoord, sizeof(input->texcoord));
            for (uint32_t component = 0u; component < 4u; ++component)
                target->mesh.colors[vertex * 4u + component] = color_byte(input->color[component]);
        }
        for (uint32_t element = 0u; element < source->index_count; ++element)
            target->mesh.indices[element] = (unsigned short)indices[source->first_index + element];
        UploadMesh(&target->mesh, false);
        if (target->mesh.vaoId == 0u) {
            message(error, error_capacity, "OpenGL mesh upload failed");
            return VG_ERROR_GPU;
        }
        target->material = source->material;
        resource->estimated_bytes +=
            (uint64_t)source->vertex_count * (3u + 3u + 2u + 1u) * sizeof(float) +
            (uint64_t)source->index_count * sizeof(unsigned short);
    }
    return VG_OK;
}

static VgResult resource_upload_textures(VgGpuResource *resource, const VgStaticModelIr *model,
                                         char *error, uint32_t error_capacity) {
    const VgModelTextureIr *textures = VG_MODEL_IR_ARRAY_CONST(model, VgModelTextureIr, textures);
    const VgModelImageIr *images = VG_MODEL_IR_ARRAY_CONST(model, VgModelImageIr, images);
    const uint8_t *image_data = (const uint8_t *)model + model->image_data_offset;
    for (uint32_t index = 0u; index < model->texture_count; ++index) {
        const VgModelImageIr *source = &images[textures[index].image];
        if (source->data_size > INT_MAX) {
            message(error, error_capacity, "Texture image exceeds raylib decode capacity");
            return VG_ERROR_UNSUPPORTED;
        }
        const char *extension = source->mime_type == VG_MODEL_IMAGE_PNG ? ".png" : ".jpg";
        Image decoded = LoadImageFromMemory(extension, image_data + source->data_offset,
                                            (int)source->data_size);
        if (decoded.data == NULL) {
            message(error, error_capacity, "Static model texture image cannot be decoded");
            return VG_ERROR_FORMAT_VERSION;
        }
        resource->textures[index] = LoadTextureFromImage(decoded);
        UnloadImage(decoded);
        if (resource->textures[index].id == 0u) {
            message(error, error_capacity, "OpenGL texture upload failed");
            return VG_ERROR_GPU;
        }
        resource->texture_owned[index] = true;
        SetTextureFilter(resource->textures[index], TEXTURE_FILTER_POINT);
        resource->estimated_bytes += (uint64_t)resource->textures[index].width *
                                     (uint64_t)resource->textures[index].height * 4u;
    }
    return VG_OK;
}

static VgResult resource_upload(VgGpuRenderer *renderer, const VgStaticModelIr *model,
                                uint64_t bytes, VgGpuResource **out_resource, char *error,
                                uint32_t error_capacity) {
    VgResult result = model_ir_validate(model, bytes, error, error_capacity);
    if (result != VG_OK)
        return result;
    VgGpuResource *resource = calloc(1u, sizeof(*resource));
    if (resource == NULL) {
        message(error, error_capacity, "Out of memory creating GPU model");
        return VG_ERROR_OUT_OF_MEMORY;
    }
    resource->magic = VG_GPU_RESOURCE_MAGIC;
    resource->primitive_count = model->primitive_count;
    resource->mesh_count = model->mesh_count;
    resource->node_count = model->node_count;
    resource->material_count = model->material_count;
    resource->texture_count = model->texture_count;
    resource->primitives = calloc(model->primitive_count, sizeof(*resource->primitives));
    resource->meshes = calloc(model->mesh_count, sizeof(*resource->meshes));
    resource->node_world = calloc(model->node_count, sizeof(*resource->node_world));
    resource->node_mesh = malloc((size_t)model->node_count * sizeof(*resource->node_mesh));
    resource->node_determinant_sign =
        malloc((size_t)model->node_count * sizeof(*resource->node_determinant_sign));
    resource->materials = calloc(model->material_count, sizeof(*resource->materials));
    resource->textures = calloc(model->texture_count, sizeof(*resource->textures));
    resource->texture_owned = calloc(model->texture_count, sizeof(*resource->texture_owned));
    if ((model->primitive_count != 0u && resource->primitives == NULL) ||
        (model->mesh_count != 0u && resource->meshes == NULL) ||
        (model->node_count != 0u && (resource->node_world == NULL || resource->node_mesh == NULL ||
                                     resource->node_determinant_sign == NULL)) ||
        (model->material_count != 0u && resource->materials == NULL) ||
        (model->texture_count != 0u &&
         (resource->textures == NULL || resource->texture_owned == NULL))) {
        message(error, error_capacity, "Out of memory creating GPU model tables");
        resource_unload(resource);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    const VgModelMaterialIr *materials =
        VG_MODEL_IR_ARRAY_CONST(model, VgModelMaterialIr, materials);
    for (uint32_t index = 0u; index < model->material_count; ++index) {
        resource->materials[index].texture = materials[index].base_color_texture;
        resource->materials[index].alpha_mode = materials[index].alpha_mode;
        resource->materials[index].double_sided = materials[index].double_sided;
        memcpy(resource->materials[index].base_color, materials[index].base_color,
               sizeof(materials[index].base_color));
        memcpy(resource->materials[index].emissive, materials[index].emissive,
               sizeof(materials[index].emissive));
        resource->materials[index].alpha_cutoff = materials[index].alpha_cutoff;
    }
    result = resource_build_nodes(resource, model, error, error_capacity);
    if (result == VG_OK)
        result = resource_upload_textures(resource, model, error, error_capacity);
    if (result == VG_OK)
        result = resource_upload_meshes(resource, model, error, error_capacity);
    if (result != VG_OK) {
        resource_unload(resource);
        return result;
    }
    resource->next = renderer->resources;
    if (++renderer->next_resource_token == 0u)
        ++renderer->next_resource_token;
    resource->token = renderer->next_resource_token;
    renderer->resources = resource;
    *out_resource = resource;
    return VG_OK;
}
bool vg_gpu_renderer_validate_shader(VgGpuRenderer *renderer, const char *vs, const char *fs,
                                     char *error, size_t error_capacity) {
    if (renderer == NULL || !renderer_require_owner(renderer) || !IsWindowReady() || vs == NULL ||
        fs == NULL) {
        message(error, error_capacity, "Contexto grafico o fuente de shader invalida");
        return false;
    }
    Shader candidate = LoadShaderFromMemory(vs, fs);
    if (candidate.id == 0u || candidate.id == rlGetShaderIdDefault()) {
        if (candidate.locs && candidate.id == 0u)
            UnloadShader(candidate);
        message(error, error_capacity, "No se pudo compilar/enlazar shader GPU");
        return false;
    }
    UnloadShader(candidate);
    message(error, error_capacity, "");
    return true;
}

VgGpuRenderer *vg_gpu_renderer_create(VgGpuRendererConfig config, char *error,
                                      size_t error_capacity) {
    if (!IsWindowReady() || config.internal_width == 0u || config.internal_height == 0u ||
        config.internal_width > 4096u || config.internal_height > 4096u) {
        message(error, error_capacity, "Contexto o resolucion interna invalida");
        return NULL;
    }
    VgGpuRenderer *renderer = calloc(1u, sizeof(*renderer));
    if (!renderer) {
        message(error, error_capacity, "Sin memoria para renderer GPU");
        return NULL;
    }
    renderer->width = config.internal_width;
    renderer->height = config.internal_height;
    renderer->owner_thread = gpu_thread_current();
    renderer->target = LoadRenderTexture((int)config.internal_width, (int)config.internal_height);
    if (renderer->target.id == 0u) {
        message(error, error_capacity, "No se pudo crear render target GPU");
        free(renderer);
        return NULL;
    }
    SetTextureFilter(renderer->target.texture, TEXTURE_FILTER_POINT);
    renderer->cube = GenMeshCube(1.5f, 1.5f, 1.5f);
    if (renderer->cube.vaoId == 0u) {
        message(error, error_capacity, "No se pudo subir mesh GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    renderer->material = LoadMaterialDefault();
    if (!renderer->material.maps) {
        message(error, error_capacity, "Sin memoria para material GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    Shader shader = LoadShaderFromMemory(vertex_shader, fragment_shader);
    if (shader.id == 0u || shader.id == rlGetShaderIdDefault()) {
        if (shader.locs && shader.id == 0u)
            UnloadShader(shader);
        message(error, error_capacity, "No se pudo crear shader principal GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    renderer->material.shader = shader;
    renderer->alpha_mode_location = GetShaderLocation(shader, "alphaMode");
    renderer->alpha_cutoff_location = GetShaderLocation(shader, "alphaCutoff");
    renderer->emissive_location = GetShaderLocation(shader, "emissiveColor");
    renderer->error_material_location = GetShaderLocation(shader, "errorMaterial");

    Image image = GenImageColor(4, 4, (Color){32, 132, 244, 255});
    ImageDrawRectangle(&image, 1, 1, 2, 2, BLANK);
    renderer->sprite = LoadTextureFromImage(image);
    UnloadImage(image);
    if (renderer->sprite.id == 0u) {
        message(error, error_capacity, "No se pudo subir textura de sprite GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    SetTextureFilter(renderer->sprite, TEXTURE_FILTER_POINT);

    renderer->camera = (Camera3D){.position = {0.0f, -5.0f, 1.6f},
                                  .target = {0.0f, 0.0f, 0.7f},
                                  .up = {0.0f, 0.0f, 1.0f},
                                  .fovy = 55.0f,
                                  .projection = CAMERA_PERSPECTIVE};
    renderer->stats.uploads = 3u;
    renderer->stats.estimated_gpu_bytes =
        (uint64_t)config.internal_width * (uint64_t)config.internal_height * 8u +
        (uint64_t)renderer->cube.vertexCount * 32u + 64u;
    message(error, error_capacity, "");
    return renderer;
}

void vg_gpu_renderer_destroy(VgGpuRenderer *renderer) {
    if (renderer == NULL || !renderer_require_owner(renderer))
        return;
    while (renderer->resources != NULL) {
        VgGpuResource *resource = renderer->resources;
        renderer->resources = resource->next;
        resource_unload(resource);
    }
    if (renderer->sprite.id)
        UnloadTexture(renderer->sprite);
    if (renderer->material.maps)
        UnloadMaterial(renderer->material);
    if (renderer->cube.vaoId)
        UnloadMesh(renderer->cube);
    if (renderer->target.id)
        UnloadRenderTexture(renderer->target);
    free(renderer);
}

bool vg_gpu_renderer_draw_demo(VgGpuRenderer *renderer) {
    if (renderer == NULL || !renderer_require_owner(renderer) || renderer->target.id == 0u)
        return false;
    int alpha_mode = VG_MODEL_ALPHA_OPAQUE;
    int error_material = 0;
    float alpha_cutoff = 0.5f;
    float emissive[3] = {0.0f, 0.0f, 0.0f};
    SetShaderValue(renderer->material.shader, renderer->alpha_mode_location, &alpha_mode,
                   SHADER_UNIFORM_INT);
    SetShaderValue(renderer->material.shader, renderer->alpha_cutoff_location, &alpha_cutoff,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(renderer->material.shader, renderer->emissive_location, emissive,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(renderer->material.shader, renderer->error_material_location, &error_material,
                   SHADER_UNIFORM_INT);
    BeginTextureMode(renderer->target);
    ClearBackground((Color){12, 18, 26, 255});
    BeginMode3D(renderer->camera);
    renderer->material.maps[MATERIAL_MAP_DIFFUSE].color = (Color){225, 62, 54, 255};
    DrawMesh(renderer->cube, renderer->material, MatrixTranslate(0.0f, 0.0f, 0.75f));
    renderer->material.maps[MATERIAL_MAP_DIFFUSE].color = (Color){54, 196, 103, 255};
    DrawMesh(renderer->cube, renderer->material, MatrixTranslate(0.0f, 1.2f, 0.75f));
    DrawBillboardRec(renderer->camera, renderer->sprite, (Rectangle){0, 0, 4, 4},
                     (Vector3){1.6f, 0.0f, 1.0f}, (Vector2){1.0f, 1.0f}, WHITE);
    EndMode3D();
    DrawRectangle(5, 5, 92, 14, (Color){5, 8, 12, 220});
    DrawText("GPU OPENGL 3.3", 9, 8, 7, (Color){244, 174, 66, 255});
    EndTextureMode();
    renderer->stats.frame_index++;
    renderer->stats.draw_calls = 3u;
    renderer->stats.triangles = 26u;
    return true;
}

void vg_gpu_renderer_present(VgGpuRenderer *renderer) {
    if (renderer == NULL || !renderer_require_owner(renderer) || renderer->target.id == 0u)
        return;
    float width = (float)GetRenderWidth(), height = (float)GetRenderHeight();
    float scale = fminf(width / (float)renderer->width, height / (float)renderer->height);
    if (scale >= 1.0f)
        scale = floorf(scale);
    float draw_width = (float)renderer->width * scale;
    float draw_height = (float)renderer->height * scale;
    BeginDrawing();
    ClearBackground((Color){5, 8, 12, 255});
    DrawTexturePro(renderer->target.texture,
                   (Rectangle){0, 0, (float)renderer->width, -(float)renderer->height},
                   (Rectangle){(width - draw_width) * 0.5f, (height - draw_height) * 0.5f,
                               draw_width, draw_height},
                   (Vector2){0, 0}, 0.0f, WHITE);
    EndDrawing();
}

void vg_gpu_renderer_present_embedded(VgGpuRenderer *renderer) {
    if (renderer == NULL || !renderer_require_owner(renderer) || renderer->target.id == 0u)
        return;
    float width = (float)GetRenderWidth(), height = (float)GetRenderHeight();
    float scale = fminf(width / (float)renderer->width, height / (float)renderer->height);
    if (scale >= 1.0f)
        scale = floorf(scale);
    float draw_width = (float)renderer->width * scale;
    float draw_height = (float)renderer->height * scale;
    BeginDrawing();
    ClearBackground((Color){5, 8, 12, 255});
    DrawTexturePro(renderer->target.texture,
                   (Rectangle){0, 0, (float)renderer->width, -(float)renderer->height},
                   (Rectangle){(width - draw_width) * 0.5f, (height - draw_height) * 0.5f,
                               draw_width, draw_height},
                   (Vector2){0, 0}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    SwapScreenBuffer();
}

bool vg_gpu_renderer_capture(VgGpuRenderer *renderer, const char *path) {
    if (renderer == NULL || !renderer_require_owner(renderer) || renderer->target.id == 0u ||
        path == NULL || *path == '\0')
        return false;
    Image image = LoadImageFromTexture(renderer->target.texture);
    if (!image.data)
        return false;
    renderer->stats.readbacks++;
    ImageFlipVertical(&image);
    bool ok = ExportImage(image, path);
    UnloadImage(image);
    return ok;
}

bool vg_gpu_renderer_info(const VgGpuRenderer *renderer, VgGpuInfo *out_info) {
    if (renderer == NULL || !renderer_require_owner(renderer) || out_info == NULL ||
        !IsWindowReady())
        return false;
    void *address = rlGetProcAddress("glGetString");
    VgGlGetString get_string = NULL;
    if (!address || sizeof(get_string) != sizeof(address))
        return false;
    memcpy(&get_string, &address, sizeof(get_string));
    const unsigned char *vendor = get_string(VG_GL_VENDOR);
    const unsigned char *name = get_string(VG_GL_RENDERER);
    const unsigned char *version = get_string(VG_GL_VERSION);
    if (!vendor || !name || !version)
        return false;
    *out_info = (VgGpuInfo){.rlgl_version = rlGetVersion()};
    (void)snprintf(out_info->vendor, sizeof(out_info->vendor), "%s", (const char *)vendor);
    (void)snprintf(out_info->renderer, sizeof(out_info->renderer), "%s", (const char *)name);
    (void)snprintf(out_info->version, sizeof(out_info->version), "%s", (const char *)version);
    return true;
}

static bool executor_owner_thread(void *user) {
    VgGpuRenderer *renderer = (VgGpuRenderer *)user;
    return renderer_is_owner(renderer);
}

static VgResult executor_upload(void *user, VgAssetType type, const void *cpu_data,
                                uint64_t cpu_bytes, VgAssetGpuObject *out_object, char *error,
                                uint32_t error_capacity) {
    VgGpuRenderer *renderer = (VgGpuRenderer *)user;
    if (renderer == NULL || out_object == NULL || cpu_data == NULL || cpu_bytes == 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!renderer_require_owner(renderer))
        return VG_ERROR_WRONG_THREAD;
    if (type != VG_ASSET_TYPE_MESH) {
        message(error, error_capacity, "GPU backend only accepts static-model mesh IR here");
        return VG_ERROR_UNSUPPORTED;
    }
    VgGpuResource *resource = NULL;
    VgResult result = resource_upload(renderer, (const VgStaticModelIr *)cpu_data, cpu_bytes,
                                      &resource, error, error_capacity);
    if (result != VG_OK)
        return result;
    VgAssetGpuObject object = {resource->token, resource->estimated_bytes};
    *out_object = object;
    ++renderer->stats.uploads;
    ++renderer->stats.asset_uploads;
    ++renderer->stats.resident_gpu_assets;
    renderer->stats.estimated_gpu_bytes += resource->estimated_bytes;
    message(error, error_capacity, "");
    return VG_OK;
}

static void executor_release(void *user, VgAssetType type, VgAssetGpuObject object) {
    VgGpuRenderer *renderer = (VgGpuRenderer *)user;
    if (renderer == NULL || type != VG_ASSET_TYPE_MESH || object.token == 0u)
        return;
    if (!renderer_require_owner(renderer))
        return;
    VgGpuResource **link = &renderer->resources;
    while (*link != NULL && (*link)->token != object.token)
        link = &(*link)->next;
    if (*link == NULL)
        return;
    VgGpuResource *resource = *link;
    *link = resource->next;
    if (renderer->stats.estimated_gpu_bytes >= resource->estimated_bytes)
        renderer->stats.estimated_gpu_bytes -= resource->estimated_bytes;
    else
        renderer->stats.estimated_gpu_bytes = 0u;
    if (renderer->stats.resident_gpu_assets != 0u)
        --renderer->stats.resident_gpu_assets;
    ++renderer->stats.asset_releases;
    resource_unload(resource);
}

VgAssetGpuExecutor vg_gpu_renderer_asset_executor(VgGpuRenderer *renderer) {
    return (VgAssetGpuExecutor){renderer, executor_owner_thread, executor_upload, executor_release};
}

bool vg_gpu_renderer_gpu_token_alive(const VgGpuRenderer *renderer, uint64_t token) {
    return renderer_require_owner(renderer) && resource_find(renderer, token) != NULL;
}

static float vector_dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Matrix model_packet_matrix(const VgGpuRenderer *renderer, const VgGpuModelPacket *packet) {
    Matrix packet_world = matrix_from_transform(packet->transform);
    VgGpuResource *resource = resource_find(renderer, packet->gpu_token);
    if (resource != NULL && packet->node_index != VG_MODEL_NO_INDEX &&
        packet->node_index < resource->node_count)
        return MatrixMultiply(resource->node_world[packet->node_index], packet_world);
    return packet_world;
}

static bool bounds_visible(const VgGpuCamera *camera, Matrix world, VgVec3 center, VgVec3 extent,
                           float aspect, float *out_depth) {
    Vector3 position = {camera->position.x, camera->position.y, camera->position.z};
    Vector3 target = {camera->target.x, camera->target.y, camera->target.z};
    Vector3 up = Vector3Normalize((Vector3){camera->up.x, camera->up.y, camera->up.z});
    Vector3 forward = Vector3Normalize(Vector3Subtract(target, position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, up));
    up = Vector3Normalize(Vector3CrossProduct(right, forward));
    Vector3 world_center = Vector3Transform((Vector3){center.x, center.y, center.z}, world);
    Vector3 world_extent = {
        fabsf(world.m0) * extent.x + fabsf(world.m4) * extent.y + fabsf(world.m8) * extent.z,
        fabsf(world.m1) * extent.x + fabsf(world.m5) * extent.y + fabsf(world.m9) * extent.z,
        fabsf(world.m2) * extent.x + fabsf(world.m6) * extent.y + fabsf(world.m10) * extent.z};
    Vector3 delta = Vector3Subtract(world_center, position);
    float radius = Vector3Length(world_extent);
    float depth = vector_dot(delta, forward);
    if (out_depth != NULL)
        *out_depth = depth;
    if (depth + radius < camera->near_clip_metres || depth - radius > camera->far_clip_metres)
        return false;
    float horizontal = fabsf(vector_dot(delta, right));
    float vertical = fabsf(vector_dot(delta, up));
    if (camera->projection == VG_CAMERA_ORTHOGRAPHIC) {
        float half_height = camera->orthographic_height * 0.5f;
        return horizontal <= half_height * aspect + radius && vertical <= half_height + radius;
    }
    float half_height = fmaxf(depth, 0.0f) * tanf(camera->vertical_fov_radians * 0.5f);
    return depth + radius >= 0.0f && horizontal <= half_height * aspect + radius &&
           vertical <= half_height + radius;
}

typedef struct VgPacketVisibility {
    bool visible;
    float depth;
} VgPacketVisibility;

typedef struct VgTransparentItem {
    uint32_t index;
    uint32_t kind;
    float depth;
} VgTransparentItem;

enum { VG_TRANSPARENT_MODEL = 0u, VG_TRANSPARENT_SPRITE = 1u };

static void sort_transparent_back_to_front(VgTransparentItem *items, uint32_t count) {
    for (uint32_t index = 1u; index < count; ++index) {
        VgTransparentItem item = items[index];
        uint32_t insert = index;
        while (insert != 0u && items[insert - 1u].depth < item.depth) {
            items[insert] = items[insert - 1u];
            --insert;
        }
        items[insert] = item;
    }
}
static Camera3D camera_from_packet(const VgGpuCamera *camera) {
    return (Camera3D){.position = {camera->position.x, camera->position.y, camera->position.z},
                      .target = {camera->target.x, camera->target.y, camera->target.z},
                      .up = {camera->up.x, camera->up.y, camera->up.z},
                      .fovy = camera->projection == VG_CAMERA_ORTHOGRAPHIC
                                  ? camera->orthographic_height
                                  : camera->vertical_fov_radians * RAD2DEG,
                      .projection = camera->projection == VG_CAMERA_ORTHOGRAPHIC
                                        ? CAMERA_ORTHOGRAPHIC
                                        : CAMERA_PERSPECTIVE};
}

static Material material_with_stack_maps(const VgGpuRenderer *renderer,
                                         MaterialMap maps[VG_MATERIAL_MAP_COUNT]) {
    memcpy(maps, renderer->material.maps, sizeof(*maps) * VG_MATERIAL_MAP_COUNT);
    Material material = renderer->material;
    material.maps = maps;
    return material;
}
static void set_scene_material(VgGpuRenderer *renderer, Material *material,
                               const VgGpuResource *resource, uint32_t material_index,
                               bool error_material) {
    int alpha_mode = VG_MODEL_ALPHA_OPAQUE;
    float alpha_cutoff = 0.5f;
    float emissive[3] = {0.0f, 0.0f, 0.0f};
    Color color = WHITE;
    Texture2D texture = {.id = rlGetTextureIdDefault(),
                         .width = 1,
                         .height = 1,
                         .mipmaps = 1,
                         .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if (!error_material && resource != NULL && material_index < resource->material_count) {
        const VgGpuMaterialData *source = &resource->materials[material_index];
        alpha_mode = (int)source->alpha_mode;
        alpha_cutoff = source->alpha_cutoff;
        memcpy(emissive, source->emissive, sizeof(emissive));
        color = (Color){color_byte(source->base_color[0]), color_byte(source->base_color[1]),
                        color_byte(source->base_color[2]), color_byte(source->base_color[3])};
        if (source->texture < resource->texture_count)
            texture = resource->textures[source->texture];
    }
    material->maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    material->maps[MATERIAL_MAP_DIFFUSE].color = color;
    int error_value = error_material ? 1 : 0;
    SetShaderValue(material->shader, renderer->alpha_mode_location, &alpha_mode,
                   SHADER_UNIFORM_INT);
    SetShaderValue(material->shader, renderer->alpha_cutoff_location, &alpha_cutoff,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(material->shader, renderer->emissive_location, emissive, SHADER_UNIFORM_VEC3);
    SetShaderValue(material->shader, renderer->error_material_location, &error_value,
                   SHADER_UNIFORM_INT);
}

static uint32_t primitive_material(const VgGpuModelPacket *packet,
                                   const VgGpuPrimitive *primitive) {
    return packet->material_override != VG_MODEL_NO_INDEX ? packet->material_override
                                                          : primitive->material;
}

static void draw_error_packet(VgGpuRenderer *renderer, const VgGpuModelPacket *packet) {
    MaterialMap maps[VG_MATERIAL_MAP_COUNT];
    Material material = material_with_stack_maps(renderer, maps);
    set_scene_material(renderer, &material, NULL, VG_MODEL_NO_INDEX, true);
    DrawMesh(renderer->cube, material, model_packet_matrix(renderer, packet));
    ++renderer->stats.draw_calls;
    renderer->stats.triangles += (uint32_t)renderer->cube.triangleCount;
    ++renderer->stats.error_material_draws;
}

static void draw_model_pass(VgGpuRenderer *renderer, const VgGpuModelPacket *packets,
                            const VgPacketVisibility *visible, uint32_t count, uint32_t pass) {
    for (uint32_t index = 0u; index < count; ++index) {
        const VgGpuModelPacket *packet = &packets[index];
        if (!visible[index].visible)
            continue;
        VgGpuResource *resource = resource_find(renderer, packet->gpu_token);
        if (resource == NULL || (packet->flags & VG_GPU_PACKET_FORCE_ERROR_MATERIAL) != 0u) {
            if (pass == VG_MODEL_ALPHA_OPAQUE)
                draw_error_packet(renderer, packet);
            continue;
        }
        uint32_t mesh_index = packet->mesh_index;
        if (packet->node_index != VG_MODEL_NO_INDEX) {
            if (packet->node_index >= resource->node_count) {
                if (pass == VG_MODEL_ALPHA_OPAQUE)
                    draw_error_packet(renderer, packet);
                continue;
            }
            mesh_index = resource->node_mesh[packet->node_index];
        }
        if (mesh_index == VG_MODEL_NO_INDEX && resource->mesh_count != 0u)
            mesh_index = 0u;
        if (mesh_index >= resource->mesh_count) {
            if (pass == VG_MODEL_ALPHA_OPAQUE)
                draw_error_packet(renderer, packet);
            continue;
        }
        VgGpuMeshRange range = resource->meshes[mesh_index];
        Matrix transform = model_packet_matrix(renderer, packet);
        for (uint32_t primitive_index = 0u; primitive_index < range.primitive_count;
             ++primitive_index) {
            VgGpuPrimitive *primitive =
                &resource->primitives[range.first_primitive + primitive_index];
            uint32_t material_index = primitive_material(packet, primitive);
            uint32_t mode = material_index < resource->material_count
                                ? resource->materials[material_index].alpha_mode
                                : VG_MODEL_ALPHA_OPAQUE;
            if (mode != pass)
                continue;
            MaterialMap maps[VG_MATERIAL_MAP_COUNT];
            Material material = material_with_stack_maps(renderer, maps);
            set_scene_material(renderer, &material, resource, material_index,
                               material_index >= resource->material_count);
            bool double_sided = material_index < resource->material_count &&
                                resource->materials[material_index].double_sided != 0u;
            int32_t determinant_sign = 1;
            if (packet->node_index != VG_MODEL_NO_INDEX)
                determinant_sign = resource->node_determinant_sign[packet->node_index];
            float instance_determinant =
                packet->transform.scale.x * packet->transform.scale.y * packet->transform.scale.z;
            if (instance_determinant < 0.0f)
                determinant_sign = -determinant_sign;
            else if (instance_determinant == 0.0f)
                determinant_sign = 0;
            bool reverse_culling = !double_sided && determinant_sign < 0;
            if (reverse_culling)
                ++renderer->stats.reversed_winding_draws;
            if (double_sided)
                rlDisableBackfaceCulling();
            else if (reverse_culling)
                rlSetCullFace(RL_CULL_FACE_FRONT);
            if ((packet->flags & VG_GPU_PACKET_WIREFRAME) != 0u)
                rlEnableWireMode();
            DrawMesh(primitive->mesh, material, transform);
            if ((packet->flags & VG_GPU_PACKET_WIREFRAME) != 0u)
                rlDisableWireMode();
            if (double_sided)
                rlEnableBackfaceCulling();
            else if (reverse_culling)
                rlSetCullFace(RL_CULL_FACE_BACK);
            ++renderer->stats.draw_calls;
            renderer->stats.triangles += (uint32_t)primitive->mesh.triangleCount;
            if (material_index >= resource->material_count)
                ++renderer->stats.error_material_draws;
        }
    }
}

static void draw_sprite_pass(VgGpuRenderer *renderer, Camera3D camera,
                             const VgGpuSpritePacket *packets, const VgPacketVisibility *visible,
                             uint32_t count, uint32_t pass) {
    for (uint32_t index = 0u; index < count; ++index) {
        const VgGpuSpritePacket *packet = &packets[index];
        if (!visible[index].visible || packet->material_mode != pass)
            continue;
        VgGpuResource *resource = resource_find(renderer, packet->gpu_token);
        bool error = resource == NULL || packet->texture_index >= resource->texture_count ||
                     (packet->flags & VG_GPU_PACKET_FORCE_ERROR_MATERIAL) != 0u;
        Texture2D texture = error ? renderer->sprite : resource->textures[packet->texture_index];
        MaterialMap maps[VG_MATERIAL_MAP_COUNT];
        Material material = material_with_stack_maps(renderer, maps);
        set_scene_material(renderer, &material, NULL, VG_MODEL_NO_INDEX, error);
        float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] >= 0)
            SetShaderValue(material.shader, material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], white,
                           SHADER_UNIFORM_VEC4);
        int alpha_mode = (int)packet->material_mode;
        SetShaderValue(material.shader, renderer->alpha_mode_location, &alpha_mode,
                       SHADER_UNIFORM_INT);
        SetShaderValue(material.shader, renderer->alpha_cutoff_location, &packet->alpha_cutoff,
                       SHADER_UNIFORM_FLOAT);
        Color tint = {color_byte(packet->tint[0]), color_byte(packet->tint[1]),
                      color_byte(packet->tint[2]), color_byte(packet->tint[3])};
        BeginShaderMode(material.shader);
        DrawBillboardPro(camera, texture,
                         (Rectangle){0, 0, (float)texture.width, (float)texture.height},
                         (Vector3){packet->position.x, packet->position.y, packet->position.z},
                         (Vector3){0, 0, 1}, (Vector2){packet->size.x, packet->size.y},
                         (Vector2){packet->size.x * 0.5f, packet->size.y * 0.5f}, 0.0f, tint);
        EndShaderMode();
        ++renderer->stats.draw_calls;
        renderer->stats.triangles += 2u;
        if (error)
            ++renderer->stats.error_material_draws;
    }
}

static bool model_packet_has_pass(const VgGpuRenderer *renderer, const VgGpuModelPacket *packet,
                                  uint32_t pass) {
    VgGpuResource *resource = resource_find(renderer, packet->gpu_token);
    if (resource == NULL || (packet->flags & VG_GPU_PACKET_FORCE_ERROR_MATERIAL) != 0u)
        return false;
    uint32_t mesh_index = packet->mesh_index;
    if (packet->node_index != VG_MODEL_NO_INDEX) {
        if (packet->node_index >= resource->node_count)
            return false;
        mesh_index = resource->node_mesh[packet->node_index];
    }
    if (mesh_index == VG_MODEL_NO_INDEX && resource->mesh_count != 0u)
        mesh_index = 0u;
    if (mesh_index >= resource->mesh_count)
        return false;
    VgGpuMeshRange range = resource->meshes[mesh_index];
    for (uint32_t index = 0u; index < range.primitive_count; ++index) {
        VgGpuPrimitive *primitive = &resource->primitives[range.first_primitive + index];
        uint32_t material_index = primitive_material(packet, primitive);
        if (material_index < resource->material_count &&
            resource->materials[material_index].alpha_mode == pass)
            return true;
    }
    return false;
}
static bool vec3_finite(VgVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool transform_finite(VgTransform value) {
    return vec3_finite(value.position) && vec3_finite(value.scale) && isfinite(value.rotation.x) &&
           isfinite(value.rotation.y) && isfinite(value.rotation.z) && isfinite(value.rotation.w);
}

static bool camera_valid(const VgGpuCamera *camera) {
    if (camera == NULL || !vec3_finite(camera->position) || !vec3_finite(camera->target) ||
        !vec3_finite(camera->up) || !isfinite(camera->near_clip_metres) ||
        !isfinite(camera->far_clip_metres) || camera->near_clip_metres <= 0.0f ||
        camera->far_clip_metres <= camera->near_clip_metres ||
        (camera->projection != VG_CAMERA_PERSPECTIVE &&
         camera->projection != VG_CAMERA_ORTHOGRAPHIC) ||
        (camera->projection == VG_CAMERA_PERSPECTIVE &&
         (!isfinite(camera->vertical_fov_radians) || camera->vertical_fov_radians <= 0.0f ||
          camera->vertical_fov_radians >= PI)) ||
        (camera->projection == VG_CAMERA_ORTHOGRAPHIC &&
         (!isfinite(camera->orthographic_height) || camera->orthographic_height <= 0.0f)))
        return false;
    Vector3 forward =
        Vector3Subtract((Vector3){camera->target.x, camera->target.y, camera->target.z},
                        (Vector3){camera->position.x, camera->position.y, camera->position.z});
    Vector3 up = {camera->up.x, camera->up.y, camera->up.z};
    return Vector3LengthSqr(forward) > 1.0e-8f && Vector3LengthSqr(up) > 1.0e-8f &&
           Vector3LengthSqr(Vector3CrossProduct(forward, up)) > 1.0e-8f;
}

bool vg_gpu_renderer_draw_scene(VgGpuRenderer *renderer, const VgGpuCamera *camera,
                                const VgGpuModelPacket *models, uint32_t model_count,
                                const VgGpuSpritePacket *sprites, uint32_t sprite_count) {
    if (renderer == NULL || !renderer_require_owner(renderer) || !camera_valid(camera) ||
        (model_count != 0u && models == NULL) || (sprite_count != 0u && sprites == NULL) ||
        sprite_count > UINT32_MAX - model_count)
        return false;
    for (uint32_t index = 0u; index < model_count; ++index) {
        if (!transform_finite(models[index].transform) ||
            !vec3_finite(models[index].bounds_center) ||
            !vec3_finite(models[index].bounds_extent) || models[index].bounds_extent.x < 0.0f ||
            models[index].bounds_extent.y < 0.0f || models[index].bounds_extent.z < 0.0f)
            return false;
    }
    for (uint32_t index = 0u; index < sprite_count; ++index) {
        if (!vec3_finite(sprites[index].position) || !vec3_finite(sprites[index].size) ||
            sprites[index].size.x < 0.0f || sprites[index].size.y < 0.0f ||
            sprites[index].material_mode > VG_MODEL_ALPHA_BLEND ||
            !finite_values(sprites[index].tint, 4u) || !isfinite(sprites[index].alpha_cutoff) ||
            sprites[index].alpha_cutoff < 0.0f || sprites[index].alpha_cutoff > 1.0f)
            return false;
    }

    uint32_t packet_count = model_count + sprite_count;
    VgPacketVisibility *model_visibility = calloc(model_count, sizeof(*model_visibility));
    VgPacketVisibility *sprite_visibility = calloc(sprite_count, sizeof(*sprite_visibility));
    VgTransparentItem *transparent = calloc(packet_count, sizeof(*transparent));
    if ((model_count != 0u && model_visibility == NULL) ||
        (sprite_count != 0u && sprite_visibility == NULL) ||
        (packet_count != 0u && transparent == NULL)) {
        free(transparent);
        free(sprite_visibility);
        free(model_visibility);
        return false;
    }

    renderer->stats.draw_calls = 0u;
    renderer->stats.triangles = 0u;
    renderer->stats.packets_submitted = packet_count;
    renderer->stats.packets_visible = 0u;
    renderer->stats.packets_culled = 0u;
    renderer->stats.error_material_draws = 0u;
    renderer->stats.reversed_winding_draws = 0u;
    float aspect = (float)renderer->width / (float)renderer->height;
    uint32_t transparent_count = 0u;
    for (uint32_t index = 0u; index < model_count; ++index) {
        Matrix world = model_packet_matrix(renderer, &models[index]);
        bool inside =
            bounds_visible(camera, world, models[index].bounds_center, models[index].bounds_extent,
                           aspect, &model_visibility[index].depth);
        model_visibility[index].visible =
            inside || (models[index].flags & VG_GPU_PACKET_DISABLE_CULLING) != 0u;
        if (model_visibility[index].visible) {
            ++renderer->stats.packets_visible;
            if (model_packet_has_pass(renderer, &models[index], VG_MODEL_ALPHA_BLEND))
                transparent[transparent_count++] =
                    (VgTransparentItem){index, VG_TRANSPARENT_MODEL, model_visibility[index].depth};
        } else {
            ++renderer->stats.packets_culled;
        }
    }
    for (uint32_t index = 0u; index < sprite_count; ++index) {
        Matrix world = MatrixTranslate(sprites[index].position.x, sprites[index].position.y,
                                       sprites[index].position.z);
        VgVec3 extent = {sprites[index].size.x * 0.5f, 0.05f, sprites[index].size.y * 0.5f};
        bool inside = bounds_visible(camera, world, (VgVec3){0, 0, 0}, extent, aspect,
                                     &sprite_visibility[index].depth);
        sprite_visibility[index].visible =
            inside || (sprites[index].flags & VG_GPU_PACKET_DISABLE_CULLING) != 0u;
        if (sprite_visibility[index].visible) {
            ++renderer->stats.packets_visible;
            if (sprites[index].material_mode == VG_MODEL_ALPHA_BLEND)
                transparent[transparent_count++] = (VgTransparentItem){
                    index, VG_TRANSPARENT_SPRITE, sprite_visibility[index].depth};
        } else {
            ++renderer->stats.packets_culled;
        }
    }
    sort_transparent_back_to_front(transparent, transparent_count);
    renderer->stats.transparent_packets = transparent_count;
    renderer->stats.transparent_order_hash = 1469598103934665603ull;
    for (uint32_t index = 0u; index < transparent_count; ++index) {
        uint64_t value = ((uint64_t)transparent[index].kind << 32u) | transparent[index].index;
        renderer->stats.transparent_order_hash ^= value;
        renderer->stats.transparent_order_hash *= 1099511628211ull;
    }

    Camera3D native_camera = camera_from_packet(camera);
    int alpha_mode = VG_MODEL_ALPHA_OPAQUE;
    int error_material = 0;
    float alpha_cutoff = 0.5f;
    float emissive[3] = {0.0f, 0.0f, 0.0f};
    SetShaderValue(renderer->material.shader, renderer->alpha_mode_location, &alpha_mode,
                   SHADER_UNIFORM_INT);
    SetShaderValue(renderer->material.shader, renderer->alpha_cutoff_location, &alpha_cutoff,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(renderer->material.shader, renderer->emissive_location, emissive,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(renderer->material.shader, renderer->error_material_location, &error_material,
                   SHADER_UNIFORM_INT);
    BeginTextureMode(renderer->target);
    ClearBackground((Color){12, 18, 26, 255});
    BeginMode3D(native_camera);
    rlEnableDepthTest();
    rlEnableDepthMask();
    draw_model_pass(renderer, models, model_visibility, model_count, VG_MODEL_ALPHA_OPAQUE);
    draw_model_pass(renderer, models, model_visibility, model_count, VG_MODEL_ALPHA_MASK);
    draw_sprite_pass(renderer, native_camera, sprites, sprite_visibility, sprite_count,
                     VG_MODEL_ALPHA_OPAQUE);
    draw_sprite_pass(renderer, native_camera, sprites, sprite_visibility, sprite_count,
                     VG_MODEL_ALPHA_MASK);
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    VgPacketVisibility one_visible = {true, 0.0f};
    for (uint32_t index = 0u; index < transparent_count; ++index) {
        if (transparent[index].kind == VG_TRANSPARENT_MODEL)
            draw_model_pass(renderer, &models[transparent[index].index], &one_visible, 1u,
                            VG_MODEL_ALPHA_BLEND);
        else
            draw_sprite_pass(renderer, native_camera, &sprites[transparent[index].index],
                             &one_visible, 1u, VG_MODEL_ALPHA_BLEND);
    }
    EndBlendMode();
    rlEnableDepthMask();
    EndMode3D();
    EndTextureMode();
    ++renderer->stats.frame_index;
    free(transparent);
    free(sprite_visibility);
    free(model_visibility);
    return true;
}

static VgVec3 quaternion_rotate(VgQuat rotation, VgVec3 value) {
    Vector3 rotated =
        Vector3RotateByQuaternion((Vector3){value.x, value.y, value.z},
                                  (Quaternion){rotation.x, rotation.y, rotation.z, rotation.w});
    return (VgVec3){rotated.x, rotated.y, rotated.z};
}

static bool world_entity_rendered(uint8_t state) {
    return state == VG_ENTITY_ACTIVE || state == VG_ENTITY_PENDING_DESTROY;
}

VgResult vg_gpu_renderer_draw_world(VgGpuRenderer *renderer, VgContext *context, VgWorld world) {
    if (renderer == NULL || !renderer_require_owner(renderer) || !vg_runtime_context_valid(context))
        return VG_ERROR_INVALID_ARGUMENT;
    VgResult result = vg_asset_require_gpu_executor(context, renderer);
    if (result != VG_OK)
        return result;
    VgWorldState *state = NULL;
    result = vg_runtime_resolve_world(context, world, NULL, &state);
    if (result != VG_OK)
        return result;
    result = vg_asset_flush_gpu(context);
    if (result != VG_OK)
        return result;

    uint32_t model_count = 0u;
    uint32_t sprite_count = 0u;
    uint32_t camera_index = UINT32_MAX;
    for (uint32_t index = 0u; index < state->entity_capacity; ++index) {
        const VgEntitySlot *slot = &state->entities[index];
        if (!world_entity_rendered(slot->state))
            continue;
        if (camera_index == UINT32_MAX && slot->has_camera)
            camera_index = index;
        if (slot->has_mesh_renderer)
            ++model_count;
        if (slot->has_sprite_renderer)
            ++sprite_count;
    }
    if (camera_index == UINT32_MAX)
        return VG_ERROR_NOT_FOUND;

    VgGpuModelPacket *models = calloc(model_count, sizeof(*models));
    VgGpuSpritePacket *sprites = calloc(sprite_count, sizeof(*sprites));
    if ((model_count != 0u && models == NULL) || (sprite_count != 0u && sprites == NULL)) {
        free(sprites);
        free(models);
        return VG_ERROR_OUT_OF_MEMORY;
    }

    VgTransform camera_transform;
    VgMatrix camera_matrix;
    result = vg_world_entity_matrix(state, camera_index, UINT32_MAX, NULL, UINT16_MAX, 0u,
                                    &camera_matrix);
    if (result != VG_OK || !vg_matrix_to_transform(camera_matrix, &camera_transform)) {
        free(sprites);
        free(models);
        return result != VG_OK ? result : VG_ERROR_UNSUPPORTED;
    }
    const VgCameraDesc *camera_desc = &state->entities[camera_index].camera;
    VgVec3 forward = quaternion_rotate(camera_transform.rotation, (VgVec3){0.0f, 1.0f, 0.0f});
    VgVec3 up = quaternion_rotate(camera_transform.rotation, (VgVec3){0.0f, 0.0f, 1.0f});
    VgGpuCamera camera = {.position = camera_transform.position,
                          .target = {camera_transform.position.x + forward.x,
                                     camera_transform.position.y + forward.y,
                                     camera_transform.position.z + forward.z},
                          .up = up,
                          .projection = camera_desc->projection,
                          .vertical_fov_radians = camera_desc->vertical_fov_radians,
                          .orthographic_height = camera_desc->orthographic_height,
                          .near_clip_metres = camera_desc->near_clip_metres,
                          .far_clip_metres = camera_desc->far_clip_metres};

    uint32_t model_index = 0u;
    uint32_t sprite_index = 0u;
    for (uint32_t index = 0u; index < state->entity_capacity; ++index) {
        const VgEntitySlot *slot = &state->entities[index];
        if (!world_entity_rendered(slot->state) ||
            (!slot->has_mesh_renderer && !slot->has_sprite_renderer))
            continue;
        VgTransform transform;
        VgMatrix matrix;
        result = vg_world_entity_matrix(state, index, UINT32_MAX, NULL, UINT16_MAX, 0u, &matrix);
        if (result != VG_OK || !vg_matrix_to_transform(matrix, &transform)) {
            free(sprites);
            free(models);
            return result != VG_OK ? result : VG_ERROR_UNSUPPORTED;
        }
        if (slot->has_mesh_renderer) {
            VgAssetGpuObject object = {0};
            uint64_t published_version = 0u;
            (void)vg_asset_component_gpu_object(context, slot->mesh_asset, &object,
                                                &published_version);
            (void)published_version;
            const VgMeshRendererDesc *source = &slot->mesh_renderer;
            models[model_index++] = (VgGpuModelPacket){
                object.token,  source->node_index, source->mesh_index,    source->material_override,
                source->flags, transform,          source->bounds_center, source->bounds_extent};
        }
        if (slot->has_sprite_renderer) {
            VgAssetGpuObject object = {0};
            uint64_t published_version = 0u;
            (void)vg_asset_component_gpu_object(context, slot->sprite_asset, &object,
                                                &published_version);
            (void)published_version;
            const VgSpriteRendererDesc *source = &slot->sprite_renderer;
            VgGpuSpritePacket *packet = &sprites[sprite_index++];
            packet->gpu_token = object.token;
            packet->texture_index = source->texture_index;
            packet->material_mode = source->alpha_mode;
            packet->flags = source->flags;
            packet->position = transform.position;
            packet->size = (VgVec3){source->width_metres * transform.scale.x,
                                    source->height_metres * transform.scale.z, 0.0f};
            memcpy(packet->tint, source->tint, sizeof(packet->tint));
            packet->alpha_cutoff = source->alpha_cutoff;
        }
    }
    bool drawn =
        vg_gpu_renderer_draw_scene(renderer, &camera, models, model_count, sprites, sprite_count);
    free(sprites);
    free(models);
    return drawn ? VG_OK : VG_ERROR_GPU;
}

VgFrameStats vg_gpu_renderer_stats(const VgGpuRenderer *renderer) {
    if (!renderer_is_owner(renderer))
        return (VgFrameStats){0};
    VgFrameStats stats = renderer->stats;
    stats.wrong_thread_calls =
        atomic_load_explicit(&renderer->wrong_thread_calls, memory_order_relaxed);
    return stats;
}
