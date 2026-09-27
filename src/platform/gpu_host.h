#ifndef VESTIGIO_GPU_HOST_H
#define VESTIGIO_GPU_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(VG_GPU_HOST_BUILD)
#define VG_GPU_HOST_API __declspec(dllexport)
#elif defined(_WIN32)
#define VG_GPU_HOST_API __declspec(dllimport)
#else
#define VG_GPU_HOST_API
#endif

typedef struct VgGpuHost VgGpuHost;

VG_GPU_HOST_API VgGpuHost *vg_gpu_host_create(void *parent_window, unsigned int width,
                                              unsigned int height, char *error,
                                              size_t error_capacity);
VG_GPU_HOST_API void *vg_gpu_host_window(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_render(VgGpuHost *host);
/* E01: load a native level and its referenced Atrium model. Edit owns the
 * document; Play instantiates a separate world and Stop discards it. */
VG_GPU_HOST_API int32_t vg_gpu_host_open_level(VgGpuHost *host, const char *level_path,
                                              const char *model_path, char *error,
                                              size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_set_mode(VgGpuHost *host, int32_t play);
VG_GPU_HOST_API int32_t vg_gpu_host_mode(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_frame(VgGpuHost *host, double elapsed_seconds,
                                          float move_x, float move_y, float look_x,
                                          float look_y, int32_t jump, int32_t focused);
/* Normalized viewport point, selecting mesh bounds by UUID. No hit returns 0. */
VG_GPU_HOST_API int32_t vg_gpu_host_pick(VgGpuHost *host, float u, float v,
                                         char *uuid, size_t uuid_capacity);
VG_GPU_HOST_API uint64_t vg_gpu_host_document_revision(const VgGpuHost *host);
VG_GPU_HOST_API uint64_t vg_gpu_host_readbacks(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_camera_position(const VgGpuHost *host,
                                                    float *x, float *y, float *z);
VG_GPU_HOST_API int32_t vg_gpu_host_set_camera_mode(VgGpuHost *host, int32_t mode);
VG_GPU_HOST_API int32_t vg_gpu_host_frame_selection(VgGpuHost *host);
enum {
    VG_GPU_PICK_MESH = 1u,
    VG_GPU_PICK_CAMERA = 2u,
    VG_GPU_PICK_LIGHT = 4u,
    VG_GPU_PICK_TRIGGER = 8u
};
VG_GPU_HOST_API int32_t vg_gpu_host_set_pick_mask(VgGpuHost *host, uint32_t mask);
VG_GPU_HOST_API int32_t vg_gpu_host_resize(VgGpuHost *host, unsigned int width,
                                           unsigned int height);
VG_GPU_HOST_API int32_t vg_gpu_host_size(VgGpuHost *host, unsigned int *width,
                                         unsigned int *height);
VG_GPU_HOST_API int32_t vg_gpu_host_capture(VgGpuHost *host, const char *path);
VG_GPU_HOST_API int32_t vg_gpu_host_focus(VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_has_focus(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_destroy(VgGpuHost *host);

#endif /* VESTIGIO_GPU_HOST_H */
