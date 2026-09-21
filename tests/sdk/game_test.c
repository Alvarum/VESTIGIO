#include "vestigio/vestigio.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct TestAllocator {
    uint32_t calls;
    uint32_t fail_call;
    uint32_t outstanding;
} TestAllocator;

typedef struct AllocationHeader {
    TestAllocator *owner;
} AllocationHeader;

typedef struct GameState {
    VgGame *game;
    VgWorld world;
    VgEntity entity;
    char trace[64];
    uint32_t trace_length;
    uint32_t shutdown_count;
    uint32_t log_count;
    uint32_t payload_value;
    bool fail_init;
    bool fail_world;
    bool emit_from_event;
    bool shutdown_saw_world;
    VgResult nested_step;
    VgResult nested_draw;
    VgResult nested_destroy;
    VgResult nested_world;
    VgResult nested_create;
    VgResult emitted_result;
} GameState;

static void *test_allocate(void *user, uint64_t size) {
    TestAllocator *allocator = user;
    ++allocator->calls;
    if (allocator->fail_call != 0u && allocator->calls == allocator->fail_call)
        return NULL;
    if (size > (uint64_t)(SIZE_MAX - sizeof(AllocationHeader)))
        return NULL;
    AllocationHeader *header = malloc(sizeof(*header) + (size_t)size);
    if (header == NULL)
        return NULL;
    header->owner = allocator;
    ++allocator->outstanding;
    return header + 1;
}

static void test_deallocate(void *user, void *allocation) {
    TestAllocator *allocator = user;
    if (allocation == NULL)
        return;
    AllocationHeader *header = (AllocationHeader *)allocation - 1;
    if (header->owner == allocator && allocator->outstanding != 0u)
        --allocator->outstanding;
    free(header);
}

static void append_trace(GameState *state, char value) {
    if (state->trace_length + 1u < sizeof(state->trace)) {
        state->trace[state->trace_length++] = value;
        state->trace[state->trace_length] = '\0';
    }
}

static void test_log(void *user, VgLogSeverity severity, const char *message) {
    (void)severity;
    (void)message;
    GameState *state = user;
    ++state->log_count;
}

static VgResult game_init(VgContext *context, void *user) {
    (void)context;
    GameState *state = user;
    append_trace(state, 'I');
    return state->fail_init ? VG_ERROR_CONFLICT : VG_OK;
}

static VgResult game_world_ready(VgContext *context, VgWorld world, void *user) {
    (void)context;
    (void)world;
    GameState *state = user;
    append_trace(state, 'W');
    return state->fail_world ? VG_ERROR_CONFLICT : VG_OK;
}

static void game_fixed_update(VgContext *context, VgWorld world, float dt_seconds, void *user) {
    (void)world;
    (void)dt_seconds;
    GameState *state = user;
    append_trace(state, 'F');
    VgStepInfo nested_step = {0};
    nested_step.struct_size = sizeof(nested_step);
    nested_step.api_version = VG_API_VERSION;
    VgUiFrame nested_frame = {sizeof(nested_frame), VG_API_VERSION, 0u, 1u, 1u, 0.0f, 0u};
    state->nested_step = vg_game_step(state->game, 0.0, &nested_step);
    state->nested_draw = vg_game_draw_ui(state->game, &nested_frame);
    state->nested_destroy = vg_game_destroy(state->game);
    state->nested_world = vg_game_set_world(state->game, state->world);
    VgGameCallbacks nested_callbacks = {0};
    nested_callbacks.struct_size = sizeof(nested_callbacks);
    nested_callbacks.api_version = VG_API_VERSION;
    VgGame *nested_game = NULL;
    state->nested_create = vg_game_create(context, NULL, &nested_callbacks, &nested_game);
    vg_context_destroy(context);
}

static void game_event(VgContext *context, const VgEvent *event, void *user) {
    (void)context;
    GameState *state = user;
    append_trace(state, (char)('0' + event->type));
    if (event->payload_size == sizeof(uint32_t))
        memcpy(&state->payload_value, event->payload, sizeof(uint32_t));
    if (event->type == 1u && state->emit_from_event) {
        VgEvent generated = {0};
        generated.struct_size = sizeof(generated);
        generated.api_version = VG_API_VERSION;
        generated.type = 3u;
        state->emitted_result = vg_game_emit_event(state->game, &generated);
    }
}

static void game_draw_ui(VgContext *context, VgUiFrame *frame, void *user) {
    (void)context;
    GameState *state = user;
    append_trace(state, 'D');
    state->nested_draw = vg_game_draw_ui(state->game, frame);
}

static void game_shutdown(VgContext *context, void *user) {
    GameState *state = user;
    append_trace(state, 'S');
    ++state->shutdown_count;
    VgTransform transform;
    state->shutdown_saw_world =
        vg_entity_get_local_transform(context, state->entity, &transform) == VG_OK;
}

static VgGameCallbacks callbacks_for(GameState *state) {
    VgGameCallbacks callbacks = {0};
    callbacks.struct_size = sizeof(callbacks);
    callbacks.api_version = VG_API_VERSION;
    callbacks.user = state;
    callbacks.init = game_init;
    callbacks.world_ready = game_world_ready;
    callbacks.fixed_update = game_fixed_update;
    callbacks.event = game_event;
    callbacks.draw_ui = game_draw_ui;
    callbacks.shutdown = game_shutdown;
    return callbacks;
}

static int create_context_world(GameState *state, TestAllocator *allocator, VgContext **context) {
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    context_desc.user = state;
    context_desc.log = test_log;
    if (allocator != NULL) {
        context_desc.allocator_user = allocator;
        context_desc.allocate = test_allocate;
        context_desc.deallocate = test_deallocate;
    }
    if (vg_context_create(&context_desc, context) != VG_OK)
        return 1;
    VgWorldDesc world_desc = {sizeof(world_desc), VG_API_VERSION, 2u, 8u};
    if (vg_world_create(*context, &world_desc, &state->world) != VG_OK ||
        vg_entity_create(*context, state->world, &state->entity) != VG_OK)
        return 1;
    return 0;
}

static int test_lifecycle_stepping_events_and_camera(void) {
    GameState state = {0};
    VgContext *context = NULL;
    CHECK(create_context_world(&state, NULL, &context) == 0);

    VgCameraDesc camera = {sizeof(camera), VG_API_VERSION, VG_CAMERA_PERSPECTIVE, 0u, 1.0f, 0.0f,
                           0.1f,           100.0f};
    CHECK(vg_camera_set(context, state.entity, &camera) == VG_OK);
    VgCameraDesc actual = {0};
    actual.struct_size = sizeof(actual);
    actual.api_version = VG_API_VERSION;
    CHECK(vg_camera_get(context, state.entity, &actual) == VG_OK);
    CHECK(actual.projection == VG_CAMERA_PERSPECTIVE);
    VgCameraDesc camera_prefix = {0};
    camera_prefix.struct_size = offsetof(VgCameraDesc, projection);
    camera_prefix.api_version = VG_API_VERSION;
    camera_prefix.projection = UINT32_C(0xA5A5A5A5);
    CHECK(vg_camera_get(context, state.entity, &camera_prefix) == VG_OK);
    CHECK(vg_camera_get(context, state.entity, &camera_prefix) == VG_OK);
    CHECK(camera_prefix.struct_size == offsetof(VgCameraDesc, projection));
    CHECK(camera_prefix.projection == UINT32_C(0xA5A5A5A5));
    VgCameraDesc invalid = camera;
    invalid.near_clip_metres = invalid.far_clip_metres;
    CHECK(vg_camera_set(context, state.entity, &invalid) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(vg_camera_get(context, state.entity, &actual) == VG_OK);
    CHECK(actual.near_clip_metres == camera.near_clip_metres);

    VgGameDesc description = {sizeof(description), VG_API_VERSION, 0.1, 1.0, 2u, 2u, 2u, 0u};
    VgGameCallbacks callbacks = callbacks_for(&state);
    CHECK(vg_game_create(context, &description, &callbacks, &state.game) == VG_OK);
    CHECK(strcmp(state.trace, "I") == 0);
    CHECK(vg_game_set_world(state.game, state.world) == VG_OK);
    CHECK(strcmp(state.trace, "IW") == 0);
    CHECK(vg_world_destroy(context, state.world) == VG_ERROR_CONFLICT);

    VgWorld other_world = {0};
    VgEntity other_entity = {0};
    CHECK(vg_world_create(context, NULL, &other_world) == VG_OK);
    CHECK(vg_entity_create(context, other_world, &other_entity) == VG_OK);
    VgEvent wrong_world_event = {0};
    wrong_world_event.struct_size = sizeof(wrong_world_event);
    wrong_world_event.api_version = VG_API_VERSION;
    wrong_world_event.type = 9u;
    wrong_world_event.source = other_entity;
    CHECK(vg_game_emit_event(state.game, &wrong_world_event) == VG_ERROR_WRONG_WORLD);
    wrong_world_event.source.value = VG_INVALID_HANDLE_VALUE;
    wrong_world_event.target = other_entity;
    CHECK(vg_game_emit_event(state.game, &wrong_world_event) == VG_ERROR_WRONG_WORLD);

    state.emit_from_event = true;
    uint32_t payload = UINT32_C(0x12345678);
    VgEvent first = {0};
    first.struct_size = sizeof(first);
    first.api_version = VG_API_VERSION;
    first.type = 1u;
    first.source = state.entity;
    first.payload_size = sizeof(payload);
    memcpy(first.payload, &payload, sizeof(payload));
    VgEvent second = {0};
    second.struct_size = sizeof(second);
    second.api_version = VG_API_VERSION;
    second.type = 2u;
    CHECK(vg_game_emit_event(state.game, &first) == VG_OK);
    memset(first.payload, 0, sizeof(payload));
    CHECK(vg_game_emit_event(state.game, &second) == VG_OK);
    CHECK(vg_game_emit_event(state.game, &second) == VG_ERROR_CAPACITY);

    VgStepInfo step = {0};
    step.struct_size = sizeof(step);
    step.api_version = VG_API_VERSION;
    CHECK(vg_game_step(state.game, 0.1, &step) == VG_OK);
    CHECK(step.fixed_steps == 1u && step.events_dispatched == 2u && step.pending_events == 1u);
    CHECK(strcmp(state.trace, "IWF12") == 0);
    CHECK(state.payload_value == payload);
    CHECK(state.emitted_result == VG_OK);
    CHECK(state.nested_step == VG_ERROR_REENTRANT);
    CHECK(state.nested_draw == VG_ERROR_REENTRANT);
    CHECK(state.nested_destroy == VG_ERROR_REENTRANT);
    CHECK(state.nested_world == VG_ERROR_REENTRANT);
    CHECK(state.nested_create == VG_ERROR_REENTRANT);
    CHECK(state.log_count != 0u);

    CHECK(vg_game_step(state.game, 0.1, &step) == VG_OK);
    CHECK(step.fixed_steps == 1u && step.events_dispatched == 1u && step.pending_events == 0u);
    CHECK(strcmp(state.trace, "IWF12F3") == 0);
    VgStepInfo step_prefix = {0};
    step_prefix.struct_size = offsetof(VgStepInfo, fixed_steps);
    step_prefix.api_version = VG_API_VERSION;
    step_prefix.fixed_steps = UINT32_C(0xA5A5A5A5);
    CHECK(vg_game_step(state.game, 0.0, &step_prefix) == VG_OK);
    CHECK(vg_game_step(state.game, 0.0, &step_prefix) == VG_OK);
    CHECK(step_prefix.struct_size == offsetof(VgStepInfo, fixed_steps));
    CHECK(step_prefix.fixed_steps == UINT32_C(0xA5A5A5A5));
    VgUiFrame frame = {sizeof(frame), VG_API_VERSION, 7u, 640u, 360u, 9.0f, 0u};
    CHECK(vg_game_draw_ui(state.game, &frame) == VG_OK);
    CHECK(strcmp(state.trace, "IWF12F3D") == 0);
    CHECK(frame.interpolation_alpha >= 0.0f && frame.interpolation_alpha < 1.0f);
    VgUiFrame frame_prefix = {0};
    frame_prefix.struct_size = offsetof(VgUiFrame, frame_index);
    frame_prefix.api_version = VG_API_VERSION;
    frame_prefix.frame_index = UINT64_C(0xA5A5A5A5A5A5A5A5);
    CHECK(vg_game_draw_ui(state.game, &frame_prefix) == VG_OK);
    CHECK(vg_game_draw_ui(state.game, &frame_prefix) == VG_OK);
    CHECK(frame_prefix.struct_size == offsetof(VgUiFrame, frame_index));
    CHECK(frame_prefix.frame_index == UINT64_C(0xA5A5A5A5A5A5A5A5));

    CHECK(vg_game_step(state.game, 0.35, &step) == VG_OK);
    CHECK(step.fixed_steps == 2u && step.dropped_fixed_steps == 1u);
    CHECK(fabsf(step.interpolation_alpha - 0.5f) < 0.0001f);
    CHECK(vg_game_destroy(state.game) == VG_OK);
    CHECK(strcmp(state.trace, "IWF12F3DDDFFS") == 0);
    CHECK(state.shutdown_count == 1u && state.shutdown_saw_world);
    CHECK(vg_world_destroy(context, other_world) == VG_OK);
    CHECK(vg_world_destroy(context, state.world) == VG_OK);
    vg_context_destroy(context);
    return 0;
}

static int test_failure_and_transaction_boundaries(void) {
    GameState state = {0};
    VgContext *context = NULL;
    CHECK(create_context_world(&state, NULL, &context) == 0);
    VgGameCallbacks callbacks = callbacks_for(&state);
    state.fail_init = true;
    VgGame *sentinel = (VgGame *)(uintptr_t)UINT64_C(0x1234);
    CHECK(vg_game_create(context, NULL, &callbacks, &sentinel) == VG_ERROR_CONFLICT);
    CHECK(sentinel == (VgGame *)(uintptr_t)UINT64_C(0x1234));
    CHECK(strcmp(state.trace, "IS") == 0 && state.shutdown_count == 1u);

    memset(state.trace, 0, sizeof(state.trace));
    state.trace_length = 0u;
    state.shutdown_count = 0u;
    state.fail_init = false;
    VgGameDesc reserved_description = {0};
    reserved_description.struct_size = sizeof(reserved_description);
    reserved_description.api_version = VG_API_VERSION;
    reserved_description.reserved = 1u;
    sentinel = (VgGame *)(uintptr_t)UINT64_C(0x5678);
    CHECK(vg_game_create(context, &reserved_description, &callbacks, &sentinel) ==
          VG_ERROR_INVALID_ARGUMENT);
    CHECK(sentinel == (VgGame *)(uintptr_t)UINT64_C(0x5678));
    CHECK(strcmp(state.trace, "") == 0);
    CHECK(vg_game_create(context, NULL, &callbacks, &state.game) == VG_OK);
    state.fail_world = true;
    CHECK(vg_game_set_world(state.game, state.world) == VG_ERROR_CONFLICT);
    VgStepInfo info = {sizeof(info), VG_API_VERSION, 99u, 0u, 0u, 0u, 0.0, 0.0, 0.0f, 0u};
    CHECK(vg_game_step(state.game, 1.0 / 60.0, &info) == VG_ERROR_CONFLICT);
    CHECK(info.fixed_steps == 99u);
    state.fail_world = false;
    CHECK(vg_game_set_world(state.game, state.world) == VG_OK);
    VgWorld replacement_world = {0};
    CHECK(vg_world_create(context, NULL, &replacement_world) == VG_OK);
    state.fail_world = true;
    CHECK(vg_game_set_world(state.game, replacement_world) == VG_ERROR_CONFLICT);
    CHECK(vg_world_destroy(context, state.world) == VG_ERROR_CONFLICT);
    CHECK(vg_world_destroy(context, replacement_world) == VG_OK);
    state.fail_world = false;

    VgEvent invalid_event = {0};
    invalid_event.struct_size = offsetof(VgEvent, payload);
    invalid_event.api_version = VG_API_VERSION + 1u;
    CHECK(vg_game_emit_event(state.game, &invalid_event) == VG_ERROR_INVALID_ARGUMENT);
    VgStepInfo invalid_info = info;
    invalid_info.api_version = VG_API_VERSION + 1u;
    CHECK(vg_game_step(state.game, 0.1, &invalid_info) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(invalid_info.fixed_steps == info.fixed_steps);

    CHECK(vg_game_clear_world(state.game) == VG_OK);
    CHECK(vg_game_clear_world(state.game) == VG_ERROR_NOT_FOUND);
    CHECK(vg_game_destroy(state.game) == VG_OK);
    CHECK(state.shutdown_count == 1u);
    CHECK(vg_world_destroy(context, state.world) == VG_OK);
    vg_context_destroy(context);
    return 0;
}

static int test_oom_and_context_cleanup_order(void) {
    TestAllocator allocator = {0};
    GameState state = {0};
    VgContext *context = NULL;
    CHECK(create_context_world(&state, &allocator, &context) == 0);
    VgGameCallbacks callbacks = callbacks_for(&state);
    VgGame *sentinel = (VgGame *)(uintptr_t)UINT64_C(0xCAFE);

    allocator.fail_call = allocator.calls + 1u;
    CHECK(vg_game_create(context, NULL, &callbacks, &sentinel) == VG_ERROR_OUT_OF_MEMORY);
    CHECK(sentinel == (VgGame *)(uintptr_t)UINT64_C(0xCAFE));
    CHECK(state.trace_length == 0u);
    allocator.fail_call = allocator.calls + 2u;
    CHECK(vg_game_create(context, NULL, &callbacks, &sentinel) == VG_ERROR_OUT_OF_MEMORY);
    CHECK(sentinel == (VgGame *)(uintptr_t)UINT64_C(0xCAFE));
    CHECK(state.trace_length == 0u);

    allocator.fail_call = 0u;
    CHECK(vg_game_create(context, NULL, &callbacks, &state.game) == VG_OK);
    CHECK(vg_game_set_world(state.game, state.world) == VG_OK);
    vg_context_destroy(context);
    CHECK(state.shutdown_count == 1u);
    CHECK(state.shutdown_saw_world);
    CHECK(allocator.outstanding == 0u);
    return 0;
}

int main(void) {
    CHECK(test_lifecycle_stepping_events_and_camera() == 0);
    CHECK(test_failure_and_transaction_boundaries() == 0);
    CHECK(test_oom_and_context_cleanup_order() == 0);
    puts("PASS vestigio game lifecycle, fixed step, FIFO events, camera, OOM and cleanup order");
    return 0;
}
