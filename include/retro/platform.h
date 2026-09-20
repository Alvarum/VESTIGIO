/* Adaptador de plataforma. El tipo incompleto oculta raylib y sus recursos GPU.
 * Sólo puede existir una instancia de ventana a la vez (restricción de raylib). */
#ifndef RETRO_PLATFORM_H
#define RETRO_PLATFORM_H
#include "retro/input.h"
#include "retro/render.h"

typedef struct RePlatform RePlatform;
typedef struct RePlatformConfig {
    const char *title;
    int framebuffer_width, framebuffer_height;
    bool hidden, audio;
} RePlatformConfig;
[[nodiscard]] RePlatform *re_platform_open(RePlatformConfig config, ReError *error);
void re_platform_close(RePlatform *platform);
ReInput re_platform_input(RePlatform *platform);
void re_platform_capture_mouse(RePlatform *platform, bool captured);
void re_platform_present(RePlatform *platform, const ReRenderer *renderer);
double re_platform_time(void);
/* Rutas relativas al ejecutable, nunca al directorio actual. false = truncado. */
[[nodiscard]] bool re_platform_application_path(const char *relative, char *out, size_t capacity);
[[nodiscard]] bool re_platform_asset_path(const char *relative, char *out, size_t capacity);
/* Crea %LOCALAPPDATA%/RetroForge/<project_id> y devuelve un archivo dentro.
 * project_id sólo admite ASCII alfanumérico, guion y guion bajo. */
[[nodiscard]] bool re_platform_user_path(const char *project_id, const char *relative, char *out,
                                         size_t capacity);
[[nodiscard]] bool re_platform_image_load(const char *path, ReTexture *out);
[[nodiscard]] bool re_platform_capture_png(const ReRenderer *renderer, const char *path);
/* Captura de la presentación real: lee el framebuffer GPU tras present(). */
[[nodiscard]] bool re_platform_capture_window(const char *path);
void re_platform_resize(int width, int height);
/* La plataforma COPIA PCM mono de 16 bits; el llamante puede liberar samples.
 * Devuelve -1 si no hay dispositivo o slots. Capacidad: 16 sonidos. */
int re_platform_sound(RePlatform *platform, const int16_t *samples, size_t count,
                      unsigned int rate);
void re_platform_play(RePlatform *platform, int sound, float volume, float pan);
bool re_platform_audio_ready(const RePlatform *platform);
#endif
