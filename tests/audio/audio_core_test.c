#include "audio/audio_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct MockVoice {
    uint64_t token;
    uint64_t sound;
    float gain;
    float pan;
    bool playing;
    bool paused;
} MockVoice;

typedef struct MockMusic {
    uint64_t token;
    uint64_t source;
    float gain;
    bool open;
    bool playing;
    bool paused;
} MockMusic;

typedef struct MockAudio {
    VgResult open_result;
    VgResult next_voice_result;
    VgResult next_music_result;
    VgResult music_update_result;
    uint64_t next_token;
    uint32_t device_opens;
    uint32_t device_closes;
    uint32_t sound_releases;
    uint64_t released_sound;
    uint32_t voice_starts;
    uint32_t voice_stops;
    uint32_t voice_pause_calls;
    uint32_t music_opens;
    uint32_t music_closes;
    uint32_t music_updates;
    MockVoice voices[16];
    MockMusic music[8];
} MockAudio;

typedef struct FailAllocator {
    uint32_t calls;
    uint32_t fail_at;
    uint32_t live;
} FailAllocator;

static void *fail_allocate(void *user, uint64_t size) {
    FailAllocator *allocator = user;
    ++allocator->calls;
    if (allocator->fail_at != 0u && allocator->calls == allocator->fail_at)
        return NULL;
    if (size > SIZE_MAX)
        return NULL;
    void *allocation = malloc((size_t)size);
    if (allocation != NULL)
        ++allocator->live;
    return allocation;
}

static void fail_deallocate(void *user, void *allocation) {
    FailAllocator *allocator = user;
    if (allocation != NULL) {
        --allocator->live;
        free(allocation);
    }
}

static VgResult mock_device_open(void *user) {
    MockAudio *mock = user;
    ++mock->device_opens;
    return mock->open_result;
}

static void mock_device_close(void *user) {
    MockAudio *mock = user;
    ++mock->device_closes;
}

static void mock_sound_release(void *user, uint64_t sound_token) {
    MockAudio *mock = user;
    ++mock->sound_releases;
    mock->released_sound = sound_token;
}

static MockVoice *mock_voice_find(MockAudio *mock, uint64_t token) {
    for (uint32_t index = 0u; index < 16u; ++index)
        if (mock->voices[index].token == token)
            return &mock->voices[index];
    return NULL;
}

static VgResult mock_voice_start(void *user, uint64_t sound_token, bool loop,
                                 uint64_t *out_voice_token) {
    (void)loop;
    MockAudio *mock = user;
    if (mock->next_voice_result != VG_OK) {
        VgResult result = mock->next_voice_result;
        mock->next_voice_result = VG_OK;
        return result;
    }
    for (uint32_t index = 0u; index < 16u; ++index) {
        if (mock->voices[index].playing)
            continue;
        MockVoice *voice = &mock->voices[index];
        voice->token = ++mock->next_token;
        voice->sound = sound_token;
        voice->playing = true;
        *out_voice_token = voice->token;
        ++mock->voice_starts;
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

static void mock_voice_stop(void *user, uint64_t voice_token) {
    MockAudio *mock = user;
    MockVoice *voice = mock_voice_find(mock, voice_token);
    if (voice != NULL)
        voice->playing = false;
    ++mock->voice_stops;
}

static void mock_voice_set_paused(void *user, uint64_t voice_token, bool paused) {
    MockAudio *mock = user;
    MockVoice *voice = mock_voice_find(mock, voice_token);
    if (voice != NULL)
        voice->paused = paused;
    ++mock->voice_pause_calls;
}

static void mock_voice_set_mix(void *user, uint64_t voice_token, float gain, float pan) {
    MockAudio *mock = user;
    MockVoice *voice = mock_voice_find(mock, voice_token);
    if (voice != NULL) {
        voice->gain = gain;
        voice->pan = pan;
    }
}

static bool mock_voice_is_playing(void *user, uint64_t voice_token) {
    MockVoice *voice = mock_voice_find(user, voice_token);
    return voice != NULL && voice->playing;
}

static MockMusic *mock_music_find(MockAudio *mock, uint64_t token) {
    for (uint32_t index = 0u; index < 8u; ++index)
        if (mock->music[index].token == token)
            return &mock->music[index];
    return NULL;
}

static VgResult mock_music_open(void *user, uint64_t source_token, uint64_t *out_stream_token) {
    MockAudio *mock = user;
    if (mock->next_music_result != VG_OK) {
        VgResult result = mock->next_music_result;
        mock->next_music_result = VG_OK;
        return result;
    }
    for (uint32_t index = 0u; index < 8u; ++index) {
        if (mock->music[index].open)
            continue;
        MockMusic *music = &mock->music[index];
        music->token = ++mock->next_token;
        music->source = source_token;
        music->open = true;
        *out_stream_token = music->token;
        ++mock->music_opens;
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

static void mock_music_close(void *user, uint64_t stream_token) {
    MockAudio *mock = user;
    MockMusic *music = mock_music_find(mock, stream_token);
    if (music != NULL)
        music->open = false;
    ++mock->music_closes;
}

static void mock_music_play(void *user, uint64_t stream_token, bool loop) {
    (void)loop;
    MockMusic *music = mock_music_find(user, stream_token);
    if (music != NULL)
        music->playing = true;
}

static void mock_music_stop(void *user, uint64_t stream_token) {
    MockMusic *music = mock_music_find(user, stream_token);
    if (music != NULL)
        music->playing = false;
}

static void mock_music_set_paused(void *user, uint64_t stream_token, bool paused) {
    MockMusic *music = mock_music_find(user, stream_token);
    if (music != NULL)
        music->paused = paused;
}

static void mock_music_set_gain(void *user, uint64_t stream_token, float gain) {
    MockMusic *music = mock_music_find(user, stream_token);
    if (music != NULL)
        music->gain = gain;
}

static VgResult mock_music_update(void *user, uint64_t stream_token) {
    MockAudio *mock = user;
    if (mock_music_find(mock, stream_token) == NULL)
        return VG_ERROR_INVALID_HANDLE;
    ++mock->music_updates;
    return mock->music_update_result;
}

static VgAudioBackend mock_backend(MockAudio *mock) {
    return (VgAudioBackend){.user = mock,
                            .device_open = mock_device_open,
                            .device_close = mock_device_close,
                            .sound_release = mock_sound_release,
                            .voice_start = mock_voice_start,
                            .voice_stop = mock_voice_stop,
                            .voice_set_paused = mock_voice_set_paused,
                            .voice_set_mix = mock_voice_set_mix,
                            .voice_is_playing = mock_voice_is_playing,
                            .music_open = mock_music_open,
                            .music_close = mock_music_close,
                            .music_play = mock_music_play,
                            .music_stop = mock_music_stop,
                            .music_set_paused = mock_music_set_paused,
                            .music_set_gain = mock_music_set_gain,
                            .music_update = mock_music_update};
}

static VgAudioCoreConfig mock_config(MockAudio *mock) {
    return (VgAudioCoreConfig){.max_sounds = 2u,
                               .max_emitters = 4u,
                               .max_voices = 2u,
                               .max_music_streams = 2u,
                               .backend = mock_backend(mock)};
}

static VgAudioEmitterDesc emitter_desc(VgAudioSound sound, uint64_t world, float x,
                                       uint32_t priority) {
    return (VgAudioEmitterDesc){.world_id = world,
                                .sound = sound,
                                .bus = VG_AUDIO_BUS_SFX,
                                .position = {x, 0.0f, 0.0f},
                                .gain = 1.0f,
                                .min_distance = 1.0f,
                                .max_distance = 9.0f,
                                .priority = priority,
                                .spatial = true,
                                .loop = false};
}

static int test_shared_sound_worlds_spatial_pause_and_priority(void) {
    MockAudio mock = {.open_result = VG_OK};
    VgAudioCoreConfig config = mock_config(&mock);
    VgAudioCore *core = NULL;
    CHECK(vg_audio_core_create(&config, &core) == VG_OK && core != NULL);
    CHECK(vg_audio_core_set_bus_gain(core, VG_AUDIO_BUS_MASTER, 0.5f) == VG_OK);
    CHECK(vg_audio_core_set_bus_gain(core, VG_AUDIO_BUS_SFX, 0.8f) == VG_OK);
    VgAudioListener listener = {.position = {0, 0, 0}, .right = {2, 0, 0}};
    CHECK(vg_audio_core_set_listener(core, &listener) == VG_OK);

    VgAudioSound sound = {0};
    CHECK(vg_audio_core_sound_create(core, 77u, 700u, &sound) == VG_OK);
    VgAudioEmitterDesc left_desc = emitter_desc(sound, 11u, -5.0f, 1u);
    VgAudioEmitterDesc right_desc = emitter_desc(sound, 22u, 5.0f, 2u);
    VgAudioEmitter left = {0}, right = {0};
    CHECK(vg_audio_core_emitter_create(core, &left_desc, &left) == VG_OK);
    CHECK(vg_audio_core_emitter_create(core, &right_desc, &right) == VG_OK);
    VgAudioVoice left_voice = {0}, right_voice = {0};
    CHECK(vg_audio_core_emitter_play(core, left, 1001u, &left_voice) == VG_OK);
    CHECK(vg_audio_core_emitter_play(core, right, 2001u, &right_voice) == VG_OK);
    CHECK(left_voice.value != 0u && right_voice.value != 0u &&
          left_voice.value != right_voice.value);
    CHECK(mock.voice_starts == 2u && mock.voices[0].sound == 700u && mock.voices[1].sound == 700u);
    CHECK(fabsf(mock.voices[0].gain - 0.2f) < 0.0001f && mock.voices[0].pan < -0.99f);
    CHECK(fabsf(mock.voices[1].gain - 0.2f) < 0.0001f && mock.voices[1].pan > 0.99f);

    VgAudioVoice duplicate = {UINT64_MAX};
    CHECK(vg_audio_core_emitter_play(core, left, 1001u, &duplicate) == VG_OK);
    CHECK(duplicate.value == left_voice.value && mock.voice_starts == 2u);
    CHECK(vg_audio_core_set_paused(core, false, true) == VG_OK);
    CHECK(mock.voices[0].paused && mock.voices[1].paused);
    uint32_t pause_calls = mock.voice_pause_calls;
    CHECK(vg_audio_core_set_paused(core, true, false) == VG_OK);
    CHECK(mock.voice_pause_calls == pause_calls);
    CHECK(vg_audio_core_set_paused(core, false, false) == VG_OK);
    CHECK(!mock.voices[0].paused && !mock.voices[1].paused);

    CHECK(vg_audio_core_world_unload(core, 11u) == VG_OK);
    VgAudioCoreStats stats = vg_audio_core_stats(core);
    CHECK(stats.emitters == 1u && stats.voices == 1u && mock.voice_stops == 1u);
    CHECK(vg_audio_core_sound_release(core, sound) == VG_OK);
    CHECK(mock.sound_releases == 0u);
    CHECK(vg_audio_core_world_unload(core, 22u) == VG_OK);
    CHECK(mock.sound_releases == 1u && mock.released_sound == 700u);
    stats = vg_audio_core_stats(core);
    CHECK(stats.sounds == 0u && stats.emitters == 0u && stats.voices == 0u &&
          stats.duplicate_events == 1u);
    vg_audio_core_destroy(core);
    CHECK(mock.device_closes == 1u && mock.sound_releases == 1u);
    return 0;
}

static int test_voice_limit_priority_and_failed_candidate(void) {
    MockAudio mock = {.open_result = VG_OK};
    VgAudioCoreConfig config = mock_config(&mock);
    VgAudioCore *core = NULL;
    CHECK(vg_audio_core_create(&config, &core) == VG_OK);
    VgAudioSound sound = {0};
    CHECK(vg_audio_core_sound_create(core, 88u, 800u, &sound) == VG_OK);
    VgAudioEmitterDesc low_desc = emitter_desc(sound, 1u, 0.0f, 1u);
    VgAudioEmitterDesc equal_desc = emitter_desc(sound, 1u, 0.0f, 1u);
    VgAudioEmitterDesc high_desc = emitter_desc(sound, 1u, 0.0f, 9u);
    VgAudioEmitter low = {0}, equal = {0}, high = {0};
    CHECK(vg_audio_core_emitter_create(core, &low_desc, &low) == VG_OK);
    CHECK(vg_audio_core_emitter_create(core, &equal_desc, &equal) == VG_OK);
    CHECK(vg_audio_core_emitter_create(core, &high_desc, &high) == VG_OK);
    CHECK(vg_audio_core_emitter_play(core, low, 1u, NULL) == VG_OK);
    CHECK(vg_audio_core_emitter_play(core, equal, 2u, NULL) == VG_OK);
    VgAudioVoice unchanged = {UINT64_MAX};
    CHECK(vg_audio_core_emitter_play(core, equal, 3u, &unchanged) == VG_ERROR_CAPACITY);
    CHECK(unchanged.value == UINT64_MAX && mock.voice_stops == 0u);
    mock.next_voice_result = VG_ERROR_IO;
    CHECK(vg_audio_core_emitter_play(core, high, 4u, NULL) == VG_ERROR_IO);
    CHECK(vg_audio_core_stats(core).voices == 2u && mock.voice_stops == 0u);
    CHECK(vg_audio_core_emitter_play(core, high, 5u, NULL) == VG_OK);
    VgAudioCoreStats stats = vg_audio_core_stats(core);
    CHECK(stats.voices == 2u && stats.voice_steals == 1u && mock.voice_stops == 1u);
    vg_audio_core_destroy(core);
    return 0;
}

static int test_streaming_music_and_persistent_buses(void) {
    MockAudio mock = {.open_result = VG_OK};
    VgAudioCoreConfig config = mock_config(&mock);
    VgAudioCore *core = NULL;
    CHECK(vg_audio_core_create(&config, &core) == VG_OK);
    CHECK(vg_audio_core_set_bus_gain(core, VG_AUDIO_BUS_MASTER, 0.5f) == VG_OK);
    CHECK(vg_audio_core_set_bus_gain(core, VG_AUDIO_BUS_MUSIC, 0.4f) == VG_OK);
    VgAudioMusicDesc description = {
        .world_id = 44u, .source_token = 900u, .gain = 0.5f, .loop = true};
    VgAudioMusic music = {0};
    CHECK(vg_audio_core_music_start(core, &description, &music) == VG_OK);
    CHECK(music.value != 0u && mock.music_opens == 1u && mock.music[0].source == 900u);
    CHECK(fabsf(mock.music[0].gain - 0.1f) < 0.0001f);
    CHECK(vg_audio_core_update(core) == VG_OK);
    CHECK(vg_audio_core_update(core) == VG_OK);
    CHECK(mock.music_updates == 2u && vg_audio_core_stats(core).stream_updates == 2u);
    CHECK(vg_audio_core_set_paused(core, true, false) == VG_OK && mock.music[0].paused);
    CHECK(vg_audio_core_world_unload(core, 44u) == VG_OK);
    CHECK(mock.music_closes == 1u && vg_audio_core_stats(core).music_streams == 0u);
    float gain = 0.0f;
    CHECK(vg_audio_core_get_bus_gain(core, VG_AUDIO_BUS_MASTER, &gain) == VG_OK && gain == 0.5f);
    CHECK(vg_audio_core_get_bus_gain(core, VG_AUDIO_BUS_MUSIC, &gain) == VG_OK && gain == 0.4f);
    vg_audio_core_destroy(core);
    return 0;
}

static int test_device_absent_is_reported_and_safe(void) {
    MockAudio mock = {.open_result = VG_ERROR_IO};
    VgAudioCoreConfig config = mock_config(&mock);
    VgAudioCore *core = NULL;
    CHECK(vg_audio_core_create(&config, &core) == VG_OK && core != NULL);
    VgAudioCoreStats stats = vg_audio_core_stats(core);
    CHECK(stats.device_state == VG_AUDIO_DEVICE_UNAVAILABLE &&
          stats.last_device_result == VG_ERROR_IO);
    CHECK(vg_audio_core_set_bus_gain(core, VG_AUDIO_BUS_AMBIENCE, 0.25f) == VG_OK);
    float gain = 0.0f;
    CHECK(vg_audio_core_get_bus_gain(core, VG_AUDIO_BUS_AMBIENCE, &gain) == VG_OK && gain == 0.25f);
    VgAudioSound sound = {0};
    CHECK(vg_audio_core_sound_create(core, 99u, 990u, &sound) == VG_OK);
    VgAudioEmitterDesc description = emitter_desc(sound, 1u, 0.0f, 1u);
    VgAudioEmitter emitter = {0};
    CHECK(vg_audio_core_emitter_create(core, &description, &emitter) == VG_OK);
    VgAudioVoice voice = {UINT64_MAX};
    CHECK(vg_audio_core_emitter_play(core, emitter, 1u, &voice) == VG_ERROR_IO);
    CHECK(voice.value == UINT64_MAX && mock.voice_starts == 0u);
    VgAudioMusic music = {UINT64_MAX};
    VgAudioMusicDesc music_desc = {.source_token = 3u, .gain = 1.0f};
    CHECK(vg_audio_core_music_start(core, &music_desc, &music) == VG_ERROR_IO);
    CHECK(music.value == UINT64_MAX && mock.music_opens == 0u);
    CHECK(vg_audio_core_update(core) == VG_ERROR_IO);
    vg_audio_core_destroy(core);
    CHECK(mock.device_closes == 0u && mock.sound_releases == 1u);
    return 0;
}

static int test_create_oom_is_transactional(void) {
    for (uint32_t fail_at = 1u; fail_at <= 5u; ++fail_at) {
        MockAudio mock = {.open_result = VG_OK};
        FailAllocator allocator = {.fail_at = fail_at};
        VgAudioCoreConfig config = mock_config(&mock);
        config.allocator_user = &allocator;
        config.allocate = fail_allocate;
        config.deallocate = fail_deallocate;
        VgAudioCore *core = (VgAudioCore *)(uintptr_t)UINTPTR_MAX;
        CHECK(vg_audio_core_create(&config, &core) == VG_ERROR_OUT_OF_MEMORY);
        CHECK(core == (VgAudioCore *)(uintptr_t)UINTPTR_MAX);
        CHECK(allocator.live == 0u && mock.device_opens == 0u);
    }
    return 0;
}

int main(void) {
    CHECK(test_shared_sound_worlds_spatial_pause_and_priority() == 0);
    CHECK(test_voice_limit_priority_and_failed_candidate() == 0);
    CHECK(test_streaming_music_and_persistent_buses() == 0);
    CHECK(test_device_absent_is_reported_and_safe() == 0);
    CHECK(test_create_oom_is_transactional() == 0);
    puts("PASS audio core ownership, limits, spatial mix, pause, worlds, streams and no-device");
    return 0;
}
