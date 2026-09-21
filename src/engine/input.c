/* Conservación de eventos y prevención de la espiral de recuperación. */
#include "retro/input.h"

void re_input_accumulate(ReInput *pending, ReInput frame) {
    if (!frame.focused) {
        bool quit = pending->quit || frame.quit;
        *pending = (ReInput){.quit = quit};
        return;
    }
    pending->movement = frame.movement;
    pending->held = frame.held;
    pending->pressed |= frame.pressed;
    pending->released |= frame.released;
    pending->look = re_add2(pending->look, frame.look);
    pending->focused = frame.focused;
    pending->quit = pending->quit || frame.quit;
}
ReInput re_input_consume(ReInput *pending) {
    ReInput result = *pending;
    pending->pressed = 0;
    pending->released = 0;
    pending->look = re_v2(0, 0);
    return result;
}
int re_clock_advance(ReClock *clock, double elapsed, bool active) {
    if (!active) {
        clock->accumulator = 0;
        return 0;
    }
    if (!isfinite(elapsed) || elapsed < 0)
        elapsed = 0;
    if (elapsed > 0.25)
        elapsed = 0.25;
    const double tick = 1.0 / 60.0;
    clock->accumulator += elapsed;
    int ticks = (int)(clock->accumulator / tick);
    clock->accumulator -= (double)ticks * tick;
    if (ticks > 8) {
        clock->dropped_ticks += (unsigned int)(ticks - 8);
        ticks = 8;
    }
    return ticks;
}
float re_clock_alpha(const ReClock *clock) {
    return (float)(clock->accumulator * 60.0);
}
