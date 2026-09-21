#include "runtime/runtime_internal.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    VG_GAME_MAGIC = 0x5647474Du,
    VG_DEFAULT_GAME_STEPS = 8u,
    VG_DEFAULT_EVENT_CAPACITY = 256u,
    VG_DEFAULT_EVENTS_PER_TICK = 64u,
    VG_MAX_GAME_STEPS = 1024u,
    VG_MAX_EVENT_CAPACITY = 65536u
};

struct VgGame {
    uint32_t magic;
    VgContext *context;
    VgGame *next;
    VgGameCallbacks callbacks;
    VgEvent *events;
    VgWorld world;
    uint32_t event_capacity;
    uint32_t event_head;
    uint32_t event_count;
    uint32_t max_events_per_tick;
    uint32_t max_fixed_steps;
    double fixed_delta;
    double max_frame_delta;
    double accumulator;
    float interpolation_alpha;
    bool initialized;
    bool world_is_ready;
    bool operating;
};

static bool vg_game_valid(const VgGame *game) {
    return game != NULL && game->magic == VG_GAME_MAGIC && vg_runtime_context_valid(game->context);
}

static bool vg_game_field_present(uint32_t struct_size, size_t offset, size_t field_size) {
    return (uint64_t)struct_size >= (uint64_t)offset + (uint64_t)field_size;
}

static void vg_game_enter_callback(VgGame *game) {
    ++game->context->game_callback_depth;
}

static void vg_game_leave_callback(VgGame *game) {
    --game->context->game_callback_depth;
}

static void vg_game_call_shutdown(VgGame *game) {
    if (!game->initialized)
        return;
    game->initialized = false;
    if (game->callbacks.shutdown != NULL) {
        vg_game_enter_callback(game);
        game->callbacks.shutdown(game->context, game->callbacks.user);
        vg_game_leave_callback(game);
    }
}

static void vg_game_unlink(VgGame *game) {
    VgGame **link = &game->context->games;
    while (*link != NULL && *link != game)
        link = &(*link)->next;
    if (*link == game)
        *link = game->next;
}

static void vg_game_release(VgGame *game) {
    VgContext *context = game->context;
    VgEvent *events = game->events;
    game->magic = 0u;
    vg_runtime_deallocate(context, events);
    vg_runtime_deallocate(context, game);
}

VgResult vg_game_create(VgContext *context, const VgGameDesc *description,
                        const VgGameCallbacks *callbacks, VgGame **out_game) {
    if (!vg_runtime_context_valid(context) || out_game == NULL || callbacks == NULL ||
        callbacks->struct_size < offsetof(VgGameCallbacks, user) + sizeof(callbacks->user) ||
        callbacks->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    if (context->destroying || context->game_callback_depth != 0u)
        return VG_ERROR_REENTRANT;

    double fixed_delta = 1.0 / 60.0;
    double max_frame_delta = 0.25;
    uint32_t max_fixed_steps = VG_DEFAULT_GAME_STEPS;
    uint32_t event_capacity = VG_DEFAULT_EVENT_CAPACITY;
    uint32_t max_events_per_tick = VG_DEFAULT_EVENTS_PER_TICK;
    if (description != NULL) {
        if (description->struct_size <
                offsetof(VgGameDesc, api_version) + sizeof(description->api_version) ||
            description->api_version != VG_API_VERSION)
            return VG_ERROR_INVALID_ARGUMENT;
        if (vg_game_field_present(description->struct_size, offsetof(VgGameDesc, reserved),
                                  sizeof(description->reserved)) &&
            description->reserved != 0u)
            return VG_ERROR_INVALID_ARGUMENT;
#define VG_READ_GAME_DESC(field, destination)                                                      \
    do {                                                                                           \
        if (vg_game_field_present(description->struct_size, offsetof(VgGameDesc, field),           \
                                  sizeof(description->field)) &&                                   \
            description->field != 0)                                                               \
            destination = description->field;                                                      \
    } while (0)
        VG_READ_GAME_DESC(fixed_delta_seconds, fixed_delta);
        VG_READ_GAME_DESC(max_frame_delta_seconds, max_frame_delta);
        VG_READ_GAME_DESC(max_fixed_steps_per_frame, max_fixed_steps);
        VG_READ_GAME_DESC(event_capacity, event_capacity);
        VG_READ_GAME_DESC(max_events_per_tick, max_events_per_tick);
#undef VG_READ_GAME_DESC
    }
    if (!isfinite(fixed_delta) || !isfinite(max_frame_delta) || fixed_delta < 0.000001 ||
        max_frame_delta < fixed_delta || max_frame_delta > 10.0 || max_fixed_steps == 0u ||
        max_fixed_steps > VG_MAX_GAME_STEPS || event_capacity == 0u ||
        event_capacity > VG_MAX_EVENT_CAPACITY || max_events_per_tick == 0u ||
        max_events_per_tick > event_capacity)
        return VG_ERROR_INVALID_ARGUMENT;

    VgGame *game = vg_runtime_allocate(context, sizeof(*game));
    if (game == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memset(game, 0, sizeof(*game));
    VgEvent *events = vg_runtime_allocate(context, sizeof(*events) * event_capacity);
    if (events == NULL) {
        vg_runtime_deallocate(context, game);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    memset(events, 0, sizeof(*events) * event_capacity);
    game->magic = VG_GAME_MAGIC;
    game->context = context;
    game->events = events;
    game->event_capacity = event_capacity;
    game->max_events_per_tick = max_events_per_tick;
    game->max_fixed_steps = max_fixed_steps;
    game->fixed_delta = fixed_delta;
    game->max_frame_delta = max_frame_delta;
    size_t callback_bytes = callbacks->struct_size;
    if (callback_bytes > sizeof(game->callbacks))
        callback_bytes = sizeof(game->callbacks);
    memcpy(&game->callbacks, callbacks, callback_bytes);
    game->callbacks.struct_size = sizeof(VgGameCallbacks);

    game->operating = true;
    game->initialized = true;
    VgResult result = VG_OK;
    if (game->callbacks.init != NULL) {
        vg_game_enter_callback(game);
        result = game->callbacks.init(context, game->callbacks.user);
        vg_game_leave_callback(game);
    }
    game->operating = false;
    if (result != VG_OK) {
        game->operating = true;
        vg_game_call_shutdown(game);
        game->operating = false;
        vg_game_release(game);
        return result;
    }
    game->next = context->games;
    context->games = game;
    *out_game = game;
    return VG_OK;
}

VgResult vg_game_set_world(VgGame *game, VgWorld world) {
    if (!vg_game_valid(game))
        return VG_ERROR_INVALID_ARGUMENT;
    if (game->operating || game->context->game_callback_depth != 0u || game->context->destroying)
        return VG_ERROR_REENTRANT;
    VgResult result = vg_runtime_resolve_world(game->context, world, NULL, NULL);
    if (result != VG_OK)
        return result;
    if (game->world_is_ready && game->world.value == world.value)
        return VG_ERROR_CONFLICT;
    if (game->event_count != 0u)
        return VG_ERROR_CONFLICT;
    VgWorld previous_world = game->world;
    bool previous_ready = game->world_is_ready;
    game->world = world;
    game->world_is_ready = true;
    game->event_head = 0u;
    game->operating = true;
    if (game->callbacks.world_ready != NULL) {
        vg_game_enter_callback(game);
        result = game->callbacks.world_ready(game->context, world, game->callbacks.user);
        vg_game_leave_callback(game);
    }
    game->operating = false;
    if (result != VG_OK) {
        game->world = previous_world;
        game->world_is_ready = previous_ready;
        game->event_head = 0u;
        game->event_count = 0u;
        return result;
    }
    game->accumulator = 0.0;
    game->interpolation_alpha = 0.0f;
    return VG_OK;
}

VgResult vg_game_clear_world(VgGame *game) {
    if (!vg_game_valid(game))
        return VG_ERROR_INVALID_ARGUMENT;
    if (game->operating || game->context->game_callback_depth != 0u || game->context->destroying)
        return VG_ERROR_REENTRANT;
    if (!game->world_is_ready)
        return VG_ERROR_NOT_FOUND;
    game->world.value = VG_INVALID_HANDLE_VALUE;
    game->world_is_ready = false;
    game->event_head = 0u;
    game->event_count = 0u;
    game->accumulator = 0.0;
    game->interpolation_alpha = 0.0f;
    return VG_OK;
}

VgResult vg_game_emit_event(VgGame *game, const VgEvent *event) {
    if (!vg_game_valid(game) || event == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (game->context->destroying || !game->world_is_ready)
        return VG_ERROR_CONFLICT;
    if (event->struct_size < offsetof(VgEvent, payload) || event->api_version != VG_API_VERSION ||
        event->payload_size > VG_EVENT_PAYLOAD_CAPACITY ||
        (uint64_t)event->struct_size < (uint64_t)offsetof(VgEvent, payload) + event->payload_size ||
        event->reserved != 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    uint32_t bound_world_index = 0u;
    VgResult result =
        vg_runtime_resolve_world(game->context, game->world, &bound_world_index, NULL);
    if (result != VG_OK)
        return result;
    if (event->source.value != VG_INVALID_HANDLE_VALUE) {
        uint32_t event_world_index = 0u;
        result =
            vg_runtime_resolve_entity(game->context, event->source, &event_world_index, NULL, NULL);
        if (result != VG_OK)
            return result;
        if (event_world_index != bound_world_index)
            return VG_ERROR_WRONG_WORLD;
    }
    if (event->target.value != VG_INVALID_HANDLE_VALUE) {
        uint32_t event_world_index = 0u;
        result =
            vg_runtime_resolve_entity(game->context, event->target, &event_world_index, NULL, NULL);
        if (result != VG_OK)
            return result;
        if (event_world_index != bound_world_index)
            return VG_ERROR_WRONG_WORLD;
    }
    if (game->event_count >= game->event_capacity)
        return VG_ERROR_CAPACITY;
    uint32_t tail = (game->event_head + game->event_count) % game->event_capacity;
    VgEvent copy = {0};
    copy.struct_size = sizeof(copy);
    copy.api_version = VG_API_VERSION;
    copy.type = event->type;
    copy.flags = event->flags;
    copy.source = event->source;
    copy.target = event->target;
    copy.payload_size = event->payload_size;
    if (copy.payload_size != 0u)
        memcpy(copy.payload, event->payload, copy.payload_size);
    game->events[tail] = copy;
    ++game->event_count;
    return VG_OK;
}

static uint32_t vg_game_drain_events(VgGame *game) {
    uint32_t dispatched = 0u;
    while (dispatched < game->max_events_per_tick && game->event_count != 0u) {
        VgEvent event = game->events[game->event_head];
        game->event_head = (game->event_head + 1u) % game->event_capacity;
        --game->event_count;
        ++dispatched;
        if (game->callbacks.event != NULL) {
            vg_game_enter_callback(game);
            game->callbacks.event(game->context, &event, game->callbacks.user);
            vg_game_leave_callback(game);
        }
    }
    return dispatched;
}

VgResult vg_game_step(VgGame *game, double elapsed_seconds, VgStepInfo *out_info) {
    if (!vg_game_valid(game) || out_info == NULL ||
        out_info->struct_size < offsetof(VgStepInfo, api_version) + sizeof(out_info->api_version) ||
        out_info->api_version != VG_API_VERSION || !isfinite(elapsed_seconds) ||
        elapsed_seconds < 0.0)
        return VG_ERROR_INVALID_ARGUMENT;
    if (game->operating || game->context->game_callback_depth != 0u || game->context->destroying)
        return VG_ERROR_REENTRANT;
    if (!game->world_is_ready)
        return VG_ERROR_CONFLICT;
    VgResult result = vg_runtime_resolve_world(game->context, game->world, NULL, NULL);
    if (result != VG_OK)
        return result;

    double accepted_elapsed = elapsed_seconds;
    if (accepted_elapsed > game->max_frame_delta)
        accepted_elapsed = game->max_frame_delta;
    game->accumulator += accepted_elapsed;
    uint32_t steps = 0u;
    uint32_t events = 0u;
    game->operating = true;
    while (game->accumulator >= game->fixed_delta && steps < game->max_fixed_steps) {
        if (game->callbacks.fixed_update != NULL) {
            vg_game_enter_callback(game);
            game->callbacks.fixed_update(game->context, game->world, (float)game->fixed_delta,
                                         game->callbacks.user);
            vg_game_leave_callback(game);
        }
        events += vg_game_drain_events(game);
        game->accumulator -= game->fixed_delta;
        ++steps;
    }
    game->operating = false;

    uint32_t dropped_steps = 0u;
    double dropped_simulation = 0.0;
    if (game->accumulator >= game->fixed_delta) {
        double whole_steps = floor(game->accumulator / game->fixed_delta);
        if (whole_steps > (double)UINT32_MAX)
            dropped_steps = UINT32_MAX;
        else
            dropped_steps = (uint32_t)whole_steps;
        dropped_simulation = whole_steps * game->fixed_delta;
        game->accumulator -= dropped_simulation;
    }
    game->interpolation_alpha = (float)(game->accumulator / game->fixed_delta);
    VgStepInfo info = {sizeof(info),
                       VG_API_VERSION,
                       steps,
                       events,
                       dropped_steps,
                       game->event_count,
                       (double)steps * game->fixed_delta,
                       (elapsed_seconds - accepted_elapsed) + dropped_simulation,
                       game->interpolation_alpha,
                       0u};
    uint32_t output_capacity = out_info->struct_size;
    info.struct_size = output_capacity;
    size_t output_bytes = output_capacity;
    if (output_bytes > sizeof(info))
        output_bytes = sizeof(info);
    memcpy(out_info, &info, output_bytes);
    return VG_OK;
}

VgResult vg_game_draw_ui(VgGame *game, VgUiFrame *frame) {
    if (!vg_game_valid(game) || frame == NULL ||
        frame->struct_size < offsetof(VgUiFrame, api_version) + sizeof(frame->api_version) ||
        frame->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    if (game->operating || game->context->game_callback_depth != 0u || game->context->destroying)
        return VG_ERROR_REENTRANT;
    if (!game->world_is_ready)
        return VG_ERROR_CONFLICT;
    uint32_t frame_capacity = frame->struct_size;
    VgUiFrame callback_frame = {0};
    size_t frame_bytes = frame_capacity;
    if (frame_bytes > sizeof(callback_frame))
        frame_bytes = sizeof(callback_frame);
    memcpy(&callback_frame, frame, frame_bytes);
    callback_frame.struct_size = sizeof(callback_frame);
    callback_frame.api_version = VG_API_VERSION;
    if (callback_frame.reserved != 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    game->operating = true;
    callback_frame.interpolation_alpha = game->interpolation_alpha;
    if (game->callbacks.draw_ui != NULL) {
        vg_game_enter_callback(game);
        game->callbacks.draw_ui(game->context, &callback_frame, game->callbacks.user);
        vg_game_leave_callback(game);
    }
    game->operating = false;
    callback_frame.struct_size = frame_capacity;
    memcpy(frame, &callback_frame, frame_bytes);
    return VG_OK;
}

VgResult vg_game_destroy(VgGame *game) {
    if (!vg_game_valid(game))
        return VG_ERROR_INVALID_ARGUMENT;
    if (game->operating || game->context->game_callback_depth != 0u || game->context->destroying)
        return VG_ERROR_REENTRANT;
    game->operating = true;
    vg_game_call_shutdown(game);
    vg_game_unlink(game);
    game->operating = false;
    vg_game_release(game);
    return VG_OK;
}

bool vg_game_world_is_bound(const VgContext *context, VgWorld world) {
    if (!vg_runtime_context_valid(context))
        return false;
    for (const VgGame *game = context->games; game != NULL; game = game->next) {
        if (game->world_is_ready && game->world.value == world.value)
            return true;
    }
    return false;
}

void vg_game_destroy_all(VgContext *context) {
    while (context->games != NULL) {
        VgGame *game = context->games;
        game->operating = true;
        vg_game_call_shutdown(game);
        game->operating = false;
        context->games = game->next;
        vg_game_release(game);
    }
}
