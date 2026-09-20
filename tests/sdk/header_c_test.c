#include "vestigio/vestigio.h"

#include <stdint.h>

_Static_assert(VG_API_VERSION == 1u, "Unexpected v0.1 encoding");
_Static_assert(sizeof(((VgWorld){0}).value) == sizeof(uint64_t), "World handle must be 64-bit");
_Static_assert(sizeof(((VgEntity){0}).value) == sizeof(uint64_t), "Entity handle must be 64-bit");

static void log_message(void *user, VgLogSeverity severity, const char *message) {
    (void)user;
    (void)severity;
    (void)message;
}

int main(void) {
    VgContextDesc description = {sizeof(description), VG_API_VERSION, VG_BACKEND_DEFAULT, 0u,
                                 (void *)0,           log_message};
    VgTransform transform = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
    return description.struct_size > 0u && transform.rotation.w == 1.0f ? 0 : 1;
}
