#include "render/render_contract.h"

#include <stddef.h>

_Static_assert(sizeof(VgFrameStats) >= sizeof(uint64_t) * 2u, "Stats must retain 64-bit counters");
_Static_assert(offsetof(VgDrawPacket, transform) > offsetof(VgDrawPacket, material),
               "Draw packet layout changed unexpectedly");

int main(void) {
    VgRenderContract contract = {VG_BACKEND_OPENGL_33, true, false};
    VgSurfaceInfo surface = {sizeof(surface), 640u, 360u, 1.0f, 1u, VG_SURFACE_READY};
    return contract.context_thread_only && !contract.normal_present_requires_readback &&
                   surface.state == VG_SURFACE_READY
               ? 0
               : 1;
}
