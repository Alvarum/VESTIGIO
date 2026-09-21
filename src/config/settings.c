#include "vestigio/vestigio.h"

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

enum { VG_SETTINGS_FILE_VERSION = 1u, VG_SETTINGS_PATH_CAPACITY = 4096u };

static const VgInputBinding vg_default_bindings[] = {
    {VG_ACTION_MOVE_FORWARD, VG_INPUT_KEY_W, 0u}, {VG_ACTION_MOVE_BACKWARD, VG_INPUT_KEY_S, 0u},
    {VG_ACTION_MOVE_LEFT, VG_INPUT_KEY_A, 0u},    {VG_ACTION_MOVE_RIGHT, VG_INPUT_KEY_D, 0u},
    {VG_ACTION_JUMP, VG_INPUT_KEY_SPACE, 0u},     {VG_ACTION_PRIMARY, VG_INPUT_MOUSE_PRIMARY, 0u},
    {VG_ACTION_INTERACT, VG_INPUT_KEY_E, 0u},     {VG_ACTION_PAUSE, VG_INPUT_KEY_ESCAPE, 0u},
    {VG_ACTION_ACCEPT, VG_INPUT_KEY_ENTER, 0u},   {VG_ACTION_UI_UP, VG_INPUT_KEY_UP, 0u},
    {VG_ACTION_UI_DOWN, VG_INPUT_KEY_DOWN, 0u},   {VG_ACTION_UI_LEFT, VG_INPUT_KEY_LEFT, 0u},
    {VG_ACTION_UI_RIGHT, VG_INPUT_KEY_RIGHT, 0u}, {VG_ACTION_MAP, VG_INPUT_KEY_F1, 0u},
    {VG_ACTION_WIREFRAME, VG_INPUT_KEY_F2, 0u},   {VG_ACTION_DEPTH, VG_INPUT_KEY_F3, 0u},
    {VG_ACTION_STATS, VG_INPUT_KEY_F4, 0u},       {VG_ACTION_QUICK_SAVE, VG_INPUT_KEY_F5, 0u},
    {VG_ACTION_QUICK_LOAD, VG_INPUT_KEY_F9, 0u},
};

static bool vg_settings_field_present(uint32_t capacity, size_t offset, size_t size) {
    return (uint64_t)capacity >= (uint64_t)offset + (uint64_t)size;
}

static VgSettingMask vg_settings_mask_for_capacity(uint32_t capacity, uint32_t binding_count) {
    VgSettingMask mask = 0u;
    if (vg_settings_field_present(capacity, offsetof(VgSettingsLayer, internal_height),
                                  sizeof(((VgSettingsLayer *)0)->internal_height)))
        mask |= VG_SETTING_INTERNAL_RESOLUTION;
    if (vg_settings_field_present(capacity, offsetof(VgSettingsLayer, fullscreen),
                                  sizeof(((VgSettingsLayer *)0)->fullscreen)))
        mask |= VG_SETTING_FULLSCREEN;
    if (vg_settings_field_present(capacity, offsetof(VgSettingsLayer, vsync),
                                  sizeof(((VgSettingsLayer *)0)->vsync)))
        mask |= VG_SETTING_VSYNC;
    if (vg_settings_field_present(capacity, offsetof(VgSettingsLayer, frame_cap),
                                  sizeof(((VgSettingsLayer *)0)->frame_cap)))
        mask |= VG_SETTING_FRAME_CAP;
    if (vg_settings_field_present(capacity, offsetof(VgSettingsLayer, look_sensitivity),
                                  sizeof(((VgSettingsLayer *)0)->look_sensitivity)))
        mask |= VG_SETTING_LOOK_SENSITIVITY;
    if (vg_settings_field_present(capacity, offsetof(VgSettingsLayer, bindings),
                                  sizeof(((VgSettingsLayer *)0)->bindings[0]) * binding_count))
        mask |= VG_SETTING_BINDINGS;
    return mask;
}

static bool vg_settings_header_valid(const VgSettingsLayer *settings) {
    return settings != NULL &&
           vg_settings_field_present(settings->struct_size, offsetof(VgSettingsLayer, api_version),
                                     sizeof(settings->api_version)) &&
           settings->api_version == VG_API_VERSION;
}

static bool vg_diagnostic_valid(const VgSettingsDiagnostic *diagnostic) {
    return diagnostic == NULL ||
           (vg_settings_field_present(diagnostic->struct_size,
                                      offsetof(VgSettingsDiagnostic, api_version),
                                      sizeof(diagnostic->api_version)) &&
            diagnostic->api_version == VG_API_VERSION);
}

static void vg_diagnostic_set(VgSettingsDiagnostic *diagnostic, VgResult result, uint32_t line,
                              const char *message) {
    if (diagnostic == NULL)
        return;
    uint32_t capacity = diagnostic->struct_size;
    VgSettingsDiagnostic value;
    memset(&value, 0, sizeof(value));
    value.struct_size = capacity;
    value.api_version = VG_API_VERSION;
    value.result = result;
    value.line = line;
    (void)snprintf(value.message, sizeof(value.message), "%s", message == NULL ? "" : message);
    size_t bytes = capacity;
    if (bytes > sizeof(value))
        bytes = sizeof(value);
    memcpy(diagnostic, &value, bytes);
}

static void vg_settings_copy_out(VgSettingsLayer *output, VgSettingsLayer value) {
    uint32_t capacity = output->struct_size;
    value.present &= vg_settings_mask_for_capacity(capacity, value.binding_count);
    if ((value.present & VG_SETTING_BINDINGS) == 0u)
        value.binding_count = 0u;
    value.struct_size = capacity;
    size_t bytes = capacity;
    if (bytes > sizeof(value))
        bytes = sizeof(value);
    memcpy(output, &value, bytes);
}

static FILE *vg_settings_open(const char *utf8_path, const char *mode) {
#ifdef _WIN32
    wchar_t path_wide[VG_SETTINGS_PATH_CAPACITY];
    wchar_t mode_wide[16];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path, -1, path_wide,
                            VG_SETTINGS_PATH_CAPACITY) == 0 ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, mode_wide,
                            (int)(sizeof(mode_wide) / sizeof(mode_wide[0]))) == 0)
        return NULL;
    return _wfopen(path_wide, mode_wide);
#else
    return fopen(utf8_path, mode);
#endif
}

static bool vg_settings_path_exists(const char *utf8_path) {
#ifdef _WIN32
    wchar_t path_wide[VG_SETTINGS_PATH_CAPACITY];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path, -1, path_wide,
                            VG_SETTINGS_PATH_CAPACITY) == 0)
        return false;
    return GetFileAttributesW(path_wide) != INVALID_FILE_ATTRIBUTES;
#else
    return access(utf8_path, F_OK) == 0;
#endif
}

static void vg_settings_remove(const char *utf8_path) {
#ifdef _WIN32
    wchar_t path_wide[VG_SETTINGS_PATH_CAPACITY];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_path, -1, path_wide,
                            VG_SETTINGS_PATH_CAPACITY) != 0)
        (void)_wremove(path_wide);
#else
    (void)remove(utf8_path);
#endif
}

static VgSettingsLayer vg_settings_default_value(void) {
    VgSettingsLayer value;
    memset(&value, 0, sizeof(value));
    value.struct_size = sizeof(value);
    value.api_version = VG_API_VERSION;
    value.present = VG_SETTINGS_ALL;
    value.internal_width = 480u;
    value.internal_height = 270u;
    value.fullscreen = 0u;
    value.vsync = 1u;
    value.frame_cap = 120u;
    value.look_sensitivity = 0.0025f;
    value.binding_count = (uint32_t)(sizeof(vg_default_bindings) / sizeof(vg_default_bindings[0]));
    memcpy(value.bindings, vg_default_bindings, sizeof(vg_default_bindings));
    return value;
}

VgResult vg_settings_defaults(VgSettingsLayer *out_settings) {
    if (!vg_settings_header_valid(out_settings))
        return VG_ERROR_INVALID_ARGUMENT;
    VgSettingsLayer value = vg_settings_default_value();
    vg_settings_copy_out(out_settings, value);
    return VG_OK;
}

VgResult vg_settings_validate(const VgSettingsLayer *settings,
                              VgSettingsDiagnostic *out_diagnostic) {
    if (!vg_settings_header_valid(settings) || !vg_diagnostic_valid(out_diagnostic))
        return VG_ERROR_INVALID_ARGUMENT;
    if (!vg_settings_field_present(settings->struct_size, offsetof(VgSettingsLayer, present),
                                   sizeof(settings->present))) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "settings present mask is unavailable");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    if (vg_settings_field_present(settings->struct_size, offsetof(VgSettingsLayer, reserved),
                                  sizeof(settings->reserved)) &&
        settings->reserved != 0u) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "settings contain nonzero reserved data");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    if ((settings->present & ~(VgSettingMask)VG_SETTINGS_ALL) != 0u) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "settings contain unknown flags");
        return VG_ERROR_INVALID_ARGUMENT;
    }
#define VG_REQUIRE_SETTING(mask, field)                                                            \
    do {                                                                                           \
        if ((settings->present & (mask)) != 0u &&                                                  \
            !vg_settings_field_present(settings->struct_size, offsetof(VgSettingsLayer, field),    \
                                       sizeof(settings->field))) {                                 \
            vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,                       \
                              "settings field is outside struct_size");                            \
            return VG_ERROR_INVALID_ARGUMENT;                                                      \
        }                                                                                          \
    } while (0)
    VG_REQUIRE_SETTING(VG_SETTING_INTERNAL_RESOLUTION, internal_height);
    VG_REQUIRE_SETTING(VG_SETTING_FULLSCREEN, fullscreen);
    VG_REQUIRE_SETTING(VG_SETTING_VSYNC, vsync);
    VG_REQUIRE_SETTING(VG_SETTING_FRAME_CAP, frame_cap);
    VG_REQUIRE_SETTING(VG_SETTING_LOOK_SENSITIVITY, look_sensitivity);
    VG_REQUIRE_SETTING(VG_SETTING_BINDINGS, binding_count);
#undef VG_REQUIRE_SETTING
    if ((settings->present & VG_SETTING_INTERNAL_RESOLUTION) != 0u &&
        (settings->internal_width < 160u || settings->internal_width > 8192u ||
         settings->internal_height < 90u || settings->internal_height > 8192u)) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "internal resolution is outside 160x90..8192x8192");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    if (((settings->present & VG_SETTING_FULLSCREEN) != 0u && settings->fullscreen > 1u) ||
        ((settings->present & VG_SETTING_VSYNC) != 0u && settings->vsync > 1u)) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "fullscreen and vsync must be zero or one");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    if ((settings->present & VG_SETTING_FRAME_CAP) != 0u && settings->frame_cap != 0u &&
        (settings->frame_cap < 30u || settings->frame_cap > 1000u)) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "frame cap must be zero or between 30 and 1000");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    if ((settings->present & VG_SETTING_LOOK_SENSITIVITY) != 0u &&
        (!isfinite(settings->look_sensitivity) || settings->look_sensitivity < 0.0001f ||
         settings->look_sensitivity > 0.1f)) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "look sensitivity must be finite and between 0.0001 and 0.1");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    bool binding_count_present =
        vg_settings_field_present(settings->struct_size, offsetof(VgSettingsLayer, binding_count),
                                  sizeof(settings->binding_count));
    if ((settings->present & VG_SETTING_BINDINGS) == 0u) {
        if (binding_count_present && settings->binding_count != 0u) {
            vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                              "binding_count requires the bindings override flag");
            return VG_ERROR_INVALID_ARGUMENT;
        }
    } else if (settings->binding_count == 0u ||
               settings->binding_count > VG_SETTINGS_MAX_BINDINGS) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_CAPACITY, 0u,
                          "bindings must contain between 1 and 32 entries");
        return VG_ERROR_CAPACITY;
    }
    if ((settings->present & VG_SETTING_BINDINGS) != 0u &&
        !vg_settings_field_present(settings->struct_size, offsetof(VgSettingsLayer, bindings),
                                   sizeof(settings->bindings[0]) * settings->binding_count)) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                          "bindings are outside struct_size");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0u;
         (settings->present & VG_SETTING_BINDINGS) != 0u && index < settings->binding_count;
         ++index) {
        const VgInputBinding *binding = &settings->bindings[index];
        if (binding->action == 0u || (binding->action & (binding->action - 1u)) != 0u ||
            binding->code == 0u || binding->reserved != 0u) {
            vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, 0u,
                              "each binding needs one action bit, one code and zero reserved data");
            return VG_ERROR_INVALID_ARGUMENT;
        }
        for (uint32_t prior = 0u; prior < index; ++prior) {
            if (settings->bindings[prior].code == binding->code) {
                vg_diagnostic_set(out_diagnostic, VG_ERROR_CONFLICT, 0u,
                                  "one physical input code is bound more than once");
                return VG_ERROR_CONFLICT;
            }
        }
    }
    vg_diagnostic_set(out_diagnostic, VG_OK, 0u, "");
    return VG_OK;
}

static void vg_settings_overlay(VgSettingsLayer *resolved, const VgSettingsLayer *layer) {
    if ((layer->present & VG_SETTING_INTERNAL_RESOLUTION) != 0u) {
        resolved->internal_width = layer->internal_width;
        resolved->internal_height = layer->internal_height;
    }
    if ((layer->present & VG_SETTING_FULLSCREEN) != 0u)
        resolved->fullscreen = layer->fullscreen;
    if ((layer->present & VG_SETTING_VSYNC) != 0u)
        resolved->vsync = layer->vsync;
    if ((layer->present & VG_SETTING_FRAME_CAP) != 0u)
        resolved->frame_cap = layer->frame_cap;
    if ((layer->present & VG_SETTING_LOOK_SENSITIVITY) != 0u)
        resolved->look_sensitivity = layer->look_sensitivity;
    if ((layer->present & VG_SETTING_BINDINGS) != 0u) {
        resolved->binding_count = layer->binding_count;
        memset(resolved->bindings, 0, sizeof(resolved->bindings));
        memcpy(resolved->bindings, layer->bindings,
               sizeof(resolved->bindings[0]) * layer->binding_count);
    }
}

VgResult vg_settings_resolve(const VgSettingsLayer *project, const VgSettingsLayer *user,
                             const VgSettingsLayer *session, VgSettingsLayer *out_resolved,
                             VgSettingsDiagnostic *out_diagnostic) {
    if (!vg_settings_header_valid(out_resolved) || !vg_diagnostic_valid(out_diagnostic))
        return VG_ERROR_INVALID_ARGUMENT;
    const VgSettingsLayer *layers[] = {project, user, session};
    for (size_t index = 0u; index < sizeof(layers) / sizeof(layers[0]); ++index) {
        if (layers[index] != NULL) {
            VgResult result = vg_settings_validate(layers[index], out_diagnostic);
            if (result != VG_OK)
                return result;
        }
    }
    VgSettingsLayer resolved = vg_settings_default_value();
    for (size_t index = 0u; index < sizeof(layers) / sizeof(layers[0]); ++index) {
        if (layers[index] != NULL)
            vg_settings_overlay(&resolved, layers[index]);
    }
    vg_settings_copy_out(out_resolved, resolved);
    vg_diagnostic_set(out_diagnostic, VG_OK, 0u, "");
    return VG_OK;
}

VgResult vg_settings_diff(const VgSettingsLayer *before, const VgSettingsLayer *after,
                          VgSettingsChanges *out_changes) {
    if (!vg_settings_header_valid(before) || !vg_settings_header_valid(after) ||
        out_changes == NULL ||
        !vg_settings_field_present(out_changes->struct_size,
                                   offsetof(VgSettingsChanges, api_version),
                                   sizeof(out_changes->api_version)) ||
        out_changes->api_version != VG_API_VERSION)
        return VG_ERROR_INVALID_ARGUMENT;
    VgSettingsLayer first = vg_settings_default_value();
    VgSettingsLayer second = vg_settings_default_value();
    VgResult result = vg_settings_resolve(before, NULL, NULL, &first, NULL);
    if (result != VG_OK)
        return result;
    result = vg_settings_resolve(after, NULL, NULL, &second, NULL);
    if (result != VG_OK)
        return result;
    VgSettingMask changed = 0u;
    if (first.internal_width != second.internal_width ||
        first.internal_height != second.internal_height)
        changed |= VG_SETTING_INTERNAL_RESOLUTION;
    if (first.fullscreen != second.fullscreen)
        changed |= VG_SETTING_FULLSCREEN;
    if (first.vsync != second.vsync)
        changed |= VG_SETTING_VSYNC;
    if (first.frame_cap != second.frame_cap)
        changed |= VG_SETTING_FRAME_CAP;
    if (first.look_sensitivity != second.look_sensitivity)
        changed |= VG_SETTING_LOOK_SENSITIVITY;
    if (first.binding_count != second.binding_count ||
        memcmp(first.bindings, second.bindings, sizeof(first.bindings[0]) * first.binding_count) !=
            0)
        changed |= VG_SETTING_BINDINGS;
    uint32_t capacity = out_changes->struct_size;
    VgSettingsChanges changes = {capacity, VG_API_VERSION, changed & VG_SETTINGS_APPLY_IMMEDIATE,
                                 changed & VG_SETTINGS_RECREATE_TARGETS,
                                 changed & VG_SETTINGS_RECREATE_SURFACE};
    size_t output_bytes = capacity;
    if (output_bytes > sizeof(changes))
        output_bytes = sizeof(changes);
    memcpy(out_changes, &changes, output_bytes);
    return VG_OK;
}

static bool vg_settings_flush(FILE *file) {
    if (fflush(file) != 0)
        return false;
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}

static bool vg_settings_atomic_replace(const char *temporary, const char *destination) {
#ifdef _WIN32
    wchar_t temporary_wide[VG_SETTINGS_PATH_CAPACITY];
    wchar_t destination_wide[VG_SETTINGS_PATH_CAPACITY];
    int temporary_count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, temporary, -1,
                                              temporary_wide, VG_SETTINGS_PATH_CAPACITY);
    int destination_count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, destination, -1,
                                                destination_wide, VG_SETTINGS_PATH_CAPACITY);
    if (temporary_count == 0 || destination_count == 0)
        return false;
    DWORD attributes = GetFileAttributesW(destination_wide);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return ReplaceFileW(destination_wide, temporary_wide, NULL, REPLACEFILE_WRITE_THROUGH, NULL,
                            NULL) != 0;
    return MoveFileExW(temporary_wide, destination_wide, MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, destination) == 0;
#endif
}

VgResult vg_settings_save_file(const char *utf8_path, const VgSettingsLayer *settings,
                               VgSettingsDiagnostic *out_diagnostic) {
    if (utf8_path == NULL || utf8_path[0] == '\0' || !vg_diagnostic_valid(out_diagnostic))
        return VG_ERROR_INVALID_ARGUMENT;
    VgResult result = vg_settings_validate(settings, out_diagnostic);
    if (result != VG_OK)
        return result;
    if (vg_settings_path_exists(utf8_path)) {
        VgSettingsLayer existing;
        memset(&existing, 0, sizeof(existing));
        existing.struct_size = sizeof(existing);
        existing.api_version = VG_API_VERSION;
        result = vg_settings_load_file(utf8_path, &existing, out_diagnostic);
        if (result != VG_OK)
            return result;
    }
    char temporary[VG_SETTINGS_PATH_CAPACITY];
#ifdef _WIN32
    unsigned long process_id = GetCurrentProcessId();
#else
    unsigned long process_id = (unsigned long)getpid();
#endif
    int written = snprintf(temporary, sizeof(temporary), "%s.tmp.%lu", utf8_path, process_id);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_CAPACITY, 0u, "settings path is too long");
        return VG_ERROR_CAPACITY;
    }
    FILE *file = vg_settings_open(temporary, "wb");
    if (file == NULL) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_IO, 0u, "cannot create settings temp file");
        return VG_ERROR_IO;
    }
    bool ok = fprintf(file, "VESTIGIO_SETTINGS %u\n", VG_SETTINGS_FILE_VERSION) > 0 &&
              fprintf(file, "present %08X\n", settings->present) > 0;
    if (ok && (settings->present & VG_SETTING_INTERNAL_RESOLUTION) != 0u)
        ok = fprintf(file, "internal_resolution %u %u\n", settings->internal_width,
                     settings->internal_height) > 0;
    if (ok && (settings->present & VG_SETTING_FULLSCREEN) != 0u)
        ok = fprintf(file, "fullscreen %u\n", settings->fullscreen) > 0;
    if (ok && (settings->present & VG_SETTING_VSYNC) != 0u)
        ok = fprintf(file, "vsync %u\n", settings->vsync) > 0;
    if (ok && (settings->present & VG_SETTING_FRAME_CAP) != 0u)
        ok = fprintf(file, "frame_cap %u\n", settings->frame_cap) > 0;
    if (ok && (settings->present & VG_SETTING_LOOK_SENSITIVITY) != 0u)
        ok = fprintf(file, "look_sensitivity %.9g\n", (double)settings->look_sensitivity) > 0;
    if (ok && (settings->present & VG_SETTING_BINDINGS) != 0u) {
        ok = fprintf(file, "binding_count %u\n", settings->binding_count) > 0;
        for (uint32_t index = 0u; ok && index < settings->binding_count; ++index)
            ok = fprintf(file, "binding %016llX %08X\n",
                         (unsigned long long)settings->bindings[index].action,
                         settings->bindings[index].code) > 0;
    }
    if (ok)
        ok = vg_settings_flush(file);
    if (fclose(file) != 0)
        ok = false;
    if (ok)
        ok = vg_settings_atomic_replace(temporary, utf8_path);
    if (!ok) {
        vg_settings_remove(temporary);
        vg_diagnostic_set(out_diagnostic, VG_ERROR_IO, 0u,
                          "settings write failed; previous file was preserved");
        return VG_ERROR_IO;
    }
    vg_diagnostic_set(out_diagnostic, VG_OK, 0u, "");
    return VG_OK;
}

static bool vg_no_extra_text(const char *text, int consumed) {
    if (consumed < 0)
        return false;
    for (const char *cursor = text + consumed; *cursor != '\0'; ++cursor) {
        if (*cursor != ' ' && *cursor != '\t')
            return false;
    }
    return true;
}

static VgResult vg_settings_parse_line(VgSettingsLayer *candidate, uint32_t *seen,
                                       uint32_t *parsed_bindings, uint32_t *declared_bindings,
                                       const char *line, uint32_t line_number,
                                       VgSettingsDiagnostic *diagnostic) {
    int consumed = -1;
    unsigned int first = 0u;
    unsigned int second = 0u;
    unsigned long long action = 0u;
    float decimal = 0.0f;
#define VG_PARSE_UNIQUE(mask, message)                                                             \
    do {                                                                                           \
        if ((*seen & (mask)) != 0u) {                                                              \
            vg_diagnostic_set(diagnostic, VG_ERROR_CONFLICT, line_number, message);                \
            return VG_ERROR_CONFLICT;                                                              \
        }                                                                                          \
        *seen |= (mask);                                                                           \
    } while (0)
    if (sscanf(line, "present %x %n", &first, &consumed) == 1 && vg_no_extra_text(line, consumed)) {
        VG_PARSE_UNIQUE(UINT32_C(0x80000000), "duplicate present line");
        candidate->present = first;
        return VG_OK;
    }
    if (sscanf(line, "internal_resolution %u %u %n", &first, &second, &consumed) == 2 &&
        vg_no_extra_text(line, consumed)) {
        VG_PARSE_UNIQUE(VG_SETTING_INTERNAL_RESOLUTION, "duplicate internal_resolution line");
        candidate->internal_width = first;
        candidate->internal_height = second;
        return VG_OK;
    }
    if (sscanf(line, "fullscreen %u %n", &first, &consumed) == 1 &&
        vg_no_extra_text(line, consumed)) {
        VG_PARSE_UNIQUE(VG_SETTING_FULLSCREEN, "duplicate fullscreen line");
        candidate->fullscreen = first;
        return VG_OK;
    }
    if (sscanf(line, "vsync %u %n", &first, &consumed) == 1 && vg_no_extra_text(line, consumed)) {
        VG_PARSE_UNIQUE(VG_SETTING_VSYNC, "duplicate vsync line");
        candidate->vsync = first;
        return VG_OK;
    }
    if (sscanf(line, "frame_cap %u %n", &first, &consumed) == 1 &&
        vg_no_extra_text(line, consumed)) {
        VG_PARSE_UNIQUE(VG_SETTING_FRAME_CAP, "duplicate frame_cap line");
        candidate->frame_cap = first;
        return VG_OK;
    }
    if (sscanf(line, "look_sensitivity %f %n", &decimal, &consumed) == 1 &&
        vg_no_extra_text(line, consumed)) {
        VG_PARSE_UNIQUE(VG_SETTING_LOOK_SENSITIVITY, "duplicate look_sensitivity line");
        candidate->look_sensitivity = decimal;
        return VG_OK;
    }
    if (sscanf(line, "binding_count %u %n", &first, &consumed) == 1 &&
        vg_no_extra_text(line, consumed)) {
        VG_PARSE_UNIQUE(VG_SETTING_BINDINGS, "duplicate binding_count line");
        *declared_bindings = first;
        return VG_OK;
    }
    if (sscanf(line, "binding %llx %x %n", &action, &first, &consumed) == 2 &&
        vg_no_extra_text(line, consumed)) {
        if (*parsed_bindings >= VG_SETTINGS_MAX_BINDINGS) {
            vg_diagnostic_set(diagnostic, VG_ERROR_CAPACITY, line_number, "too many binding lines");
            return VG_ERROR_CAPACITY;
        }
        candidate->bindings[*parsed_bindings] =
            (VgInputBinding){(VgActionSet)action, (VgInputCode)first, 0u};
        ++*parsed_bindings;
        return VG_OK;
    }
#undef VG_PARSE_UNIQUE
    vg_diagnostic_set(diagnostic, VG_ERROR_INVALID_ARGUMENT, line_number,
                      "unknown or malformed settings line");
    return VG_ERROR_INVALID_ARGUMENT;
}

VgResult vg_settings_load_file(const char *utf8_path, VgSettingsLayer *out_settings,
                               VgSettingsDiagnostic *out_diagnostic) {
    if (utf8_path == NULL || utf8_path[0] == '\0' || !vg_settings_header_valid(out_settings) ||
        !vg_diagnostic_valid(out_diagnostic))
        return VG_ERROR_INVALID_ARGUMENT;
    FILE *file = vg_settings_open(utf8_path, "rb");
    if (file == NULL) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_IO, 0u, "cannot open settings file");
        return VG_ERROR_IO;
    }
    VgSettingsLayer candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.struct_size = sizeof(candidate);
    candidate.api_version = VG_API_VERSION;
    char line[512];
    uint32_t line_number = 0u;
    uint32_t seen = 0u;
    uint32_t parsed_bindings = 0u;
    uint32_t declared_bindings = 0u;
    VgResult result = VG_OK;
    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        size_t length = strlen(line);
        if (length == sizeof(line) - 1u && line[length - 1u] != '\n') {
            result = VG_ERROR_CAPACITY;
            vg_diagnostic_set(out_diagnostic, result, line_number, "settings line is too long");
            break;
        }
        while (length != 0u && (line[length - 1u] == '\n' || line[length - 1u] == '\r'))
            line[--length] = '\0';
        if (line_number == 1u) {
            int consumed = -1;
            unsigned int version = 0u;
            if (sscanf(line, "VESTIGIO_SETTINGS %u %n", &version, &consumed) != 1 ||
                !vg_no_extra_text(line, consumed)) {
                result = VG_ERROR_INVALID_ARGUMENT;
                vg_diagnostic_set(out_diagnostic, result, line_number,
                                  "invalid settings file header");
                break;
            }
            if (version != VG_SETTINGS_FILE_VERSION) {
                result = VG_ERROR_FORMAT_VERSION;
                vg_diagnostic_set(out_diagnostic, result, line_number,
                                  "unsupported settings file version");
                break;
            }
            continue;
        }
        result = vg_settings_parse_line(&candidate, &seen, &parsed_bindings, &declared_bindings,
                                        line, line_number, out_diagnostic);
        if (result != VG_OK)
            break;
    }
    if (result == VG_OK && ferror(file) != 0) {
        result = VG_ERROR_IO;
        vg_diagnostic_set(out_diagnostic, result, line_number, "settings read failed");
    }
    if (fclose(file) != 0 && result == VG_OK) {
        result = VG_ERROR_IO;
        vg_diagnostic_set(out_diagnostic, result, line_number, "settings close failed");
    }
    if (result != VG_OK)
        return result;
    if (line_number == 0u || (seen & UINT32_C(0x80000000)) == 0u) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, line_number,
                          "settings file is missing its present mask");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    VgSettingMask described = seen & VG_SETTINGS_ALL;
    if (described != candidate.present || (((candidate.present & VG_SETTING_BINDINGS) != 0u) &&
                                           (declared_bindings != parsed_bindings))) {
        vg_diagnostic_set(out_diagnostic, VG_ERROR_INVALID_ARGUMENT, line_number,
                          "settings fields do not match the present mask or binding count");
        return VG_ERROR_INVALID_ARGUMENT;
    }
    candidate.binding_count = parsed_bindings;
    result = vg_settings_validate(&candidate, out_diagnostic);
    if (result != VG_OK)
        return result;
    vg_settings_copy_out(out_settings, candidate);
    vg_diagnostic_set(out_diagnostic, VG_OK, 0u, "");
    return VG_OK;
}
