#ifndef VESTIGIO_GPU_HOST_H
#define VESTIGIO_GPU_HOST_H

#include <stdbool.h>
#include <stddef.h>

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
VG_GPU_HOST_API bool vg_gpu_host_render(VgGpuHost *host);
VG_GPU_HOST_API bool vg_gpu_host_resize(VgGpuHost *host, unsigned int width, unsigned int height);
VG_GPU_HOST_API bool vg_gpu_host_capture(VgGpuHost *host, const char *path);
VG_GPU_HOST_API bool vg_gpu_host_focus(VgGpuHost *host);
VG_GPU_HOST_API bool vg_gpu_host_has_focus(const VgGpuHost *host);
VG_GPU_HOST_API void vg_gpu_host_destroy(VgGpuHost *host);

#endif /* VESTIGIO_GPU_HOST_H */
