/* Entrada semántica y reloj fijo. No hay teclas concretas en la simulación. */
#ifndef RETRO_INPUT_H
#define RETRO_INPUT_H
#include "retro/math.h"
#include <stdint.h>
enum ReAction {
    RE_JUMP = 1u << 0,
    RE_PRIMARY = 1u << 1,
    RE_INTERACT = 1u << 2,
    RE_PAUSE = 1u << 3,
    RE_ACCEPT = 1u << 4,
    RE_UP = 1u << 5,
    RE_DOWN = 1u << 6,
    RE_LEFT = 1u << 7,
    RE_RIGHT = 1u << 8,
    RE_MAP = 1u << 9,
    RE_WIRE = 1u << 10,
    RE_DEPTH = 1u << 11,
    RE_STATS = 1u << 12,
    RE_QUICK_SAVE = 1u << 13,
    RE_QUICK_LOAD = 1u << 14
};
typedef struct ReInput {
    ReVec2 movement; /* x=strafe, y=adelante, longitud <=1. */
    ReVec2 look;     /* Desplazamiento del ratón, en píxeles relativos. */
    uint32_t pressed, held;
    bool focused, quit;
} ReInput;
typedef struct ReClock {
    double accumulator;
    unsigned int dropped_ticks;
} ReClock;
/* Un frame sin tick conserva pulsaciones y movimiento de ratón. consume borra
 * sólo eventos; las teclas mantenidas siguen activas en ticks posteriores. */
void re_input_accumulate(ReInput *pending, ReInput frame);
ReInput re_input_consume(ReInput *pending);
/* Añade tiempo real acotado. Devuelve <=8 ticks y deja resto para interpolar. */
int re_clock_advance(ReClock *clock, double elapsed, bool active);
float re_clock_alpha(const ReClock *clock);
#define RE_FIXED_DT (1.0f / 60.0f)
#endif
