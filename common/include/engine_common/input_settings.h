#ifndef ENGINE_COMMON_INPUT_SETTINGS_H
#define ENGINE_COMMON_INPUT_SETTINGS_H

/* Input codes and layered settings are intentionally shared by both engines.
 * The Vg names remain ABI-stable from the initial public contract. */
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(VG_SHARED)
#if defined(VG_BUILD)
#define VG_API __declspec(dllexport)
#else
#define VG_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(VG_SHARED)
#define VG_API __attribute__((visibility("default")))
#else
#define VG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define VG_API_VERSION_MAJOR 0u
#define VG_API_VERSION_MINOR 1u
#define VG_API_VERSION ((VG_API_VERSION_MAJOR << 16u) | VG_API_VERSION_MINOR)
#define VG_INVALID_HANDLE_VALUE UINT64_C(0)

typedef int32_t VgResult;
enum {
    VG_OK = 0,
    VG_ERROR_INVALID_ARGUMENT = -1,
    VG_ERROR_INVALID_HANDLE = -2,
    VG_ERROR_WRONG_CONTEXT = -3,
    VG_ERROR_NOT_FOUND = -4,
    VG_ERROR_CAPACITY = -5,
    VG_ERROR_OUT_OF_MEMORY = -6,
    VG_ERROR_IO = -7,
    VG_ERROR_FORMAT_VERSION = -8,
    VG_ERROR_UNSUPPORTED = -9,
    VG_ERROR_GPU = -10,
    VG_ERROR_CONFLICT = -11,
    VG_ERROR_REENTRANT = -12,
    VG_ERROR_WRONG_WORLD = -13,
    VG_ERROR_WRONG_TYPE = -14,
    VG_ERROR_WRONG_THREAD = -15
};

typedef uint64_t VgActionSet;
#define VG_ACTION_JUMP (UINT64_C(1) << 0u)
#define VG_ACTION_PRIMARY (UINT64_C(1) << 1u)
#define VG_ACTION_INTERACT (UINT64_C(1) << 2u)
#define VG_ACTION_PAUSE (UINT64_C(1) << 3u)
#define VG_ACTION_ACCEPT (UINT64_C(1) << 4u)
#define VG_ACTION_UI_UP (UINT64_C(1) << 5u)
#define VG_ACTION_UI_DOWN (UINT64_C(1) << 6u)
#define VG_ACTION_UI_LEFT (UINT64_C(1) << 7u)
#define VG_ACTION_UI_RIGHT (UINT64_C(1) << 8u)
#define VG_ACTION_MAP (UINT64_C(1) << 9u)
#define VG_ACTION_WIREFRAME (UINT64_C(1) << 10u)
#define VG_ACTION_DEPTH (UINT64_C(1) << 11u)
#define VG_ACTION_STATS (UINT64_C(1) << 12u)
#define VG_ACTION_QUICK_SAVE (UINT64_C(1) << 13u)
#define VG_ACTION_QUICK_LOAD (UINT64_C(1) << 14u)
#define VG_ACTION_MOVE_FORWARD (UINT64_C(1) << 15u)
#define VG_ACTION_MOVE_BACKWARD (UINT64_C(1) << 16u)
#define VG_ACTION_MOVE_LEFT (UINT64_C(1) << 17u)
#define VG_ACTION_MOVE_RIGHT (UINT64_C(1) << 18u)

/* Stable physical codes use USB HID keyboard usages. Pointer buttons occupy
 * the private 0x0001xxxx range so persisted bindings are host independent. */
typedef uint32_t VgInputCode;
#define VG_INPUT_KEY_A UINT32_C(0x04)
#define VG_INPUT_KEY_D UINT32_C(0x07)
#define VG_INPUT_KEY_E UINT32_C(0x08)
#define VG_INPUT_KEY_S UINT32_C(0x16)
#define VG_INPUT_KEY_W UINT32_C(0x1A)
#define VG_INPUT_KEY_ENTER UINT32_C(0x28)
#define VG_INPUT_KEY_ESCAPE UINT32_C(0x29)
#define VG_INPUT_KEY_SPACE UINT32_C(0x2C)
#define VG_INPUT_KEY_F1 UINT32_C(0x3A)
#define VG_INPUT_KEY_F2 UINT32_C(0x3B)
#define VG_INPUT_KEY_F3 UINT32_C(0x3C)
#define VG_INPUT_KEY_F4 UINT32_C(0x3D)
#define VG_INPUT_KEY_F5 UINT32_C(0x3E)
#define VG_INPUT_KEY_F9 UINT32_C(0x42)
#define VG_INPUT_KEY_RIGHT UINT32_C(0x4F)
#define VG_INPUT_KEY_LEFT UINT32_C(0x50)
#define VG_INPUT_KEY_DOWN UINT32_C(0x51)
#define VG_INPUT_KEY_UP UINT32_C(0x52)
#define VG_INPUT_MOUSE_PRIMARY UINT32_C(0x00010001)

typedef struct VgInputSample {
    uint32_t struct_size;
    uint32_t api_version;
    VgActionSet pressed;
    VgActionSet held;
    VgActionSet released;
    float look_delta_x;
    float look_delta_y;
    uint32_t focused;
    uint32_t reserved;
} VgInputSample;

typedef struct VgInputState {
    uint32_t struct_size;
    uint32_t api_version;
    VgActionSet pressed;
    VgActionSet held;
    VgActionSet released;
    float look_delta_x;
    float look_delta_y;
    uint32_t focused;
    uint32_t reserved;
    uint64_t tick_index;
} VgInputState;

#define VG_SETTINGS_MAX_BINDINGS 32u

typedef uint32_t VgSettingMask;
enum {
    VG_SETTING_INTERNAL_RESOLUTION = 1u << 0u,
    VG_SETTING_FULLSCREEN = 1u << 1u,
    VG_SETTING_VSYNC = 1u << 2u,
    VG_SETTING_FRAME_CAP = 1u << 3u,
    VG_SETTING_LOOK_SENSITIVITY = 1u << 4u,
    VG_SETTING_BINDINGS = 1u << 5u,
    VG_SETTING_VISUAL_PROFILE = 1u << 6u,
    VG_SETTING_AUDIO_MASTER_GAIN = 1u << 7u,
    VG_SETTING_AUDIO_MUSIC_GAIN = 1u << 8u,
    VG_SETTING_AUDIO_SFX_GAIN = 1u << 9u,
    VG_SETTING_AUDIO_AMBIENCE_GAIN = 1u << 10u,
    VG_SETTINGS_ALL = (1u << 11u) - 1u,
    VG_SETTINGS_APPLY_IMMEDIATE =
        VG_SETTING_FRAME_CAP | VG_SETTING_LOOK_SENSITIVITY | VG_SETTING_BINDINGS |
        VG_SETTING_VISUAL_PROFILE | VG_SETTING_AUDIO_MASTER_GAIN |
        VG_SETTING_AUDIO_MUSIC_GAIN | VG_SETTING_AUDIO_SFX_GAIN |
        VG_SETTING_AUDIO_AMBIENCE_GAIN,
    VG_SETTINGS_RECREATE_TARGETS = VG_SETTING_INTERNAL_RESOLUTION,
    VG_SETTINGS_RECREATE_SURFACE = VG_SETTING_FULLSCREEN | VG_SETTING_VSYNC
};

typedef enum VgVisualProfile {
    VG_VISUAL_PROFILE_CLEAN = 0u,
    VG_VISUAL_PROFILE_RETRO = 1u
} VgVisualProfile;

typedef struct VgInputBinding {
    VgActionSet action;
    VgInputCode code;
    uint32_t reserved;
} VgInputBinding;

typedef struct VgSettingsLayer {
    uint32_t struct_size;
    uint32_t api_version;
    VgSettingMask present;
    uint32_t internal_width;
    uint32_t internal_height;
    uint32_t fullscreen;
    uint32_t vsync;
    uint32_t frame_cap;
    float look_sensitivity;
    uint32_t binding_count;
    uint32_t reserved;
    VgInputBinding bindings[VG_SETTINGS_MAX_BINDINGS];
    uint32_t visual_profile;
    float audio_master_gain;
    float audio_music_gain;
    float audio_sfx_gain;
    float audio_ambience_gain;
} VgSettingsLayer;

typedef struct VgSettingsChanges {
    uint32_t struct_size;
    uint32_t api_version;
    VgSettingMask immediate;
    VgSettingMask recreate_targets;
    VgSettingMask recreate_surface;
} VgSettingsChanges;

typedef struct VgSettingsDiagnostic {
    uint32_t struct_size;
    uint32_t api_version;
    VgResult result;
    uint32_t line;
    char message[160];
} VgSettingsDiagnostic;

/* Settings layers resolve in project -> user -> session order without mutating
 * any layer. VSync and frame cap affect presentation pacing only; fixed_delta
 * remains owned by VgGameDesc. Save publishes by durable temporary file plus
 * atomic replacement, so a failed write preserves the prior file. */
VG_API VgResult vg_settings_defaults(VgSettingsLayer *out_settings);
VG_API VgResult vg_settings_validate(const VgSettingsLayer *settings,
                                     VgSettingsDiagnostic *out_diagnostic);
VG_API VgResult vg_settings_resolve(const VgSettingsLayer *project, const VgSettingsLayer *user,
                                    const VgSettingsLayer *session, VgSettingsLayer *out_resolved,
                                    VgSettingsDiagnostic *out_diagnostic);
VG_API VgResult vg_settings_diff(const VgSettingsLayer *before, const VgSettingsLayer *after,
                                 VgSettingsChanges *out_changes);
VG_API VgResult vg_settings_load_file(const char *utf8_path, VgSettingsLayer *out_settings,
                                      VgSettingsDiagnostic *out_diagnostic);
VG_API VgResult vg_settings_save_file(const char *utf8_path, const VgSettingsLayer *settings,
                                      VgSettingsDiagnostic *out_diagnostic);

#ifdef __cplusplus
}
#endif
#endif
