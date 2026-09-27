#ifndef VESTIGIO_GLTF_IMPORT_H
#define VESTIGIO_GLTF_IMPORT_H

#include "assets/asset_registry.h"
#include "assets/import/model_ir.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { VG_GLTF_IMPORTER_VERSION = 1u };

typedef enum VgGltfDiagnosticCode {
    VG_GLTF_DIAGNOSTIC_NONE = 0,
    VG_GLTF_DIAGNOSTIC_MALFORMED,
    VG_GLTF_DIAGNOSTIC_UNSUPPORTED_VERSION,
    VG_GLTF_DIAGNOSTIC_UNSUPPORTED_FEATURE,
    VG_GLTF_DIAGNOSTIC_LIMIT,
    VG_GLTF_DIAGNOSTIC_IO,
    VG_GLTF_DIAGNOSTIC_OUT_OF_MEMORY,
    VG_GLTF_DIAGNOSTIC_FINGERPRINT_MISMATCH
} VgGltfDiagnosticCode;

typedef struct VgGltfDiagnostic {
    uint32_t code;
    char message[192];
} VgGltfDiagnostic;

typedef struct VgGltfImportOptions {
    uint32_t struct_size;
    uint32_t api_version;
    float uniform_scale;
    uint32_t max_nodes;
    uint32_t max_meshes;
    uint32_t max_primitives;
    uint32_t max_vertices;
    uint32_t max_indices;
    uint32_t max_materials;
    uint32_t max_textures;
    uint32_t max_images;
    uint64_t max_source_bytes;
    uint64_t max_dependency_bytes;
    uint64_t max_working_bytes;
    uint64_t max_output_bytes;
} VgGltfImportOptions;

/* read() must enforce its virtual root, allocate through memory, and return ownership. */
typedef VgResult (*VgGltfReadFn)(void *user, const VgAssetMemory *memory, const char *path,
                                 void **out_data, uint64_t *out_size, char *error,
                                 uint32_t error_capacity);

typedef struct VgGltfIo {
    void *user;
    VgGltfReadFn read;
} VgGltfIo;

typedef struct VgGltfImporter {
    VgGltfIo io;
} VgGltfImporter;

void vg_gltf_default_options(VgGltfImportOptions *options);
/* source_path is a normalized, root-relative virtual path using '/' separators. */
VgResult vg_gltf_import(const VgGltfImporter *importer, const VgAssetMemory *memory,
                        const char *source_path, const void *source_data, uint64_t source_size,
                        const VgGltfImportOptions *options, VgStaticModelIr **out_model,
                        uint64_t *out_bytes, VgGltfDiagnostic *diagnostic);
void vg_gltf_model_destroy(const VgAssetMemory *memory, VgStaticModelIr *model);

/* importer is borrowed and must outlive registration and all decode callbacks. */
VgAssetDecoder vg_gltf_asset_decoder(const VgGltfImporter *importer);

#ifdef __cplusplus
}
#endif

#endif
