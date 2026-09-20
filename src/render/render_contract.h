#ifndef VESTIGIO_RENDER_CONTRACT_H
#define VESTIGIO_RENDER_CONTRACT_H

/* Internal contract between runtime/hosts and the selected GPU backend.
 * It is not installed as part of the game SDK and does not expose raylib. */

#include "vestigio/vestigio.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum VgSurfaceState {
    VG_SURFACE_UNAVAILABLE = 0,
    VG_SURFACE_READY,
    VG_SURFACE_MINIMIZED,
    VG_SURFACE_LOST
} VgSurfaceState;

typedef struct VgSurfaceInfo {
    uint32_t struct_size;
    uint32_t physical_width;
    uint32_t physical_height;
    float dpi_scale;
    uint64_t revision;
    VgSurfaceState state;
} VgSurfaceInfo;

typedef struct VgDrawPacket {
    VgAsset mesh;
    VgAsset material;
    VgTransform transform;
    VgVec3 bounds_center;
    VgVec3 bounds_extent;
    uint32_t flags;
} VgDrawPacket;

typedef struct VgFrameStats {
    uint64_t frame_index;
    uint32_t draw_calls;
    uint32_t triangles;
    uint32_t uploads;
    uint32_t readbacks;
    uint64_t estimated_gpu_bytes;
} VgFrameStats;

/* The platform owns the native window and graphics context. The renderer owns
 * GPU resources/targets created inside that context. A normal present cannot
 * require CPU pixels; capture is a separate requested operation. */
typedef struct VgRenderContract {
    VgBackend backend;
    bool context_thread_only;
    bool normal_present_requires_readback;
} VgRenderContract;

#endif /* VESTIGIO_RENDER_CONTRACT_H */
