#ifndef VESTIGIO_AUDIO_CORE_H
#define VESTIGIO_AUDIO_CORE_H

#include "vestigio/vestigio.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct VgAudioCore VgAudioCore;

typedef struct VgAudioSound {
    uint64_t value;
} VgAudioSound;

typedef struct VgAudioEmitter {
    uint64_t value;
} VgAudioEmitter;

typedef struct VgAudioVoice {
    uint64_t value;
} VgAudioVoice;

typedef struct VgAudioMusic {
    uint64_t value;
} VgAudioMusic;

typedef uint32_t VgAudioBus;
enum {
    VG_AUDIO_BUS_MASTER = 0u,
    VG_AUDIO_BUS_MUSIC = 1u,
    VG_AUDIO_BUS_SFX = 2u,
    VG_AUDIO_BUS_AMBIENCE = 3u,
    VG_AUDIO_BUS_COUNT = 4u
};

typedef uint32_t VgAudioDeviceState;
enum { VG_AUDIO_DEVICE_STOPPED = 0u, VG_AUDIO_DEVICE_READY = 1u, VG_AUDIO_DEVICE_UNAVAILABLE = 2u };

/* The platform adapter owns actual PCM/stream objects. Sound tokens are shared,
 * voice tokens are mutable instances, and music tokens must remain streamed. */
typedef struct VgAudioBackend {
    void *user;
    VgResult (*device_open)(void *user);
    void (*device_close)(void *user);
    void (*sound_release)(void *user, uint64_t sound_token);
    VgResult (*voice_start)(void *user, uint64_t sound_token, bool loop, uint64_t *out_voice_token);
    void (*voice_stop)(void *user, uint64_t voice_token);
    void (*voice_set_paused)(void *user, uint64_t voice_token, bool paused);
    void (*voice_set_mix)(void *user, uint64_t voice_token, float linear_gain, float pan);
    bool (*voice_is_playing)(void *user, uint64_t voice_token);
    VgResult (*music_open)(void *user, uint64_t source_token, uint64_t *out_stream_token);
    void (*music_close)(void *user, uint64_t stream_token);
    void (*music_play)(void *user, uint64_t stream_token, bool loop);
    void (*music_stop)(void *user, uint64_t stream_token);
    void (*music_set_paused)(void *user, uint64_t stream_token, bool paused);
    void (*music_set_gain)(void *user, uint64_t stream_token, float linear_gain);
    VgResult (*music_update)(void *user, uint64_t stream_token);
} VgAudioBackend;

typedef struct VgAudioCoreConfig {
    uint32_t max_sounds;
    uint32_t max_emitters;
    uint32_t max_voices;
    uint32_t max_music_streams;
    void *allocator_user;
    VgAllocateFn allocate;
    VgDeallocateFn deallocate;
    VgAudioBackend backend;
} VgAudioCoreConfig;

typedef struct VgAudioEmitterDesc {
    uint64_t world_id;
    VgAudioSound sound;
    VgAudioBus bus;
    VgVec3 position;
    float gain;
    float min_distance;
    float max_distance;
    uint32_t priority;
    bool spatial;
    bool loop;
} VgAudioEmitterDesc;

typedef struct VgAudioListener {
    VgVec3 position;
    /* Normalized listener-right vector. Pan is dot(direction, right). */
    VgVec3 right;
} VgAudioListener;

typedef struct VgAudioMusicDesc {
    uint64_t world_id;
    uint64_t source_token;
    float gain;
    bool loop;
} VgAudioMusicDesc;

typedef struct VgAudioCoreStats {
    VgAudioDeviceState device_state;
    VgResult last_device_result;
    uint32_t sounds;
    uint32_t emitters;
    uint32_t voices;
    uint32_t music_streams;
    uint64_t voice_starts;
    uint64_t voice_stops;
    uint64_t voice_steals;
    uint64_t duplicate_events;
    uint64_t stream_updates;
} VgAudioCoreStats;

VgResult vg_audio_core_create(const VgAudioCoreConfig *config, VgAudioCore **out_core);
void vg_audio_core_destroy(VgAudioCore *core);

VgAudioCoreStats vg_audio_core_stats(const VgAudioCore *core);
VgResult vg_audio_core_set_bus_gain(VgAudioCore *core, VgAudioBus bus, float linear_gain);
VgResult vg_audio_core_get_bus_gain(const VgAudioCore *core, VgAudioBus bus,
                                    float *out_linear_gain);
VgResult vg_audio_core_set_listener(VgAudioCore *core, const VgAudioListener *listener);
VgResult vg_audio_core_set_paused(VgAudioCore *core, bool simulation_paused, bool focus_paused);

/* sound_token is an already decoded platform sound. The core releases it once
 * the public owner, all emitters and all voices have released their references. */
VgResult vg_audio_core_sound_create(VgAudioCore *core, uint64_t resource_key, uint64_t sound_token,
                                    VgAudioSound *out_sound);
VgResult vg_audio_core_sound_release(VgAudioCore *core, VgAudioSound sound);

VgResult vg_audio_core_emitter_create(VgAudioCore *core, const VgAudioEmitterDesc *description,
                                      VgAudioEmitter *out_emitter);
VgResult vg_audio_core_emitter_update(VgAudioCore *core, VgAudioEmitter emitter,
                                      const VgAudioEmitterDesc *description);
VgResult vg_audio_core_emitter_destroy(VgAudioCore *core, VgAudioEmitter emitter);
/* event_id is a stable interaction/event serial. Repeating it is idempotent. */
VgResult vg_audio_core_emitter_play(VgAudioCore *core, VgAudioEmitter emitter, uint64_t event_id,
                                    VgAudioVoice *out_voice);
VgResult vg_audio_core_voice_stop(VgAudioCore *core, VgAudioVoice voice);

VgResult vg_audio_core_music_start(VgAudioCore *core, const VgAudioMusicDesc *description,
                                   VgAudioMusic *out_music);
VgResult vg_audio_core_music_stop(VgAudioCore *core, VgAudioMusic music);

VgResult vg_audio_core_update(VgAudioCore *core);
VgResult vg_audio_core_world_unload(VgAudioCore *core, uint64_t world_id);

#endif /* VESTIGIO_AUDIO_CORE_H */
