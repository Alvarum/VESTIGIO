#include "audio/raylib_audio.h"

#include "raylib.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#undef Rectangle
#undef ShowCursor
#undef CloseWindow
#endif

typedef struct SoundSlot {
    uint64_t token;
    Sound sound;
} SoundSlot;

typedef struct VoiceSlot {
    uint64_t token;
    Sound alias;
    bool loop;
    bool paused;
} VoiceSlot;

typedef struct SourceSlot {
    uint64_t token;
    unsigned char *encoded_wav;
    int byte_count;
} SourceSlot;

typedef struct MusicSlot {
    uint64_t token;
    Music music;
    bool paused;
} MusicSlot;

struct VgRaylibAudio {
    SoundSlot *sounds;
    VoiceSlot *voices;
    SourceSlot *sources;
    MusicSlot *music;
    uint32_t sound_capacity;
    uint32_t voice_capacity;
    uint32_t source_capacity;
    uint32_t music_capacity;
    uint64_t next_token;
    bool device_open;
    bool owns_device;
    char last_error[256];
};

static void set_error(VgRaylibAudio *audio, const char *value) {
    if (audio != NULL)
        (void)snprintf(audio->last_error, sizeof(audio->last_error), "%s", value);
}

/* raylib's file-name loaders use narrow fopen on Windows. Read paths as UTF-8
 * ourselves, then hand the encoded WAV bytes to raylib's memory decoders. */
static VgResult read_wav_file(VgRaylibAudio *audio, const char *path,
                              unsigned char **out_bytes, int *out_size) {
    FILE *file = NULL;
#ifdef _WIN32
    int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path,
                                          -1, NULL, 0);
    if (wide_length <= 0) {
        set_error(audio, "Ruta de audio no es UTF-8 valido");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    wchar_t *wide_path = malloc((size_t)wide_length * sizeof(*wide_path));
    if (wide_path == NULL)
        return VG_ERROR_OUT_OF_MEMORY;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                            wide_path, wide_length) == wide_length)
        file = _wfopen(wide_path, L"rb");
    free(wide_path);
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) {
        set_error(audio, "No se encontro archivo WAV de audio");
        return VG_ERROR_NOT_FOUND;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        (void)fclose(file);
        return VG_ERROR_IO;
    }
    long length = ftell(file);
    if (length <= 0 || length > 128L * 1024L * 1024L ||
        fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        set_error(audio, "Archivo WAV vacio o demasiado grande");
        return VG_ERROR_IO;
    }
    unsigned char *bytes = malloc((size_t)length);
    if (bytes == NULL) {
        (void)fclose(file);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    size_t read_count = fread(bytes, 1u, (size_t)length, file);
    (void)fclose(file);
    if (read_count != (size_t)length) {
        free(bytes);
        set_error(audio, "No se pudo leer WAV completo");
        return VG_ERROR_IO;
    }
    *out_bytes = bytes;
    *out_size = (int)length;
    return VG_OK;
}

static uint64_t token_next(VgRaylibAudio *audio) {
    if (audio->next_token == UINT64_MAX)
        return 0u;
    return ++audio->next_token;
}

static SoundSlot *sound_slot(VgRaylibAudio *audio, uint64_t token) {
    if (audio == NULL || token == 0u)
        return NULL;
    for (uint32_t i = 0u; i < audio->sound_capacity; ++i)
        if (audio->sounds[i].token == token)
            return &audio->sounds[i];
    return NULL;
}

static VoiceSlot *voice_slot(VgRaylibAudio *audio, uint64_t token) {
    if (audio == NULL || token == 0u)
        return NULL;
    for (uint32_t i = 0u; i < audio->voice_capacity; ++i)
        if (audio->voices[i].token == token)
            return &audio->voices[i];
    return NULL;
}

static SourceSlot *source_slot(VgRaylibAudio *audio, uint64_t token) {
    if (audio == NULL || token == 0u)
        return NULL;
    for (uint32_t i = 0u; i < audio->source_capacity; ++i)
        if (audio->sources[i].token == token)
            return &audio->sources[i];
    return NULL;
}

static MusicSlot *music_slot(VgRaylibAudio *audio, uint64_t token) {
    if (audio == NULL || token == 0u)
        return NULL;
    for (uint32_t i = 0u; i < audio->music_capacity; ++i)
        if (audio->music[i].token == token)
            return &audio->music[i];
    return NULL;
}

static void voice_release(VoiceSlot *slot) {
    if (slot == NULL || slot->token == 0u)
        return;
    StopSound(slot->alias);
    UnloadSoundAlias(slot->alias);
    *slot = (VoiceSlot){0};
}

static void music_release(MusicSlot *slot) {
    if (slot == NULL || slot->token == 0u)
        return;
    StopMusicStream(slot->music);
    UnloadMusicStream(slot->music);
    *slot = (MusicSlot){0};
}

static VgResult device_open(void *user) {
    VgRaylibAudio *audio = user;
    if (audio == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (audio->device_open)
        return VG_OK;
    bool already_open = IsAudioDeviceReady();
    if (!already_open)
        InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        set_error(audio, "Dispositivo de audio no disponible");
        return VG_ERROR_IO;
    }
    audio->device_open = true;
    audio->owns_device = !already_open;
    set_error(audio, "");
    return VG_OK;
}

static void device_close(void *user) {
    VgRaylibAudio *audio = user;
    if (audio == NULL || !audio->device_open)
        return;
    /* A load can succeed just before core registration fails. Release those
     * unclaimed raylib objects while the device still exists. */
    if (audio->voices != NULL)
        for (uint32_t i = 0u; i < audio->voice_capacity; ++i)
            voice_release(&audio->voices[i]);
    if (audio->music != NULL)
        for (uint32_t i = 0u; i < audio->music_capacity; ++i)
            music_release(&audio->music[i]);
    if (audio->sounds != NULL)
        for (uint32_t i = 0u; i < audio->sound_capacity; ++i)
            if (audio->sounds[i].token != 0u) {
                UnloadSound(audio->sounds[i].sound);
                audio->sounds[i] = (SoundSlot){0};
            }
    if (audio->owns_device)
        CloseAudioDevice();
    audio->device_open = false;
    audio->owns_device = false;
}

static void sound_release(void *user, uint64_t token) {
    VgRaylibAudio *audio = user;
    SoundSlot *slot = sound_slot(audio, token);
    if (slot == NULL)
        return;
    /* Core guarantees that no aliases remain when this callback runs. */
    UnloadSound(slot->sound);
    *slot = (SoundSlot){0};
}

static VgResult voice_start(void *user, uint64_t sound_token, bool loop,
                            uint64_t *out_voice_token) {
    VgRaylibAudio *audio = user;
    if (audio == NULL || out_voice_token == NULL || !audio->device_open)
        return VG_ERROR_INVALID_ARGUMENT;
    SoundSlot *sound = sound_slot(audio, sound_token);
    if (sound == NULL)
        return VG_ERROR_INVALID_HANDLE;
    for (uint32_t i = 0u; i < audio->voice_capacity; ++i) {
        VoiceSlot *voice = &audio->voices[i];
        if (voice->token != 0u)
            continue;
        Sound alias = LoadSoundAlias(sound->sound);
        if (!IsSoundValid(alias)) {
            set_error(audio, "No se pudo crear voz de audio independiente");
            return VG_ERROR_IO;
        }
        uint64_t token = token_next(audio);
        if (token == 0u) {
            UnloadSoundAlias(alias);
            return VG_ERROR_CAPACITY;
        }
        *voice = (VoiceSlot){.token = token, .alias = alias, .loop = loop};
        PlaySound(alias);
        *out_voice_token = token;
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

static void voice_stop(void *user, uint64_t token) {
    voice_release(voice_slot(user, token));
}

static void voice_set_paused(void *user, uint64_t token, bool paused) {
    VoiceSlot *voice = voice_slot(user, token);
    if (voice == NULL || voice->paused == paused)
        return;
    voice->paused = paused;
    if (paused)
        PauseSound(voice->alias);
    else
        ResumeSound(voice->alias);
}

static void voice_set_mix(void *user, uint64_t token, float gain, float pan) {
    VoiceSlot *voice = voice_slot(user, token);
    if (voice == NULL)
        return;
    SetSoundVolume(voice->alias, gain);
    SetSoundPan(voice->alias, pan);
}

static bool voice_is_playing(void *user, uint64_t token) {
    VoiceSlot *voice = voice_slot(user, token);
    if (voice == NULL)
        return false;
    if (voice->paused)
        return true;
    if (IsSoundPlaying(voice->alias))
        return true;
    if (voice->loop) {
        PlaySound(voice->alias);
        return IsSoundPlaying(voice->alias);
    }
    voice_release(voice);
    return false;
}

static VgResult music_open(void *user, uint64_t source_token,
                           uint64_t *out_stream_token) {
    VgRaylibAudio *audio = user;
    if (audio == NULL || out_stream_token == NULL || !audio->device_open)
        return VG_ERROR_INVALID_ARGUMENT;
    SourceSlot *source = source_slot(audio, source_token);
    if (source == NULL)
        return VG_ERROR_INVALID_HANDLE;
    for (uint32_t i = 0u; i < audio->music_capacity; ++i) {
        MusicSlot *slot = &audio->music[i];
        if (slot->token != 0u)
            continue;
        Music music = LoadMusicStreamFromMemory(".wav", source->encoded_wav,
                                               source->byte_count);
        if (!IsMusicValid(music)) {
            set_error(audio, "No se pudo abrir stream de musica");
            return VG_ERROR_IO;
        }
        uint64_t token = token_next(audio);
        if (token == 0u) {
            UnloadMusicStream(music);
            return VG_ERROR_CAPACITY;
        }
        *slot = (MusicSlot){.token = token, .music = music};
        *out_stream_token = token;
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

static void music_close(void *user, uint64_t token) {
    music_release(music_slot(user, token));
}

static void music_play(void *user, uint64_t token, bool loop) {
    MusicSlot *slot = music_slot(user, token);
    if (slot == NULL)
        return;
    slot->music.looping = loop;
    PlayMusicStream(slot->music);
}

static void music_stop(void *user, uint64_t token) {
    MusicSlot *slot = music_slot(user, token);
    if (slot != NULL)
        StopMusicStream(slot->music);
}

static void music_set_paused(void *user, uint64_t token, bool paused) {
    MusicSlot *slot = music_slot(user, token);
    if (slot == NULL || slot->paused == paused)
        return;
    slot->paused = paused;
    if (paused)
        PauseMusicStream(slot->music);
    else
        ResumeMusicStream(slot->music);
}

static void music_set_gain(void *user, uint64_t token, float gain) {
    MusicSlot *slot = music_slot(user, token);
    if (slot != NULL)
        SetMusicVolume(slot->music, gain);
}

static VgResult music_update(void *user, uint64_t token) {
    MusicSlot *slot = music_slot(user, token);
    if (slot == NULL)
        return VG_ERROR_INVALID_HANDLE;
    if (!slot->paused)
        UpdateMusicStream(slot->music);
    return VG_OK;
}

VgRaylibAudio *vg_raylib_audio_create(uint32_t max_sounds, uint32_t max_voices,
                                      uint32_t max_music_sources,
                                      uint32_t max_music_streams) {
    if (max_sounds == 0u || max_voices == 0u || max_music_sources == 0u ||
        max_music_streams == 0u || max_sounds > 1024u || max_voices > 1024u ||
        max_music_sources > 1024u || max_music_streams > 1024u)
        return NULL;
    VgRaylibAudio *audio = calloc(1u, sizeof(*audio));
    if (audio == NULL)
        return NULL;
    audio->sound_capacity = max_sounds;
    /* Core starts a replacement before stopping its stolen victim. */
    audio->voice_capacity = max_voices + 1u;
    audio->source_capacity = max_music_sources;
    audio->music_capacity = max_music_streams;
    audio->sounds = calloc(max_sounds, sizeof(*audio->sounds));
    audio->voices = calloc(audio->voice_capacity, sizeof(*audio->voices));
    audio->sources = calloc(max_music_sources, sizeof(*audio->sources));
    audio->music = calloc(max_music_streams, sizeof(*audio->music));
    if (audio->sounds == NULL || audio->voices == NULL || audio->sources == NULL ||
        audio->music == NULL) {
        vg_raylib_audio_destroy(audio);
        return NULL;
    }
    return audio;
}

void vg_raylib_audio_destroy(VgRaylibAudio *audio) {
    if (audio == NULL)
        return;
    if (audio->voices != NULL)
        for (uint32_t i = 0u; i < audio->voice_capacity; ++i)
            voice_release(&audio->voices[i]);
    if (audio->music != NULL)
        for (uint32_t i = 0u; i < audio->music_capacity; ++i)
            music_release(&audio->music[i]);
    if (audio->sounds != NULL)
        for (uint32_t i = 0u; i < audio->sound_capacity; ++i)
            if (audio->sounds[i].token != 0u)
                sound_release(audio, audio->sounds[i].token);
    if (audio->sources != NULL)
        for (uint32_t i = 0u; i < audio->source_capacity; ++i)
            free(audio->sources[i].encoded_wav);
    device_close(audio);
    free(audio->music);
    free(audio->sources);
    free(audio->voices);
    free(audio->sounds);
    free(audio);
}

VgAudioBackend vg_raylib_audio_backend(VgRaylibAudio *audio) {
    if (audio == NULL)
        return (VgAudioBackend){0};
    return (VgAudioBackend){.user = audio,
                            .device_open = device_open,
                            .device_close = device_close,
                            .sound_release = sound_release,
                            .voice_start = voice_start,
                            .voice_stop = voice_stop,
                            .voice_set_paused = voice_set_paused,
                            .voice_set_mix = voice_set_mix,
                            .voice_is_playing = voice_is_playing,
                            .music_open = music_open,
                            .music_close = music_close,
                            .music_play = music_play,
                            .music_stop = music_stop,
                            .music_set_paused = music_set_paused,
                            .music_set_gain = music_set_gain,
                            .music_update = music_update};
}

VgResult vg_raylib_audio_sound_load(VgRaylibAudio *audio, const char *path,
                                     uint64_t *out_sound_token) {
    if (audio == NULL || path == NULL || path[0] == '\0' || out_sound_token == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!audio->device_open) {
        set_error(audio, "Dispositivo de audio no disponible");
        return VG_ERROR_IO;
    }
    for (uint32_t i = 0u; i < audio->sound_capacity; ++i) {
        SoundSlot *slot = &audio->sounds[i];
        if (slot->token != 0u)
            continue;
        unsigned char *bytes = NULL;
        int byte_count = 0;
        VgResult read_result = read_wav_file(audio, path, &bytes, &byte_count);
        if (read_result != VG_OK)
            return read_result;
        Wave wave = LoadWaveFromMemory(".wav", bytes, byte_count);
        free(bytes);
        if (!IsWaveValid(wave)) {
            if (wave.data != NULL)
                UnloadWave(wave);
            set_error(audio, "Archivo WAV invalido");
            return VG_ERROR_IO;
        }
        Sound sound = LoadSoundFromWave(wave);
        UnloadWave(wave);
        if (!IsSoundValid(sound)) {
            set_error(audio, "No se pudo cargar archivo de sonido");
            return VG_ERROR_IO;
        }
        uint64_t token = token_next(audio);
        if (token == 0u) {
            UnloadSound(sound);
            return VG_ERROR_CAPACITY;
        }
        *slot = (SoundSlot){.token = token, .sound = sound};
        *out_sound_token = token;
        set_error(audio, "");
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

VgResult vg_raylib_audio_music_source_add(VgRaylibAudio *audio,
                                           const char *path,
                                           uint64_t *out_source_token) {
    if (audio == NULL || path == NULL || path[0] == '\0' ||
        out_source_token == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    for (uint32_t i = 0u; i < audio->source_capacity; ++i) {
        SourceSlot *slot = &audio->sources[i];
        if (slot->token != 0u)
            continue;
        unsigned char *bytes = NULL;
        int byte_count = 0;
        VgResult read_result = read_wav_file(audio, path, &bytes, &byte_count);
        if (read_result != VG_OK)
            return read_result;
        uint64_t token = token_next(audio);
        if (token == 0u) {
            free(bytes);
            return VG_ERROR_CAPACITY;
        }
        *slot = (SourceSlot){.token = token, .encoded_wav = bytes,
                             .byte_count = byte_count};
        *out_source_token = token;
        set_error(audio, "");
        return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}

const char *vg_raylib_audio_last_error(const VgRaylibAudio *audio) {
    return audio != NULL ? audio->last_error : "Adapter de audio nulo";
}
