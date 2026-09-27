#include "audio/raylib_audio.h"

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static void wait_for_clip_end(void) {
#ifdef _WIN32
    Sleep(900u);
#else
    const struct timespec duration = {.tv_sec = 0, .tv_nsec = 900000000L};
    (void)nanosleep(&duration, NULL);
#endif
}

#ifdef _WIN32
static bool copy_to_unicode_directory(const char *ambience, const char *door) {
    wchar_t source[4096];
    if (!CreateDirectoryW(L"vestigio-audio-\x00f1", NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ambience, -1,
                            source, 4096) == 0 ||
        !CopyFileW(source, L"vestigio-audio-\x00f1\\atrium-ambience.wav", FALSE))
        return false;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, door, -1,
                            source, 4096) == 0 ||
        !CopyFileW(source, L"vestigio-audio-\x00f1\\door-move.wav", FALSE))
        return false;
    return true;
}

static void remove_unicode_directory(void) {
    (void)DeleteFileW(L"vestigio-audio-\x00f1\\atrium-ambience.wav");
    (void)DeleteFileW(L"vestigio-audio-\x00f1\\door-move.wav");
    (void)RemoveDirectoryW(L"vestigio-audio-\x00f1");
}
#endif

#define CHECK(expr) do { if (!(expr)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, \
                  #expr, vg_raylib_audio_last_error(adapter)); \
    result = 1; goto cleanup; \
} } while (0)

int main(int argc, char **argv) {
    if (argc != 3) {
        (void)fprintf(stderr, "Uso: raylib_audio_test <ambience.wav> <door-move.wav>\n");
        return 2;
    }
    int result = 0;
    VgRaylibAudio *adapter = vg_raylib_audio_create(2u, 4u, 1u, 1u);
    if (adapter == NULL) {
        (void)fprintf(stderr, "FAIL no se pudo crear adapter de audio\n");
        return 1;
    }
    VgAudioCore *core = NULL;
#ifdef _WIN32
    CHECK(copy_to_unicode_directory(argv[1], argv[2]));
    const char *ambience_path = "vestigio-audio-\xc3\xb1/atrium-ambience.wav";
    const char *door_path = "vestigio-audio-\xc3\xb1/door-move.wav";
#else
    const char *ambience_path = argv[1];
    const char *door_path = argv[2];
#endif
    VgAudioCoreConfig config = {.max_sounds = 2u,
                                .max_emitters = 2u,
                                .max_voices = 4u,
                                .max_music_streams = 1u,
                                .backend = vg_raylib_audio_backend(adapter)};
    CHECK(vg_audio_core_create(&config, &core) == VG_OK);
    VgAudioCoreStats stats = vg_audio_core_stats(core);
    if (stats.device_state == VG_AUDIO_DEVICE_UNAVAILABLE) {
        (void)printf("SKIP audio device unavailable: %s (result=%d)\n",
                     vg_raylib_audio_last_error(adapter), stats.last_device_result);
        goto cleanup;
    }
    CHECK(stats.device_state == VG_AUDIO_DEVICE_READY);
    uint64_t ambience_token = 0u, door_token = 0u, source_token = 0u;
    CHECK(vg_raylib_audio_sound_load(adapter, ambience_path, &ambience_token) == VG_OK);
    CHECK(vg_raylib_audio_sound_load(adapter, door_path, &door_token) == VG_OK);
    CHECK(ambience_token != 0u && door_token != 0u && ambience_token != door_token);
    VgAudioSound ambience_sound = {0}, door_sound = {0};
    CHECK(vg_audio_core_sound_create(core, 101u, ambience_token, &ambience_sound) == VG_OK);
    CHECK(vg_audio_core_sound_create(core, 102u, door_token, &door_sound) == VG_OK);
    VgAudioEmitterDesc ambience = {.world_id = 7u,
                                   .sound = ambience_sound,
                                   .bus = VG_AUDIO_BUS_AMBIENCE,
                                   .gain = 0.05f,
                                   .min_distance = 0.0f,
                                   .max_distance = 10.0f,
                                   .loop = true};
    VgAudioEmitterDesc door = {.world_id = 7u,
                               .sound = door_sound,
                               .bus = VG_AUDIO_BUS_SFX,
                               .position = {2.0f, 0.0f, 0.0f},
                               .gain = 0.05f,
                               .min_distance = 0.5f,
                               .max_distance = 10.0f,
                               .spatial = true};
    VgAudioEmitter ambience_emitter = {0}, door_emitter = {0};
    CHECK(vg_audio_core_emitter_create(core, &ambience, &ambience_emitter) == VG_OK);
    CHECK(vg_audio_core_emitter_create(core, &door, &door_emitter) == VG_OK);
    VgAudioListener listener = {.right = {1.0f, 0.0f, 0.0f}};
    CHECK(vg_audio_core_set_listener(core, &listener) == VG_OK);
    CHECK(vg_audio_core_set_bus_gain(core, VG_AUDIO_BUS_AMBIENCE, 0.5f) == VG_OK);
    VgAudioVoice ambience_voice = {0}, door_voice_a = {0}, door_voice_b = {0};
    CHECK(vg_audio_core_emitter_play(core, ambience_emitter, 1u, &ambience_voice) == VG_OK);
    CHECK(vg_audio_core_emitter_play(core, door_emitter, 2u, &door_voice_a) == VG_OK);
    CHECK(vg_audio_core_emitter_play(core, door_emitter, 3u, &door_voice_b) == VG_OK);
    CHECK(door_voice_a.value != door_voice_b.value);
    CHECK(vg_audio_core_stats(core).voices == 3u);
    CHECK(vg_audio_core_voice_stop(core, door_voice_a) == VG_OK);
    CHECK(vg_audio_core_stats(core).voices == 2u);
    CHECK(vg_audio_core_set_paused(core, false, true) == VG_OK);
    CHECK(vg_audio_core_update(core) == VG_OK);
    CHECK(vg_audio_core_stats(core).voices == 2u);
    CHECK(vg_audio_core_set_paused(core, false, false) == VG_OK);

    /* Direct backend check: two aliases of one decoded Sound are independent. */
    VgAudioBackend backend = vg_raylib_audio_backend(adapter);
    uint64_t alias_a = 0u, alias_b = 0u;
    CHECK(backend.voice_start(backend.user, door_token, false, &alias_a) == VG_OK);
    CHECK(backend.voice_start(backend.user, door_token, false, &alias_b) == VG_OK);
    CHECK(alias_a != alias_b && backend.voice_is_playing(backend.user, alias_a) &&
          backend.voice_is_playing(backend.user, alias_b));
    backend.voice_stop(backend.user, alias_a);
    CHECK(backend.voice_is_playing(backend.user, alias_b));
    backend.voice_stop(backend.user, alias_b);
    uint64_t loop_alias = 0u;
    CHECK(backend.voice_start(backend.user, door_token, true, &loop_alias) == VG_OK);
    wait_for_clip_end(); /* door WAV lasts about 0.7 s; next poll restarts it */
    CHECK(backend.voice_is_playing(backend.user, loop_alias));
    backend.voice_stop(backend.user, loop_alias);

    CHECK(vg_raylib_audio_music_source_add(adapter, ambience_path,
                                            &source_token) == VG_OK);
    VgAudioMusicDesc music_desc = {.world_id = 7u,
                                   .source_token = source_token,
                                   .bus = VG_AUDIO_BUS_AMBIENCE,
                                   .gain = 0.05f,
                                   .loop = true};
    VgAudioMusic music = {0};
    CHECK(vg_audio_core_music_start(core, &music_desc, &music) == VG_OK);
    for (int i = 0; i < 8; ++i)
        CHECK(vg_audio_core_update(core) == VG_OK);
    CHECK(vg_audio_core_stats(core).stream_updates >= 8u);
    CHECK(vg_audio_core_set_paused(core, true, false) == VG_OK);
    CHECK(vg_audio_core_update(core) == VG_OK);
    CHECK(vg_audio_core_set_paused(core, false, false) == VG_OK);
    CHECK(vg_audio_core_music_stop(core, music) == VG_OK);
    CHECK(vg_audio_core_world_unload(core, 7u) == VG_OK);
    CHECK(vg_audio_core_stats(core).voices == 0u);
    CHECK(vg_audio_core_stats(core).emitters == 0u);
    CHECK(vg_audio_core_sound_release(core, ambience_sound) == VG_OK);
    CHECK(vg_audio_core_sound_release(core, door_sound) == VG_OK);
    (void)printf("PASS raylib audio device, independent aliases, pause, loop, streamed music\n");
cleanup:
    vg_audio_core_destroy(core);
    vg_raylib_audio_destroy(adapter);
#ifdef _WIN32
    remove_unicode_directory();
#endif
    return result;
}
