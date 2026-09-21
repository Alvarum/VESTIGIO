#include "assets/import/gltf_import.h"

VgResult vg_assets_enable_static_model_importer(VgContext *context) {
    VgAssetDecoder decoder = vg_gltf_asset_decoder(NULL);
    return vg_asset_set_decoder(context, VG_ASSET_TYPE_MESH, &decoder);
}
