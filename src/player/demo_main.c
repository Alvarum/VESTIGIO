#include "player/demo_3d.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

static int usage(void) {
    (void)fprintf(stderr,
                  "Uso: vestigio_player [--smoke frames] [--capture png] [--resolution WxH] "
                  "[--fullscreen|--windowed] [--vsync|--no-vsync] "
                  "[--frame-cap hz] [--sensitivity valor] [--show-colliders] "
                  "[--level archivo.level.json] [--settings archivo] "
                  "[--visual clean|retro] [--smoke-door]\n");
    return 2;
}

static int demo_main(int argc, char **argv) {
    int smoke_frames = 0;
    const char *capture = NULL;
    bool show_colliders = false;
    bool smoke_door = false;
    const char *level_path = NULL;
    const char *settings_path = NULL;
    VgSettingsLayer session = {0};
    session.struct_size = sizeof(session);
    session.api_version = VG_API_VERSION;
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        if (strcmp(argument, "--capture") == 0 && index + 1 < argc) {
            capture = argv[++index];
        } else if (strcmp(argument, "--show-colliders") == 0) {
            show_colliders = true;
        } else if (strcmp(argument, "--level") == 0 && index + 1 < argc) {
            level_path = argv[++index];
        } else if (strcmp(argument, "--settings") == 0 && index + 1 < argc) {
            settings_path = argv[++index];
        } else if (strcmp(argument, "--visual") == 0 && index + 1 < argc) {
            const char *visual = argv[++index];
            if (strcmp(visual, "clean") == 0)
                session.visual_profile = VG_VISUAL_PROFILE_CLEAN;
            else if (strcmp(visual, "retro") == 0)
                session.visual_profile = VG_VISUAL_PROFILE_RETRO;
            else
                return usage();
            session.present |= VG_SETTING_VISUAL_PROFILE;
        } else if (strcmp(argument, "--smoke-door") == 0) {
            smoke_door = true;
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
    return vg_demo_3d_run(smoke_frames, capture, show_colliders, level_path,
                          smoke_door, settings_path,
                          session.present != 0u ? &session : NULL);
}

int main(int argc, char **argv) {
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int wide_count = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (wide == NULL)
        return 2;
    if (wide_count < 1) {
        (void)LocalFree(wide);
        return 2;
    }
    char **utf8 = calloc((size_t)wide_count, sizeof(*utf8));
    if (utf8 == NULL) {
        (void)LocalFree(wide);
        return 2;
    }
    bool ok = true;
    for (int i = 0; i < wide_count; ++i) {
        int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                         wide[i], -1, NULL, 0, NULL, NULL);
        if (length <= 0 || (utf8[i] = malloc((size_t)length)) == NULL ||
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[i], -1,
                                utf8[i], length, NULL, NULL) == 0) {
            ok = false;
            break;
        }
    }
    int result = ok ? demo_main(wide_count, utf8) : 2;
    for (int i = 0; i < wide_count; ++i)
        free(utf8[i]);
    free(utf8);
    (void)LocalFree(wide);
    return result;
#else
    return demo_main(argc, argv);
#endif
}
