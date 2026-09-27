#include "player/demo_3d.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    (void)fprintf(stderr,
                  "Uso: vestigio_player [--smoke frames] [--capture png] [--resolution WxH] "
                  "[--fullscreen|--windowed] [--vsync|--no-vsync] "
                  "[--frame-cap hz] [--sensitivity valor] [--show-colliders]\n");
    return 2;
}

int main(int argc, char **argv) {
    int smoke_frames = 0;
    const char *capture = NULL;
    bool show_colliders = false;
    VgSettingsLayer session = {0};
    session.struct_size = sizeof(session);
    session.api_version = VG_API_VERSION;
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        if (strcmp(argument, "--capture") == 0 && index + 1 < argc) {
            capture = argv[++index];
        } else if (strcmp(argument, "--show-colliders") == 0) {
            show_colliders = true;
        } else if (strcmp(argument, "--smoke") == 0 && index + 1 < argc) {
            char *end = NULL;
            errno = 0;
            long value = strtol(argv[++index], &end, 10);
            if (errno != 0 || end == argv[index] || *end != '\0' || value < 1 || value > 100000)
                return usage();
            smoke_frames = (int)value;
        } else if (strcmp(argument, "--resolution") == 0 && index + 1 < argc) {
            unsigned int width = 0u, height = 0u;
            char tail = '\0';
            if (sscanf(argv[++index], "%ux%u%c", &width, &height, &tail) != 2)
                return usage();
            session.present |= VG_SETTING_INTERNAL_RESOLUTION;
            session.internal_width = width;
            session.internal_height = height;
        } else if (strcmp(argument, "--fullscreen") == 0) {
            session.present |= VG_SETTING_FULLSCREEN;
            session.fullscreen = 1u;
        } else if (strcmp(argument, "--windowed") == 0) {
            session.present |= VG_SETTING_FULLSCREEN;
            session.fullscreen = 0u;
        } else if (strcmp(argument, "--vsync") == 0) {
            session.present |= VG_SETTING_VSYNC;
            session.vsync = 1u;
        } else if (strcmp(argument, "--no-vsync") == 0) {
            session.present |= VG_SETTING_VSYNC;
            session.vsync = 0u;
        } else if (strcmp(argument, "--frame-cap") == 0 && index + 1 < argc) {
            char *end = NULL;
            errno = 0;
            unsigned long value = strtoul(argv[++index], &end, 10);
            if (errno != 0 || end == argv[index] || *end != '\0' || value > UINT32_MAX)
                return usage();
            session.present |= VG_SETTING_FRAME_CAP;
            session.frame_cap = (uint32_t)value;
        } else if (strcmp(argument, "--sensitivity") == 0 && index + 1 < argc) {
            char *end = NULL;
            errno = 0;
            float value = strtof(argv[++index], &end);
            if (errno != 0 || end == argv[index] || *end != '\0')
                return usage();
            session.present |= VG_SETTING_LOOK_SENSITIVITY;
            session.look_sensitivity = value;
        } else {
            return usage();
        }
    }
    return vg_demo_3d_run(smoke_frames, capture, show_colliders,
                          session.present != 0u ? &session : NULL);
}
