#include "assets/import/gltf_import.h"
#include "assets/import/sha256.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestAllocator {
    size_t live;
    size_t calls;
    size_t fail_at;
} TestAllocator;

static void *test_allocate(void *user, uint64_t size) {
    TestAllocator *allocator = (TestAllocator *)user;
    void *data;
    ++allocator->calls;
    if (allocator->fail_at != 0u && allocator->calls == allocator->fail_at)
        return NULL;
    if (size > SIZE_MAX - sizeof(size_t))
        return NULL;
    data = malloc((size_t)size + sizeof(size_t));
    if (data == NULL)
        return NULL;
    *(size_t *)data = (size_t)size;
    allocator->live += (size_t)size;
    return (size_t *)data + 1;
}

static void test_deallocate(void *user, void *pointer) {
    TestAllocator *allocator = (TestAllocator *)user;
    size_t *header;
    if (pointer == NULL)
        return;
    header = (size_t *)pointer - 1;
    allocator->live -= *header;
    free(header);
}

static int read_file(const char *path, void **out_data, uint64_t *out_size) {
    FILE *file = fopen(path, "rb");
    long size;
    void *data;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    data = malloc((size_t)size);
    if (data == NULL || fread(data, 1u, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return 0;
    }
    fclose(file);
    *out_data = data;
    *out_size = (uint64_t)size;
    return 1;
}

static VgResult resolver_read(void *user, const VgAssetMemory *memory, const char *path,
                              void **out_data, uint64_t *out_size, char *error,
                              uint32_t error_capacity) {
    FILE *file;
    long size;
    void *data;
    (void)user;
    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL)
            fclose(file);
        if (error != NULL && error_capacity != 0u)
            snprintf(error, error_capacity, "cannot read %s", path);
        return VG_ERROR_IO;
    }
    data = memory->allocate(memory->user, (uint64_t)size);
    if (data == NULL) {
        fclose(file);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    if (fread(data, 1u, (size_t)size, file) != (size_t)size) {
        memory->deallocate(memory->user, data);
        fclose(file);
        return VG_ERROR_IO;
    }
    fclose(file);
    *out_data = data;
    *out_size = (uint64_t)size;
    return VG_OK;
}

static int near(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static void store_u32_le(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)(value >> 16u);
    bytes[3] = (uint8_t)(value >> 24u);
}

static int test_sha256(void) {
    static const uint8_t expected[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                                         0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                         0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                         0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    VgSha256 hash;
    uint8_t digest[32];
    vg_sha256_init(&hash);
    vg_sha256_update(&hash, "abc", 3u);
    vg_sha256_finish(&hash, digest);
    return memcmp(digest, expected, sizeof(expected)) == 0;
}

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);        \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)

static int import_path(const char *path, TestAllocator *allocator, VgStaticModelIr **out_model,
                       uint64_t *out_bytes, VgGltfDiagnostic *diagnostic) {
    VgAssetMemory memory = {allocator, test_allocate, test_deallocate};
    void *source = NULL;
    uint64_t source_size = 0u;
    VgResult result;
    CHECK(read_file(path, &source, &source_size));
    result = vg_gltf_import(NULL, &memory, path, source, source_size, NULL, out_model, out_bytes,
                            diagnostic);
    free(source);
    return result;
}

static int test_valid(const char *root) {
    char path[512];
    TestAllocator allocator = {0};
    VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
    VgStaticModelIr *first = NULL, *second = NULL;
    uint64_t first_bytes = 0u, second_bytes = 0u;
    VgGltfDiagnostic diagnostic;
    const VgModelNodeIr *nodes;
    const VgModelVertexIr *vertices;
    const VgModelMaterialIr *materials;
    const uint32_t *indices;
    snprintf(path, sizeof(path), "%s/static_scene.gltf", root);
    CHECK(import_path(path, &allocator, &first, &first_bytes, &diagnostic) == VG_OK);
    CHECK(import_path(path, &allocator, &second, &second_bytes, &diagnostic) == VG_OK);
    CHECK(first != NULL && first_bytes == first->total_bytes && first_bytes == second_bytes);
    CHECK(first->node_count == 2u && first->primitive_count == 1u && first->vertex_count == 3u);
    CHECK(first->index_count == 3u && first->material_count == 1u && first->image_count == 1u);
    CHECK(memcmp(first, second, (size_t)first_bytes) == 0);
    nodes = VG_MODEL_IR_ARRAY_CONST(first, VgModelNodeIr, nodes);
    CHECK(strcmp(vg_model_ir_string(first, nodes[0].name_offset), "Pivot") == 0);
    CHECK(nodes[1].parent == 0u && nodes[1].mesh == 0u);
    CHECK(near(nodes[0].local_transform[12], 1.0f));
    CHECK(near(nodes[0].local_transform[13], -3.0f));
    CHECK(near(nodes[0].local_transform[14], 2.0f));
    vertices = VG_MODEL_IR_ARRAY_CONST(first, VgModelVertexIr, vertices);
    CHECK(near(vertices[2].position[0], 0.0f));
    CHECK(near(vertices[2].position[1], 0.0f));
    CHECK(near(vertices[2].position[2], 1.0f));
    indices = VG_MODEL_IR_ARRAY_CONST(first, uint32_t, indices);
    CHECK(indices[0] == 0u && indices[1] == 1u && indices[2] == 2u);
    materials = VG_MODEL_IR_ARRAY_CONST(first, VgModelMaterialIr, materials);
    CHECK(materials[0].alpha_mode == VG_MODEL_ALPHA_MASK && materials[0].double_sided == 1u);
    CHECK(near(materials[0].alpha_cutoff, 0.25f) && materials[0].base_color_texture == 0u);
    vg_gltf_model_destroy(&memory, first);
    vg_gltf_model_destroy(&memory, second);
    CHECK(allocator.live == 0u);
    return 1;
}

static int test_nonindexed(const char *root) {
    char path[512];
    TestAllocator allocator = {0};
    VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
    VgStaticModelIr *model = NULL;
    uint64_t bytes = 0u;
    VgGltfDiagnostic diagnostic;
    const uint32_t *indices;
    const VgModelVertexIr *vertices;
    snprintf(path, sizeof(path), "%s/nonindexed.gltf", root);
    CHECK(import_path(path, &allocator, &model, &bytes, &diagnostic) == VG_OK);
    indices = VG_MODEL_IR_ARRAY_CONST(model, uint32_t, indices);
    vertices = VG_MODEL_IR_ARRAY_CONST(model, VgModelVertexIr, vertices);
    CHECK(indices[0] == 0u && indices[1] == 1u && indices[2] == 2u);
    CHECK(near(vertices[0].normal[1], -1.0f));
    vg_gltf_model_destroy(&memory, model);
    CHECK(allocator.live == 0u);
    return 1;
}

static int test_glb(const char *root) {
    char path[512];
    TestAllocator allocator = {0};
    VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
    VgStaticModelIr *model = NULL;
    uint64_t bytes = 0u;
    VgGltfDiagnostic diagnostic;
    snprintf(path, sizeof(path), "%s/static_triangle.glb", root);
    CHECK(import_path(path, &allocator, &model, &bytes, &diagnostic) == VG_OK);
    CHECK(model->primitive_count == 1u && model->vertex_count == 3u && model->index_count == 3u);
    vg_gltf_model_destroy(&memory, model);
    CHECK(allocator.live == 0u);
    return 1;
}

static int test_glb_workspace_uses_json_size(void) {
    static const char json_template[] =
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
        "\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{\"attributes\":{"
        "\"POSITION\":0}}]}],\"buffers\":[{\"byteLength\":%u}],\"bufferViews\":[{"
        "\"buffer\":0,\"byteLength\":36}],\"accessors\":[{\"bufferView\":0,"
        "\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}]}";
    const uint32_t binary_size = 8u * 1024u * 1024u;
    char json[1024];
    int json_chars = snprintf(json, sizeof(json), json_template, binary_size);
    uint32_t json_size, total_size;
    uint8_t *source;
    float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    TestAllocator allocator = {0};
    VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
    VgGltfImportOptions options;
    VgGltfDiagnostic diagnostic;
    VgStaticModelIr *model = NULL;
    uint64_t model_bytes = 0u;
    CHECK(json_chars > 0 && (size_t)json_chars < sizeof(json));
    json_size = ((uint32_t)json_chars + 3u) & ~3u;
    total_size = 12u + 8u + json_size + 8u + binary_size;
    source = (uint8_t *)calloc(1u, total_size);
    CHECK(source != NULL);
    memcpy(source, "glTF", 4u);
    store_u32_le(source + 4u, 2u);
    store_u32_le(source + 8u, total_size);
    store_u32_le(source + 12u, json_size);
    store_u32_le(source + 16u, 0x4e4f534au);
    memcpy(source + 20u, json, (size_t)json_chars);
    memset(source + 20u + json_chars, ' ', json_size - (uint32_t)json_chars);
    store_u32_le(source + 20u + json_size, binary_size);
    store_u32_le(source + 24u + json_size, 0x004e4942u);
    memcpy(source + 28u + json_size, positions, sizeof(positions));
    vg_gltf_default_options(&options);
    options.max_working_bytes = 128u * 1024u;
    CHECK(vg_gltf_import(NULL, &memory, "large.glb", source, total_size, &options, &model,
                         &model_bytes, &diagnostic) == VG_OK);
    CHECK(model != NULL && model->vertex_count == 3u && model->index_count == 3u);
    vg_gltf_model_destroy(&memory, model);
    free(source);
    CHECK(allocator.live == 0u);
    return 1;
}

static int test_invalid(const char *root, const char *name, VgResult expected) {
    char path[512];
    TestAllocator allocator = {0};
    VgStaticModelIr *model = (VgStaticModelIr *)(uintptr_t)1u;
    uint64_t bytes = 42u;
    VgGltfDiagnostic diagnostic;
    VgResult result;
    snprintf(path, sizeof(path), "%s/%s", root, name);
    result = (VgResult)import_path(path, &allocator, &model, &bytes, &diagnostic);
    if (result != expected)
        fprintf(stderr, "%s: got %d expected %d (%s)\n", name, result, expected,
                diagnostic.message);
    CHECK(result == expected);
    CHECK(model == (VgStaticModelIr *)(uintptr_t)1u && bytes == 42u);
    CHECK(allocator.live == 0u);
    return 1;
}

static int test_oom(const char *root) {
    char path[512];
    size_t fail_at, allocation_count;
    void *source = NULL;
    uint64_t source_size = 0u;
    TestAllocator baseline = {0};
    VgAssetMemory baseline_memory = {&baseline, test_allocate, test_deallocate};
    VgStaticModelIr *baseline_model = NULL;
    uint64_t baseline_bytes = 0u;
    VgGltfDiagnostic baseline_diagnostic;
    snprintf(path, sizeof(path), "%s/static_scene.gltf", root);
    CHECK(read_file(path, &source, &source_size));
    CHECK(vg_gltf_import(NULL, &baseline_memory, path, source, source_size, NULL, &baseline_model,
                         &baseline_bytes, &baseline_diagnostic) == VG_OK);
    allocation_count = baseline.calls;
    vg_gltf_model_destroy(&baseline_memory, baseline_model);
    CHECK(baseline.live == 0u);
    for (fail_at = 1u; fail_at <= allocation_count; ++fail_at) {
        TestAllocator allocator = {0};
        VgStaticModelIr *model = (VgStaticModelIr *)(uintptr_t)1u;
        uint64_t bytes = 91u;
        VgGltfDiagnostic diagnostic;
        VgResult result;
        VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
        allocator.fail_at = fail_at;
        result = vg_gltf_import(NULL, &memory, path, source, source_size, NULL, &model, &bytes,
                                &diagnostic);
        CHECK(result == VG_ERROR_OUT_OF_MEMORY);
        CHECK(model == (VgStaticModelIr *)(uintptr_t)1u && bytes == 91u);
        CHECK(allocator.live == 0u);
    }
    free(source);
    return 1;
}

static int test_external_and_limits(const char *root) {
    char path[512];
    void *source = NULL;
    uint64_t source_size = 0u, bytes = 0u;
    TestAllocator allocator = {0};
    VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
    VgGltfImporter importer = {{NULL, resolver_read}};
    VgGltfImportOptions options;
    VgGltfDiagnostic diagnostic;
    VgStaticModelIr *model = NULL;
    const VgModelImageIr *images;
    const VgModelDependencyIr *dependencies;
    snprintf(path, sizeof(path), "%s/external_dependencies.gltf", root);
    CHECK(read_file(path, &source, &source_size));
    vg_gltf_default_options(&options);
    CHECK(vg_gltf_import(&importer, &memory, path, source, source_size, &options, &model, &bytes,
                         &diagnostic) == VG_OK);
    CHECK(model->dependency_count == 2u);
    images = VG_MODEL_IR_ARRAY_CONST(model, VgModelImageIr, images);
    dependencies = VG_MODEL_IR_ARRAY_CONST(model, VgModelDependencyIr, dependencies);
    CHECK(images[0].dependency == 1u);
    CHECK(strcmp(vg_model_ir_string(model, dependencies[0].path_offset), "triangle.bin") == 0);
    CHECK(strcmp(vg_model_ir_string(model, dependencies[1].path_offset), "pixel.png") == 0);
    vg_gltf_model_destroy(&memory, model);
    model = (VgStaticModelIr *)(uintptr_t)1u;
    bytes = 17u;
    options.max_dependency_bytes = 50u;
    CHECK(vg_gltf_import(&importer, &memory, path, source, source_size, &options, &model, &bytes,
                         &diagnostic) == VG_ERROR_CAPACITY);
    CHECK(model == (VgStaticModelIr *)(uintptr_t)1u && bytes == 17u);
    free(source);
    CHECK(allocator.live == 0u);
    return 1;
}

static int test_output_limit(const char *root) {
    char path[512];
    void *source = NULL;
    uint64_t source_size = 0u, bytes = 73u;
    TestAllocator allocator = {0};
    VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
    VgGltfImportOptions options;
    VgGltfDiagnostic diagnostic;
    VgStaticModelIr *model = (VgStaticModelIr *)(uintptr_t)1u;
    snprintf(path, sizeof(path), "%s/static_scene.gltf", root);
    CHECK(read_file(path, &source, &source_size));
    vg_gltf_default_options(&options);
    options.max_output_bytes = sizeof(VgStaticModelIr);
    CHECK(vg_gltf_import(NULL, &memory, path, source, source_size, &options, &model, &bytes,
                         &diagnostic) == VG_ERROR_CAPACITY);
    CHECK(model == (VgStaticModelIr *)(uintptr_t)1u && bytes == 73u);
    free(source);
    CHECK(allocator.live == 0u);
    return 1;
}

static int test_decoder_fingerprint(const char *root) {
    char path[512], error[192];
    void *source = NULL, *decoded = NULL;
    uint64_t source_size = 0u, bytes = 0u;
    TestAllocator allocator = {0};
    VgAssetMemory memory = {&allocator, test_allocate, test_deallocate};
    VgAssetSourceDesc descriptor;
    VgAssetDecoder decoder = vg_gltf_asset_decoder(NULL);
    VgStaticModelIr *model;
    snprintf(path, sizeof(path), "%s/static_scene.gltf", root);
    CHECK(read_file(path, &source, &source_size));
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.struct_size = sizeof(descriptor);
    descriptor.type = VG_ASSET_TYPE_MESH;
    descriptor.importer_version = VG_GLTF_IMPORTER_VERSION;
    descriptor.source_path = path;
    descriptor.source_data = source;
    descriptor.source_size = source_size;
    CHECK(decoder.decode(decoder.user, &memory, &descriptor, &decoded, &bytes, error,
                         sizeof(error)) == VG_OK);
    model = (VgStaticModelIr *)decoded;
    memcpy(descriptor.fingerprint, model->source_fingerprint, 32u);
    decoder.destroy(decoder.user, &memory, decoded);
    decoded = NULL;
    bytes = 0u;
    CHECK(decoder.decode(decoder.user, &memory, &descriptor, &decoded, &bytes, error,
                         sizeof(error)) == VG_OK);
    decoder.destroy(decoder.user, &memory, decoded);
    decoded = NULL;
    bytes = 0u;
    descriptor.fingerprint[0] ^= 1u;
    CHECK(decoder.decode(decoder.user, &memory, &descriptor, &decoded, &bytes, error,
                         sizeof(error)) == VG_ERROR_CONFLICT);
    CHECK(decoded == NULL && bytes == 0u);
    free(source);
    CHECK(allocator.live == 0u);
    return 1;
}

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "tests/assets/fixtures";
    if (!test_sha256() || !test_valid(root) || !test_nonindexed(root) || !test_glb(root) ||
        !test_glb_workspace_uses_json_size() ||
        !test_invalid(root, "required_extension.gltf", VG_ERROR_UNSUPPORTED) ||
        !test_invalid(root, "out_of_bounds.gltf", VG_ERROR_INVALID_ARGUMENT) ||
        !test_invalid(root, "truncated.gltf", VG_ERROR_INVALID_ARGUMENT) ||
        !test_invalid(root, "nonfinite_transform.gltf", VG_ERROR_INVALID_ARGUMENT) ||
        !test_invalid(root, "nonfinite_material.gltf", VG_ERROR_INVALID_ARGUMENT) ||
        !test_invalid(root, "unsafe_uri.gltf", VG_ERROR_IO) || !test_oom(root) ||
        !test_external_and_limits(root) || !test_output_limit(root) ||
        !test_decoder_fingerprint(root))
        return 1;
    puts("gltf_import_test: PASS");
    return 0;
}
