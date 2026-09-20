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
    size_t sound_count;
    bool audio, hidden, captured;
};

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
    if (config.audio) {
        InitAudioDevice();
        p->audio = IsAudioDeviceReady();
        if (!p->audio)
            (void)fprintf(stderr, "Audio no disponible: el juego continuara sin sonido.\n");
    }
    SetTargetFPS(config.hidden ? 0 : 120);
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
    const struct {
        int key;
        uint32_t action;
    } bindings[] = {{KEY_SPACE, RE_JUMP},    {KEY_E, RE_INTERACT},   {KEY_ESCAPE, RE_PAUSE},
                    {KEY_ENTER, RE_ACCEPT},  {KEY_UP, RE_UP},        {KEY_DOWN, RE_DOWN},
                    {KEY_LEFT, RE_LEFT},     {KEY_RIGHT, RE_RIGHT},  {KEY_F1, RE_MAP},
                    {KEY_F2, RE_WIRE},       {KEY_F3, RE_DEPTH},     {KEY_F4, RE_STATS},
                    {KEY_F5, RE_QUICK_SAVE}, {KEY_F9, RE_QUICK_LOAD}};
    for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
        if (IsKeyPressed(bindings[i].key))
            input.pressed |= bindings[i].action;
        if (IsKeyDown(bindings[i].key))
            input.held |= bindings[i].action;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        input.pressed |= RE_PRIMARY;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        input.held |= RE_PRIMARY;
    input.movement = re_v2((float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A)),
                           (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S)));
    if (re_length2(input.movement) > 1)
        input.movement = re_normalize2(input.movement);
    if (p->captured && input.focused) {
        Vector2 mouse = GetMouseDelta();
        input.look = re_v2(mouse.x, mouse.y);
    }
    if (IsKeyPressed(KEY_F11))
        ToggleBorderlessWindowed();
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
