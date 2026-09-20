/* Sesión jugable compartida entre Player y Studio.
 * create copia el proyecto: el llamante conserva su propiedad y puede liberarlo.
 * destroy libera texturas, framebuffer y estado en el mismo módulo que los creó.
 * Las llamadas de un handle se realizan desde un único hilo; copy_pixels copia
 * RGBA8 al búfer del llamante y no entrega punteros con vida útil ambigua a WPF. */
#ifndef RETRO_SESSION_H
#define RETRO_SESSION_H
#include "retro/input.h"
#include "retro/project.h"
#include "retro/render.h"
typedef struct ReGameSession ReGameSession;
int re_session_create(const ReProject *project, int preview, int menu, ReGameSession **out,
                      ReError *error);
void re_session_destroy(ReGameSession *session);
/* elapsed en segundos; movimiento normalizado; look en píxeles relativos.
 * single_step avanza exactamente un tick, aunque focused sea cero. */
void re_session_frame(ReGameSession *session, double elapsed, float move_x, float move_y,
                      float look_x, float look_y, uint32_t pressed, uint32_t held, int focused,
                      int single_step);
const ReRenderer *re_session_renderer(const ReGameSession *session);
int re_session_copy_pixels(const ReGameSession *session, void *destination, uint32_t bytes);
/* Bits: 1 = salir; 2 = la partida acepta mirada/movimiento. */
int re_session_flags(const ReGameSession *session);
size_t re_session_memory(const ReGameSession *session);
#endif
