#include "audio/audio_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct VgAudioSoundSlot {
    uint32_t generation;
    uint32_t emitter_refs;
    uint32_t voice_refs;
    bool used;
    bool external_ref;
    uint64_t resource_key;
    uint64_t token;
} VgAudioSoundSlot;

typedef struct VgAudioEmitterSlot {
    uint32_t generation;
    bool used;
    uint32_t sound_index;
    uint32_t sound_generation;
    VgAudioEmitterDesc description;
    uint64_t last_event_id;
    VgAudioVoice last_voice;
} VgAudioEmitterSlot;

typedef struct VgAudioVoiceSlot {
    uint32_t generation;
    bool used;
    uint32_t sound_index;
    uint32_t sound_generation;
    uint32_t emitter_index;
    uint32_t emitter_generation;
    uint32_t priority;
    uint64_t sequence;
    uint64_t token;
} VgAudioVoiceSlot;

typedef struct VgAudioMusicSlot {
    uint32_t generation;
    bool used;
    uint64_t world_id;
    uint64_t token;
    float gain;
    bool loop;
} VgAudioMusicSlot;

struct VgAudioCore {
    VgAudioCoreConfig config;
    VgAudioSoundSlot *sounds;
    VgAudioEmitterSlot *emitters;
    VgAudioVoiceSlot *voices;
    VgAudioMusicSlot *music;
    VgAudioListener listener;
    float bus_gain[VG_AUDIO_BUS_COUNT];
    bool simulation_paused;
    bool focus_paused;
    bool device_open;
    uint64_t next_sequence;
    VgAudioCoreStats stats;
};

static void *default_allocate(void *user, uint64_t size) {
    (void)user;
    return size <= SIZE_MAX ? malloc((size_t)size) : NULL;
}

static void default_deallocate(void *user, void *allocation) {
    (void)user;
    free(allocation);
}

static bool finite_vec3(VgVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static uint64_t handle_make(uint32_t index, uint32_t generation) {
    return ((uint64_t)generation << 32u) | ((uint64_t)index + 1u);
}

static bool handle_split(uint64_t value, uint32_t capacity, uint32_t *out_index,
                         uint32_t *out_generation) {
    uint32_t low = (uint32_t)value;
    uint32_t generation = (uint32_t)(value >> 32u);
    if (low == 0u || generation == 0u || low - 1u >= capacity)
        return false;
    *out_index = low - 1u;
    *out_generation = generation;
    return true;
}

static uint32_t next_generation(uint32_t generation) {
    ++generation;
    return generation == 0u ? 1u : generation;
}

static bool backend_valid(const VgAudioBackend *backend) {
    return backend != NULL && backend->device_open != NULL && backend->device_close != NULL &&
           backend->sound_release != NULL && backend->voice_start != NULL &&
           backend->voice_stop != NULL && backend->voice_set_paused != NULL &&
           backend->voice_set_mix != NULL && backend->voice_is_playing != NULL &&
           backend->music_open != NULL && backend->music_close != NULL &&
           backend->music_play != NULL && backend->music_stop != NULL &&
           backend->music_set_paused != NULL && backend->music_set_gain != NULL &&
           backend->music_update != NULL;
}

static VgAudioSoundSlot *sound_resolve(VgAudioCore *core, VgAudioSound sound, uint32_t *out_index) {
    uint32_t index, generation;
    if (core == NULL || !handle_split(sound.value, core->config.max_sounds, &index, &generation))
        return NULL;
    VgAudioSoundSlot *slot = &core->sounds[index];
    if (!slot->used || !slot->external_ref || slot->generation != generation)
        return NULL;
    if (out_index != NULL)
        *out_index = index;
    return slot;
}

static VgAudioEmitterSlot *emitter_resolve(VgAudioCore *core, VgAudioEmitter emitter,
                                           uint32_t *out_index) {
    uint32_t index, generation;
    if (core == NULL ||
        !handle_split(emitter.value, core->config.max_emitters, &index, &generation))
        return NULL;
    VgAudioEmitterSlot *slot = &core->emitters[index];
    if (!slot->used || slot->generation != generation)
        return NULL;
    if (out_index != NULL)
        *out_index = index;
    return slot;
}

static VgAudioVoiceSlot *voice_resolve(VgAudioCore *core, VgAudioVoice voice, uint32_t *out_index) {
    uint32_t index, generation;
    if (core == NULL || !handle_split(voice.value, core->config.max_voices, &index, &generation))
        return NULL;
    VgAudioVoiceSlot *slot = &core->voices[index];
    if (!slot->used || slot->generation != generation)
        return NULL;
    if (out_index != NULL)
        *out_index = index;
    return slot;
}

static VgAudioMusicSlot *music_resolve(VgAudioCore *core, VgAudioMusic music, uint32_t *out_index) {
    uint32_t index, generation;
    if (core == NULL ||
        !handle_split(music.value, core->config.max_music_streams, &index, &generation))
        return NULL;
    VgAudioMusicSlot *slot = &core->music[index];
    if (!slot->used || slot->generation != generation)
        return NULL;
    if (out_index != NULL)
        *out_index = index;
    return slot;
}

static bool core_paused(const VgAudioCore *core) {
    return core->simulation_paused || core->focus_paused;
}

static VgResult device_error(const VgAudioCore *core) {
    return core->stats.last_device_result != VG_OK ? core->stats.last_device_result : VG_ERROR_IO;
}

static void sound_collect(VgAudioCore *core, uint32_t index) {
    VgAudioSoundSlot *sound = &core->sounds[index];
    if (!sound->used || sound->external_ref || sound->emitter_refs != 0u || sound->voice_refs != 0u)
        return;
    if (sound->token != 0u)
        core->config.backend.sound_release(core->config.backend.user, sound->token);
    uint32_t generation = next_generation(sound->generation);
    memset(sound, 0, sizeof(*sound));
    sound->generation = generation;
    if (core->stats.sounds != 0u)
        --core->stats.sounds;
}

static void voice_finish(VgAudioCore *core, uint32_t index, bool stop_backend) {
    VgAudioVoiceSlot *voice = &core->voices[index];
    if (!voice->used)
        return;
    if (stop_backend)
        core->config.backend.voice_stop(core->config.backend.user, voice->token);
    uint32_t sound_index = voice->sound_index;
    uint32_t sound_generation = voice->sound_generation;
    uint32_t generation = next_generation(voice->generation);
    memset(voice, 0, sizeof(*voice));
    voice->generation = generation;
    if (core->stats.voices != 0u)
        --core->stats.voices;
    ++core->stats.voice_stops;
    VgAudioSoundSlot *sound = &core->sounds[sound_index];
    if (sound->used && sound->generation == sound_generation && sound->voice_refs != 0u) {
        --sound->voice_refs;
        sound_collect(core, sound_index);
    }
}

static float clamp01(float value) {
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static void emitter_mix(const VgAudioCore *core, const VgAudioEmitterSlot *emitter, float *out_gain,
                        float *out_pan) {
    float attenuation = 1.0f;
    float pan = 0.0f;
    if (emitter->description.spatial) {
        float dx = emitter->description.position.x - core->listener.position.x;
        float dy = emitter->description.position.y - core->listener.position.y;
        float dz = emitter->description.position.z - core->listener.position.z;
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (distance >= emitter->description.max_distance)
            attenuation = 0.0f;
        else if (distance > emitter->description.min_distance)
            attenuation =
                1.0f - (distance - emitter->description.min_distance) /
                           (emitter->description.max_distance - emitter->description.min_distance);
        if (distance > 1.0e-6f)
            pan = (dx * core->listener.right.x + dy * core->listener.right.y +
                   dz * core->listener.right.z) /
                  distance;
    }
    float bus = core->bus_gain[emitter->description.bus];
    *out_gain = clamp01(core->bus_gain[VG_AUDIO_BUS_MASTER] * bus * emitter->description.gain *
                        attenuation);
    *out_pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);
}

static void voice_apply_mix(VgAudioCore *core, VgAudioVoiceSlot *voice) {
    if (voice->emitter_index >= core->config.max_emitters)
        return;
    VgAudioEmitterSlot *emitter = &core->emitters[voice->emitter_index];
    if (!emitter->used || emitter->generation != voice->emitter_generation)
        return;
    float gain, pan;
    emitter_mix(core, emitter, &gain, &pan);
    core->config.backend.voice_set_mix(core->config.backend.user, voice->token, gain, pan);
}

VgResult vg_audio_core_create(const VgAudioCoreConfig *config, VgAudioCore **out_core) {
    if (config == NULL || out_core == NULL || config->max_sounds == 0u ||
        config->max_emitters == 0u || config->max_voices == 0u || config->max_music_streams == 0u ||
        !backend_valid(&config->backend) ||
        ((config->allocate == NULL) != (config->deallocate == NULL)))
        return VG_ERROR_INVALID_ARGUMENT;
    VgAllocateFn allocate = config->allocate != NULL ? config->allocate : default_allocate;
    VgDeallocateFn deallocate =
        config->deallocate != NULL ? config->deallocate : default_deallocate;
    VgAudioCore *core = allocate(config->allocator_user, sizeof(*core));
    if (core == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    memset(core, 0, sizeof(*core));
    core->config = *config;
    core->config.allocate = allocate;
    core->config.deallocate = deallocate;
#define ALLOC_ARRAY(field, count)                                                                  \
    do {                                                                                           \
        uint64_t bytes_ = (uint64_t)(count) * sizeof(*core->field);                                \
        core->field = allocate(config->allocator_user, bytes_);                                    \
        if (core->field == NULL)                                                                   \
            goto oom;                                                                              \
        memset(core->field, 0, (size_t)bytes_);                                                    \
    } while (0)
    ALLOC_ARRAY(sounds, config->max_sounds);
    ALLOC_ARRAY(emitters, config->max_emitters);
    ALLOC_ARRAY(voices, config->max_voices);
    ALLOC_ARRAY(music, config->max_music_streams);
#undef ALLOC_ARRAY
    for (uint32_t bus = 0u; bus < VG_AUDIO_BUS_COUNT; ++bus)
        core->bus_gain[bus] = 1.0f;
    core->listener.right.x = 1.0f;
    VgResult open_result = core->config.backend.device_open(core->config.backend.user);
    core->stats.last_device_result = open_result;
    core->device_open = open_result == VG_OK;
    core->stats.device_state =
        core->device_open ? VG_AUDIO_DEVICE_READY : VG_AUDIO_DEVICE_UNAVAILABLE;
    *out_core = core;
    return VG_OK;
oom:
    deallocate(config->allocator_user, core->music);
    deallocate(config->allocator_user, core->voices);
    deallocate(config->allocator_user, core->emitters);
    deallocate(config->allocator_user, core->sounds);
    deallocate(config->allocator_user, core);
    return VG_ERROR_OUT_OF_MEMORY;
}

void vg_audio_core_destroy(VgAudioCore *core) {
    if (core == NULL)
        return;
    for (uint32_t index = 0u; index < core->config.max_voices; ++index)
        voice_finish(core, index, true);
    for (uint32_t index = 0u; index < core->config.max_music_streams; ++index) {
        if (!core->music[index].used)
            continue;
        core->config.backend.music_stop(core->config.backend.user, core->music[index].token);
        core->config.backend.music_close(core->config.backend.user, core->music[index].token);
    }
    for (uint32_t index = 0u; index < core->config.max_sounds; ++index) {
        VgAudioSoundSlot *sound = &core->sounds[index];
        if (sound->used && sound->token != 0u)
            core->config.backend.sound_release(core->config.backend.user, sound->token);
    }
    if (core->device_open)
        core->config.backend.device_close(core->config.backend.user);
    VgDeallocateFn deallocate = core->config.deallocate;
    void *allocator_user = core->config.allocator_user;
    deallocate(allocator_user, core->music);
    deallocate(allocator_user, core->voices);
    deallocate(allocator_user, core->emitters);
    deallocate(allocator_user, core->sounds);
    deallocate(allocator_user, core);
}

VgAudioCoreStats vg_audio_core_stats(const VgAudioCore *core) {
    return core != NULL ? core->stats : (VgAudioCoreStats){0};
}

VgResult vg_audio_core_set_bus_gain(VgAudioCore *core, VgAudioBus bus, float linear_gain) {
    if (core == NULL || bus >= VG_AUDIO_BUS_COUNT || !isfinite(linear_gain) || linear_gain < 0.0f ||
        linear_gain > 1.0f)
        return VG_ERROR_INVALID_ARGUMENT;
    core->bus_gain[bus] = linear_gain;
    for (uint32_t index = 0u; index < core->config.max_voices; ++index)
        if (core->voices[index].used)
            voice_apply_mix(core, &core->voices[index]);
    for (uint32_t index = 0u; index < core->config.max_music_streams; ++index)
        if (core->music[index].used)
            core->config.backend.music_set_gain(
                core->config.backend.user, core->music[index].token,
                clamp01(core->bus_gain[VG_AUDIO_BUS_MASTER] * core->bus_gain[VG_AUDIO_BUS_MUSIC] *
                        core->music[index].gain));
    return VG_OK;
}

VgResult vg_audio_core_get_bus_gain(const VgAudioCore *core, VgAudioBus bus,
                                    float *out_linear_gain) {
    if (core == NULL || bus >= VG_AUDIO_BUS_COUNT || out_linear_gain == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    *out_linear_gain = core->bus_gain[bus];
    return VG_OK;
}

VgResult vg_audio_core_set_listener(VgAudioCore *core, const VgAudioListener *listener) {
    if (core == NULL || listener == NULL || !finite_vec3(listener->position) ||
        !finite_vec3(listener->right))
        return VG_ERROR_INVALID_ARGUMENT;
    float length =
        sqrtf(listener->right.x * listener->right.x + listener->right.y * listener->right.y +
              listener->right.z * listener->right.z);
    if (length <= 1.0e-6f)
        return VG_ERROR_INVALID_ARGUMENT;
    core->listener = *listener;
    core->listener.right.x /= length;
    core->listener.right.y /= length;
    core->listener.right.z /= length;
    for (uint32_t index = 0u; index < core->config.max_voices; ++index)
        if (core->voices[index].used)
            voice_apply_mix(core, &core->voices[index]);
    return VG_OK;
}

VgResult vg_audio_core_set_paused(VgAudioCore *core, bool simulation_paused, bool focus_paused) {
    if (core == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    bool was_paused = core_paused(core);
    core->simulation_paused = simulation_paused;
    core->focus_paused = focus_paused;
    bool paused = core_paused(core);
    if (was_paused == paused)
        return VG_OK;
    for (uint32_t index = 0u; index < core->config.max_voices; ++index)
        if (core->voices[index].used)
            core->config.backend.voice_set_paused(core->config.backend.user,
                                                  core->voices[index].token, paused);
    for (uint32_t index = 0u; index < core->config.max_music_streams; ++index)
        if (core->music[index].used)
            core->config.backend.music_set_paused(core->config.backend.user,
                                                  core->music[index].token, paused);
    return VG_OK;
}

VgResult vg_audio_core_sound_create(VgAudioCore *core, uint64_t resource_key, uint64_t sound_token,
                                    VgAudioSound *out_sound) {
    if (core == NULL || resource_key == 0u || sound_token == 0u || out_sound == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    for (uint32_t index = 0u; index < core->config.max_sounds; ++index)
        if (core->sounds[index].used && core->sounds[index].resource_key == resource_key)
            return VG_ERROR_CONFLICT;
    for (uint32_t index = 0u; index < core->config.max_sounds; ++index) {
        VgAudioSoundSlot *slot = &core->sounds[index];
        if (slot->used)
            continue;
        if (slot->generation == 0u)
            slot->generation = 1u;
        slot->used = true;
        slot->external_ref = true;
        slot->resource_key = resource_key;
        slot->token = sound_token;
        out_sound->value = handle_make(index, slot->generation);
        ++core->stats.sounds;
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

VgResult vg_audio_core_sound_release(VgAudioCore *core, VgAudioSound sound) {
    uint32_t index;
    VgAudioSoundSlot *slot = sound_resolve(core, sound, &index);
    if (slot == NULL)
        return VG_ERROR_INVALID_HANDLE;
    slot->external_ref = false;
    sound_collect(core, index);
    return VG_OK;
}

static bool emitter_desc_valid(const VgAudioEmitterDesc *description) {
    return description != NULL && description->bus > VG_AUDIO_BUS_MASTER &&
           description->bus < VG_AUDIO_BUS_COUNT && finite_vec3(description->position) &&
           isfinite(description->gain) && description->gain >= 0.0f && description->gain <= 1.0f &&
           isfinite(description->min_distance) && isfinite(description->max_distance) &&
           description->min_distance >= 0.0f &&
           description->max_distance > description->min_distance;
}

VgResult vg_audio_core_emitter_create(VgAudioCore *core, const VgAudioEmitterDesc *description,
                                      VgAudioEmitter *out_emitter) {
    if (core == NULL || !emitter_desc_valid(description) || out_emitter == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    uint32_t sound_index;
    VgAudioSoundSlot *sound = sound_resolve(core, description->sound, &sound_index);
    if (sound == NULL)
        return VG_ERROR_INVALID_HANDLE;
    for (uint32_t index = 0u; index < core->config.max_emitters; ++index) {
        VgAudioEmitterSlot *slot = &core->emitters[index];
        if (slot->used)
            continue;
        if (slot->generation == 0u)
            slot->generation = 1u;
        slot->used = true;
        slot->sound_index = sound_index;
        slot->sound_generation = sound->generation;
        slot->description = *description;
        ++sound->emitter_refs;
        out_emitter->value = handle_make(index, slot->generation);
        ++core->stats.emitters;
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

static void stop_emitter_voices(VgAudioCore *core, uint32_t emitter_index,
                                uint32_t emitter_generation) {
    for (uint32_t index = 0u; index < core->config.max_voices; ++index) {
        VgAudioVoiceSlot *voice = &core->voices[index];
        if (voice->used && voice->emitter_index == emitter_index &&
            voice->emitter_generation == emitter_generation)
            voice_finish(core, index, true);
    }
}

VgResult vg_audio_core_emitter_update(VgAudioCore *core, VgAudioEmitter emitter,
                                      const VgAudioEmitterDesc *description) {
    uint32_t index;
    VgAudioEmitterSlot *slot = emitter_resolve(core, emitter, &index);
    if (slot == NULL)
        return VG_ERROR_INVALID_HANDLE;
    if (!emitter_desc_valid(description))
        return VG_ERROR_INVALID_ARGUMENT;
    uint32_t sound_index;
    VgAudioSoundSlot *sound = sound_resolve(core, description->sound, &sound_index);
    if (sound == NULL)
        return VG_ERROR_INVALID_HANDLE;
    if (sound_index != slot->sound_index || sound->generation != slot->sound_generation)
        return VG_ERROR_CONFLICT;
    slot->description = *description;
    for (uint32_t voice_index = 0u; voice_index < core->config.max_voices; ++voice_index) {
        VgAudioVoiceSlot *voice = &core->voices[voice_index];
        if (voice->used && voice->emitter_index == index &&
            voice->emitter_generation == slot->generation)
            voice_apply_mix(core, voice);
    }
    return VG_OK;
}

VgResult vg_audio_core_emitter_destroy(VgAudioCore *core, VgAudioEmitter emitter) {
    uint32_t index;
    VgAudioEmitterSlot *slot = emitter_resolve(core, emitter, &index);
    if (slot == NULL)
        return VG_ERROR_INVALID_HANDLE;
    uint32_t sound_index = slot->sound_index;
    uint32_t sound_generation = slot->sound_generation;
    stop_emitter_voices(core, index, slot->generation);
    uint32_t generation = next_generation(slot->generation);
    memset(slot, 0, sizeof(*slot));
    slot->generation = generation;
    if (core->stats.emitters != 0u)
        --core->stats.emitters;
    VgAudioSoundSlot *sound = &core->sounds[sound_index];
    if (sound->used && sound->generation == sound_generation && sound->emitter_refs != 0u) {
        --sound->emitter_refs;
        sound_collect(core, sound_index);
    }
    return VG_OK;
}

VgResult vg_audio_core_emitter_play(VgAudioCore *core, VgAudioEmitter emitter, uint64_t event_id,
                                    VgAudioVoice *out_voice) {
    uint32_t emitter_index;
    VgAudioEmitterSlot *source = emitter_resolve(core, emitter, &emitter_index);
    if (source == NULL)
        return VG_ERROR_INVALID_HANDLE;
    if (event_id == 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    if (source->last_event_id == event_id) {
        if (out_voice != NULL)
            *out_voice = source->last_voice;
        ++core->stats.duplicate_events;
        return VG_OK;
    }
    if (!core->device_open)
        return device_error(core);
    uint32_t selected = UINT32_MAX;
    uint32_t victim = UINT32_MAX;
    for (uint32_t index = 0u; index < core->config.max_voices; ++index) {
        VgAudioVoiceSlot *voice = &core->voices[index];
        if (!voice->used) {
            selected = index;
            break;
        }
        if (victim == UINT32_MAX || voice->priority < core->voices[victim].priority ||
            (voice->priority == core->voices[victim].priority &&
             voice->sequence < core->voices[victim].sequence))
            victim = index;
    }
    if (selected == UINT32_MAX) {
        if (victim == UINT32_MAX || source->description.priority <= core->voices[victim].priority)
            return VG_ERROR_CAPACITY;
        selected = victim;
    }
    VgAudioSoundSlot *sound = &core->sounds[source->sound_index];
    if (!sound->used || sound->generation != source->sound_generation)
        return VG_ERROR_INVALID_HANDLE;
    uint64_t token = 0u;
    VgResult result = core->config.backend.voice_start(core->config.backend.user, sound->token,
                                                       source->description.loop, &token);
    if (result != VG_OK || token == 0u)
        return result != VG_OK ? result : VG_ERROR_IO;
    if (core->voices[selected].used) {
        voice_finish(core, selected, true);
        ++core->stats.voice_steals;
    }
    VgAudioVoiceSlot *voice = &core->voices[selected];
    if (voice->generation == 0u)
        voice->generation = 1u;
    voice->used = true;
    voice->sound_index = source->sound_index;
    voice->sound_generation = source->sound_generation;
    voice->emitter_index = emitter_index;
    voice->emitter_generation = source->generation;
    voice->priority = source->description.priority;
    voice->sequence = ++core->next_sequence;
    voice->token = token;
    ++sound->voice_refs;
    ++core->stats.voices;
    ++core->stats.voice_starts;
    voice_apply_mix(core, voice);
    core->config.backend.voice_set_paused(core->config.backend.user, token, core_paused(core));
    source->last_event_id = event_id;
    source->last_voice.value = handle_make(selected, voice->generation);
    if (out_voice != NULL)
        *out_voice = source->last_voice;
    return VG_OK;
}

VgResult vg_audio_core_voice_stop(VgAudioCore *core, VgAudioVoice voice) {
    uint32_t index;
    if (voice_resolve(core, voice, &index) == NULL)
        return VG_ERROR_INVALID_HANDLE;
    voice_finish(core, index, true);
    return VG_OK;
}

VgResult vg_audio_core_music_start(VgAudioCore *core, const VgAudioMusicDesc *description,
                                   VgAudioMusic *out_music) {
    if (core == NULL || description == NULL || out_music == NULL ||
        description->source_token == 0u || !isfinite(description->gain) ||
        description->gain < 0.0f || description->gain > 1.0f)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!core->device_open)
        return device_error(core);
    uint32_t selected = UINT32_MAX;
    for (uint32_t index = 0u; index < core->config.max_music_streams; ++index)
        if (!core->music[index].used) {
            selected = index;
            break;
        }
    if (selected == UINT32_MAX)
        return VG_ERROR_CAPACITY;
    uint64_t token = 0u;
    VgResult result = core->config.backend.music_open(core->config.backend.user,
                                                      description->source_token, &token);
    if (result != VG_OK || token == 0u)
        return result != VG_OK ? result : VG_ERROR_IO;
    VgAudioMusicSlot *slot = &core->music[selected];
    if (slot->generation == 0u)
        slot->generation = 1u;
    slot->used = true;
    slot->world_id = description->world_id;
    slot->token = token;
    slot->gain = description->gain;
    slot->loop = description->loop;
    core->config.backend.music_set_gain(core->config.backend.user, token,
                                        clamp01(core->bus_gain[VG_AUDIO_BUS_MASTER] *
                                                core->bus_gain[VG_AUDIO_BUS_MUSIC] * slot->gain));
    core->config.backend.music_play(core->config.backend.user, token, slot->loop);
    core->config.backend.music_set_paused(core->config.backend.user, token, core_paused(core));
    out_music->value = handle_make(selected, slot->generation);
    ++core->stats.music_streams;
    return VG_OK;
}

VgResult vg_audio_core_music_stop(VgAudioCore *core, VgAudioMusic music) {
    uint32_t index;
    VgAudioMusicSlot *slot = music_resolve(core, music, &index);
    if (slot == NULL)
        return VG_ERROR_INVALID_HANDLE;
    core->config.backend.music_stop(core->config.backend.user, slot->token);
    core->config.backend.music_close(core->config.backend.user, slot->token);
    uint32_t generation = next_generation(slot->generation);
    memset(slot, 0, sizeof(*slot));
    slot->generation = generation;
    if (core->stats.music_streams != 0u)
        --core->stats.music_streams;
    return VG_OK;
}

VgResult vg_audio_core_update(VgAudioCore *core) {
    if (core == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!core->device_open)
        return device_error(core);
    for (uint32_t index = 0u; index < core->config.max_voices; ++index) {
        VgAudioVoiceSlot *voice = &core->voices[index];
        if (!voice->used)
            continue;
        if (!core->config.backend.voice_is_playing(core->config.backend.user, voice->token))
            voice_finish(core, index, false);
        else
            voice_apply_mix(core, voice);
    }
    for (uint32_t index = 0u; index < core->config.max_music_streams; ++index) {
        VgAudioMusicSlot *slot = &core->music[index];
        if (!slot->used)
            continue;
        VgResult result = core->config.backend.music_update(core->config.backend.user, slot->token);
        if (result != VG_OK) {
            core->stats.last_device_result = result;
            return result;
        }
        ++core->stats.stream_updates;
    }
    return VG_OK;
}

VgResult vg_audio_core_world_unload(VgAudioCore *core, uint64_t world_id) {
    if (core == NULL || world_id == 0u)
        return VG_ERROR_INVALID_ARGUMENT;
    for (uint32_t index = 0u; index < core->config.max_emitters; ++index) {
        VgAudioEmitterSlot *slot = &core->emitters[index];
        if (slot->used && slot->description.world_id == world_id) {
            VgAudioEmitter handle = {handle_make(index, slot->generation)};
            (void)vg_audio_core_emitter_destroy(core, handle);
        }
    }
    for (uint32_t index = 0u; index < core->config.max_music_streams; ++index) {
        VgAudioMusicSlot *slot = &core->music[index];
        if (slot->used && slot->world_id == world_id) {
            VgAudioMusic handle = {handle_make(index, slot->generation)};
            (void)vg_audio_core_music_stop(core, handle);
        }
    }
    return VG_OK;
}
