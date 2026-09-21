#include "platform/gpu_host.h"

#include "platform/win32_embed.h"
#include "raylib.h"
#include "render/gpu_raylib/gpu_renderer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

struct VgGpuHost {
    VgGpuRenderer *renderer;
    void *window;
};

static VgGpuHost *active_host;

static void host_error(char *out, size_t capacity, const char *message) {
    if (out && capacity)
        (void)snprintf(out, capacity, "%s", message);
}

VgGpuHost *vg_gpu_host_create(void *parent_window, unsigned int width, unsigned int height,
                              char *error, size_t error_capacity) {
    if (active_host) {
        host_error(error, error_capacity, "Ya existe una superficie GPU en este proceso");
        return NULL;
    }
    if (!parent_window || width == 0u || height == 0u || width > (unsigned int)INT_MAX ||
        height > (unsigned int)INT_MAX) {
        host_error(error, error_capacity, "Parent o dimensiones de viewport invalidas");
        return NULL;
    }
    VgGpuHost *host = calloc(1u, sizeof(*host));
    if (!host) {
        host_error(error, error_capacity, "Sin memoria para host GPU");
        return NULL;
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_RESIZABLE);
    InitWindow((int)width, (int)height, "VESTIGIO GPU viewport");
    if (!IsWindowReady()) {
        host_error(error, error_capacity, "No se pudo crear ventana/contexto GPU");
        free(host);
        return NULL;
    }
    SetExitKey(KEY_NULL);
    SetTargetFPS(0);
    host->window = GetWindowHandle();
    if (!vg_win32_embed_window(host->window, parent_window, (int)width, (int)height)) {
        host_error(error, error_capacity, "No se pudo alojar la superficie GPU en WPF");
        CloseWindow();
        free(host);
        return NULL;
    }
    host->renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){320u, 180u}, error, error_capacity);
    if (!host->renderer) {
        CloseWindow();
        free(host);
        return NULL;
    }
    active_host = host;
    host_error(error, error_capacity, "");
    return host;
}

void *vg_gpu_host_window(const VgGpuHost *host) {
    return host ? host->window : NULL;
}

bool vg_gpu_host_render(VgGpuHost *host) {
    if (!host || host != active_host || !host->renderer)
        return false;
    if (!vg_gpu_renderer_draw_demo(host->renderer))
        return false;
    vg_gpu_renderer_present(host->renderer);
    return true;
}

bool vg_gpu_host_resize(VgGpuHost *host, unsigned int width, unsigned int height) {
    if (!host || host != active_host || !host->window || width == 0u || height == 0u ||
        width > (unsigned int)INT_MAX || height > (unsigned int)INT_MAX)
        return false;
    return vg_win32_resize_embedded(host->window, (int)width, (int)height);
}

bool vg_gpu_host_capture(VgGpuHost *host, const char *path) {
    return host && host == active_host && host->renderer && path && path[0] != '\0' &&
           vg_gpu_renderer_capture(host->renderer, path);
}

bool vg_gpu_host_focus(VgGpuHost *host) {
    return host && host == active_host && vg_win32_focus_embedded(host->window);
}

bool vg_gpu_host_has_focus(const VgGpuHost *host) {
    return host && host == active_host && vg_win32_embedded_has_focus(host->window);
}

void vg_gpu_host_destroy(VgGpuHost *host) {
    if (!host)
        return;
    if (host == active_host) {
        vg_gpu_renderer_destroy(host->renderer);
        CloseWindow();
        active_host = NULL;
    }
    free(host);
}
