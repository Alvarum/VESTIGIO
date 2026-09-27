#include "audio/atrium_audio.h"
#include "audio/raylib_audio.h"

#include <stdio.h>
#include <stdlib.h>

struct VgAtriumAudio {
    VgRaylibAudio *backend;
    VgAudioCore *core;
    VgAudioSound door_sound;
    VgAudioMusic ambience_music;
    VgAudioEmitter door;
    VgAudioEmitterDesc door_description;
    uint64_t next_event;
    bool ready;
};

static void atrium_error(char *error, size_t capacity, const char *message) {
    if (error != NULL && capacity != 0u)
        (void)snprintf(error, capacity, "%s", message);
}

VgAtriumAudio *vg_atrium_audio_create(const char *audio_directory,
                                      char *error, size_t error_capacity) {
    if (audio_directory == NULL || audio_directory[0] == '\0') {
        atrium_error(error, error_capacity, "Audio: ruta de assets invalida");
        return NULL;
    }
    VgAtriumAudio *audio = calloc(1u, sizeof(*audio));
    if (audio == NULL) {
        atrium_error(error, error_capacity, "Audio: sin memoria");
        return NULL;
    }
    audio->backend = vg_raylib_audio_create(1u, 8u, 1u, 1u);
    if (audio->backend == NULL) {
        atrium_error(error, error_capacity, "Audio: backend no disponible");
        return audio;
    }
    VgAudioCoreConfig config = {0};
    config.max_sounds = 1u;
    config.max_emitters = 1u;
    config.max_voices = 8u;
    config.max_music_streams = 1u;
    config.backend = vg_raylib_audio_backend(audio->backend);
    if (vg_audio_core_create(&config, &audio->core) != VG_OK) {
        atrium_error(error, error_capacity, "Audio: no se pudo crear mezclador");
        return audio;
    }
    VgAudioCoreStats stats = vg_audio_core_stats(audio->core);
    if (stats.device_state != VG_AUDIO_DEVICE_READY) {
        atrium_error(error, error_capacity, "Audio: dispositivo no disponible");
        return audio;
    }
    char ambience_path[2048], door_path[2048];
    int first = snprintf(ambience_path, sizeof(ambience_path), "%s/atrium-ambience.wav",
                         audio_directory);
    int second = snprintf(door_path, sizeof(door_path), "%s/door-move.wav",
                          audio_directory);
    if (first < 0 || (size_t)first >= sizeof(ambience_path) ||
        second < 0 || (size_t)second >= sizeof(door_path)) {
        atrium_error(error, error_capacity, "Audio: ruta demasiado larga");
        return audio;
    }
    uint64_t ambience_source = 0u;
    if (vg_raylib_audio_music_source_add(audio->backend, ambience_path,
                                          &ambience_source) != VG_OK) {
        atrium_error(error, error_capacity, vg_raylib_audio_last_error(audio->backend));
        return audio;
    }
    uint64_t token = 0u;
    if (vg_raylib_audio_sound_load(audio->backend, door_path, &token) != VG_OK ||
        vg_audio_core_sound_create(audio->core, 1u, token, &audio->door_sound) != VG_OK) {
        atrium_error(error, error_capacity, vg_raylib_audio_last_error(audio->backend));
        return audio;
    }
    VgAudioMusicDesc ambience = {0};
    ambience.world_id = 1u;
    ambience.source_token = ambience_source;
    ambience.bus = VG_AUDIO_BUS_AMBIENCE;
    ambience.gain = 0.45f;
    ambience.loop = true;
    audio->door_description = (VgAudioEmitterDesc){0};
    audio->door_description.world_id = 1u;
    audio->door_description.sound = audio->door_sound;
    audio->door_description.bus = VG_AUDIO_BUS_SFX;
    audio->door_description.gain = 1.0f;
    audio->door_description.min_distance = 0.8f;
    audio->door_description.max_distance = 12.0f;
    audio->door_description.priority = 4u;
    audio->door_description.spatial = true;
    if (vg_audio_core_emitter_create(audio->core, &audio->door_description,
                                      &audio->door) != VG_OK ||
        vg_audio_core_music_start(audio->core, &ambience,
                                   &audio->ambience_music) != VG_OK) {
        atrium_error(error, error_capacity, "Audio: no se pudo iniciar ambiente");
        return audio;
    }
    audio->next_event = 1u;
    audio->ready = true;
    atrium_error(error, error_capacity, "");
    return audio;
}

void vg_atrium_audio_destroy(VgAtriumAudio *audio) {
    if (audio == NULL)
        return;
    vg_audio_core_destroy(audio->core);
    vg_raylib_audio_destroy(audio->backend);
    free(audio);
}

bool vg_atrium_audio_ready(const VgAtriumAudio *audio) {
    return audio != NULL && audio->ready;
}

VgAudioCoreStats vg_atrium_audio_stats(const VgAtriumAudio *audio) {
    return audio != NULL ? vg_audio_core_stats(audio->core) : (VgAudioCoreStats){0};
}

VgResult vg_atrium_audio_set_gain(VgAtriumAudio *audio, VgAudioBus bus, float gain) {
    return audio != NULL && audio->core != NULL
        ? vg_audio_core_set_bus_gain(audio->core, bus, gain) : VG_ERROR_NOT_FOUND;
}

VgResult vg_atrium_audio_set_listener(VgAtriumAudio *audio,
                                      VgVec3 position, VgVec3 right) {
    if (audio == NULL || !audio->ready)
        return VG_ERROR_NOT_FOUND;
    VgAudioListener listener = {position, right};
    return vg_audio_core_set_listener(audio->core, &listener);
}

VgResult vg_atrium_audio_set_paused(VgAtriumAudio *audio,
                                    bool simulation_paused, bool focus_paused) {
    return audio != NULL && audio->ready
        ? vg_audio_core_set_paused(audio->core, simulation_paused, focus_paused)
        : VG_ERROR_NOT_FOUND;
}

VgResult vg_atrium_audio_play_door(VgAtriumAudio *audio, VgVec3 position) {
    if (audio == NULL || !audio->ready)
        return VG_ERROR_NOT_FOUND;
    audio->door_description.position = position;
    VgResult result = vg_audio_core_emitter_update(audio->core, audio->door,
                                                    &audio->door_description);
    return result == VG_OK
        ? vg_audio_core_emitter_play(audio->core, audio->door, ++audio->next_event, NULL)
        : result;
}

VgResult vg_atrium_audio_update(VgAtriumAudio *audio) {
    return audio != NULL && audio->ready
        ? vg_audio_core_update(audio->core) : VG_ERROR_NOT_FOUND;
}
