#ifndef VESTIGIO_RAYLIB_AUDIO_H
#define VESTIGIO_RAYLIB_AUDIO_H

#include "audio/audio_core.h"

#include <stddef.h>
#include <stdint.h>

typedef struct VgRaylibAudio VgRaylibAudio;

/* Create the adapter before the core; core creation calls backend.device_open.
 * Destroy the core first, then this adapter. All calls use the audio owner thread. */
VgRaylibAudio *vg_raylib_audio_create(uint32_t max_sounds, uint32_t max_voices,
                                      uint32_t max_music_sources,
                                      uint32_t max_music_streams);
void vg_raylib_audio_destroy(VgRaylibAudio *audio);
VgAudioBackend vg_raylib_audio_backend(VgRaylibAudio *audio);

/* WAV paths are UTF-8, including on Windows. Load after core creation has
 * opened the device. The core owns sound_token
 * after vg_audio_core_sound_create; an unregistered token is freed on destroy. */
VgResult vg_raylib_audio_sound_load(VgRaylibAudio *audio, const char *path,
                                     uint64_t *out_sound_token);
/* Keeps encoded WAV bytes alive while streams use them. Each music_open
 * creates its own streaming decoder; music_close releases it. */
VgResult vg_raylib_audio_music_source_add(VgRaylibAudio *audio, const char *path,
                                           uint64_t *out_source_token);
const char *vg_raylib_audio_last_error(const VgRaylibAudio *audio);

#endif
