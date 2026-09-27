/* Player es el anfitrión de ventana/entrada de la misma sesión que Studio. */
#include "retro/platform.h"
#include "retro/session.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    const char *override = nullptr, *capture = nullptr;
    int smoke_frames = 0;
    bool show_menu = false;
    VgSettingsLayer session_settings = {0};
    session_settings.struct_size = sizeof(session_settings);
    session_settings.api_version = VG_API_VERSION;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--project") == 0 && i + 1 < argc)
            override = argv[++i];
        else if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
            capture = argv[++i];
        else if (strcmp(argv[i], "--menu") == 0)
            show_menu = true;
        else if (strcmp(argv[i], "--fullscreen") == 0) {
            session_settings.present |= VG_SETTING_FULLSCREEN;
            session_settings.fullscreen = 1u;
        } else if (strcmp(argv[i], "--windowed") == 0) {
            session_settings.present |= VG_SETTING_FULLSCREEN;
            session_settings.fullscreen = 0u;
        } else if (strcmp(argv[i], "--vsync") == 0) {
            session_settings.present |= VG_SETTING_VSYNC;
            session_settings.vsync = 1u;
        } else if (strcmp(argv[i], "--no-vsync") == 0) {
            session_settings.present |= VG_SETTING_VSYNC;
            session_settings.vsync = 0u;
        } else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            unsigned int width = 0u, height = 0u;
            char tail = '\0';
            if (sscanf(argv[++i], "%ux%u%c", &width, &height, &tail) != 2)
                return 2;
            session_settings.present |= VG_SETTING_INTERNAL_RESOLUTION;
            session_settings.internal_width = width;
            session_settings.internal_height = height;
        } else if (strcmp(argv[i], "--frame-cap") == 0 && i + 1 < argc) {
            char *end = nullptr;
            errno = 0;
            unsigned long value = strtoul(argv[++i], &end, 10);
            if (errno || end == argv[i] || *end || value > UINT32_MAX)
                return 2;
            session_settings.present |= VG_SETTING_FRAME_CAP;
            session_settings.frame_cap = (uint32_t)value;
        } else if (strcmp(argv[i], "--sensitivity") == 0 && i + 1 < argc) {
            char *end = nullptr;
            errno = 0;
            float value = strtof(argv[++i], &end);
            if (errno || end == argv[i] || *end)
                return 2;
            session_settings.present |= VG_SETTING_LOOK_SENSITIVITY;
            session_settings.look_sensitivity = value;
        } else if (strcmp(argv[i], "--smoke") == 0 && i + 1 < argc) {
            char *end = nullptr;
            errno = 0;
            long value = strtol(argv[++i], &end, 10);
            if (errno || end == argv[i] || *end || value < 1 || value > 100000)
                return 2;
            smoke_frames = (int)value;
        } else {
            (void)fprintf(stderr, "Uso: retro_player [--project archivo] [--smoke frames] "
                                  "[--capture png] [--menu] [--resolution WxH] "
                                  "[--fullscreen|--windowed] [--vsync|--no-vsync] "
                                  "[--frame-cap hz] [--sensitivity valor]\n");
            return 2;
        }
    }
    ReError error = {0};
    RePlatform *platform = nullptr;
    char manifest[2048] = {0};
    ReProject *project = calloc(1, sizeof(*project));
    ReGameSession *session = nullptr;
    double *samples = nullptr;
    int result = 1;
    if (!override && re_platform_application_path("project.retro", manifest, sizeof(manifest))) {
        FILE *probe = fopen(manifest, "rb");
        if (probe)
            (void)fclose(probe);
        else
            (void)re_platform_asset_path("studio/haunted.retro", manifest, sizeof(manifest));
    }
    VgSettingsLayer resolved = {0};
    resolved.struct_size = sizeof(resolved);
    resolved.api_version = VG_API_VERSION;
    const VgSettingsLayer *session_ptr = session_settings.present != 0u ? &session_settings : NULL;
    if (!project || !re_project_load(override ? override : manifest, project, &error) ||
        !re_session_resolve_settings(project, session_ptr, &resolved, &error)) {
        (void)fprintf(stderr, "Proyecto:%zu: %s\n", error.line, error.message);
        goto cleanup;
    }
    if (resolved.internal_width > 4096u || resolved.internal_height > 4096u) {
        (void)fprintf(
            stderr, "Proyecto:0: la resolucion interna maxima del renderer actual es 4096x4096\n");
        goto cleanup;
    }
    RePlatformConfig platform_config = {.title = "RetroForge Player",
                                        .framebuffer_width = (int)resolved.internal_width,
                                        .framebuffer_height = (int)resolved.internal_height,
                                        .hidden = smoke_frames > 0,
                                        .audio = smoke_frames == 0,
                                        .fullscreen = resolved.fullscreen != 0u,
                                        .vsync = resolved.vsync != 0u,
                                        .frame_cap = resolved.frame_cap,
                                        .bindings = resolved.bindings,
                                        .binding_count = resolved.binding_count};
    platform = re_platform_open(platform_config, &error);
    if (!platform ||
        !re_session_create_configured(project, smoke_frames > 0, smoke_frames ? show_menu : 1,
                                      session_ptr, &session, &error)) {
        (void)fprintf(stderr, "Proyecto:%zu: %s\n", error.line, error.message);
        goto cleanup;
    }
    samples = smoke_frames ? calloc((size_t)smoke_frames, sizeof(*samples)) : nullptr;
    double previous = re_platform_time(), total = 0, maximum = 0, session_total = 0;
    int frames = 0;
    while (!(re_session_flags(session) & 1) && (!smoke_frames || frames < smoke_frames)) {
        double start = re_platform_time();
        ReInput input = re_platform_input(platform);
        if (input.quit)
            break;
        re_platform_capture_mouse(platform, input.focused && (re_session_flags(session) & 2));
        re_session_frame(session, start - previous, input.movement.x, input.movement.y,
                         input.look.x, input.look.y, input.pressed, input.held, input.released,
                         input.focused, 0);
        previous = start;
        session_total += re_platform_time() - start;
        re_platform_present(platform, re_session_renderer(session));
        double elapsed = re_platform_time() - start;
        if (samples)
            samples[frames] = elapsed;
        total += elapsed;
        if (elapsed > maximum)
            maximum = elapsed;
        frames++;
    }
    if (smoke_frames) {
        double p95 = maximum;
        if (samples && frames) {
            qsort(samples, (size_t)frames, sizeof(*samples), compare_double);
            p95 = samples[((size_t)frames * 95u + 99u) / 100u - 1u];
        }
        (void)printf("frames=%d frame_mean_ms=%.3f frame_p95_ms=%.3f frame_max_ms=%.3f "
                     "session_mean_ms=%.3f entities=%zu lights=%zu cpu_memory_mib=%.2f\n",
                     frames, frames ? total * 1000 / (double)frames : 0, p95 * 1000, maximum * 1000,
                     frames ? session_total * 1000 / (double)frames : 0,
                     project->world.marker_count, project->interactions.light_count,
                     (double)re_session_memory(session) / (1024 * 1024));
    }
    result = capture && !re_platform_capture_png(re_session_renderer(session), capture) ? 1 : 0;
cleanup:
    free(samples);
    re_session_destroy(session);
    free(project);
    re_platform_close(platform);
    return result;
}
