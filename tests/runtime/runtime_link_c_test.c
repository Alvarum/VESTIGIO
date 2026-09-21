#include "vestigio/vestigio.h"

int main(void) {
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    VgContext *context = 0;
    if (vg_context_create(&description, &context) != VG_OK)
        return 1;
    VgWorld world = {0};
    if (vg_world_create(context, 0, &world) != VG_OK) {
        vg_context_destroy(context);
        return 2;
    }
    VgEntity entity = {0};
    VgResult result = vg_entity_create(context, world, &entity);
    vg_context_destroy(context);
    return result == VG_OK && entity.value != VG_INVALID_HANDLE_VALUE ? 0 : 3;
}
