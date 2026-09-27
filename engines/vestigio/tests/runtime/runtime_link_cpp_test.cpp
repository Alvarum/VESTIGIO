#include "vestigio/vestigio.h"

int main() {
    VgContextDesc description{};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    VgContext *context = nullptr;
    if (vg_context_create(&description, &context) != VG_OK)
        return 1;
    VgWorld world{};
    if (vg_world_create(context, nullptr, &world) != VG_OK) {
        vg_context_destroy(context);
        return 2;
    }
    VgEntity entity{};
    const VgResult result = vg_entity_create(context, world, &entity);
    vg_context_destroy(context);
    return result == VG_OK && entity.value != VG_INVALID_HANDLE_VALUE ? 0 : 3;
}
