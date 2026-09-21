#include "assets/import/gltf_import.h"

#include "assets/import/sha256.h"
#include "cgltf.h"

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct ImportSession {
    const VgGltfImporter *importer;
    const VgAssetMemory *memory;
    uint64_t dependency_limit;
    uint64_t dependency_total;
    uint8_t *arena;
    uint64_t arena_capacity;
    uint64_t arena_offset;
    VgResult io_result;
    char io_error[192];
} ImportSession;

typedef struct LoadedImage {
    void *owned;
    const uint8_t *data;
    uint64_t size;
    uint32_t mime;
    uint32_t dependency;
} LoadedImage;

#if defined(__GNUC__) || defined(__clang__)
static void diag_set(VgGltfDiagnostic *diag, uint32_t code, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
#endif
static void diag_set(VgGltfDiagnostic *diag, uint32_t code, const char *format, ...) {
    va_list args;
    if (diag == NULL)
        return;
    diag->code = code;
    va_start(args, format);
    (void)vsnprintf(diag->message, sizeof(diag->message), format, args);
    va_end(args);
}

static void *mem_alloc(const VgAssetMemory *memory, uint64_t size) {
    if (memory == NULL || memory->allocate == NULL)
        return NULL;
    if (size == 0u)
        size = 1u;
    return memory->allocate(memory->user, size);
}

static void mem_free(const VgAssetMemory *memory, void *data) {
    if (data != NULL && memory != NULL && memory->deallocate != NULL) {
        memory->deallocate(memory->user, data);
    }
}

static void *cg_alloc(void *user, cgltf_size size) {
    ImportSession *session = (ImportSession *)user;
    uint64_t aligned;
    if (size == 0u)
        size = 1u;
    if (session->arena_offset > UINT64_MAX - (_Alignof(max_align_t) - 1u))
        return NULL;
    aligned = (session->arena_offset + _Alignof(max_align_t) - 1u) &
              ~((uint64_t)_Alignof(max_align_t) - 1u);
    if ((uint64_t)size > session->arena_capacity ||
        aligned > session->arena_capacity - (uint64_t)size)
        return NULL;
    session->arena_offset = aligned + (uint64_t)size;
    return session->arena + aligned;
}

static void cg_free(void *user, void *data) {
    (void)user;
    (void)data;
}

static int path_is_safe(const char *path) {
    const char *segment = path;
    const char *cursor;
    if (path == NULL || path[0] == '\0' || path[0] == '/' || path[0] == '\\' ||
        (path[0] != '\0' && path[1] == ':') || strstr(path, "://") != NULL)
        return 0;
    if (strchr(path, '%') != NULL || strchr(path, '?') != NULL || strchr(path, '#') != NULL ||
        strchr(path, ':') != NULL)
        return 0;
    for (cursor = path;; ++cursor) {
        if (*cursor == '\\')
            return 0;
        if (*cursor == '/' || *cursor == '\0') {
            const size_t length = (size_t)(cursor - segment);
            if (length == 0u || (length == 1u && segment[0] == '.') ||
                (length == 2u && segment[0] == '.' && segment[1] == '.'))
                return 0;
            if (*cursor == '\0')
                break;
            segment = cursor + 1;
        }
    }
    return 1;
}

static cgltf_result cg_read(const cgltf_memory_options *memory_options,
                            const cgltf_file_options *file_options, const char *path,
                            cgltf_size *size, void **data) {
    ImportSession *session = (ImportSession *)file_options->user_data;
    uint64_t bytes = 0u;
    VgResult result;
    (void)memory_options;
    *data = NULL;
    if (session->importer == NULL || session->importer->io.read == NULL || !path_is_safe(path)) {
        session->io_result = VG_ERROR_IO;
        (void)snprintf(session->io_error, sizeof(session->io_error),
                       "unsafe or unavailable dependency: %s", path);
        return cgltf_result_io_error;
    }
    result = session->importer->io.read(session->importer->io.user, session->memory, path, data,
                                        &bytes, session->io_error, sizeof(session->io_error));
    session->io_result = result;
    if (result != VG_OK || *data == NULL || bytes > session->dependency_limit ||
        session->dependency_total > session->dependency_limit - bytes || bytes > SIZE_MAX) {
        if (*data != NULL)
            mem_free(session->memory, *data);
        *data = NULL;
        if (bytes > session->dependency_limit ||
            session->dependency_total > session->dependency_limit - bytes)
            session->io_result = VG_ERROR_CAPACITY;
        return result == VG_ERROR_OUT_OF_MEMORY ? cgltf_result_out_of_memory
                                                : cgltf_result_io_error;
    }
    session->dependency_total += bytes;
    *size = (cgltf_size)bytes;
    return cgltf_result_success;
}

static void cg_release(const cgltf_memory_options *memory_options,
                       const cgltf_file_options *file_options, void *data, cgltf_size size) {
    ImportSession *session = (ImportSession *)file_options->user_data;
    (void)memory_options;
    (void)size;
    mem_free(session->memory, data);
}

static int add_u64(uint64_t *value, uint64_t add) {
    if (*value > UINT64_MAX - add)
        return 0;
    *value += add;
    return 1;
}

static int add_string_size(uint64_t *total, const char *value) {
    const size_t length = value != NULL ? strlen(value) : 0u;
    return length != SIZE_MAX && add_u64(total, (uint64_t)length + 1u);
}

static uint32_t read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static VgResult parser_json_size(const void *source_data, uint64_t source_size,
                                 uint64_t *out_json_size, VgGltfDiagnostic *diag) {
    const uint8_t *bytes = (const uint8_t *)source_data;
    if (source_size >= 4u && memcmp(bytes, "glTF", 4u) == 0) {
        uint32_t version, total_length, json_length, chunk_type;
        if (source_size < 20u) {
            diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED, "GLB header is truncated");
            return VG_ERROR_INVALID_ARGUMENT;
        }
        version = read_u32_le(bytes + 4u);
        total_length = read_u32_le(bytes + 8u);
        json_length = read_u32_le(bytes + 12u);
        chunk_type = read_u32_le(bytes + 16u);
        if (version != 2u) {
            diag_set(diag, VG_GLTF_DIAGNOSTIC_UNSUPPORTED_VERSION, "only GLB 2.0 is supported");
            return VG_ERROR_FORMAT_VERSION;
        }
        if ((uint64_t)total_length != source_size || chunk_type != 0x4e4f534au ||
            (uint64_t)json_length > source_size - 20u) {
            diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED,
                     "GLB length or first JSON chunk is invalid");
            return VG_ERROR_INVALID_ARGUMENT;
        }
        *out_json_size = json_length;
        return VG_OK;
    }
    *out_json_size = source_size;
    return VG_OK;
}

static int align_add(uint64_t *offset, uint64_t alignment, uint64_t count, uint64_t item_size,
                     uint32_t *result) {
    uint64_t aligned;
    if (count != 0u && item_size > UINT64_MAX / count)
        return 0;
    aligned = (*offset + alignment - 1u) & ~(alignment - 1u);
    if (aligned > UINT32_MAX || aligned > UINT64_MAX - count * item_size)
        return 0;
    *result = (uint32_t)aligned;
    *offset = aligned + count * item_size;
    return 1;
}

static uint32_t ptr_index(const void *pointer, const void *base, size_t size) {
    ptrdiff_t byte_offset;
    if (pointer == NULL)
        return VG_MODEL_NO_INDEX;
    byte_offset = (const uint8_t *)pointer - (const uint8_t *)base;
    if (byte_offset < 0 || size == 0u || (size_t)byte_offset / size > UINT32_MAX)
        return VG_MODEL_NO_INDEX;
    return (uint32_t)((size_t)byte_offset / size);
}

static cgltf_accessor *find_attribute(const cgltf_primitive *primitive, cgltf_attribute_type type,
                                      int index) {
    cgltf_size i;
    for (i = 0; i < primitive->attributes_count; ++i) {
        if (primitive->attributes[i].type == type && primitive->attributes[i].index == index)
            return primitive->attributes[i].data;
    }
    return NULL;
}

static int required_extensions_supported(const cgltf_data *data) {
    cgltf_size i;
    for (i = 0; i < data->extensions_required_count; ++i) {
        const char *name = data->extensions_required[i];
        if (strcmp(name, "KHR_materials_unlit") != 0)
            return 0;
    }
    return 1;
}

static void multiply4(const float a[16], const float b[16], float out[16]) {
    unsigned column, row, k;
    float temp[16];
    for (column = 0; column < 4u; ++column)
        for (row = 0; row < 4u; ++row) {
            float value = 0.0f;
            for (k = 0; k < 4u; ++k)
                value += a[k * 4u + row] * b[column * 4u + k];
            temp[column * 4u + row] = value;
        }
    memcpy(out, temp, sizeof(temp));
}

static void convert_matrix(const float source[16], float out[16], float scale) {
    static const float basis[16] = {1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1};
    static const float inverse[16] = {1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
    float temp[16];
    multiply4(basis, source, temp);
    multiply4(temp, inverse, out);
    out[12] *= scale;
    out[13] *= scale;
    out[14] *= scale;
}

static int32_t determinant_sign(const float matrix[16]) {
    const float determinant = matrix[0] * (matrix[5] * matrix[10] - matrix[9] * matrix[6]) -
                              matrix[4] * (matrix[1] * matrix[10] - matrix[9] * matrix[2]) +
                              matrix[8] * (matrix[1] * matrix[6] - matrix[5] * matrix[2]);
    return determinant < 0.0f ? -1 : (determinant > 0.0f ? 1 : 0);
}

static void convert_vector(const float source[3], float out[3], float scale) {
    out[0] = source[0] * scale;
    out[1] = -source[2] * scale;
    out[2] = source[1] * scale;
}

static void normalize3(float value[3]) {
    const float length = sqrtf(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    if (length > 1.0e-20f) {
        value[0] /= length;
        value[1] /= length;
        value[2] /= length;
    }
}

static int data_uri(const char *uri) {
    return uri != NULL && strncmp(uri, "data:", 5u) == 0;
}

static int decode64(const char *uri, const VgAssetMemory *memory, void **out, uint64_t *out_size) {
    const char *comma = strchr(uri, ',');
    const char *p;
    uint64_t capacity, written = 0u;
    uint8_t *bytes;
    unsigned accumulator = 0u, bits = 0u;
    size_t encoded_size;
    if (comma == NULL || strstr(uri, ";base64,") == NULL)
        return 0;
    encoded_size = strlen(comma + 1u);
    if (encoded_size == 0u || encoded_size % 4u != 0u)
        return 0;
    if (encoded_size > (SIZE_MAX - 3u) / 3u * 4u)
        return 0;
    capacity = (uint64_t)(encoded_size / 4u) * 3u;
    bytes = (uint8_t *)mem_alloc(memory, capacity);
    if (bytes == NULL)
        return -1;
    for (p = comma + 1u; *p != '\0' && *p != '='; ++p) {
        unsigned value;
        if (*p >= 'A' && *p <= 'Z')
            value = (unsigned)(*p - 'A');
        else if (*p >= 'a' && *p <= 'z')
            value = (unsigned)(*p - 'a') + 26u;
        else if (*p >= '0' && *p <= '9')
            value = (unsigned)(*p - '0') + 52u;
        else if (*p == '+')
            value = 62u;
        else if (*p == '/')
            value = 63u;
        else {
            mem_free(memory, bytes);
            return 0;
        }
        accumulator = (accumulator << 6u) | value;
        bits += 6u;
        if (bits >= 8u) {
            bits -= 8u;
            bytes[written++] = (uint8_t)(accumulator >> bits);
        }
    }
    if (bits != 0u && bits != 2u && bits != 4u) {
        mem_free(memory, bytes);
        return 0;
    }
    if (*p == '=' && p[1] != '\0' && !(p[1] == '=' && p[2] == '\0')) {
        mem_free(memory, bytes);
        return 0;
    }
    *out = bytes;
    *out_size = written;
    return 1;
}

static uint32_t mime_type(const cgltf_image *image) {
    const char *mime = image->mime_type;
    if (mime == NULL && image->uri != NULL)
        mime = image->uri;
    if (mime != NULL && strstr(mime, "image/png") != NULL)
        return VG_MODEL_IMAGE_PNG;
    if (mime != NULL && (strstr(mime, "image/jpeg") != NULL || strstr(mime, "image/jpg") != NULL))
        return VG_MODEL_IMAGE_JPEG;
    if (mime != NULL) {
        const size_t length = strlen(mime);
        if (length >= 4u && strcmp(mime + length - 4u, ".png") == 0)
            return VG_MODEL_IMAGE_PNG;
        if (length >= 4u && strcmp(mime + length - 4u, ".jpg") == 0)
            return VG_MODEL_IMAGE_JPEG;
        if (length >= 5u && strcmp(mime + length - 5u, ".jpeg") == 0)
            return VG_MODEL_IMAGE_JPEG;
    }
    return VG_MODEL_IMAGE_UNKNOWN;
}

static int join_relative(const char *source_path, const char *uri, char *out, size_t capacity) {
    const char *slash;
    size_t prefix = 0u, uri_size;
    if (!path_is_safe(uri))
        return 0;
    if (source_path != NULL) {
        slash = strrchr(source_path, '/');
        if (slash != NULL)
            prefix = (size_t)(slash - source_path) + 1u;
    }
    uri_size = strlen(uri);
    if (prefix + uri_size + 1u > capacity)
        return 0;
    if (prefix != 0u)
        memcpy(out, source_path, prefix);
    memcpy(out + prefix, uri, uri_size + 1u);
    return path_is_safe(out);
}

void vg_gltf_default_options(VgGltfImportOptions *options) {
    if (options == NULL)
        return;
    memset(options, 0, sizeof(*options));
    options->struct_size = sizeof(*options);
    options->api_version = VG_GLTF_IMPORTER_VERSION;
    options->uniform_scale = 1.0f;
    options->max_nodes = 65536u;
    options->max_meshes = 16384u;
    options->max_primitives = 65536u;
    options->max_vertices = 16000000u;
    options->max_indices = 48000000u;
    options->max_materials = 16384u;
    options->max_textures = 16384u;
    options->max_images = 16384u;
    options->max_source_bytes = 256u * 1024u * 1024u;
    options->max_dependency_bytes = 512u * 1024u * 1024u;
    options->max_working_bytes = 1024ull * 1024ull * 1024ull;
    options->max_output_bytes = 1024ull * 1024ull * 1024ull;
}

static VgResult validate_and_count(const cgltf_data *data, const VgGltfImportOptions *limits,
                                   uint64_t *primitive_count, uint64_t *vertex_count,
                                   uint64_t *index_count, VgGltfDiagnostic *diag) {
    cgltf_size m, p, n;
    if (strcmp(data->asset.version != NULL ? data->asset.version : "", "2.0") != 0) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_UNSUPPORTED_VERSION, "only glTF 2.0 is supported");
        return VG_ERROR_FORMAT_VERSION;
    }
    if (!required_extensions_supported(data)) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_UNSUPPORTED_FEATURE, "unsupported required extension");
        return VG_ERROR_UNSUPPORTED;
    }
    if (data->skins_count != 0u || data->animations_count != 0u) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_UNSUPPORTED_FEATURE,
                 "skins and animations are not static-model input");
        return VG_ERROR_UNSUPPORTED;
    }
    if (data->nodes_count > limits->max_nodes || data->meshes_count > limits->max_meshes ||
        data->materials_count > limits->max_materials ||
        data->textures_count > limits->max_textures || data->images_count > limits->max_images)
        goto capacity;
    for (m = 0; m < data->materials_count; ++m) {
        if (data->materials[m].has_pbr_metallic_roughness &&
            data->materials[m].pbr_metallic_roughness.base_color_texture.texture != NULL &&
            data->materials[m].pbr_metallic_roughness.base_color_texture.texcoord != 0) {
            diag_set(diag, VG_GLTF_DIAGNOSTIC_UNSUPPORTED_FEATURE,
                     "only base-color TEXCOORD_0 is supported");
            return VG_ERROR_UNSUPPORTED;
        }
    }
    for (n = 0; n < data->nodes_count; ++n) {
        const cgltf_node *cursor = &data->nodes[n];
        cgltf_size depth = 0u;
        while (cursor != NULL && depth <= data->nodes_count) {
            cursor = cursor->parent;
            ++depth;
        }
        if (cursor != NULL) {
            diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED, "node hierarchy contains a cycle");
            return VG_ERROR_INVALID_ARGUMENT;
        }
    }
    for (m = 0; m < data->meshes_count; ++m)
        for (p = 0; p < data->meshes[m].primitives_count; ++p) {
            const cgltf_primitive *primitive = &data->meshes[m].primitives[p];
            cgltf_accessor *position = find_attribute(primitive, cgltf_attribute_type_position, 0);
            cgltf_accessor *normal = find_attribute(primitive, cgltf_attribute_type_normal, 0);
            cgltf_accessor *uv = find_attribute(primitive, cgltf_attribute_type_texcoord, 0);
            cgltf_accessor *color = find_attribute(primitive, cgltf_attribute_type_color, 0);
            const uint64_t indices = primitive->indices != NULL
                                         ? primitive->indices->count
                                         : (position != NULL ? position->count : 0u);
            if (primitive->type != cgltf_primitive_type_triangles ||
                primitive->targets_count != 0u || primitive->has_draco_mesh_compression ||
                position == NULL || position->type != cgltf_type_vec3 || position->is_sparse ||
                indices % 3u != 0u) {
                diag_set(diag, VG_GLTF_DIAGNOSTIC_UNSUPPORTED_FEATURE,
                         "primitive must be non-sparse TRIANGLES with POSITION");
                return VG_ERROR_UNSUPPORTED;
            }
            if ((normal != NULL && (normal->type != cgltf_type_vec3 ||
                                    normal->count != position->count || normal->is_sparse)) ||
                (uv != NULL &&
                 (uv->type != cgltf_type_vec2 || uv->count != position->count || uv->is_sparse)) ||
                (color != NULL &&
                 ((color->type != cgltf_type_vec3 && color->type != cgltf_type_vec4) ||
                  color->count != position->count || color->is_sparse)) ||
                (primitive->indices != NULL && (primitive->indices->type != cgltf_type_scalar ||
                                                primitive->indices->is_sparse))) {
                diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED,
                         "attribute or index accessor has incompatible type/count");
                return VG_ERROR_INVALID_ARGUMENT;
            }
            if (!add_u64(primitive_count, 1u) ||
                !add_u64(vertex_count, normal != NULL ? position->count : indices) ||
                !add_u64(index_count, indices))
                goto capacity;
        }
    if (*primitive_count > limits->max_primitives || *vertex_count > limits->max_vertices ||
        *index_count > limits->max_indices)
        goto capacity;
    return VG_OK;
capacity:
    diag_set(diag, VG_GLTF_DIAGNOSTIC_LIMIT, "model exceeds configured import limits");
    return VG_ERROR_CAPACITY;
}

static uint32_t copy_string(char *strings, uint32_t *cursor, const char *value) {
    uint32_t offset = *cursor;
    const size_t length = value != NULL ? strlen(value) : 0u;
    memcpy(strings + *cursor, value != NULL ? value : "", length + 1u);
    *cursor += (uint32_t)length + 1u;
    return offset;
}

static void hash_u32(VgSha256 *hash, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u), (uint8_t)(value >> 16u),
                        (uint8_t)(value >> 24u)};
    vg_sha256_update(hash, bytes, sizeof(bytes));
}

static void hash_u64(VgSha256 *hash, uint64_t value) {
    uint8_t bytes[8];
    unsigned i;
    for (i = 0; i < 8u; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8u));
    vg_sha256_update(hash, bytes, sizeof(bytes));
}

static void hash_string(VgSha256 *hash, const char *value) {
    const uint64_t length = (uint64_t)strlen(value);
    hash_u64(hash, length);
    vg_sha256_update(hash, value, (size_t)length);
}

static void hash_options(VgSha256 *hash, const VgGltfImportOptions *options) {
    uint32_t scale_bits;
    memcpy(&scale_bits, &options->uniform_scale, sizeof(scale_bits));
    hash_u32(hash, options->api_version);
    hash_u32(hash, scale_bits);
    hash_u32(hash, options->max_nodes);
    hash_u32(hash, options->max_meshes);
    hash_u32(hash, options->max_primitives);
    hash_u32(hash, options->max_vertices);
    hash_u32(hash, options->max_indices);
    hash_u32(hash, options->max_materials);
    hash_u32(hash, options->max_textures);
    hash_u32(hash, options->max_images);
    hash_u64(hash, options->max_source_bytes);
    hash_u64(hash, options->max_dependency_bytes);
    hash_u64(hash, options->max_working_bytes);
    hash_u64(hash, options->max_output_bytes);
}

static VgResult import_impl(const VgGltfImporter *importer, const VgAssetMemory *memory,
                            const char *source_path, const void *source_data, uint64_t source_size,
                            const VgGltfImportOptions *limits, VgStaticModelIr **out_model,
                            uint64_t *out_bytes, VgGltfDiagnostic *diag) {
    ImportSession session;
    cgltf_options cg_options;
    cgltf_data *data = NULL;
    cgltf_result cg_result;
    uint64_t primitive_count = 0u, vertex_count = 0u, index_count = 0u, layout;
    uint64_t strings_size = 1u, image_bytes = 0u;
    uint32_t dep_count = 0u, external_buffer_count = 0u, external_image_count = 0u,
             strings_cursor = 1u;
    LoadedImage *loaded = NULL;
    VgStaticModelIr *model = NULL;
    VgResult result = VG_ERROR_INVALID_ARGUMENT;
    cgltf_size i, m, p;
    VgSha256 fingerprint;
    uint64_t json_size;
    if (source_size > limits->max_source_bytes || source_size > SIZE_MAX) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_LIMIT, "source exceeds configured size limit");
        return VG_ERROR_CAPACITY;
    }
    result = parser_json_size(source_data, source_size, &json_size, diag);
    if (result != VG_OK)
        return result;
    memset(&session, 0, sizeof(session));
    session.importer = importer;
    session.memory = memory;
    session.dependency_limit = limits->max_dependency_bytes;
    session.io_result = VG_OK;
    if (limits->max_working_bytes < 65536u ||
        json_size > (limits->max_working_bytes - 65536u) / 32u) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_LIMIT,
                 "source exceeds bounded parser working-memory budget");
        return VG_ERROR_CAPACITY;
    }
    session.arena_capacity = json_size * 32u + 65536u;
    session.arena = (uint8_t *)mem_alloc(memory, session.arena_capacity);
    if (session.arena == NULL) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_OUT_OF_MEMORY,
                 "out of memory reserving parser workspace");
        return VG_ERROR_OUT_OF_MEMORY;
    }
    memset(&cg_options, 0, sizeof(cg_options));
    cg_options.memory.alloc_func = cg_alloc;
    cg_options.memory.free_func = cg_free;
    cg_options.memory.user_data = &session;
    cg_options.file.read = cg_read;
    cg_options.file.release = cg_release;
    cg_options.file.user_data = &session;
    cg_result = cgltf_parse(&cg_options, source_data, (cgltf_size)source_size, &data);
    if (cg_result != cgltf_result_success)
        goto cg_failure;
    result = validate_and_count(data, limits, &primitive_count, &vertex_count, &index_count, diag);
    if (result != VG_OK)
        goto cleanup;
    if (cgltf_validate(data) != cgltf_result_success) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED, "cgltf validation rejected the document");
        result = VG_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    for (i = 0; i < data->buffers_count; ++i)
        if (data->buffers[i].uri != NULL && !data_uri(data->buffers[i].uri) &&
            !path_is_safe(data->buffers[i].uri)) {
            diag_set(diag, VG_GLTF_DIAGNOSTIC_IO, "unsafe buffer URI");
            result = VG_ERROR_IO;
            goto cleanup;
        }
    cg_result =
        cgltf_load_buffers(&cg_options, data, source_path != NULL ? source_path : "model.gltf");
    if (cg_result != cgltf_result_success)
        goto cg_failure;
    if (data->images_count != 0u) {
        loaded = (LoadedImage *)mem_alloc(memory, data->images_count * sizeof(*loaded));
        if (loaded == NULL) {
            result = VG_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
        memset(loaded, 0, data->images_count * sizeof(*loaded));
    }
    for (i = 0; i < data->images_count; ++i) {
        cgltf_image *image = &data->images[i];
        loaded[i].mime = mime_type(image);
        loaded[i].dependency = VG_MODEL_NO_INDEX;
        if (loaded[i].mime == VG_MODEL_IMAGE_UNKNOWN) {
            diag_set(diag, VG_GLTF_DIAGNOSTIC_UNSUPPORTED_FEATURE,
                     "only PNG and JPEG images are supported");
            result = VG_ERROR_UNSUPPORTED;
            goto cleanup;
        }
        if (image->buffer_view != NULL) {
            cgltf_buffer_view *view = image->buffer_view;
            if (view->buffer == NULL || view->buffer->data == NULL) {
                result = VG_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            loaded[i].data = (const uint8_t *)view->buffer->data + view->offset;
            loaded[i].size = view->size;
        } else if (data_uri(image->uri)) {
            int decoded = decode64(image->uri, memory, &loaded[i].owned, &loaded[i].size);
            if (decoded <= 0) {
                result = decoded < 0 ? VG_ERROR_OUT_OF_MEMORY : VG_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            loaded[i].data = (const uint8_t *)loaded[i].owned;
        } else {
            if (!path_is_safe(image->uri) || importer == NULL || importer->io.read == NULL) {
                result = VG_ERROR_IO;
                goto cleanup;
            }
            char resolved[1024];
            if (!join_relative(source_path, image->uri, resolved, sizeof(resolved))) {
                result = VG_ERROR_IO;
                goto cleanup;
            }
            result = importer->io.read(importer->io.user, memory, resolved, &loaded[i].owned,
                                       &loaded[i].size, diag != NULL ? diag->message : NULL,
                                       diag != NULL ? (uint32_t)sizeof(diag->message) : 0u);
            if (result != VG_OK || loaded[i].owned == NULL ||
                loaded[i].size > limits->max_dependency_bytes ||
                session.dependency_total > limits->max_dependency_bytes - loaded[i].size) {
                if (result == VG_OK)
                    result = VG_ERROR_CAPACITY;
                goto cleanup;
            }
            session.dependency_total += loaded[i].size;
            loaded[i].data = (const uint8_t *)loaded[i].owned;
            loaded[i].dependency = external_image_count++;
        }
        if (!add_u64(&image_bytes, loaded[i].size)) {
            result = VG_ERROR_CAPACITY;
            goto cleanup;
        }
    }
    for (i = 0; i < data->buffers_count; ++i)
        if (data->buffers[i].uri != NULL && !data_uri(data->buffers[i].uri))
            ++external_buffer_count;
    dep_count = external_buffer_count + external_image_count;
    for (i = 0; i < data->images_count; ++i)
        if (loaded[i].dependency != VG_MODEL_NO_INDEX)
            loaded[i].dependency += external_buffer_count;
    for (i = 0; i < data->nodes_count; ++i)
        if (!add_string_size(&strings_size, data->nodes[i].name))
            goto layout_fail;
    for (i = 0; i < data->meshes_count; ++i)
        if (!add_string_size(&strings_size, data->meshes[i].name))
            goto layout_fail;
    for (i = 0; i < data->materials_count; ++i)
        if (!add_string_size(&strings_size, data->materials[i].name))
            goto layout_fail;
    for (i = 0; i < data->images_count; ++i)
        if (!add_string_size(&strings_size, data->images[i].name))
            goto layout_fail;
    for (i = 0; i < data->buffers_count; ++i)
        if (data->buffers[i].uri != NULL && !data_uri(data->buffers[i].uri))
            if (!add_string_size(&strings_size, data->buffers[i].uri))
                goto layout_fail;
    for (i = 0; i < data->images_count; ++i)
        if (loaded[i].dependency != VG_MODEL_NO_INDEX)
            if (!add_string_size(&strings_size, data->images[i].uri))
                goto layout_fail;
    layout = sizeof(*model);
#define ADD_LAYOUT(field, type, count)                                                             \
    if (!align_add(&layout, _Alignof(type), (count), sizeof(type), &model_offsets.field))          \
    goto layout_fail
    {
        VgStaticModelIr model_offsets;
        memset(&model_offsets, 0, sizeof(model_offsets));
        ADD_LAYOUT(nodes_offset, VgModelNodeIr, data->nodes_count);
        ADD_LAYOUT(meshes_offset, VgModelMeshIr, data->meshes_count);
        ADD_LAYOUT(primitives_offset, VgModelPrimitiveIr, primitive_count);
        ADD_LAYOUT(vertices_offset, VgModelVertexIr, vertex_count);
        ADD_LAYOUT(indices_offset, uint32_t, index_count);
        ADD_LAYOUT(materials_offset, VgModelMaterialIr, data->materials_count);
        ADD_LAYOUT(textures_offset, VgModelTextureIr, data->textures_count);
        ADD_LAYOUT(images_offset, VgModelImageIr, data->images_count);
        ADD_LAYOUT(dependencies_offset, VgModelDependencyIr, dep_count);
        ADD_LAYOUT(strings_offset, char, strings_size);
        ADD_LAYOUT(image_data_offset, uint8_t, image_bytes);
        if (layout > limits->max_output_bytes || layout > UINT32_MAX)
            goto layout_fail;
        model = (VgStaticModelIr *)mem_alloc(memory, layout);
        if (model == NULL) {
            result = VG_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
        memset(model, 0, layout);
        *model = model_offsets;
    }
#undef ADD_LAYOUT
    model->magic = VG_MODEL_IR_MAGIC;
    model->version = VG_MODEL_IR_VERSION;
    model->importer_version = VG_GLTF_IMPORTER_VERSION;
    model->total_bytes = layout;
    model->node_count = (uint32_t)data->nodes_count;
    model->mesh_count = (uint32_t)data->meshes_count;
    model->primitive_count = (uint32_t)primitive_count;
    model->vertex_count = (uint32_t)vertex_count;
    model->index_count = (uint32_t)index_count;
    model->material_count = (uint32_t)data->materials_count;
    model->texture_count = (uint32_t)data->textures_count;
    model->image_count = (uint32_t)data->images_count;
    model->dependency_count = dep_count;
    model->strings_size = (uint32_t)strings_size;
    model->image_data_size = (uint32_t)image_bytes;
    {
        VgModelNodeIr *nodes = VG_MODEL_IR_ARRAY(model, VgModelNodeIr, nodes);
        VgModelMeshIr *meshes = VG_MODEL_IR_ARRAY(model, VgModelMeshIr, meshes);
        VgModelPrimitiveIr *primitives = VG_MODEL_IR_ARRAY(model, VgModelPrimitiveIr, primitives);
        VgModelVertexIr *vertices = VG_MODEL_IR_ARRAY(model, VgModelVertexIr, vertices);
        uint32_t *indices = VG_MODEL_IR_ARRAY(model, uint32_t, indices);
        VgModelMaterialIr *materials = VG_MODEL_IR_ARRAY(model, VgModelMaterialIr, materials);
        VgModelTextureIr *textures = VG_MODEL_IR_ARRAY(model, VgModelTextureIr, textures);
        VgModelImageIr *images = VG_MODEL_IR_ARRAY(model, VgModelImageIr, images);
        VgModelDependencyIr *dependencies =
            VG_MODEL_IR_ARRAY(model, VgModelDependencyIr, dependencies);
        char *strings = VG_MODEL_IR_ARRAY(model, char, strings);
        uint8_t *image_data = VG_MODEL_IR_ARRAY(model, uint8_t, image_data);
        uint32_t primitive_cursor = 0u, vertex_cursor = 0u, index_cursor = 0u, image_cursor = 0u,
                 dependency_cursor = 0u;
        strings[0] = '\0';
        model->geometry_bounds_min[0] = model->geometry_bounds_min[1] =
            model->geometry_bounds_min[2] = FLT_MAX;
        model->geometry_bounds_max[0] = model->geometry_bounds_max[1] =
            model->geometry_bounds_max[2] = -FLT_MAX;
        for (i = 0; i < data->nodes_count; ++i) {
            float local[16];
            cgltf_node_transform_local(&data->nodes[i], local);
            nodes[i].name_offset = copy_string(strings, &strings_cursor, data->nodes[i].name);
            nodes[i].parent = ptr_index(data->nodes[i].parent, data->nodes, sizeof(*data->nodes));
            nodes[i].mesh = ptr_index(data->nodes[i].mesh, data->meshes, sizeof(*data->meshes));
            convert_matrix(local, nodes[i].local_transform, limits->uniform_scale);
            {
                unsigned component;
                for (component = 0; component < 16u; ++component)
                    if (!isfinite(nodes[i].local_transform[component])) {
                        diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED,
                                 "node transform contains a non-finite value");
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
            }
            nodes[i].determinant_sign = determinant_sign(nodes[i].local_transform);
            nodes[i].scene_root = 0u;
            if (data->scene != NULL) {
                cgltf_size root_index;
                for (root_index = 0; root_index < data->scene->nodes_count; ++root_index)
                    if (data->scene->nodes[root_index] == &data->nodes[i])
                        nodes[i].scene_root = 1u;
            }
        }
        for (m = 0; m < data->meshes_count; ++m) {
            meshes[m].name_offset = copy_string(strings, &strings_cursor, data->meshes[m].name);
            meshes[m].first_primitive = primitive_cursor;
            meshes[m].primitive_count = (uint32_t)data->meshes[m].primitives_count;
            for (p = 0; p < data->meshes[m].primitives_count; ++p, ++primitive_cursor) {
                cgltf_primitive *source = &data->meshes[m].primitives[p];
                cgltf_accessor *position = find_attribute(source, cgltf_attribute_type_position, 0);
                cgltf_accessor *normal = find_attribute(source, cgltf_attribute_type_normal, 0);
                cgltf_accessor *uv = find_attribute(source, cgltf_attribute_type_texcoord, 0);
                cgltf_accessor *color = find_attribute(source, cgltf_attribute_type_color, 0);
                uint32_t v, j;
                VgModelPrimitiveIr *target = &primitives[primitive_cursor];
                target->first_vertex = vertex_cursor;
                target->vertex_count =
                    (uint32_t)(normal != NULL
                                   ? position->count
                                   : (source->indices ? source->indices->count : position->count));
                target->first_index = index_cursor;
                target->index_count =
                    (uint32_t)(source->indices ? source->indices->count : position->count);
                target->material =
                    ptr_index(source->material, data->materials, sizeof(*data->materials));
                for (j = 0; j < 3u; ++j) {
                    target->bounds_min[j] = FLT_MAX;
                    target->bounds_max[j] = -FLT_MAX;
                }
                for (v = 0; v < target->vertex_count; ++v) {
                    uint64_t source_vertex =
                        normal != NULL ? v
                                       : (source->indices != NULL
                                              ? cgltf_accessor_read_index(source->indices, v)
                                              : v);
                    float value[4] = {0, 0, 0, 1};
                    VgModelVertexIr *vertex = &vertices[vertex_cursor + v];
                    if (source_vertex >= position->count ||
                        !cgltf_accessor_read_float(position, source_vertex, value, 3u)) {
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    convert_vector(value, vertex->position, limits->uniform_scale);
                    if (!isfinite(vertex->position[0]) || !isfinite(vertex->position[1]) ||
                        !isfinite(vertex->position[2])) {
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    if (normal != NULL) {
                        if (!cgltf_accessor_read_float(normal, source_vertex, value, 3u)) {
                            result = VG_ERROR_INVALID_ARGUMENT;
                            goto cleanup;
                        }
                        convert_vector(value, vertex->normal, 1.0f);
                        normalize3(vertex->normal);
                    }
                    if (uv != NULL &&
                        !cgltf_accessor_read_float(uv, source_vertex, vertex->texcoord, 2u)) {
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    vertex->color[0] = vertex->color[1] = vertex->color[2] = vertex->color[3] =
                        1.0f;
                    if (color != NULL &&
                        !cgltf_accessor_read_float(color, source_vertex, vertex->color,
                                                   color->type == cgltf_type_vec3 ? 3u : 4u)) {
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    for (j = 0; j < 3u; ++j)
                        if (!isfinite(vertex->normal[j]) || !isfinite(vertex->color[j])) {
                            result = VG_ERROR_INVALID_ARGUMENT;
                            goto cleanup;
                        }
                    if (!isfinite(vertex->texcoord[0]) || !isfinite(vertex->texcoord[1]) ||
                        !isfinite(vertex->color[3])) {
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    for (j = 0; j < 3u; ++j) {
                        if (vertex->position[j] < target->bounds_min[j])
                            target->bounds_min[j] = vertex->position[j];
                        if (vertex->position[j] > target->bounds_max[j])
                            target->bounds_max[j] = vertex->position[j];
                        if (vertex->position[j] < model->geometry_bounds_min[j])
                            model->geometry_bounds_min[j] = vertex->position[j];
                        if (vertex->position[j] > model->geometry_bounds_max[j])
                            model->geometry_bounds_max[j] = vertex->position[j];
                    }
                }
                for (v = 0; v < target->index_count; ++v) {
                    uint64_t value =
                        normal == NULL
                            ? v
                            : (source->indices ? cgltf_accessor_read_index(source->indices, v) : v);
                    if (value >= target->vertex_count) {
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    indices[index_cursor + v] = (uint32_t)value;
                }
                if (normal == NULL)
                    for (v = 0; v < target->index_count; v += 3u) {
                        VgModelVertexIr
                            *a = &vertices[vertex_cursor + indices[index_cursor + v]],
                            *b = &vertices[vertex_cursor + indices[index_cursor + v + 1u]],
                            *c = &vertices[vertex_cursor + indices[index_cursor + v + 2u]];
                        float ab[3] = {b->position[0] - a->position[0],
                                       b->position[1] - a->position[1],
                                       b->position[2] - a->position[2]};
                        float ac[3] = {c->position[0] - a->position[0],
                                       c->position[1] - a->position[1],
                                       c->position[2] - a->position[2]};
                        float n[3] = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
                                      ab[0] * ac[1] - ab[1] * ac[0]};
                        if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2] <= 1.0e-30f) {
                            result = VG_ERROR_INVALID_ARGUMENT;
                            goto cleanup;
                        }
                        for (j = 0; j < 3u; ++j) {
                            a->normal[j] += n[j];
                            b->normal[j] += n[j];
                            c->normal[j] += n[j];
                        }
                    }
                if (normal == NULL)
                    for (v = 0; v < target->vertex_count; ++v) {
                        normalize3(vertices[vertex_cursor + v].normal);
                        if (!isfinite(vertices[vertex_cursor + v].normal[0]) ||
                            !isfinite(vertices[vertex_cursor + v].normal[1]) ||
                            !isfinite(vertices[vertex_cursor + v].normal[2])) {
                            diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED,
                                     "generated normal is non-finite");
                            result = VG_ERROR_INVALID_ARGUMENT;
                            goto cleanup;
                        }
                    }
                vertex_cursor += target->vertex_count;
                index_cursor += target->index_count;
            }
        }
        for (i = 0; i < data->materials_count; ++i) {
            cgltf_material *source = &data->materials[i];
            VgModelMaterialIr *target = &materials[i];
            target->name_offset = copy_string(strings, &strings_cursor, source->name);
            target->base_color_texture = VG_MODEL_NO_INDEX;
            target->alpha_mode = (uint32_t)source->alpha_mode;
            target->double_sided = (uint32_t)source->double_sided;
            target->unlit = (uint32_t)source->unlit;
            target->alpha_cutoff = source->alpha_cutoff;
            memcpy(target->emissive, source->emissive_factor, sizeof(target->emissive));
            if (source->has_pbr_metallic_roughness) {
                memcpy(target->base_color, source->pbr_metallic_roughness.base_color_factor,
                       sizeof(target->base_color));
                target->base_color_texture =
                    ptr_index(source->pbr_metallic_roughness.base_color_texture.texture,
                              data->textures, sizeof(*data->textures));
            } else
                target->base_color[0] = target->base_color[1] = target->base_color[2] =
                    target->base_color[3] = 1.0f;
            {
                unsigned component;
                if (!isfinite(target->alpha_cutoff)) {
                    diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED,
                             "material alpha cutoff is non-finite");
                    result = VG_ERROR_INVALID_ARGUMENT;
                    goto cleanup;
                }
                for (component = 0; component < 4u; ++component)
                    if (!isfinite(target->base_color[component])) {
                        diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED,
                                 "material base color is non-finite");
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                for (component = 0; component < 3u; ++component)
                    if (!isfinite(target->emissive[component])) {
                        diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED,
                                 "material emissive color is non-finite");
                        result = VG_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
            }
        }
        for (i = 0; i < data->textures_count; ++i) {
            cgltf_sampler *s = data->textures[i].sampler;
            textures[i].image =
                ptr_index(data->textures[i].image, data->images, sizeof(*data->images));
            textures[i].mag_filter = s ? s->mag_filter : 0;
            textures[i].min_filter = s ? s->min_filter : 0;
            textures[i].wrap_s = s ? s->wrap_s : cgltf_wrap_mode_repeat;
            textures[i].wrap_t = s ? s->wrap_t : cgltf_wrap_mode_repeat;
        }
        for (i = 0; i < data->buffers_count; ++i)
            if (data->buffers[i].uri != NULL && !data_uri(data->buffers[i].uri)) {
                VgModelDependencyIr *dep = &dependencies[dependency_cursor++];
                dep->path_offset = copy_string(strings, &strings_cursor, data->buffers[i].uri);
                dep->kind = 1u;
                dep->byte_size = data->buffers[i].size;
                {
                    VgSha256 h;
                    vg_sha256_init(&h);
                    vg_sha256_update(&h, data->buffers[i].data, data->buffers[i].size);
                    vg_sha256_finish(&h, dep->sha256);
                }
            }
        for (i = 0; i < data->images_count; ++i) {
            VgModelImageIr *target = &images[i];
            target->name_offset = copy_string(strings, &strings_cursor, data->images[i].name);
            target->mime_type = loaded[i].mime;
            target->data_offset = image_cursor;
            target->data_size = (uint32_t)loaded[i].size;
            target->dependency = loaded[i].dependency;
            memcpy(image_data + image_cursor, loaded[i].data, loaded[i].size);
            image_cursor += (uint32_t)loaded[i].size;
            if (loaded[i].dependency != VG_MODEL_NO_INDEX) {
                VgModelDependencyIr *dep = &dependencies[dependency_cursor++];
                dep->path_offset = copy_string(strings, &strings_cursor, data->images[i].uri);
                dep->kind = 2u;
                dep->byte_size = loaded[i].size;
                {
                    VgSha256 h;
                    vg_sha256_init(&h);
                    vg_sha256_update(&h, loaded[i].data, loaded[i].size);
                    vg_sha256_finish(&h, dep->sha256);
                }
            }
        }
    }
    vg_sha256_init(&fingerprint);
    vg_sha256_update(&fingerprint, "Vestigio.gltf.import", 20u);
    hash_u32(&fingerprint, VG_GLTF_IMPORTER_VERSION);
    hash_options(&fingerprint, limits);
    hash_u64(&fingerprint, source_size);
    vg_sha256_update(&fingerprint, source_data, (size_t)source_size);
    {
        const VgModelDependencyIr *deps =
            VG_MODEL_IR_ARRAY_CONST(model, VgModelDependencyIr, dependencies);
        for (i = 0; i < model->dependency_count; ++i) {
            hash_string(&fingerprint, vg_model_ir_string(model, deps[i].path_offset));
            hash_u64(&fingerprint, deps[i].byte_size);
            vg_sha256_update(&fingerprint, deps[i].sha256, 32u);
        }
    }
    vg_sha256_finish(&fingerprint, model->source_fingerprint);
    *out_model = model;
    *out_bytes = layout;
    model = NULL;
    result = VG_OK;
    goto cleanup;
layout_fail:
    diag_set(diag, VG_GLTF_DIAGNOSTIC_LIMIT, "packed model layout exceeds representable limits");
    result = VG_ERROR_CAPACITY;
    goto cleanup;
cg_failure:
    if (cg_result == cgltf_result_out_of_memory) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_OUT_OF_MEMORY, "out of memory while parsing glTF");
        result = VG_ERROR_OUT_OF_MEMORY;
    } else if (session.io_result != VG_OK) {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_IO, "%s", session.io_error);
        result = session.io_result;
    } else {
        diag_set(diag, VG_GLTF_DIAGNOSTIC_MALFORMED, "cgltf rejected input (%d)", (int)cg_result);
        result = VG_ERROR_INVALID_ARGUMENT;
    }
cleanup:
    if (loaded != NULL) {
        for (i = 0; data != NULL && i < data->images_count; ++i)
            mem_free(memory, loaded[i].owned);
        mem_free(memory, loaded);
    }
    mem_free(memory, model);
    if (data != NULL)
        cgltf_free(data);
    mem_free(memory, session.arena);
    return result;
}

VgResult vg_gltf_import(const VgGltfImporter *importer, const VgAssetMemory *memory,
                        const char *source_path, const void *source_data, uint64_t source_size,
                        const VgGltfImportOptions *options, VgStaticModelIr **out_model,
                        uint64_t *out_bytes, VgGltfDiagnostic *diagnostic) {
    VgGltfImportOptions defaults;
    if (diagnostic != NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
    if (memory == NULL || memory->allocate == NULL || memory->deallocate == NULL ||
        source_data == NULL || source_size == 0u || out_model == NULL || out_bytes == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (options == NULL) {
        vg_gltf_default_options(&defaults);
        options = &defaults;
    }
    if (options->struct_size != sizeof(*options) ||
        options->api_version != VG_GLTF_IMPORTER_VERSION || !isfinite(options->uniform_scale) ||
        options->uniform_scale <= 0.0f)
        return VG_ERROR_INVALID_ARGUMENT;
    return import_impl(importer, memory, source_path, source_data, source_size, options, out_model,
                       out_bytes, diagnostic);
}

void vg_gltf_model_destroy(const VgAssetMemory *memory, VgStaticModelIr *model) {
    mem_free(memory, model);
}

static VgResult decoder_decode(void *user, const VgAssetMemory *memory,
                               const VgAssetSourceDesc *source, void **out_data,
                               uint64_t *out_bytes, char *error, uint32_t error_capacity) {
    VgGltfImportOptions defaults;
    const VgGltfImportOptions *options = NULL;
    VgGltfDiagnostic diagnostic;
    VgResult result;
    if (source == NULL || source->struct_size != sizeof(*source))
        return VG_ERROR_INVALID_ARGUMENT;
    if (source->type != VG_ASSET_TYPE_MESH || source->importer_version != VG_GLTF_IMPORTER_VERSION)
        return VG_ERROR_FORMAT_VERSION;
    if (source->options_data != NULL) {
        if (source->options_size != sizeof(VgGltfImportOptions))
            return VG_ERROR_INVALID_ARGUMENT;
        options = (const VgGltfImportOptions *)source->options_data;
    } else {
        vg_gltf_default_options(&defaults);
        options = &defaults;
    }
    result = vg_gltf_import((const VgGltfImporter *)user, memory, source->source_path,
                            source->source_data, source->source_size, options,
                            (VgStaticModelIr **)out_data, out_bytes, &diagnostic);
    if (result == VG_OK) {
        const uint8_t zero[VG_ASSET_FINGERPRINT_SIZE] = {0};
        VgStaticModelIr *model = (VgStaticModelIr *)*out_data;
        if (memcmp(source->fingerprint, zero, sizeof(zero)) != 0 &&
            memcmp(source->fingerprint, model->source_fingerprint, sizeof(zero)) != 0) {
            vg_gltf_model_destroy(memory, model);
            *out_data = NULL;
            *out_bytes = 0u;
            result = VG_ERROR_CONFLICT;
            diag_set(&diagnostic, VG_GLTF_DIAGNOSTIC_FINGERPRINT_MISMATCH,
                     "source/dependency fingerprint changed before decode");
        }
    }
    if (result != VG_OK && error != NULL && error_capacity != 0u)
        (void)snprintf(error, error_capacity, "%s", diagnostic.message);
    return result;
}

static void decoder_destroy(void *user, const VgAssetMemory *memory, void *data) {
    (void)user;
    vg_gltf_model_destroy(memory, (VgStaticModelIr *)data);
}

VgAssetDecoder vg_gltf_asset_decoder(const VgGltfImporter *importer) {
    VgAssetDecoder decoder;
    decoder.user = (void *)importer;
    decoder.decode = decoder_decode;
    decoder.destroy = decoder_destroy;
    return decoder;
}
