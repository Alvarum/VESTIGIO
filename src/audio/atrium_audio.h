#ifndef VESTIGIO_ATRIUM_AUDIO_H
#define VESTIGIO_ATRIUM_AUDIO_H

#include "audio/audio_core.h"
#include <stddef.h>

typedef struct VgAtriumAudio VgAtriumAudio;

/* Audio is optional at runtime: a missing device returns an object with
 * ready=false and a diagnostic, so rendering and play remain usable. */
VgAtriumAudio *vg_atrium_audio_create(const char *audio_directory,
                                      char *error, size_t error_capacity);
void vg_atrium_audio_destroy(VgAtriumAudio *audio);
bool vg_atrium_audio_ready(const VgAtriumAudio *audio);
VgAudioCoreStats vg_atrium_audio_stats(const VgAtriumAudio *audio);
VgResult vg_atrium_audio_set_gain(VgAtriumAudio *audio, VgAudioBus bus, float gain);
VgResult vg_atrium_audio_set_listener(VgAtriumAudio *audio,
                                      VgVec3 position, VgVec3 right);
VgResult vg_atrium_audio_set_paused(VgAtriumAudio *audio,
                                    bool simulation_paused, bool focus_paused);
VgResult vg_atrium_audio_play_door(VgAtriumAudio *audio, VgVec3 position);
VgResult vg_atrium_audio_update(VgAtriumAudio *audio);

#endif
