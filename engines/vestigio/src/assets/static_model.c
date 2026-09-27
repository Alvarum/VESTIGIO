#include "assets/import/gltf_import.h"
#include "content/room_recipe.h"

#include <stdio.h>
#include <string.h>

static VgResult static_model_decode(void *user, const VgAssetMemory *memory,
                                    const VgAssetSourceDesc *source, void **out_data,
                                    uint64_t *out_bytes, char *error, uint32_t error_capacity) {
    if (source != NULL && source->importer_version == VG_ROOM_MODEL_IMPORTER_VERSION) {
        if (source->source_data == NULL || source->source_size < sizeof(VgStaticModelIr) ||
            source->source_size > UINT32_MAX || source->source_size > 2u * 1024u * 1024u) {
            if (error != NULL && error_capacity != 0u)
                (void)snprintf(error, error_capacity, "Invalid generated room model");
            return VG_ERROR_INVALID_ARGUMENT;
        }
        const VgStaticModelIr *ir = (const VgStaticModelIr *)source->source_data;
        if (ir->magic != VG_MODEL_IR_MAGIC || ir->version != VG_MODEL_IR_VERSION ||
            ir->importer_version != VG_ROOM_MODEL_IMPORTER_VERSION ||
            ir->total_bytes != source->source_size) {
            if (error != NULL && error_capacity != 0u)
                (void)snprintf(error, error_capacity, "Generated room model header mismatch");
            return VG_ERROR_FORMAT_VERSION;
        }
        void *copy = memory->allocate(memory->user, source->source_size);
        if (copy == NULL)
            return VG_ERROR_OUT_OF_MEMORY;
        memcpy(copy, source->source_data, (size_t)source->source_size);
        *out_data = copy;
        *out_bytes = source->source_size;
        return VG_OK;
    }
    VgAssetDecoder gltf = vg_gltf_asset_decoder(NULL);
    return gltf.decode(user, memory, source, out_data, out_bytes, error, error_capacity);
}

static void static_model_destroy(void *user, const VgAssetMemory *memory, void *data) {
    (void)user;
    memory->deallocate(memory->user, data);
}

VgResult vg_assets_enable_static_model_importer(VgContext *context) {
    VgAssetDecoder decoder = {NULL, static_model_decode, static_model_destroy};
    return vg_asset_set_decoder(context, VG_ASSET_TYPE_MESH, &decoder);
}
