/* Único archivo que incluye raylib.h. No implementa reglas ni render 3D.
 * Propiedad: ventana -> textura GPU -> sonidos. Se libera en orden inverso. */
#include "raylib.h"
#include "retro/platform.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

struct RePlatform {
    Texture2D presentation;
    Sound sounds[16];
    VgInputBinding bindings[VG_SETTINGS_MAX_BINDINGS];
    size_t sound_count;
    uint32_t binding_count;
    bool audio, hidden, captured;
};

static const VgInputBinding default_bindings[] = {
    {VG_ACTION_MOVE_FORWARD, VG_INPUT_KEY_W, 0u}, {VG_ACTION_MOVE_BACKWARD, VG_INPUT_KEY_S, 0u},
    {VG_ACTION_MOVE_LEFT, VG_INPUT_KEY_A, 0u},    {VG_ACTION_MOVE_RIGHT, VG_INPUT_KEY_D, 0u},
    {VG_ACTION_JUMP, VG_INPUT_KEY_SPACE, 0u},     {VG_ACTION_PRIMARY, VG_INPUT_MOUSE_PRIMARY, 0u},
    {VG_ACTION_INTERACT, VG_INPUT_KEY_E, 0u},     {VG_ACTION_PAUSE, VG_INPUT_KEY_ESCAPE, 0u},
    {VG_ACTION_ACCEPT, VG_INPUT_KEY_ENTER, 0u},   {VG_ACTION_UI_UP, VG_INPUT_KEY_UP, 0u},
    {VG_ACTION_UI_DOWN, VG_INPUT_KEY_DOWN, 0u},   {VG_ACTION_UI_LEFT, VG_INPUT_KEY_LEFT, 0u},
    {VG_ACTION_UI_RIGHT, VG_INPUT_KEY_RIGHT, 0u}, {VG_ACTION_MAP, VG_INPUT_KEY_F1, 0u},
    {VG_ACTION_WIREFRAME, VG_INPUT_KEY_F2, 0u},   {VG_ACTION_DEPTH, VG_INPUT_KEY_F3, 0u},
    {VG_ACTION_STATS, VG_INPUT_KEY_F4, 0u},       {VG_ACTION_QUICK_SAVE, VG_INPUT_KEY_F5, 0u},
    {VG_ACTION_QUICK_LOAD, VG_INPUT_KEY_F9, 0u},
};

static int key_from_input_code(VgInputCode code) {
    switch (code) {
    case VG_INPUT_KEY_A:
        return KEY_A;
    case VG_INPUT_KEY_D:
        return KEY_D;
    case VG_INPUT_KEY_E:
        return KEY_E;
    case VG_INPUT_KEY_S:
        return KEY_S;
    case VG_INPUT_KEY_W:
        return KEY_W;
    case VG_INPUT_KEY_ENTER:
        return KEY_ENTER;
    case VG_INPUT_KEY_ESCAPE:
        return KEY_ESCAPE;
    case VG_INPUT_KEY_SPACE:
        return KEY_SPACE;
    case VG_INPUT_KEY_F1:
        return KEY_F1;
    case VG_INPUT_KEY_F2:
        return KEY_F2;
    case VG_INPUT_KEY_F3:
        return KEY_F3;
    case VG_INPUT_KEY_F4:
        return KEY_F4;
    case VG_INPUT_KEY_F5:
        return KEY_F5;
    case VG_INPUT_KEY_F9:
        return KEY_F9;
    case VG_INPUT_KEY_RIGHT:
        return KEY_RIGHT;
    case VG_INPUT_KEY_LEFT:
        return KEY_LEFT;
    case VG_INPUT_KEY_DOWN:
        return KEY_DOWN;
    case VG_INPUT_KEY_UP:
        return KEY_UP;
    default:
        return KEY_NULL;
    }
}

RePlatform *re_platform_open(RePlatformConfig config, ReError *error) {
    RePlatform *p = calloc(1, sizeof(*p));
    if (!p) {
        (void)snprintf(error->message, sizeof(error->message), "Sin memoria para plataforma");
        return nullptr;
    }
    SetTraceLogLevel(LOG_WARNING);
    unsigned int flags = FLAG_WINDOW_RESIZABLE;
    if (config.hidden)
        flags |= FLAG_WINDOW_HIDDEN;
    if (config.fullscreen)
        flags |= FLAG_FULLSCREEN_MODE;
    if (config.vsync)
        flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    InitWindow(config.framebuffer_width * 3, config.framebuffer_height * 3, config.title);
    if (!IsWindowReady()) {
        (void)snprintf(error->message, sizeof(error->message),
                       "No se pudo abrir la ventana OpenGL");
        free(p);
        return nullptr;
    }
    SetWindowMinSize(config.framebuffer_width, config.framebuffer_height);
    SetExitKey(KEY_NULL); /* Escape pertenece al menú, no al cierre inmediato. */
    Image blank = GenImageColor(config.framebuffer_width, config.framebuffer_height, BLACK);
    p->presentation = LoadTextureFromImage(blank);
    UnloadImage(blank);
    if (p->presentation.id == 0) {
        (void)snprintf(error->message, sizeof(error->message),
                       "No se pudo crear textura de presentacion");
        CloseWindow();
        free(p);
        return nullptr;
    }
    SetTextureFilter(p->presentation, TEXTURE_FILTER_POINT);
    p->hidden = config.hidden;
    if (config.binding_count > VG_SETTINGS_MAX_BINDINGS ||
        (config.binding_count != 0u && config.bindings == NULL)) {
        (void)snprintf(error->message, sizeof(error->message), "Bindings de plataforma inválidos");
        re_platform_close(p);
        return nullptr;
    }
    p->binding_count = config.binding_count;
    if (config.binding_count != 0u) {
        memcpy(p->bindings, config.bindings, sizeof(p->bindings[0]) * config.binding_count);
    } else {
        p->binding_count = (uint32_t)(sizeof(default_bindings) / sizeof(default_bindings[0]));
        memcpy(p->bindings, default_bindings, sizeof(default_bindings));
    }
    if (config.audio) {
        InitAudioDevice();
        p->audio = IsAudioDeviceReady();
        if (!p->audio)
            (void)fprintf(stderr, "Audio no disponible: el juego continuara sin sonido.\n");
    }
    SetTargetFPS(config.hidden ? 0 : (int)config.frame_cap);
    return p;
}
void re_platform_close(RePlatform *p) {
    if (!p)
        return;
    for (size_t i = 0; i < p->sound_count; i++)
        UnloadSound(p->sounds[i]);
    if (p->audio)
        CloseAudioDevice();
    UnloadTexture(p->presentation);
    CloseWindow();
    free(p);
}
ReInput re_platform_input(RePlatform *p) {
    ReInput input = {.focused = p->hidden || IsWindowFocused(), .quit = WindowShouldClose()};
    for (uint32_t i = 0u; i < p->binding_count; ++i) {
        VgInputBinding binding = p->bindings[i];
        int key = key_from_input_code(binding.code);
        bool pressed = binding.code == VG_INPUT_MOUSE_PRIMARY
                           ? IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
                           : key != KEY_NULL && IsKeyPressed(key);
        bool held = binding.code == VG_INPUT_MOUSE_PRIMARY ? IsMouseButtonDown(MOUSE_BUTTON_LEFT)
                                                           : key != KEY_NULL && IsKeyDown(key);
        bool released = binding.code == VG_INPUT_MOUSE_PRIMARY
                            ? IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
                            : key != KEY_NULL && IsKeyReleased(key);
        if (binding.action == VG_ACTION_MOVE_FORWARD)
            input.movement.y += held ? 1.0f : 0.0f;
        else if (binding.action == VG_ACTION_MOVE_BACKWARD)
            input.movement.y -= held ? 1.0f : 0.0f;
        else if (binding.action == VG_ACTION_MOVE_LEFT)
            input.movement.x -= held ? 1.0f : 0.0f;
        else if (binding.action == VG_ACTION_MOVE_RIGHT)
            input.movement.x += held ? 1.0f : 0.0f;
        else {
            uint32_t action = (uint32_t)binding.action;
            if (pressed)
                input.pressed |= action;
            if (held)
                input.held |= action;
            if (released)
                input.released |= action;
        }
    }
    if (re_length2(input.movement) > 1)
        input.movement = re_normalize2(input.movement);
    if (p->captured && input.focused) {
        Vector2 mouse = GetMouseDelta();
        input.look = re_v2(mouse.x, mouse.y);
    }
    return input;
}
void re_platform_capture_mouse(RePlatform *p, bool captured) {
    if (p->hidden || p->captured == captured)
        return;
    p->captured = captured;
    if (captured)
        DisableCursor();
    else
        EnableCursor();
}
void re_platform_present(RePlatform *p, const ReRenderer *r) {
    UpdateTexture(p->presentation, r->pixels);
    float width = (float)GetScreenWidth(), height = (float)GetScreenHeight();
    float scale = fminf(width / (float)r->width, height / (float)r->height);
    if (scale >= 1)
        scale = floorf(scale);
    float draw_width = (float)r->width * scale, draw_height = (float)r->height * scale;
    BeginDrawing();
    ClearBackground((Color){5, 8, 12, 255});
    DrawTexturePro(p->presentation, (Rectangle){0, 0, (float)r->width, (float)r->height},
                   (Rectangle){(width - draw_width) * 0.5f, (height - draw_height) * 0.5f,
                               draw_width, draw_height},
                   (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
}
double re_platform_time(void) {
    return GetTime();
}
bool re_platform_asset_path(const char *relative, char *out, size_t capacity) {
    int written = snprintf(out, capacity, "%sassets/%s", GetApplicationDirectory(), relative);
    return written >= 0 && (size_t)written < capacity;
}
bool re_platform_user_path(const char *project_id, const char *relative, char *out,
                           size_t capacity) {
    if (!project_id || !*project_id || !relative || !*relative || !out || !capacity)
        return false;
    for (const char *p = project_id; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_')
            return false;
    const char *base = getenv("LOCALAPPDATA");
    if (!base || !*base)
        return false;
    char root[1024], project[1024];
    int root_written = snprintf(root, sizeof(root), "%s/RetroForge", base);
    int project_written = snprintf(project, sizeof(project), "%s/%s", root, project_id);
    int output_written = snprintf(out, capacity, "%s/%s", project, relative);
    if (root_written < 0 || (size_t)root_written >= sizeof(root) || project_written < 0 ||
        (size_t)project_written >= sizeof(project) || output_written < 0 ||
        (size_t)output_written >= capacity)
        return false;
#ifdef _WIN32
    (void)_mkdir(root);
    (void)_mkdir(project);
#else
    (void)mkdir(root, 0755);
    (void)mkdir(project, 0755);
#endif
    return true;
}
bool re_platform_application_path(const char *relative, char *out, size_t capacity) {
    int written = snprintf(out, capacity, "%s%s", GetApplicationDirectory(), relative);
    return written >= 0 && (size_t)written < capacity;
}
bool re_platform_image_load(const char *path, ReTexture *out) {
    Image image = LoadImage(path);
    if (!image.data)
        return false;
    ReTexture candidate = {0};
    if (!re_texture_init(&candidate, image.width, image.height)) {
        UnloadImage(image);
        return false;
    }
    Color *colors = LoadImageColors(image);
    if (!colors) {
        re_texture_destroy(&candidate);
        UnloadImage(image);
        return false;
    }
    for (size_t i = 0; i < (size_t)candidate.width * (size_t)candidate.height; i++)
        candidate.pixels[i] = re_rgba(colors[i].r, colors[i].g, colors[i].b, colors[i].a);
    UnloadImageColors(colors);
    UnloadImage(image);
    *out = candidate;
    return true;
}
bool re_platform_capture_png(const ReRenderer *r, const char *path) {
    Image image = {.data = r->pixels,
                   .width = r->width,
                   .height = r->height,
                   .mipmaps = 1,
                   .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    /* Image es aquí una VISTA prestada: nunca llamamos UnloadImage(image). */
    return ExportImage(image, path);
}

bool re_platform_capture_window(const char *path) {
    Image image = LoadImageFromScreen();
    if (!image.data)
        return false;
    bool ok = ExportImage(image, path);
    UnloadImage(image);
    return ok;
}

void re_platform_resize(int width, int height) {
    if (width > 0 && height > 0)
        SetWindowSize(width, height);
}
int re_platform_sound(RePlatform *p, const int16_t *samples, size_t count, unsigned int rate) {
    if (!p->audio || p->sound_count == 16 || count > UINT_MAX || rate == 0)
        return -1;
    Wave wave = {.frameCount = (unsigned int)count,
                 .sampleRate = rate,
                 .sampleSize = 16,
                 .channels = 1,
                 .data = (void *)samples};
    Sound sound = LoadSoundFromWave(wave);
    if (!sound.stream.buffer)
        return -1;
    int id = (int)p->sound_count;
    p->sounds[p->sound_count++] = sound;
    return id;
}
void re_platform_play(RePlatform *p, int id, float volume, float pan) {
    if (!p->audio || id < 0 || id >= (int)p->sound_count)
        return;
    Sound sound = p->sounds[id];
    SetSoundVolume(sound, re_clamp(volume, 0, 1));
    SetSoundPan(sound, re_clamp(pan, 0, 1));
    PlaySound(sound);
}
bool re_platform_audio_ready(const RePlatform *p) {
    return p->audio;
}
