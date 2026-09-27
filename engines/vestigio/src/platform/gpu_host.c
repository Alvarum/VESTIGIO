#include "platform/gpu_host.h"

#include "assets/import/gltf_import.h"
#include "assets/import/sha256.h"
#include "audio/atrium_audio.h"
#include "content/document_internal.h"
#include "content/document_runtime.h"
#include "content/room_recipe.h"
#include "gamekit/atrium_animation.h"
#include "platform/win32_embed.h"
#include "raylib.h"
#include "render/gpu_raylib/gpu_renderer.h"
#include "tooling/tool_api.h"
#include "vestigio/controller.h"
#include "vestigio/door.h"
#include "world/transform_internal.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdatomic.h>
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

struct VgGpuHost {
    VgGpuRenderer *renderer;
    void *window;
    unsigned long owner_thread;
    VgContext *context;
    VgDocument *document;
    const VgDocument *resolving_document;
    VgDocumentInstance *edit;
    VgDocumentInstance *play;
    VgAtriumAnimation *animation;
    VgDoor **doors;
    size_t door_count;
    VgEntity edit_camera;
    VgEntity play_camera;
    VgControllerConfig controller;
    VgControllerState player;
    void *model_data;
    uint64_t model_size;
    VgAssetId model_id;
    bool model_registered;
    char level_directory[2048];
    struct {
        char id[37];
        char fingerprint[65];
        uint64_t version;
        uint64_t bytes;
    } imported[256];
    size_t imported_count;
    VgEntity preview_entity;
    bool preview_active;
    VgDocumentInstance *room_preview_original_edit;
    VgEntity room_preview_original_camera;
    bool room_preview_active;
    bool room_view_grid, room_view_ghost;
    float room_view_floor_z;
    bool gpu_attached;
    float edit_yaw, edit_pitch;
    float play_yaw, play_pitch;
    int32_t edit_camera_mode;
    VgEntity selected_entity;
    bool has_selection;
    char selected_uuid[37];
    size_t selection_count;
    char selection_uuids[VG_TOOL_MAX_COMMANDS][37];
    struct {
        bool active;
        bool has_preview;
        int32_t op, space, pivot, axis;
        float snap, amount;
        uint64_t revision;
        size_t count;
        size_t active_index;
        char ids[VG_TOOL_MAX_COMMANDS][37];
        char parent_ids[VG_TOOL_MAX_COMMANDS][37];
        VgDocumentTransform local[VG_TOOL_MAX_COMMANDS];
        VgTransform world[VG_TOOL_MAX_COMMANDS];
        VgMatrix parent_matrix[VG_TOOL_MAX_COMMANDS];
        VgDocumentInstance *original_edit;
        VgEntity original_camera;
    } gesture;
    VgVec3 selected_center;
    VgVec3 selected_extent;
    uint32_t pick_mask;
    int32_t gizmo_operation;
    int32_t gizmo_space;
    int32_t gizmo_pivot;
    VgVec3 orbit_target;
    float orbit_distance;
    double accumulator;
    bool jump_pending;
    bool interact_pending;
    uint64_t saved_revision;
    char *saved_json;
    size_t saved_length;
    int32_t visual_mode;
    VgAtriumAudio *audio;
    bool audio_enabled;
    char audio_directory[2048];
    float audio_gain[VG_AUDIO_BUS_COUNT];
    bool audio_focus_paused;
};

static _Atomic(VgGpuHost *) active_host;

static bool host_is_current(const VgGpuHost *host) {
    return host && host == atomic_load_explicit(&active_host, memory_order_acquire) &&
           host->owner_thread == vg_win32_thread_id();
}

static void host_error(char *out, size_t capacity, const char *message) {
    if (out && capacity)
        (void)snprintf(out, capacity, "%s", message);
}

static size_t host_collect_gizmos(const VgGpuHost *host, VgGpuGizmo *out, size_t capacity);
static size_t host_collect_room_ghosts(const VgGpuHost *host, VgGpuRoomOutline *out,
                                       VgVec3 points[][VG_ROOM_MAX_VERTICES], size_t capacity);
static void host_rebind_selection(VgGpuHost *host) {
    size_t count = host->selection_count;
    char ids[VG_TOOL_MAX_COMMANDS][37];
    memcpy(ids, host->selection_uuids, count * sizeof(ids[0]));
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    host->selection_count = 0u;
    for (size_t i = 0u; i < count; ++i)
        (void)vg_gpu_host_select_add(host, ids[i], i != 0u, false);
}
static void host_cancel_room_preview_internal(VgGpuHost *host) {
    if (host == NULL || !host->room_preview_active)
        return;
    VgDocumentInstance *candidate = host->edit;
    host->edit = host->room_preview_original_edit;
    host->edit_camera = host->room_preview_original_camera;
    host->room_preview_original_edit = NULL;
    host->room_preview_active = false;
    vg_document_instance_destroy(candidate);
    host_rebind_selection(host);
}
static void host_clear_preview(VgGpuHost *host) {
    if (host->preview_active && host->context != NULL)
        (void)vg_entity_destroy(host->context, host->preview_entity);
    host->preview_active = false;
}

/* 1 exists, 0 missing, -1 invalid/inaccessible. Settings paths are UTF-8. */
static int host_settings_file_state(const char *path) {
#ifdef _WIN32
    wchar_t wide[4096];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide,
                            (int)(sizeof(wide) / sizeof(wide[0]))) == 0)
        return -1;
    DWORD attributes = GetFileAttributesW(wide);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ? 1 : -1;
    DWORD failure = GetLastError();
    return failure == ERROR_FILE_NOT_FOUND || failure == ERROR_PATH_NOT_FOUND ? 0 : -1;
#else
    FILE *file = fopen(path, "rb");
    if (file != NULL) {
        (void)fclose(file);
        return 1;
    }
    return errno == ENOENT ? 0 : -1;
#endif
}

static bool host_apply_visual(VgGpuHost *host, const VgDocumentInstance *instance, int32_t mode,
                              char *error, size_t error_capacity) {
    VgGpuVisualSettings visual = vg_gpu_renderer_default_visual_settings();
    visual.mode = (VgGpuVisualMode)mode;
    if (instance != NULL) {
        VgDocumentEnvironment environment;
        if (!vg_document_instance_environment(instance, &environment)) {
            host_error(error, error_capacity, "No se pudo leer ambiente del nivel");
            return false;
        }
        memcpy(visual.ambient, environment.ambient_linear, sizeof(visual.ambient));
        memcpy(visual.clear_color, environment.clear_linear, sizeof(visual.clear_color));
        memcpy(visual.fog_color, environment.fog_color_linear, sizeof(visual.fog_color));
        visual.fog_enabled = environment.fog_enabled ? 1u : 0u;
        visual.fog_start = environment.fog_start;
        visual.fog_end = environment.fog_end;
        size_t count = vg_document_instance_light_count(instance);
        if (count > VG_GPU_MAX_POINT_LIGHTS) {
            host_error(error, error_capacity, "Demasiadas luces en el nivel");
            return false;
        }
        visual.point_light_count = (uint32_t)count;
        for (size_t i = 0u; i < count; ++i) {
            VgDocumentLightBinding binding;
            VgTransform world;
            if (!vg_document_instance_light_at(instance, i, &binding) ||
                vg_entity_get_world_transform(host->context, binding.entity, &world) != VG_OK) {
                host_error(error, error_capacity, "No se pudo leer pose de la luz");
                return false;
            }
            visual.lights[i].position = world.position;
            visual.lights[i].radius = binding.range;
            visual.lights[i].intensity = binding.intensity;
            memcpy(visual.lights[i].color, binding.color_linear, sizeof(visual.lights[i].color));
        }
    }
    return vg_gpu_renderer_set_visual_settings(host->renderer, &visual, error, error_capacity);
}

static void host_release_play(VgGpuHost *host) {
    vg_atrium_audio_destroy(host->audio);
    host->audio = NULL;
    vg_atrium_animation_destroy(host->animation);
    host->animation = NULL;
    for (size_t i = 0u; i < host->door_count; ++i)
        vg_door_destroy(host->doors[i]);
    free(host->doors);
    host->doors = NULL;
    host->door_count = 0u;
    vg_document_instance_destroy(host->play);
    host->play = NULL;
    host->accumulator = 0.0;
    host->jump_pending = false;
    host->interact_pending = false;
}

static void host_release_level(VgGpuHost *host) {
    host_release_play(host);
    host_cancel_room_preview_internal(host);
    host_clear_preview(host);
    if (host->gesture.has_preview)
        vg_document_instance_destroy(host->gesture.original_edit);
    memset(&host->gesture, 0, sizeof(host->gesture));
    vg_document_instance_destroy(host->edit);
    host->edit = NULL;
    vg_document_destroy(host->document);
    host->document = NULL;
    if (host->context != NULL) {
        uint32_t purged = 0u;
        (void)vg_assets_purge_unused(host->context, &purged);
        (void)vg_asset_flush_gpu(host->context);
        if (host->gpu_attached)
            (void)vg_asset_detach_gpu(host->context);
        vg_context_destroy(host->context);
        host->context = NULL;
        host->gpu_attached = false;
    }
    free(host->model_data);
    host->model_data = NULL;
    host->model_size = 0u;
    host->model_registered = false;
    host->level_directory[0] = '\0';
    host->imported_count = 0u;
    host->audio_directory[0] = '\0';
    host->audio_focus_paused = false;
    host->accumulator = 0.0;
    host->jump_pending = false;
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    host->selection_count = 0u;
    host->edit_camera_mode = 0;
    host->saved_revision = 0u;
    free(host->saved_json);
    host->saved_json = NULL;
    host->saved_length = 0u;
}

static FILE *host_open_file(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wide_path[4096], wide_mode[16];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide_path,
                            (int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0 ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wide_mode,
                            (int)(sizeof(wide_mode) / sizeof(wide_mode[0]))) == 0)
        return NULL;
    return _wfopen(wide_path, wide_mode);
#else
    return fopen(path, mode);
#endif
}

static bool host_remove_file(const char *path) {
#ifdef _WIN32
    wchar_t wide[4096];
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide,
                               (int)(sizeof(wide) / sizeof(wide[0]))) != 0 &&
           _wremove(wide) == 0;
#else
    return remove(path) == 0;
#endif
}

static bool host_create_directory(const char *path) {
#ifdef _WIN32
    wchar_t wide[4096];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide,
                            (int)(sizeof(wide) / sizeof(wide[0]))) == 0)
        return false;
    if (CreateDirectoryW(wide, NULL))
        return true;
    if (GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    DWORD attributes = GetFileAttributesW(wide);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

static bool host_read_model(const char *path, void **out_data, uint64_t *out_size) {
    FILE *file = host_open_file(path, "rb");
    if (file == NULL)
        return false;
    long size = 0;
    bool ok = fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 &&
              size <= 16 * 1024 * 1024 && fseek(file, 0, SEEK_SET) == 0;
    void *data = ok ? malloc((size_t)size) : NULL;
    ok = data != NULL && fread(data, 1u, (size_t)size, file) == (size_t)size;
    (void)fclose(file);
    if (!ok) {
        free(data);
        return false;
    }
    *out_data = data;
    *out_size = (uint64_t)size;
    return true;
}

static bool host_level_directory(const char *path, char out[2048]) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash))
        slash = backslash;
    size_t length = slash == NULL ? 0u : (size_t)(slash - path);
    if (length == 0u || length >= 2048u)
        return false;
    memcpy(out, path, length);
    out[length] = '\0';
    return true;
}

static bool host_asset_path(const VgGpuHost *host, const char *source, char out[4096]) {
    if (host->level_directory[0] == '\0' || source == NULL ||
        snprintf(out, 4096u, "%s/%s", host->level_directory, source) >= 4096)
        return false;
#ifdef _WIN32
    size_t start = strlen(host->level_directory) + 1u;
    for (size_t index = start;; ++index) {
        if (out[index] != '/' && out[index] != '\0')
            continue;
        char saved = out[index];
        out[index] = '\0';
        wchar_t wide[4096];
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, out, -1, wide,
                                (int)(sizeof(wide) / sizeof(wide[0]))) == 0) {
            out[index] = saved;
            return false;
        }
        DWORD attributes = GetFileAttributesW(wide);
        out[index] = saved;
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            return false;
        if (saved == '\0')
            break;
    }
#endif
    return true;
}

static void host_fingerprint(const void *data, size_t size, char out[65]) {
    VgSha256 hash;
    uint8_t digest[32];
    vg_sha256_init(&hash);
    vg_sha256_update(&hash, data, size);
    vg_sha256_finish(&hash, digest);
    for (size_t index = 0u; index < 32u; ++index)
        (void)snprintf(out + index * 2u, 3u, "%02x", digest[index]);
}

static const VgJsonNode *host_asset_node(const VgDocument *document, const char *id) {
    const VgJsonNode *assets = vg_json_object_get(vg_document_root(document), "assets");
    if (assets == NULL || assets->type != VG_JSON_ARRAY)
        return NULL;
    for (size_t index = 0u; index < assets->as.array.count; ++index) {
        const VgJsonNode *asset = assets->as.array.items[index];
        const VgJsonNode *asset_id = vg_json_object_get(asset, "id");
        if (asset_id != NULL && asset_id->type == VG_JSON_STRING &&
            strcmp(asset_id->as.string.data, id) == 0)
            return asset;
    }
    return NULL;
}

static void host_asset_id_text(VgAssetId id, char text[37]) {
    size_t at = 0u;
    for (size_t index = 0u; index < 16u; ++index) {
        if (index == 4u || index == 6u || index == 8u || index == 10u)
            text[at++] = '-';
        (void)snprintf(text + at, 3u, "%02x", id.bytes[index]);
        at += 2u;
    }
    text[36] = '\0';
}

static bool host_asset_id_parse(const char *text, VgAssetId *out) {
    if (text == NULL || strlen(text) != 36u || out == NULL)
        return false;
    size_t byte = 0u;
    for (size_t index = 0u; index < 36u;) {
        if (index == 8u || index == 13u || index == 18u || index == 23u) {
            if (text[index++] != '-')
                return false;
            continue;
        }
        unsigned value = 0u;
        for (size_t digit = 0u; digit < 2u; ++digit) {
            char c = text[index++];
            unsigned nibble = c >= '0' && c <= '9'   ? (unsigned)(c - '0')
                              : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10)
                                                     : 256u;
            if (nibble > 15u)
                return false;
            value = value * 16u + nibble;
        }
        out->bytes[byte++] = (uint8_t)value;
    }
    return true;
}

static char *host_json_quote(const char *text) {
    size_t length = strlen(text);
    if (length > (SIZE_MAX - 3u) / 6u)
        return NULL;
    char *quoted = malloc(length * 6u + 3u);
    if (quoted == NULL)
        return NULL;
    size_t at = 0u;
    quoted[at++] = '"';
    for (size_t index = 0u; index < length; ++index) {
        unsigned char c = (unsigned char)text[index];
        if (c == '"' || c == '\\') {
            quoted[at++] = '\\';
            quoted[at++] = (char)c;
        } else if (c < 0x20u) {
            (void)snprintf(quoted + at, 7u, "\\u%04x", c);
            at += 6u;
        } else {
            quoted[at++] = (char)c;
        }
    }
    quoted[at++] = '"';
    quoted[at] = '\0';
    return quoted;
}

static bool host_json_append(char *out, size_t capacity, size_t *length, const char *text) {
    size_t additional = strlen(text);
    if (*length >= capacity || additional >= capacity - *length)
        return false;
    memcpy(out + *length, text, additional + 1u);
    *length += additional;
    return true;
}

static char *host_asset_entry_json(const char *id, const char *name, const char *source,
                                   const char *fingerprint) {
    char *quoted_name = host_json_quote(name);
    char *quoted_source = host_json_quote(source);
    if (quoted_name == NULL || quoted_source == NULL) {
        free(quoted_name);
        free(quoted_source);
        return NULL;
    }
    size_t capacity = strlen(quoted_name) + strlen(quoted_source) + 256u;
    char *json = malloc(capacity);
    if (json != NULL)
        (void)snprintf(json, capacity,
                       "{\"id\":\"%s\",\"name\":%s,\"source\":%s,\"fingerprint\":\"%s\"}", id,
                       quoted_name, quoted_source, fingerprint);
    free(quoted_name);
    free(quoted_source);
    return json;
}

static char *host_updated_assets_json(const VgDocument *document, const char *id, const char *name,
                                      const char *source, const char *fingerprint,
                                      bool *was_replaced) {
    const VgJsonNode *assets = vg_json_object_get(vg_document_root(document), "assets");
    char *out = malloc(1024u * 1024u);
    if (out == NULL)
        return NULL;
    out[0] = '\0';
    size_t length = 0u;
    bool replaced = false;
    bool okay = host_json_append(out, 1024u * 1024u, &length, "[");
    if (assets != NULL && assets->type == VG_JSON_ARRAY) {
        for (size_t index = 0u; okay && index < assets->as.array.count; ++index) {
            const VgJsonNode *node = assets->as.array.items[index];
            const VgJsonNode *asset_id = vg_json_object_get(node, "id");
            bool match = asset_id != NULL && asset_id->type == VG_JSON_STRING &&
                         strcmp(asset_id->as.string.data, id) == 0;
            char *entry = NULL;
            size_t entry_length = 0u;
            if (match) {
                entry = host_asset_entry_json(id, name, source, fingerprint);
                replaced = true;
            } else {
                (void)vg_json_write_canonical(node, &entry, &entry_length);
            }
            okay = entry != NULL &&
                   (index == 0u || host_json_append(out, 1024u * 1024u, &length, ",")) &&
                   host_json_append(out, 1024u * 1024u, &length, entry);
            free(entry);
        }
    }
    if (okay && !replaced) {
        char *entry = host_asset_entry_json(id, name, source, fingerprint);
        okay = entry != NULL &&
               ((assets == NULL || assets->as.array.count == 0u) ||
                host_json_append(out, 1024u * 1024u, &length, ",")) &&
               host_json_append(out, 1024u * 1024u, &length, entry);
        free(entry);
    }
    okay = okay && host_json_append(out, 1024u * 1024u, &length, "]");
    if (!okay) {
        free(out);
        return NULL;
    }
    if (was_replaced != NULL)
        *was_replaced = replaced;
    return out;
}

static bool host_copy_model_file(const char *path, const void *data, size_t size) {
    char temporary[4096];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary))
        return false;
    FILE *file = host_open_file(temporary, "wb");
    if (file == NULL)
        return false;
    bool okay = fwrite(data, 1u, size, file) == size && fflush(file) == 0;
    okay = fclose(file) == 0 && okay;
    if (!okay) {
        (void)host_remove_file(temporary);
        return false;
    }
#ifdef _WIN32
    wchar_t wide_temporary[4096], wide_path[4096];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, temporary, -1, wide_temporary,
                            (int)(sizeof(wide_temporary) / sizeof(wide_temporary[0]))) == 0 ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide_path,
                            (int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0 ||
        !MoveFileExW(wide_temporary, wide_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        (void)host_remove_file(temporary);
        return false;
    }
#else
    if (rename(temporary, path) != 0) {
        (void)host_remove_file(temporary);
        return false;
    }
#endif
    return true;
}

static bool host_ensure_asset_directory(const char *level_directory) {
    char path[4096];
    if (snprintf(path, sizeof(path), "%s/assets", level_directory) >= (int)sizeof(path))
        return false;
    return host_create_directory(path);
}

static bool host_ensure_source_parents(const char *level_directory, const char *source) {
    char path[4096];
    if (snprintf(path, sizeof(path), "%s/%s", level_directory, source) >= (int)sizeof(path))
        return false;
    size_t start = strlen(level_directory) + 1u;
    for (size_t index = start; path[index] != '\0'; ++index) {
        if (path[index] != '/')
            continue;
        path[index] = '\0';
        bool okay = host_create_directory(path);
        path[index] = '/';
        if (!okay)
            return false;
    }
    return true;
}

static void *host_import_allocate(void *user, uint64_t size) {
    (void)user;
    return size <= SIZE_MAX ? malloc((size_t)size) : NULL;
}

static void host_import_deallocate(void *user, void *data) {
    (void)user;
    free(data);
}

static bool host_validate_model(const char *source, const void *data, uint64_t size, char *error,
                                size_t error_capacity) {
    VgAssetMemory memory = {NULL, host_import_allocate, host_import_deallocate};
    VgGltfImportOptions options;
    vg_gltf_default_options(&options);
    VgStaticModelIr *model = NULL;
    uint64_t bytes = 0u;
    VgGltfDiagnostic diagnostic = {0};
    VgResult result =
        vg_gltf_import(NULL, &memory, source, data, size, &options, &model, &bytes, &diagnostic);
    if (result == VG_OK)
        vg_gltf_model_destroy(&memory, model);
    else
        host_error(error, error_capacity, diagnostic.message);
    return result == VG_OK;
}

static VgResult host_register_imported(VgGpuHost *host, VgContext *context, const VgJsonNode *asset,
                                       VgAssetId id) {
    const VgJsonNode *path_node = vg_json_object_get(asset, "source");
    const VgJsonNode *fingerprint_node = vg_json_object_get(asset, "fingerprint");
    if (path_node == NULL || fingerprint_node == NULL || path_node->type != VG_JSON_STRING ||
        fingerprint_node->type != VG_JSON_STRING)
        return VG_ERROR_INVALID_ARGUMENT;
    char id_text[37];
    host_asset_id_text(id, id_text);
    char path[4096];
    if (!host_asset_path(host, path_node->as.string.data, path))
        return VG_ERROR_INVALID_ARGUMENT;
    void *data = NULL;
    uint64_t size = 0u;
    if (!host_read_model(path, &data, &size))
        return VG_ERROR_NOT_FOUND;
    char actual[65];
    host_fingerprint(data, (size_t)size, actual);
    if (strcmp(actual, fingerprint_node->as.string.data) != 0) {
        free(data);
        return VG_ERROR_CONFLICT;
    }
    size_t slot = host->imported_count;
    for (size_t index = 0u; index < host->imported_count; ++index)
        if (strcmp(host->imported[index].id, id_text) == 0) {
            slot = index;
            break;
        }
    if (slot < host->imported_count && strcmp(host->imported[slot].fingerprint, actual) == 0) {
        free(data);
        return VG_OK;
    }
    if (slot == 256u) {
        free(data);
        return VG_ERROR_CAPACITY;
    }
    uint64_t total = size;
    for (size_t index = 0u; index < host->imported_count; ++index)
        if (index != slot)
            total += host->imported[index].bytes;
    if (slot == host->imported_count && host->imported_count >= 32u) {
        free(data);
        return VG_ERROR_CAPACITY;
    }
    if (total > UINT64_C(128) * 1024u * 1024u) {
        free(data);
        return VG_ERROR_CAPACITY;
    }
    VgAssetSourceDesc source = {0};
    source.struct_size = sizeof(source);
    source.api_version = VG_API_VERSION;
    source.id = id;
    source.type = VG_ASSET_TYPE_MESH;
    source.importer_version = VG_STATIC_MODEL_IMPORTER_VERSION;
    source.version = slot < host->imported_count ? host->imported[slot].version + 1u : 1u;
    source.source_path = path_node->as.string.data;
    source.source_data = data;
    source.source_size = size;
    VgResult result = vg_asset_catalog_upsert(context, &source);
    free(data);
    if (result == VG_OK) {
        (void)snprintf(host->imported[slot].id, sizeof(host->imported[slot].id), "%s", id_text);
        (void)snprintf(host->imported[slot].fingerprint, sizeof(host->imported[slot].fingerprint),
                       "%s", actual);
        host->imported[slot].version = source.version;
        host->imported[slot].bytes = size;
        if (slot == host->imported_count)
            ++host->imported_count;
    }
    return result;
}

static VgResult host_resolve_asset(void *user, VgContext *context, VgAssetId id, VgAssetType type,
                                   VgAsset *out_asset) {
    VgGpuHost *host = user;
    static const uint8_t atrium_asset_id[16] = {0x4a, 0x30, 0x31, 0x2d, 0x61, 0x74, 0x72, 0x69,
                                                0x75, 0x6d, 0x2d, 0x6d, 0x6f, 0x64, 0x65, 0x6c};
    if (type != VG_ASSET_TYPE_MESH || host->model_data == NULL)
        return VG_ERROR_UNSUPPORTED;
    if (memcmp(id.bytes, atrium_asset_id, sizeof(atrium_asset_id)) != 0) {
        char text[37];
        host_asset_id_text(id, text);
        const VgJsonNode *asset = host_asset_node(
            host->resolving_document != NULL ? host->resolving_document : host->document, text);
        if (asset == NULL)
            return VG_ERROR_NOT_FOUND;
        VgResult registered = host_register_imported(host, context, asset, id);
        if (registered != VG_OK)
            return registered;
        VgAssetRequest request = {0};
        request.struct_size = sizeof(request);
        request.api_version = VG_API_VERSION;
        request.id = id;
        request.type = type;
        request.required_residency = VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU;
        VgResult result = vg_asset_acquire(context, &request, out_asset);
        if (result != VG_OK)
            return result;
        result = vg_asset_reload(context, *out_asset);
        if (result != VG_OK) {
            (void)vg_asset_release(context, *out_asset);
            return result;
        }
        return VG_OK;
    }
    if (host->model_registered && memcmp(host->model_id.bytes, id.bytes, 16u) != 0)
        return VG_ERROR_NOT_FOUND;
    if (!host->model_registered) {
        VgAssetSourceDesc source = {0};
        source.struct_size = sizeof(source);
        source.api_version = VG_API_VERSION;
        source.id = id;
        source.type = type;
        source.importer_version = VG_STATIC_MODEL_IMPORTER_VERSION;
        source.version = 1u;
        source.source_path = "demo/atrium.gltf";
        source.source_data = host->model_data;
        source.source_size = host->model_size;
        VgResult result = vg_asset_catalog_upsert(context, &source);
        if (result != VG_OK)
            return result;
        host->model_id = id;
        host->model_registered = true;
    }
    VgAssetRequest request = {0};
    request.struct_size = sizeof(request);
    request.api_version = VG_API_VERSION;
    request.id = id;
    request.type = type;
    request.required_residency = VG_ASSET_RESIDENCY_CPU | VG_ASSET_RESIDENCY_GPU;
    return vg_asset_acquire(context, &request, out_asset);
}

static bool host_find_camera(VgGpuHost *host, VgDocumentInstance *instance, VgEntity *out_camera) {
    size_t count = vg_document_instance_entity_count(instance);
    for (size_t index = 0u; index < count; ++index) {
        VgUuid id;
        VgEntity entity;
        VgCameraDesc camera = {0};
        camera.struct_size = sizeof(camera);
        camera.api_version = VG_API_VERSION;
        if (vg_document_instance_entity_at(instance, index, &id, &entity) &&
            vg_camera_get(host->context, entity, &camera) == VG_OK) {
            *out_camera = entity;
            return true;
        }
    }
    return false;
}

static bool host_apply_editor_hidden(VgGpuHost *host, const VgDocument *document,
                                     VgDocumentInstance *instance) {
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY ||
        entities->as.array.count != vg_document_instance_entity_count(instance))
        return false;
    for (size_t index = 0u; index < entities->as.array.count; ++index) {
        const VgJsonNode *editor = vg_json_object_get(entities->as.array.items[index], "editor");
        const VgJsonNode *hidden = vg_json_object_get(editor, "hidden");
        if (hidden == NULL || hidden->type != VG_JSON_BOOL || !hidden->as.boolean)
            continue;
        const VgJsonNode *components =
            vg_json_object_get(entities->as.array.items[index], "components");
        if (vg_json_object_get(components, "engine.mesh") == NULL &&
            vg_json_object_get(components, "vestigio.room") == NULL)
            continue;
        VgUuid uuid;
        VgEntity entity;
        if (!vg_document_instance_entity_at(instance, index, &uuid, &entity) ||
            vg_mesh_renderer_clear(host->context, entity) != VG_OK)
            return false;
    }
    return true;
}

static VgQuat host_rotation(float yaw, float pitch) {
    float sy = sinf(yaw * 0.5f), cy = cosf(yaw * 0.5f);
    float sp = sinf(pitch * 0.5f), cp = cosf(pitch * 0.5f);
    return (VgQuat){cy * sp, sy * sp, sy * cp, cy * cp};
}

static VgResult host_set_camera(VgGpuHost *host, VgEntity entity, VgVec3 position, float yaw,
                                float pitch) {
    VgTransform transform;
    VgResult result = vg_entity_get_local_transform(host->context, entity, &transform);
    if (result != VG_OK)
        return result;
    transform.position = position;
    transform.rotation = host_rotation(yaw, pitch);
    return vg_entity_set_local_transform(host->context, entity, &transform);
}

VgGpuHost *vg_gpu_host_create(void *parent_window, unsigned int width, unsigned int height,
                              char *error, size_t error_capacity) {
    if (!parent_window || width == 0u || height == 0u || width > (unsigned int)INT_MAX ||
        height > (unsigned int)INT_MAX) {
        host_error(error, error_capacity, "Parent o dimensiones de viewport invalidas");
        return NULL;
    }
    VgGpuHost *host = calloc(1u, sizeof(*host));
    if (!host) {
        host_error(error, error_capacity, "Sin memoria para host GPU");
        return NULL;
    }
    host->owner_thread = vg_win32_thread_id();
    host->audio_enabled = true;
    for (uint32_t bus = 0u; bus < VG_AUDIO_BUS_COUNT; ++bus)
        host->audio_gain[bus] = 1.0f;
    host->pick_mask =
        VG_GPU_PICK_MESH | VG_GPU_PICK_CAMERA | VG_GPU_PICK_LIGHT | VG_GPU_PICK_TRIGGER;
    host->gizmo_operation = -1;
    VgGpuHost *expected = NULL;
    if (!atomic_compare_exchange_strong_explicit(&active_host, &expected, host,
                                                 memory_order_acq_rel, memory_order_acquire)) {
        host_error(error, error_capacity, "Ya existe una superficie GPU en este proceso");
        free(host);
        return NULL;
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_RESIZABLE);
    InitWindow((int)width, (int)height, "VESTIGIO GPU viewport");
    if (!IsWindowReady()) {
        host_error(error, error_capacity, "No se pudo crear ventana/contexto GPU");
        atomic_store_explicit(&active_host, NULL, memory_order_release);
        free(host);
        return NULL;
    }
    SetExitKey(KEY_NULL);
    SetTargetFPS(0);
    host->window = GetWindowHandle();
    if (!vg_win32_embed_window(host->window, parent_window, (int)width, (int)height)) {
        host_error(error, error_capacity, "No se pudo alojar la superficie GPU en WPF");
        CloseWindow();
        atomic_store_explicit(&active_host, NULL, memory_order_release);
        free(host);
        return NULL;
    }
    host->renderer =
        vg_gpu_renderer_create((VgGpuRendererConfig){320u, 180u}, error, error_capacity);
    if (!host->renderer) {
        vg_win32_unembed_window(host->window);
        CloseWindow();
        atomic_store_explicit(&active_host, NULL, memory_order_release);
        free(host);
        return NULL;
    }
    host_error(error, error_capacity, "");
    return host;
}

void *vg_gpu_host_window(const VgGpuHost *host) {
    return host_is_current(host) ? host->window : NULL;
}

int32_t vg_gpu_host_render(VgGpuHost *host) {
    if (!host_is_current(host) || !host->renderer)
        return false;
    VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    char visual_error[192] = {0};
    if (!host_apply_visual(host, instance, host->visual_mode, visual_error, sizeof(visual_error)))
        return false;
    if (instance != NULL) {
        if (vg_gpu_renderer_draw_world(host->renderer, host->context,
                                       vg_document_instance_world(instance)) != VG_OK)
            return false;
        if (host->play == NULL && (host->room_view_grid || host->room_view_ghost)) {
            VgGpuRoomOutline ghosts[64];
            VgVec3 points[64][VG_ROOM_MAX_VERTICES];
            size_t count =
                host->room_view_ghost ? host_collect_room_ghosts(host, ghosts, points, 64u) : 0u;
            if (!vg_gpu_renderer_draw_room_editor_overlay(host->renderer, host->room_view_floor_z,
                                                          host->room_view_grid, ghosts, count))
                return false;
        }
        if (host->play == NULL && host->gizmo_operation >= 0) {
            VgGpuGizmo gizmos[VG_TOOL_MAX_COMMANDS];
            size_t count = host_collect_gizmos(host, gizmos, VG_TOOL_MAX_COMMANDS);
            if (count != 0u && !vg_gpu_renderer_draw_gizmos(host->renderer, gizmos, count))
                return false;
        }
    } else if (!vg_gpu_renderer_draw_demo(host->renderer)) {
        return false;
    }
    vg_gpu_renderer_present_embedded(host->renderer);
    return true;
}

int32_t vg_gpu_host_visual_mode(const VgGpuHost *host) {
    return host_is_current(host) && host->renderer != NULL ? host->visual_mode : -1;
}

int32_t vg_gpu_host_set_visual_mode(VgGpuHost *host, int32_t mode, char *error,
                                    size_t error_capacity) {
    if (!host_is_current(host) || host->renderer == NULL ||
        (mode != VG_GPU_VISUAL_CLEAN && mode != VG_GPU_VISUAL_RETRO)) {
        host_error(error, error_capacity, "Perfil visual invalido");
        return false;
    }
    VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    if (!host_apply_visual(host, instance, mode, error, error_capacity))
        return false;
    host->visual_mode = mode;
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_load_visual_profile(VgGpuHost *host, const char *path, char *error,
                                        size_t error_capacity) {
    if (!host_is_current(host) || path == NULL || path[0] == '\0') {
        host_error(error, error_capacity, "Ruta de settings invalida");
        return false;
    }
    int file_state = host_settings_file_state(path);
    if (file_state == 0) {
        for (uint32_t bus = 0u; bus < VG_AUDIO_BUS_COUNT; ++bus)
            host->audio_gain[bus] = 1.0f;
        return vg_gpu_host_set_visual_mode(host, VG_VISUAL_PROFILE_CLEAN, error, error_capacity);
    }
    if (file_state < 0) {
        host_error(error, error_capacity, "No se pudo leer settings visuales");
        return false;
    }
    VgSettingsLayer layer = {0};
    layer.struct_size = sizeof(layer);
    layer.api_version = VG_API_VERSION;
    VgSettingsDiagnostic diagnostic = {0};
    diagnostic.struct_size = sizeof(diagnostic);
    diagnostic.api_version = VG_API_VERSION;
    if (vg_settings_load_file(path, &layer, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgSettingsLayer resolved = {0};
    resolved.struct_size = sizeof(resolved);
    resolved.api_version = VG_API_VERSION;
    if (vg_settings_resolve(NULL, &layer, NULL, &resolved, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    host->audio_gain[VG_AUDIO_BUS_MASTER] = resolved.audio_master_gain;
    host->audio_gain[VG_AUDIO_BUS_MUSIC] = resolved.audio_music_gain;
    host->audio_gain[VG_AUDIO_BUS_SFX] = resolved.audio_sfx_gain;
    host->audio_gain[VG_AUDIO_BUS_AMBIENCE] = resolved.audio_ambience_gain;
    return vg_gpu_host_set_visual_mode(host,
                                       (layer.present & VG_SETTING_VISUAL_PROFILE) != 0u
                                           ? (int32_t)layer.visual_profile
                                           : VG_VISUAL_PROFILE_CLEAN,
                                       error, error_capacity);
}

int32_t vg_gpu_host_set_audio_enabled(VgGpuHost *host, int32_t enabled) {
    if (!host_is_current(host) || host->play != NULL || (enabled != 0 && enabled != 1))
        return false;
    host->audio_enabled = enabled != 0;
    return true;
}

int32_t vg_gpu_host_audio_device_state(const VgGpuHost *host) {
    if (!host_is_current(host))
        return -1;
    if (host->audio == NULL)
        return VG_AUDIO_DEVICE_STOPPED;
    return vg_atrium_audio_ready(host->audio) ? VG_AUDIO_DEVICE_READY : VG_AUDIO_DEVICE_UNAVAILABLE;
}

uint64_t vg_gpu_host_audio_voice_starts(const VgGpuHost *host) {
    return host_is_current(host) ? vg_atrium_audio_stats(host->audio).voice_starts : 0u;
}

uint32_t vg_gpu_host_audio_active_voices(const VgGpuHost *host) {
    return host_is_current(host) ? vg_atrium_audio_stats(host->audio).voices : 0u;
}

uint32_t vg_gpu_host_audio_music_streams(const VgGpuHost *host) {
    return host_is_current(host) ? vg_atrium_audio_stats(host->audio).music_streams : 0u;
}

uint64_t vg_gpu_host_audio_stream_updates(const VgGpuHost *host) {
    return host_is_current(host) ? vg_atrium_audio_stats(host->audio).stream_updates : 0u;
}

int32_t vg_gpu_host_audio_focus_paused(const VgGpuHost *host) {
    return host_is_current(host) && host->audio != NULL && host->audio_focus_paused;
}

float vg_gpu_host_audio_gain(const VgGpuHost *host, uint32_t bus) {
    return host_is_current(host) && bus < VG_AUDIO_BUS_COUNT ? host->audio_gain[bus] : -1.0f;
}

int32_t vg_gpu_host_set_audio_gain(VgGpuHost *host, uint32_t bus, float gain) {
    if (!host_is_current(host) || bus >= VG_AUDIO_BUS_COUNT || !isfinite(gain) || gain < 0.0f ||
        gain > 1.0f)
        return false;
    if (vg_atrium_audio_ready(host->audio) &&
        vg_atrium_audio_set_gain(host->audio, bus, gain) != VG_OK)
        return false;
    host->audio_gain[bus] = gain;
    return true;
}

int32_t vg_gpu_host_save_audio_gains(VgGpuHost *host, const char *path, char *error,
                                     size_t error_capacity) {
    if (!host_is_current(host) || path == NULL || path[0] == '\0') {
        host_error(error, error_capacity, "Ruta de settings invalida");
        return false;
    }
    VgSettingsLayer layer = {0};
    layer.struct_size = sizeof(layer);
    layer.api_version = VG_API_VERSION;
    int file_state = host_settings_file_state(path);
    VgSettingsDiagnostic diagnostic = {0};
    diagnostic.struct_size = sizeof(diagnostic);
    diagnostic.api_version = VG_API_VERSION;
    if (file_state == 1 && vg_settings_load_file(path, &layer, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    if (file_state < 0) {
        host_error(error, error_capacity, "No se pudo leer settings de audio");
        return false;
    }
    layer.present |= VG_SETTING_AUDIO_MASTER_GAIN | VG_SETTING_AUDIO_MUSIC_GAIN |
                     VG_SETTING_AUDIO_SFX_GAIN | VG_SETTING_AUDIO_AMBIENCE_GAIN;
    layer.audio_master_gain = host->audio_gain[VG_AUDIO_BUS_MASTER];
    layer.audio_music_gain = host->audio_gain[VG_AUDIO_BUS_MUSIC];
    layer.audio_sfx_gain = host->audio_gain[VG_AUDIO_BUS_SFX];
    layer.audio_ambience_gain = host->audio_gain[VG_AUDIO_BUS_AMBIENCE];
    if (vg_settings_save_file(path, &layer, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_save_visual_profile(VgGpuHost *host, const char *path, char *error,
                                        size_t error_capacity) {
    if (!host_is_current(host) || path == NULL || path[0] == '\0') {
        host_error(error, error_capacity, "Ruta de settings invalida");
        return false;
    }
    VgSettingsLayer layer = {0};
    layer.struct_size = sizeof(layer);
    layer.api_version = VG_API_VERSION;
    int file_state = host_settings_file_state(path);
    if (file_state == 1) {
        VgSettingsDiagnostic diagnostic = {0};
        diagnostic.struct_size = sizeof(diagnostic);
        diagnostic.api_version = VG_API_VERSION;
        if (vg_settings_load_file(path, &layer, &diagnostic) != VG_OK) {
            host_error(error, error_capacity, diagnostic.message);
            return false;
        }
    } else if (file_state < 0) {
        host_error(error, error_capacity, "No se pudo leer settings visuales");
        return false;
    }
    layer.present |= VG_SETTING_VISUAL_PROFILE;
    layer.visual_profile = (uint32_t)host->visual_mode;
    VgSettingsDiagnostic diagnostic = {0};
    diagnostic.struct_size = sizeof(diagnostic);
    diagnostic.api_version = VG_API_VERSION;
    if (vg_settings_save_file(path, &layer, &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    host_error(error, error_capacity, "");
    return true;
}

size_t vg_gpu_host_visual_light_count(const VgGpuHost *host) {
    if (!host_is_current(host))
        return 0u;
    const VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    return vg_document_instance_light_count(instance);
}

int32_t vg_gpu_host_visual_fog_enabled(const VgGpuHost *host) {
    if (!host_is_current(host))
        return false;
    const VgDocumentInstance *instance = host->play != NULL ? host->play : host->edit;
    VgDocumentEnvironment environment;
    return vg_document_instance_environment(instance, &environment) && environment.fog_enabled;
}

int32_t vg_gpu_host_open_level(VgGpuHost *host, const char *level_path, const char *model_path,
                               char *error, size_t error_capacity) {
    if (!host_is_current(host) || level_path == NULL || model_path == NULL ||
        level_path[0] == '\0' || model_path[0] == '\0' || host->document != NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    if (!host_level_directory(level_path, host->level_directory)) {
        host_error(error, error_capacity, "Ruta de nivel invalida");
        return false;
    }
    if (!vg_document_open_file(level_path, &host->document, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    if (!host_read_model(model_path, &host->model_data, &host->model_size)) {
        host_error(error, error_capacity, "No se pudo leer el modelo Atrium");
        goto fail;
    }
    const char *slash = strrchr(model_path, '/');
    const char *backslash = strrchr(model_path, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash))
        slash = backslash;
    if (slash == NULL || (size_t)(slash - model_path) > INT_MAX ||
        snprintf(host->audio_directory, sizeof(host->audio_directory), "%.*s/audio",
                 (int)(slash - model_path), model_path) >= (int)sizeof(host->audio_directory)) {
        host_error(error, error_capacity, "Ruta de audio Atrium demasiado larga");
        goto fail;
    }
    VgContextDesc context_desc = {0};
    context_desc.struct_size = sizeof(context_desc);
    context_desc.api_version = VG_API_VERSION;
    if (vg_context_create(&context_desc, &host->context) != VG_OK) {
        host_error(error, error_capacity, "No se pudo crear el contexto del nivel");
        goto fail;
    }
    VgAssetGpuExecutor executor = vg_gpu_renderer_asset_executor(host->renderer);
    if (vg_asset_attach_gpu(host->context, &executor) != VG_OK) {
        host_error(error, error_capacity, "No se pudo conectar el nivel a GPU");
        goto fail;
    }
    host->gpu_attached = true;
    if (vg_assets_enable_static_model_importer(host->context) != VG_OK) {
        host_error(error, error_capacity, "No se pudo habilitar el importer glTF");
        goto fail;
    }
    VgDocumentInstanceDesc description = {host_resolve_asset, host, 1u};
    if (vg_document_instantiate(host->context, host->document, &description, &host->edit,
                                &diagnostic) != VG_OK) {
        host_error(error, error_capacity, diagnostic.message);
        goto fail;
    }
    if (!host_apply_editor_hidden(host, host->document, host->edit)) {
        host_error(error, error_capacity, "No se pudieron aplicar capas de editor");
        goto fail;
    }
    if (!host_find_camera(host, host->edit, &host->edit_camera)) {
        host_error(error, error_capacity, "El nivel no contiene camara");
        goto fail;
    }
    host->edit_yaw = 0.0f;
    host->edit_pitch = -0.12f;
    host->orbit_target = (VgVec3){0.0f, 1.5f, 0.0f};
    host->orbit_distance = 8.0f;
    if (!vg_document_write_canonical(host->document, &host->saved_json, &host->saved_length,
                                     &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        goto fail;
    }
    host->saved_revision = vg_document_revision(host->document);
    host_error(error, error_capacity, "");
    return true;
fail:
    host_release_level(host);
    return false;
}

int32_t vg_gpu_host_set_mode(VgGpuHost *host, int32_t play) {
    if (!host_is_current(host) || host->edit == NULL || host->gesture.active ||
        (play != 0 && play != 1))
        return false;
    if (host->room_preview_active)
        host_cancel_room_preview_internal(host);
    if (play == 0) {
        host_release_play(host);
        return true;
    }
    if (host->play != NULL)
        return true;
    host_clear_preview(host);
    VgDocumentDiagnostic diagnostic = {0};
    VgDocumentInstanceDesc description = {host_resolve_asset, host, 2u};
    if (vg_document_instantiate(host->context, host->document, &description, &host->play,
                                &diagnostic) != VG_OK)
        return false;
    if (vg_document_instance_spatial(host->play) == NULL) {
        host_release_play(host);
        return false;
    }
    if (!host_find_camera(host, host->play, &host->play_camera)) {
        host_release_play(host);
        return false;
    }
    if (vg_atrium_animation_create(host->context, host->play, &host->animation) != VG_OK) {
        host_release_play(host);
        return false;
    }
    VgTransform camera;
    if (vg_entity_get_local_transform(host->context, host->play_camera, &camera) != VG_OK) {
        host_release_play(host);
        return false;
    }
    size_t count = vg_document_instance_door_count(host->play);
    if (count > 0u) {
        host->doors = calloc(count, sizeof(*host->doors));
        if (host->doors == NULL) {
            host_release_play(host);
            return false;
        }
        for (size_t i = 0u; i < count; ++i) {
            VgDocumentDoorBinding binding;
            if (!vg_document_instance_door_at(host->play, i, &binding)) {
                host_release_play(host);
                return false;
            }
            VgDoorDesc door = {0};
            door.struct_size = sizeof(door);
            door.api_version = VG_API_VERSION;
            door.context = host->context;
            door.spatial = vg_document_instance_spatial(host->play);
            door.hinge = binding.hinge;
            door.panel = binding.panel;
            door.collider = binding.collider;
            door.collider_description = binding.collider_description;
            door.open_angle_radians = binding.open_angle_radians;
            door.angular_speed_radians = binding.speed_radians_per_second;
            if (vg_door_create(&door, &host->doors[i]) != VG_OK) {
                host_release_play(host);
                return false;
            }
            ++host->door_count;
        }
    }
    host->controller = vg_controller_default_config();
    host->controller.height = 1.82f;
    host->controller.eye_height = 1.7f;
    host->controller.collision_mask = UINT64_C(1);
    host->player = (VgControllerState){0};
    host->player.feet = camera.position;
    host->player.feet.z -= host->controller.eye_height;
    host->player.last_ground_height = host->player.feet.z;
    host->player.grounded = 1u;
    host->play_yaw = 0.0f;
    host->play_pitch = -0.12f;
    host->accumulator = 0.0;
    host->jump_pending = false;
    host->interact_pending = false;
    host->audio_focus_paused = true;
    if (host->audio_enabled) {
        char audio_error[192] = {0};
        host->audio =
            vg_atrium_audio_create(host->audio_directory, audio_error, sizeof(audio_error));
        if (!vg_atrium_audio_ready(host->audio))
            (void)fprintf(stderr, "%s\n", audio_error);
        else {
            for (uint32_t bus = 0u; bus < VG_AUDIO_BUS_COUNT; ++bus)
                (void)vg_atrium_audio_set_gain(host->audio, bus, host->audio_gain[bus]);
            (void)vg_atrium_audio_set_paused(host->audio, false, true);
        }
    }
    return true;
}

int32_t vg_gpu_host_mode(const VgGpuHost *host) {
    if (!host_is_current(host) || host->edit == NULL)
        return -1;
    return host->play != NULL ? 1 : 0;
}

int32_t vg_gpu_host_interact(VgGpuHost *host) {
    if (!host_is_current(host) || host->play == NULL)
        return false;
    host->interact_pending = true;
    return true;
}

size_t vg_gpu_host_door_count(const VgGpuHost *host) {
    return host_is_current(host) && host->play != NULL ? host->door_count : 0u;
}

int32_t vg_gpu_host_door_angle(const VgGpuHost *host, size_t index, float *out_angle) {
    if (!host_is_current(host) || host->play == NULL || index >= host->door_count ||
        out_angle == NULL)
        return false;
    *out_angle = vg_door_angle(host->doors[index]);
    return true;
}

size_t vg_gpu_host_animation_count(const VgGpuHost *host) {
    return host_is_current(host) && host->play != NULL ? vg_atrium_animation_count(host->animation)
                                                       : 0u;
}

int32_t vg_gpu_host_animation_height(const VgGpuHost *host, size_t index, float *out_height) {
    if (!host_is_current(host) || host->play == NULL || out_height == NULL)
        return false;
    VgTransform pose;
    if (vg_atrium_animation_pose(host->animation, index, &pose) != VG_OK)
        return false;
    *out_height = pose.position.z;
    return true;
}

static VgVec3 host_rotate(VgQuat q, VgVec3 vector);

static bool host_step_edit(VgGpuHost *host, double elapsed, float x, float y, int32_t elevation) {
    VgTransform camera;
    if (vg_entity_get_local_transform(host->context, host->edit_camera, &camera) != VG_OK)
        return false;
    float length = hypotf(x, y);
    if (length > 1.0f) {
        x /= length;
        y /= length;
    }
    float dt = (float)fmin(elapsed, 0.1);
    if (host->edit_camera_mode == 1) {
        host->orbit_distance -= y * 3.2f * dt;
        if (host->orbit_distance < 1.0f)
            host->orbit_distance = 1.0f;
        if (host->orbit_distance > 80.0f)
            host->orbit_distance = 80.0f;
        float cos_pitch = cosf(host->edit_pitch);
        camera.position =
            (VgVec3){host->orbit_target.x + sinf(host->edit_yaw) * cos_pitch * host->orbit_distance,
                     host->orbit_target.y - cosf(host->edit_yaw) * cos_pitch * host->orbit_distance,
                     host->orbit_target.z - sinf(host->edit_pitch) * host->orbit_distance};
        return host_set_camera(host, host->edit_camera, camera.position, host->edit_yaw,
                               host->edit_pitch) == VG_OK;
    }
    camera.position.x += (cosf(host->edit_yaw) * x - sinf(host->edit_yaw) * y) * 3.2f * dt;
    camera.position.y += (sinf(host->edit_yaw) * x + cosf(host->edit_yaw) * y) * 3.2f * dt;
    camera.position.z += (float)elevation * 3.2f * dt;
    return host_set_camera(host, host->edit_camera, camera.position, host->edit_yaw,
                           host->edit_pitch) == VG_OK;
}

static bool host_step_play(VgGpuHost *host, double elapsed, float x, float y, bool jump) {
    VgSpatialScene *spatial = vg_document_instance_spatial(host->play);
    if (spatial == NULL)
        return false;
    float length = hypotf(x, y);
    if (length > 1.0f) {
        x /= length;
        y /= length;
    }
    VgControllerInput input = {0};
    input.move_world.x = cosf(host->play_yaw) * x - sinf(host->play_yaw) * y;
    input.move_world.y = sinf(host->play_yaw) * x + cosf(host->play_yaw) * y;
    host->jump_pending = host->jump_pending || jump;
    input.jump_pressed = host->jump_pending ? 1u : 0u;
    host->accumulator += fmin(elapsed, 0.1);
    unsigned int ticks = 0u;
    while (host->accumulator >= 1.0 / 60.0 && ticks++ < 6u) {
        if (host->interact_pending) {
            VgVec3 eye = vg_controller_eye_position(&host->controller, &host->player);
            VgVec3 forward = host_rotate(host_rotation(host->play_yaw, host->play_pitch),
                                         (VgVec3){0.0f, 1.0f, 0.0f});
            VgSpatialRayQuery ray = {eye, forward, 3.0f, host->controller.collision_mask,
                                     host->play_camera};
            VgSpatialHit hit = {0};
            bool found = false;
            if (vg_spatial_raycast(spatial, &ray, &found, &hit) != VG_OK)
                return false;
            if (found) {
                for (size_t i = 0u; i < host->door_count; ++i) {
                    VgDocumentDoorBinding binding;
                    if (!vg_document_instance_door_at(host->play, i, &binding))
                        return false;
                    if (binding.panel.value == hit.entity.value) {
                        if (vg_door_toggle(host->doors[i]) != VG_OK)
                            return false;
                        if (vg_atrium_audio_ready(host->audio)) {
                            VgTransform panel;
                            if (vg_entity_get_world_transform(host->context, binding.panel,
                                                              &panel) == VG_OK)
                                (void)vg_atrium_audio_play_door(host->audio, panel.position);
                        }
                        break;
                    }
                }
            }
            host->interact_pending = false;
        }
        for (size_t i = 0u; i < host->door_count; ++i)
            if (vg_door_step(host->doors[i], 1.0f / 60.0f, &host->controller, &host->player) !=
                VG_OK)
                return false;
        if (vg_atrium_animation_step(host->animation, 1.0 / 60.0) != VG_OK)
            return false;
        if (vg_controller_step(spatial, &host->controller, host->play_camera, &input, 1.0f / 60.0f,
                               &host->player) != VG_OK)
            return false;
        host->jump_pending = false;
        input.jump_pressed = 0u;
        host->accumulator -= 1.0 / 60.0;
    }
    VgVec3 eye = vg_controller_eye_position(&host->controller, &host->player);
    return host_set_camera(host, host->play_camera, eye, host->play_yaw, host->play_pitch) == VG_OK;
}

int32_t vg_gpu_host_frame(VgGpuHost *host, double elapsed_seconds, float move_x, float move_y,
                          float look_x, float look_y, int32_t jump, int32_t focused) {
    if (!host_is_current(host) || host->edit == NULL || !isfinite(elapsed_seconds) ||
        elapsed_seconds < 0.0 || !isfinite(move_x) || !isfinite(move_y) || !isfinite(look_x) ||
        !isfinite(look_y) || fabsf(look_x) > 10000.0f || fabsf(look_y) > 10000.0f || jump < -1 ||
        jump > 1)
        return false;
    if (focused != 0) {
        float *yaw = host->play != NULL ? &host->play_yaw : &host->edit_yaw;
        float *pitch = host->play != NULL ? &host->play_pitch : &host->edit_pitch;
        *yaw -= look_x * 0.0025f;
        *pitch -= look_y * 0.0025f;
        if (*pitch > 1.25f)
            *pitch = 1.25f;
        if (*pitch < -1.25f)
            *pitch = -1.25f;
        bool stepped = host->play != NULL
                           ? host_step_play(host, elapsed_seconds, move_x, move_y, jump != 0)
                           : host_step_edit(host, elapsed_seconds, move_x, move_y, jump);
        if (!stepped)
            return false;
    } else {
        host->accumulator = 0.0;
        host->jump_pending = false;
        host->interact_pending = false;
    }
    if (vg_atrium_audio_ready(host->audio)) {
        if (host->play != NULL) {
            VgTransform camera;
            if (vg_entity_get_world_transform(host->context, host->play_camera, &camera) == VG_OK) {
                VgVec3 right = host_rotate(camera.rotation, (VgVec3){1.0f, 0.0f, 0.0f});
                (void)vg_atrium_audio_set_listener(host->audio, camera.position, right);
            }
        }
        (void)vg_atrium_audio_set_paused(host->audio, host->play == NULL, focused == 0);
        host->audio_focus_paused = focused == 0;
        (void)vg_atrium_audio_update(host->audio);
    }
    return vg_gpu_host_render(host);
}

uint64_t vg_gpu_host_document_revision(const VgGpuHost *host) {
    return host_is_current(host) && host->document != NULL ? vg_document_revision(host->document)
                                                           : 0u;
}

static void host_format_uuid(VgUuid id, char out[37]) {
    (void)snprintf(out, 37u, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   id.bytes[0], id.bytes[1], id.bytes[2], id.bytes[3], id.bytes[4], id.bytes[5],
                   id.bytes[6], id.bytes[7], id.bytes[8], id.bytes[9], id.bytes[10], id.bytes[11],
                   id.bytes[12], id.bytes[13], id.bytes[14], id.bytes[15]);
}

static size_t host_collect_gizmos(const VgGpuHost *host, VgGpuGizmo *out, size_t capacity) {
    if (host == NULL || host->edit == NULL || host->play != NULL || host->gizmo_operation < 0 ||
        host->selection_count == 0u || capacity == 0u)
        return 0u;
    const VgJsonNode *document_entities =
        vg_json_object_get(vg_document_root(host->document), "entities");
    if (document_entities == NULL || document_entities->type != VG_JSON_ARRAY)
        return 0u;
    for (size_t item = 0u; item < host->selection_count; ++item) {
        for (size_t index = 0u; index < document_entities->as.array.count; ++index) {
            const VgJsonNode *entity = document_entities->as.array.items[index];
            const VgJsonNode *id = vg_json_object_get(entity, "id");
            if (id == NULL || id->type != VG_JSON_STRING ||
                strcmp(id->as.string.data, host->selection_uuids[item]) != 0)
                continue;
            const VgJsonNode *components = vg_json_object_get(entity, "components");
            if (vg_json_object_get(components, "vestigio.room_piece") != NULL)
                return 0u;
            break;
        }
    }
    VgGpuGizmo selected[VG_TOOL_MAX_COMMANDS];
    size_t count = 0u;
    size_t entity_count = vg_document_instance_entity_count(host->edit);
    for (size_t item = 0u; item < host->selection_count && count < VG_TOOL_MAX_COMMANDS; ++item) {
        for (size_t index = 0u; index < entity_count; ++index) {
            VgUuid id;
            VgEntity entity;
            char uuid[37];
            VgTransform transform;
            if (!vg_document_instance_entity_at(host->edit, index, &id, &entity))
                continue;
            host_format_uuid(id, uuid);
            if (strcmp(uuid, host->selection_uuids[item]) != 0)
                continue;
            if (vg_entity_get_world_transform(host->context, entity, &transform) != VG_OK)
                break;
            selected[count++] =
                (VgGpuGizmo){.position = transform.position,
                             .rotation = host->gizmo_space == 1 ? transform.rotation
                                                                : (VgQuat){0.0f, 0.0f, 0.0f, 1.0f},
                             .operation = host->gizmo_operation};
            break;
        }
    }
    if (count == 0u)
        return 0u;
    if (host->gizmo_pivot == 2) {
        size_t total = count < capacity ? count : capacity;
        memcpy(out, selected, total * sizeof(*out));
        return total;
    }
    VgGpuGizmo gizmo = selected[count - 1u];
    if (host->gizmo_pivot == 0) {
        VgVec3 center = {0};
        for (size_t index = 0u; index < count; ++index) {
            center.x += selected[index].position.x;
            center.y += selected[index].position.y;
            center.z += selected[index].position.z;
        }
        gizmo.position =
            (VgVec3){center.x / (float)count, center.y / (float)count, center.z / (float)count};
    }
    out[0] = gizmo;
    return 1u;
}

int32_t vg_gpu_host_gizmo_config(VgGpuHost *host, int32_t operation, int32_t space, int32_t pivot) {
    if (!host_is_current(host) || host->play != NULL || operation < -1 || operation > 2 ||
        space < 0 || space > 1 || pivot < 0 || pivot > 2)
        return false;
    host->gizmo_operation = operation;
    host->gizmo_space = space;
    host->gizmo_pivot = pivot;
    return true;
}

int32_t vg_gpu_host_gizmo_hit(VgGpuHost *host, float u, float v) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        host->gizmo_operation < 0 || !isfinite(u) || !isfinite(v) || u < 0.0f || u > 1.0f ||
        v < 0.0f || v > 1.0f)
        return 0;
    unsigned int width = 0u, height = 0u;
    if (!vg_win32_embedded_size(host->window, &width, &height) || width == 0u || height == 0u)
        return 0;
    float scale = fminf((float)width / 320.0f, (float)height / 180.0f);
    if (scale >= 1.0f)
        scale = floorf(scale);
    if (scale <= 0.0f)
        return 0;
    float draw_width = 320.0f * scale, draw_height = 180.0f * scale;
    float x = u * (float)width - ((float)width - draw_width) * 0.5f;
    float y = v * (float)height - ((float)height - draw_height) * 0.5f;
    if (x < 0.0f || x > draw_width || y < 0.0f || y > draw_height)
        return 0;
    VgGpuGizmo gizmos[VG_TOOL_MAX_COMMANDS];
    size_t count = host_collect_gizmos(host, gizmos, VG_TOOL_MAX_COMMANDS);
    return count == 0u ? 0
                       : vg_gpu_renderer_gizmo_hit(host->renderer, gizmos, count, x / draw_width,
                                                   y / draw_height);
}

int32_t vg_gpu_host_gizmo_drag_direction(VgGpuHost *host, float u, float v, int32_t axis,
                                         float *out_x, float *out_y) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        host->gizmo_operation < 0 || axis < 0 || axis > 2 || out_x == NULL || out_y == NULL ||
        !isfinite(u) || !isfinite(v) || u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
        return false;
    unsigned int width = 0u, height = 0u;
    if (!vg_win32_embedded_size(host->window, &width, &height) || width == 0u || height == 0u)
        return false;
    float scale = fminf((float)width / 320.0f, (float)height / 180.0f);
    if (scale >= 1.0f)
        scale = floorf(scale);
    if (scale <= 0.0f)
        return false;
    float draw_width = 320.0f * scale, draw_height = 180.0f * scale;
    float x = u * (float)width - ((float)width - draw_width) * 0.5f;
    float y = v * (float)height - ((float)height - draw_height) * 0.5f;
    if (x < 0.0f || x > draw_width || y < 0.0f || y > draw_height)
        return false;
    VgGpuGizmo gizmos[VG_TOOL_MAX_COMMANDS];
    size_t count = host_collect_gizmos(host, gizmos, VG_TOOL_MAX_COMMANDS);
    return count != 0u &&
           vg_gpu_renderer_gizmo_drag_direction(host->renderer, gizmos, count, x / draw_width,
                                                y / draw_height, axis, out_x, out_y);
}

static bool host_entity_has_mesh(const VgGpuHost *host, size_t index) {
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY || index >= entities->as.array.count)
        return false;
    const VgJsonNode *components =
        vg_json_object_get(entities->as.array.items[index], "components");
    return vg_json_object_get(components, "engine.mesh") != NULL ||
           vg_json_object_get(components, "vestigio.room") != NULL;
}

size_t vg_gpu_host_entity_count(const VgGpuHost *host) {
    if (!host_is_current(host) || host->edit == NULL)
        return 0u;
    size_t count = 0u;
    for (size_t i = 0u; i < vg_document_instance_entity_count(host->edit); ++i)
        if (host_entity_has_mesh(host, i))
            ++count;
    return count;
}

int32_t vg_gpu_host_entity_at(const VgGpuHost *host, size_t index, char *uuid,
                              size_t uuid_capacity) {
    if (!host_is_current(host) || host->edit == NULL || uuid == NULL || uuid_capacity < 37u)
        return false;
    size_t mesh_index = 0u;
    for (size_t i = 0u; i < vg_document_instance_entity_count(host->edit); ++i) {
        if (!host_entity_has_mesh(host, i))
            continue;
        if (mesh_index++ != index)
            continue;
        VgUuid id;
        VgEntity entity;
        if (!vg_document_instance_entity_at(host->edit, i, &id, &entity))
            return false;
        host_format_uuid(id, uuid);
        return true;
    }
    return false;
}

int32_t vg_gpu_host_select(VgGpuHost *host, const char *uuid) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        host->gesture.active || uuid == NULL)
        return false;
    if (uuid[0] == '\0') {
        host->has_selection = false;
        host->selected_uuid[0] = '\0';
        host->selection_count = 0u;
        return true;
    }
    char current[37];
    size_t count = vg_document_instance_entity_count(host->edit);
    for (size_t index = 0u; index < count; ++index) {
        VgUuid id;
        VgEntity entity;
        if (!vg_document_instance_entity_at(host->edit, index, &id, &entity) ||
            entity.value == host->edit_camera.value)
            continue;
        host_format_uuid(id, current);
        if (strcmp(current, uuid) != 0)
            continue;
        VgMeshRendererDesc mesh = {0};
        mesh.struct_size = sizeof(mesh);
        mesh.api_version = VG_API_VERSION;
        bool has_mesh = vg_mesh_renderer_get(host->context, entity, &mesh) == VG_OK;
        host->selected_entity = entity;
        host->selected_center = has_mesh ? mesh.bounds_center : (VgVec3){0};
        host->selected_extent = has_mesh ? mesh.bounds_extent : (VgVec3){0.25f, 0.25f, 0.25f};
        host->has_selection = true;
        (void)snprintf(host->selected_uuid, sizeof(host->selected_uuid), "%s", current);
        host->selection_count = 1u;
        (void)snprintf(host->selection_uuids[0], sizeof(host->selection_uuids[0]), "%s", current);
        if (has_mesh)
            (void)vg_asset_release(host->context, mesh.asset);
        return true;
    }
    return false;
}

int32_t vg_gpu_host_select_add(VgGpuHost *host, const char *uuid, int32_t additive,
                               int32_t toggle) {
    if (!host_is_current(host) || host->play != NULL || host->gesture.active || uuid == NULL)
        return false;
    if (uuid[0] == '\0')
        return additive ? false : vg_gpu_host_select(host, uuid);
    if (!additive)
        return vg_gpu_host_select(host, uuid);
    size_t existing = host->selection_count;
    size_t found = existing;
    for (size_t i = 0u; i < existing; ++i) {
        if (strcmp(host->selection_uuids[i], uuid) == 0) {
            found = i;
            break;
        }
    }
    if (found != existing) {
        if (!toggle) {
            char previous[VG_TOOL_MAX_COMMANDS][37];
            memcpy(previous, host->selection_uuids, existing * 37u);
            if (!vg_gpu_host_select(host, uuid))
                return false;
            memcpy(host->selection_uuids, previous, existing * 37u);
            host->selection_count = existing;
            return true;
        }
        for (size_t i = found + 1u; i < existing; ++i)
            memcpy(host->selection_uuids[i - 1u], host->selection_uuids[i], 37u);
        host->selection_count = existing - 1u;
        if (host->selection_count == 0u)
            return vg_gpu_host_select(host, "");
        if (strcmp(host->selected_uuid, uuid) == 0) {
            char remaining[VG_TOOL_MAX_COMMANDS][37];
            size_t count = host->selection_count;
            memcpy(remaining, host->selection_uuids, count * 37u);
            if (!vg_gpu_host_select(host, remaining[count - 1u]))
                return false;
            memcpy(host->selection_uuids, remaining, count * 37u);
            host->selection_count = count;
        }
        return true;
    }
    if (existing >= VG_TOOL_MAX_COMMANDS)
        return false;
    char previous[VG_TOOL_MAX_COMMANDS][37];
    memcpy(previous, host->selection_uuids, existing * 37u);
    if (!vg_gpu_host_select(host, uuid))
        return false;
    memcpy(host->selection_uuids, previous, existing * 37u);
    (void)snprintf(host->selection_uuids[existing], 37u, "%s", uuid);
    host->selection_count = existing + 1u;
    return true;
}

size_t vg_gpu_host_selection_count(const VgGpuHost *host) {
    return host_is_current(host) && host->play == NULL ? host->selection_count : 0u;
}

int32_t vg_gpu_host_selection_at(const VgGpuHost *host, size_t index, char *uuid,
                                 size_t uuid_capacity) {
    if (!host_is_current(host) || host->play != NULL || index >= host->selection_count ||
        uuid == NULL || uuid_capacity < 37u)
        return false;
    (void)snprintf(uuid, uuid_capacity, "%s", host->selection_uuids[index]);
    return true;
}

int32_t vg_gpu_host_selected_uuid(const VgGpuHost *host, char *uuid, size_t uuid_capacity) {
    if (!host_is_current(host) || !host->has_selection || uuid == NULL || uuid_capacity < 37u)
        return false;
    (void)snprintf(uuid, uuid_capacity, "%s", host->selected_uuid);
    return true;
}

int32_t vg_gpu_host_selected_transform(const VgGpuHost *host, float position[3], float rotation[4],
                                       float scale[3]) {
    if (!host_is_current(host) || host->play != NULL || !host->has_selection || position == NULL ||
        rotation == NULL || scale == NULL)
        return false;
    VgDocumentTransform transform;
    if (!vg_document_entity_transform(host->document, host->selected_uuid, &transform, NULL))
        return false;
    for (size_t i = 0u; i < 3u; ++i) {
        position[i] = (float)transform.position[i];
        scale[i] = (float)transform.scale[i];
    }
    for (size_t i = 0u; i < 4u; ++i)
        rotation[i] = (float)transform.rotation[i];
    return true;
}

static bool host_prepare_instance(VgGpuHost *host, const VgDocument *document,
                                  VgDocumentInstance **out_edit, VgEntity *out_camera,
                                  VgDocumentDiagnostic *diagnostic, bool preserve_camera) {
    VgDocumentInstanceDesc description = {host_resolve_asset, host, 1u};
    VgDocumentInstance *candidate = NULL;
    host->resolving_document = document;
    VgResult result =
        vg_document_instantiate(host->context, document, &description, &candidate, diagnostic);
    host->resolving_document = NULL;
    if (result != VG_OK)
        return false;
    if (!host_apply_editor_hidden(host, document, candidate)) {
        vg_document_instance_destroy(candidate);
        host_error(diagnostic->message, sizeof(diagnostic->message),
                   "No se pudieron aplicar capas de editor");
        return false;
    }
    VgEntity camera;
    if (!host_find_camera(host, candidate, &camera)) {
        vg_document_instance_destroy(candidate);
        host_error(diagnostic->message, sizeof(diagnostic->message), "El nivel no contiene camara");
        return false;
    }
    if (preserve_camera) {
        VgTransform old_transform;
        VgCameraDesc old_camera = {0};
        old_camera.struct_size = sizeof(old_camera);
        old_camera.api_version = VG_API_VERSION;
        if (vg_entity_get_local_transform(host->context, host->edit_camera, &old_transform) !=
                VG_OK ||
            vg_camera_get(host->context, host->edit_camera, &old_camera) != VG_OK ||
            vg_entity_set_local_transform(host->context, camera, &old_transform) != VG_OK ||
            vg_camera_set(host->context, camera, &old_camera) != VG_OK) {
            vg_document_instance_destroy(candidate);
            host_error(diagnostic->message, sizeof(diagnostic->message),
                       "No se pudo conservar la camara de edicion");
            return false;
        }
    }
    *out_edit = candidate;
    *out_camera = camera;
    return true;
}

static void host_swap_edit(VgGpuHost *host, VgDocumentInstance *candidate, VgEntity camera) {
    char selected[VG_TOOL_MAX_COMMANDS][37];
    size_t selected_count = host->selection_count;
    memcpy(selected, host->selection_uuids, selected_count * 37u);
    host_clear_preview(host);
    vg_document_instance_destroy(host->edit);
    host->edit = candidate;
    host->edit_camera = camera;
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    host->selection_count = 0u;
    bool gesture_active = host->gesture.active;
    host->gesture.active = false;
    for (size_t i = 0u; i < selected_count; ++i)
        (void)vg_gpu_host_select_add(host, selected[i], i != 0u, false);
    host->gesture.active = gesture_active;
}

static bool host_apply_batch(VgGpuHost *host, VgToolBatch *batch, VgToolResult *out_result,
                             char *error, size_t error_capacity) {
    VgDocumentDiagnostic diagnostic = {0};
    char *json = NULL;
    size_t length = 0u;
    VgToolResult preview_result;
    if (!vg_tool_preview(batch, &json, &length, &preview_result, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgDocument *preview = NULL;
    bool opened = vg_document_open_memory("<edit-preview>", json, length, &preview, &diagnostic);
    free(json);
    if (!opened) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera;
    bool prepared = host_prepare_instance(host, preview, &candidate, &camera, &diagnostic, true);
    vg_document_destroy(preview);
    if (!prepared) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult committed;
    if (!vg_tool_commit(batch, &committed, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_document_instance_destroy(candidate);
        vg_tool_cancel(batch);
        return false;
    }
    host_swap_edit(host, candidate, camera);
    if (out_result != NULL)
        *out_result = committed;
    host_error(error, error_capacity, "");
    return true;
}

static VgToolBatch *host_begin_edit(VgGpuHost *host, char *error, size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || host->edit == NULL ||
        host->play != NULL || host->gesture.active || host->room_preview_active) {
        host_error(error, error_capacity, "La edicion requiere el modo Editar");
        return NULL;
    }
    VgDocumentDiagnostic diagnostic = {0};
    VgToolBatch *batch = NULL;
    if (!vg_tool_begin(host->document, vg_document_revision(host->document), &batch, &diagnostic))
        host_error(error, error_capacity, diagnostic.message);
    return batch;
}

static bool host_result_id(const VgToolResult *result, const char *temporary, char *uuid,
                           size_t uuid_capacity) {
    for (size_t i = 0u; i < result->mapping_count; ++i) {
        if (strcmp(result->mappings[i].temporary, temporary) == 0) {
            (void)snprintf(uuid, uuid_capacity, "%s", result->mappings[i].id);
            return true;
        }
    }
    return false;
}

int32_t vg_gpu_host_assets_json(const VgGpuHost *host, char *json, size_t capacity) {
    if (!host_is_current(host) || host->document == NULL || json == NULL || capacity < 3u)
        return 0;
    const VgJsonNode *assets = vg_json_object_get(vg_document_root(host->document), "assets");
    size_t length = 0u;
    json[0] = '\0';
    if (!host_json_append(json, capacity, &length, "["))
        return 0;
    if (assets != NULL && assets->type == VG_JSON_ARRAY) {
        for (size_t index = 0u; index < assets->as.array.count; ++index) {
            const VgJsonNode *asset = assets->as.array.items[index];
            const VgJsonNode *id = vg_json_object_get(asset, "id");
            const VgJsonNode *name = vg_json_object_get(asset, "name");
            const VgJsonNode *source = vg_json_object_get(asset, "source");
            const VgJsonNode *fingerprint = vg_json_object_get(asset, "fingerprint");
            if (id == NULL || name == NULL || source == NULL || fingerprint == NULL)
                return 0;
            char path[4096];
            void *data = NULL;
            uint64_t bytes = 0u;
            char actual[65] = {0};
            bool available = host_asset_path(host, source->as.string.data, path) &&
                             host_read_model(path, &data, &bytes);
            if (available)
                host_fingerprint(data, (size_t)bytes, actual);
            free(data);
            const char *status = !available                                         ? "missing"
                                 : strcmp(actual, fingerprint->as.string.data) != 0 ? "modified"
                                                                                    : "ready";
            const char *diagnostic = !available ? "No se pudo leer el modelo"
                                     : strcmp(status, "modified") == 0
                                         ? "El fingerprint cambió; reimporta el modelo"
                                         : "";
            char *entry =
                host_asset_entry_json(id->as.string.data, name->as.string.data,
                                      source->as.string.data, fingerprint->as.string.data);
            char *quoted_diagnostic = host_json_quote(diagnostic);
            char *quoted_path = host_json_quote(source->as.string.data);
            if (entry == NULL || quoted_diagnostic == NULL || quoted_path == NULL) {
                free(entry);
                free(quoted_diagnostic);
                free(quoted_path);
                return 0;
            }
            size_t entry_length = strlen(entry);
            if (entry_length == 0u || entry[entry_length - 1u] != '}') {
                free(entry);
                free(quoted_diagnostic);
                free(quoted_path);
                return 0;
            }
            entry[entry_length - 1u] = '\0';
            char extra[4096];
            if (snprintf(extra, sizeof(extra), ",\"path\":%s,\"status\":\"%s\",\"diagnostic\":%s}",
                         quoted_path, status, quoted_diagnostic) >= (int)sizeof(extra)) {
                free(entry);
                free(quoted_diagnostic);
                free(quoted_path);
                return 0;
            }
            bool okay = (index == 0u || host_json_append(json, capacity, &length, ",")) &&
                        host_json_append(json, capacity, &length, entry) &&
                        host_json_append(json, capacity, &length, extra);
            free(entry);
            free(quoted_diagnostic);
            free(quoted_path);
            if (!okay)
                return 0;
        }
    }
    return host_json_append(json, capacity, &length, "]") ? (int32_t)length : 0;
}

static bool host_import_source(VgGpuHost *host, const char *path, const char *existing_id,
                               char *out_id, size_t id_capacity, char *error,
                               size_t error_capacity) {
    if (host == NULL || path == NULL || path[0] == '\0' || out_id == NULL || id_capacity < 37u) {
        host_error(error, error_capacity, "Ruta o UUID de asset invalido");
        return false;
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    const char *extension = strrchr(path, '.');
    bool glb =
        extension != NULL && (strcmp(extension, ".glb") == 0 || strcmp(extension, ".GLB") == 0);
    bool gltf =
        extension != NULL && (strcmp(extension, ".gltf") == 0 || strcmp(extension, ".GLTF") == 0);
    if (!glb && !gltf) {
        host_error(error, error_capacity, "Se admite .glb o .gltf con datos embebidos");
        vg_tool_cancel(batch);
        return false;
    }
    void *data = NULL;
    uint64_t bytes = 0u;
    if (!host_read_model(path, &data, &bytes)) {
        host_error(error, error_capacity, "No se pudo leer el modelo (maximo 16 MiB)");
        vg_tool_cancel(batch);
        return false;
    }
    char fingerprint[65];
    host_fingerprint(data, (size_t)bytes, fingerprint);
    const VgJsonNode *previous = NULL;
    char id[37];
    if (existing_id != NULL) {
        VgAssetId parsed;
        if (!host_asset_id_parse(existing_id, &parsed) ||
            (previous = host_asset_node(host->document, existing_id)) == NULL) {
            host_error(error, error_capacity, "Asset para reimportar no existe");
            goto fail;
        }
        (void)snprintf(id, sizeof(id), "%s", existing_id);
    } else {
        const VgJsonNode *assets = vg_json_object_get(vg_document_root(host->document), "assets");
        if (assets != NULL && assets->type == VG_JSON_ARRAY) {
            for (size_t index = 0u; index < assets->as.array.count; ++index) {
                const VgJsonNode *asset = assets->as.array.items[index];
                const VgJsonNode *old_hash = vg_json_object_get(asset, "fingerprint");
                const VgJsonNode *old_id = vg_json_object_get(asset, "id");
                if (old_hash != NULL && old_id != NULL &&
                    strcmp(old_hash->as.string.data, fingerprint) == 0) {
                    (void)snprintf(out_id, id_capacity, "%s", old_id->as.string.data);
                    host_error(error, error_capacity, "");
                    free(data);
                    vg_tool_cancel(batch);
                    return true;
                }
            }
        }
        char path_hash[65];
        host_fingerprint(path, strlen(path), path_hash);
        (void)snprintf(id, sizeof(id), "%.8s-%.4s-4%.3s-8%.3s-%.12s", path_hash, path_hash + 8,
                       path_hash + 13, path_hash + 17, path_hash + 20);
        if (host_asset_node(host->document, id) != NULL) {
            host_error(error, error_capacity, "Colision de UUID de asset");
            goto fail;
        }
    }
    char source[128];
    if (snprintf(source, sizeof(source), "assets/%s.%s", id, glb ? "glb" : "gltf") >=
        (int)sizeof(source)) {
        host_error(error, error_capacity, "Ruta de asset demasiado larga");
        goto fail;
    }
    if (!host_validate_model(source, data, bytes, error, error_capacity))
        goto fail;
    const char *name = NULL;
    if (previous != NULL) {
        const VgJsonNode *name_node = vg_json_object_get(previous, "name");
        name = name_node->as.string.data;
    } else {
        const char *slash = strrchr(path, '/');
        const char *backslash = strrchr(path, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash))
            slash = backslash;
        name = slash == NULL ? path : slash + 1u;
    }
    if (name[0] == '\0' || strlen(name) > 128u) {
        host_error(error, error_capacity, "Nombre de asset invalido");
        goto fail;
    }
    char destination[4096];
    if (!host_ensure_asset_directory(host->level_directory) ||
        !host_asset_path(host, source, destination)) {
        host_error(error, error_capacity, "No se pudo preparar carpeta de assets");
        goto fail;
    }
    void *backup = NULL;
    uint64_t backup_bytes = 0u;
    bool had_destination = host_read_model(destination, &backup, &backup_bytes);
    bool same_file = strcmp(path, destination) == 0;
    if (!same_file && !host_copy_model_file(destination, data, (size_t)bytes)) {
        host_error(error, error_capacity, "No se pudo copiar el modelo al proyecto");
        free(backup);
        goto fail;
    }
    char *manifest = host_updated_assets_json(host->document, id, name, source, fingerprint, NULL);
    VgDocumentDiagnostic diagnostic = {0};
    bool had_manifest = manifest != NULL;
    bool queued = had_manifest && vg_tool_set_assets(batch, manifest, &diagnostic);
    free(manifest);
    bool committed = queued && host_apply_batch(host, batch, NULL, error, error_capacity);
    if (!committed) {
        if (!queued) {
            host_error(error, error_capacity,
                       !had_manifest ? "No se pudo construir manifest" : diagnostic.message);
            vg_tool_cancel(batch);
        }
        if (!same_file) {
            bool restored = false;
            if (had_destination)
                restored = host_copy_model_file(destination, backup, (size_t)backup_bytes);
            else
                restored = host_remove_file(destination);
            if (!restored)
                host_error(error, error_capacity,
                           "Fallo importacion y rollback; revisa asset copiado");
        }
        free(backup);
        free(data);
        return false;
    }
    free(backup);
    free(data);
    (void)snprintf(out_id, id_capacity, "%s", id);
    host_error(error, error_capacity, "");
    return true;
fail:
    free(data);
    vg_tool_cancel(batch);
    return false;
}

int32_t vg_gpu_host_import_asset(VgGpuHost *host, const char *path, char *id, size_t id_capacity,
                                 char *error, size_t error_capacity) {
    return host_import_source(host, path, NULL, id, id_capacity, error, error_capacity);
}

int32_t vg_gpu_host_reimport_asset(VgGpuHost *host, const char *id, const char *path, char *error,
                                   size_t error_capacity) {
    char result_id[37];
    return host_import_source(host, path, id, result_id, sizeof(result_id), error, error_capacity);
}

int32_t vg_gpu_host_rename_asset(VgGpuHost *host, const char *id, const char *name, char *error,
                                 size_t error_capacity) {
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    const VgJsonNode *asset = id == NULL ? NULL : host_asset_node(host->document, id);
    if (asset == NULL || name == NULL || name[0] == '\0' || strlen(name) > 128u) {
        host_error(error, error_capacity, "Asset o nombre invalido");
        vg_tool_cancel(batch);
        return false;
    }
    const char *source = vg_json_object_get(asset, "source")->as.string.data;
    const char *fingerprint = vg_json_object_get(asset, "fingerprint")->as.string.data;
    char *manifest = host_updated_assets_json(host->document, id, name, source, fingerprint, NULL);
    VgDocumentDiagnostic diagnostic = {0};
    bool queued = manifest != NULL && vg_tool_set_assets(batch, manifest, &diagnostic);
    free(manifest);
    if (!queued) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    return host_apply_batch(host, batch, NULL, error, error_capacity);
}

int32_t vg_gpu_host_place_asset(VgGpuHost *host, const char *id, uint32_t node_index, char *uuid,
                                size_t uuid_capacity, char *error, size_t error_capacity) {
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    if (id == NULL || host_asset_node(host->document, id) == NULL || uuid == NULL ||
        uuid_capacity < 37u) {
        host_error(error, error_capacity, "Selecciona un asset importado");
        vg_tool_cancel(batch);
        return false;
    }
    char components[256];
    (void)snprintf(components, sizeof(components),
                   "{\"engine.mesh\":{\"version\":1,\"asset\":\"%s\",\"node_index\":%u}}", id,
                   node_index);
    VgDocumentTransform transform = {{0.0, -3.0, 1.0}, {0.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}};
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_tool_create_entity(batch, "$imported-model", NULL, &transform, components,
                               &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    if (!host_result_id(&result, "$imported-model", uuid, uuid_capacity)) {
        host_error(error, error_capacity, "No se devolvio el UUID colocado");
        return false;
    }
    (void)vg_gpu_host_select(host, uuid);
    return true;
}

int32_t vg_gpu_host_preview_asset(VgGpuHost *host, const char *id, char *error,
                                  size_t error_capacity) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL) {
        host_error(error, error_capacity, "Preview requiere modo Editar");
        return false;
    }
    host_clear_preview(host);
    if (id == NULL || id[0] == '\0') {
        host_error(error, error_capacity, "");
        return true;
    }
    VgAssetId asset_id;
    const VgJsonNode *entry = host_asset_node(host->document, id);
    if (entry == NULL || !host_asset_id_parse(id, &asset_id)) {
        host_error(error, error_capacity, "Asset no existe");
        return false;
    }
    VgAsset asset = {VG_INVALID_HANDLE_VALUE};
    VgResult result = host_resolve_asset(host, host->context, asset_id, VG_ASSET_TYPE_MESH, &asset);
    if (result != VG_OK) {
        host_error(error, error_capacity, "No se pudo cargar el asset para preview");
        return false;
    }
    VgEntity entity;
    result = vg_entity_create(host->context, vg_document_instance_world(host->edit), &entity);
    if (result != VG_OK) {
        (void)vg_asset_release(host->context, asset);
        host_error(error, error_capacity, "No se pudo crear preview");
        return false;
    }
    VgMeshRendererDesc mesh = {0};
    mesh.struct_size = sizeof(mesh);
    mesh.api_version = VG_API_VERSION;
    mesh.asset = asset;
    mesh.mesh_index = VG_RENDER_DEFAULT_INDEX;
    mesh.material_override = VG_RENDER_DEFAULT_INDEX;
    mesh.bounds_extent = (VgVec3){0.5f, 0.5f, 0.5f};
    result = vg_mesh_renderer_set(host->context, entity, &mesh);
    (void)vg_asset_release(host->context, asset);
    VgTransform camera;
    if (result == VG_OK)
        result = vg_entity_get_world_transform(host->context, host->edit_camera, &camera);
    if (result == VG_OK) {
        VgVec3 direction = host_rotate(camera.rotation, (VgVec3){0.0f, 1.0f, 0.0f});
        VgTransform pose = {0};
        pose.position =
            (VgVec3){camera.position.x + direction.x * 3.0f, camera.position.y + direction.y * 3.0f,
                     camera.position.z + direction.z * 3.0f};
        pose.rotation = (VgQuat){0.0f, 0.0f, 0.0f, 1.0f};
        pose.scale = (VgVec3){0.8f, 0.8f, 0.8f};
        result = vg_entity_set_local_transform(host->context, entity, &pose);
    }
    if (result != VG_OK) {
        (void)vg_entity_destroy(host->context, entity);
        host_error(error, error_capacity, "No se pudo preparar preview GPU");
        return false;
    }
    host->preview_entity = entity;
    host->preview_active = true;
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_add_mesh(VgGpuHost *host, char *uuid, size_t uuid_capacity, char *error,
                             size_t error_capacity) {
    if (uuid == NULL || uuid_capacity < 37u) {
        host_error(error, error_capacity, "Se requiere un UUID de salida");
        return false;
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    static const char *components =
        "{\"engine.mesh\":{\"version\":1,\"asset\":\"4a30312d-6174-7269-756d-2d6d6f64656c\",\"node_"
        "index\":2},"
        "\"engine.collider\":{\"version\":1,\"shape\":\"box\",\"motion\":\"static\","
        "\"center\":[0,0,0],\"half_extents\":[0.5,0.5,0.5]}}";
    VgDocumentTransform transform = {{0.0, -3.0, 0.95}, {0.0, 0.0, 0.0, 1.0}, {0.65, 0.65, 1.9}};
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_tool_create_entity(batch, "$new-pillar", NULL, &transform, components, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    if (!host_result_id(&result, "$new-pillar", uuid, uuid_capacity)) {
        host_error(error, error_capacity, "No se devolvio el UUID creado");
        return false;
    }
    (void)vg_gpu_host_select(host, uuid);
    return true;
}

/* The Atrium fixture has an existing walkable floor. The room uses that same
 * floor and six unit-cube instances; the box collider is exactly each cube's
 * authored transform. A fixed template keeps this hobby slice small while the
 * broader room/portal tools in E04 remain separate work. */
typedef struct VgHostRoomPiece {
    const char *temporary;
    const char *part;
    const char *label;
    double x, y, z;
    double sx, sy, sz;
} VgHostRoomPiece;

static const VgHostRoomPiece host_room_pieces[] = {
    {"$room-front-left", "front_left", "Sala: jamba izquierda", -1.355, -4.0, 1.4, 1.11, 0.18, 2.8},
    {"$room-front-right", "front_right", "Sala: jamba derecha", 1.355, -4.0, 1.4, 1.11, 0.18, 2.8},
    {"$room-lintel", "lintel", "Sala: dintel", 0.0, -4.0, 2.5, 1.6, 0.18, 0.6},
    {"$room-left", "left", "Sala: muro izquierdo", -2.0, -6.0, 1.4, 0.18, 4.0, 2.8},
    {"$room-right", "right", "Sala: muro derecho", 2.0, -6.0, 1.4, 0.18, 4.0, 2.8},
    {"$room-back", "back", "Sala: muro posterior", 0.0, -8.0, 1.4, 3.82, 0.18, 2.8},
};

static const char *host_room_part(const VgJsonNode *entity) {
    const VgJsonNode *components = vg_json_object_get(entity, "components");
    const VgJsonNode *marker = vg_json_object_get(components, "vestigio.room_piece");
    const VgJsonNode *preset = vg_json_object_get(marker, "preset");
    const VgJsonNode *part = vg_json_object_get(marker, "part");
    if (preset == NULL || preset->type != VG_JSON_STRING ||
        strcmp(preset->as.string.data, "atrium-doorway-v1") != 0 || part == NULL ||
        part->type != VG_JSON_STRING)
        return NULL;
    return part->as.string.data;
}

static bool host_selected_room_piece(const VgGpuHost *host) {
    if (host == NULL || host->document == NULL || !host->has_selection)
        return false;
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return false;
    for (size_t i = 0u; i < entities->as.array.count; ++i) {
        const VgJsonNode *entity = entities->as.array.items[i];
        const VgJsonNode *id = vg_json_object_get(entity, "id");
        if (id != NULL && id->type == VG_JSON_STRING &&
            strcmp(id->as.string.data, host->selected_uuid) == 0)
            return host_room_part(entity) != NULL;
    }
    return false;
}

static const VgJsonNode *host_document_entity(const VgGpuHost *host, const char *uuid) {
    if (host == NULL || host->document == NULL || uuid == NULL)
        return NULL;
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return NULL;
    for (size_t i = 0u; i < entities->as.array.count; ++i) {
        const VgJsonNode *entity = entities->as.array.items[i];
        const VgJsonNode *id = vg_json_object_get(entity, "id");
        if (id != NULL && id->type == VG_JSON_STRING && strcmp(id->as.string.data, uuid) == 0)
            return entity;
    }
    return NULL;
}

static bool host_find_instance_uuid(const VgGpuHost *host, const char *uuid, VgEntity *out_entity) {
    if (host == NULL || host->edit == NULL || uuid == NULL)
        return false;
    for (size_t i = 0u; i < vg_document_instance_entity_count(host->edit); ++i) {
        VgUuid id;
        VgEntity entity;
        char text[37];
        if (!vg_document_instance_entity_at(host->edit, i, &id, &entity))
            return false;
        host_format_uuid(id, text);
        if (strcmp(text, uuid) == 0) {
            *out_entity = entity;
            return true;
        }
    }
    return false;
}

static size_t host_collect_room_ghosts(const VgGpuHost *host, VgGpuRoomOutline *out,
                                       VgVec3 points[][VG_ROOM_MAX_VERTICES], size_t capacity) {
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return 0u;
    size_t count = 0u;
    for (size_t i = 0u; i < entities->as.array.count && count < capacity; ++i) {
        const VgJsonNode *entity_node = entities->as.array.items[i];
        const VgJsonNode *recipe_node =
            vg_json_object_get(vg_json_object_get(entity_node, "components"), "vestigio.room");
        if (recipe_node == NULL)
            continue;
        VgRoomRecipe recipe;
        if (!vg_room_recipe_parse(recipe_node, &recipe, NULL, 0u))
            continue;
        const VgJsonNode *id = vg_json_object_get(entity_node, "id");
        VgEntity entity;
        VgTransform world;
        if (id == NULL || id->type != VG_JSON_STRING ||
            !host_find_instance_uuid(host, id->as.string.data, &entity) ||
            vg_entity_get_world_transform(host->context, entity, &world) != VG_OK)
            continue;
        if (fabsf(world.position.z + recipe.floor_z * world.scale.z - host->room_view_floor_z) <=
            0.25f)
            continue;
        for (uint32_t j = 0u; j < recipe.vertex_count; ++j) {
            VgVec3 point = {recipe.vertices[j][0] * world.scale.x,
                            recipe.vertices[j][1] * world.scale.y, recipe.floor_z * world.scale.z};
            VgVec3 rotated = host_rotate(world.rotation, point);
            points[count][j] = (VgVec3){world.position.x + rotated.x, world.position.y + rotated.y,
                                        world.position.z + rotated.z};
        }
        out[count] = (VgGpuRoomOutline){points[count], recipe.vertex_count};
        ++count;
    }
    return count;
}

static bool host_selection_editable(const VgGpuHost *host, bool structural, char *error,
                                    size_t error_capacity) {
    if (host->selection_count == 0u) {
        host_error(error, error_capacity, "Selecciona al menos un objeto");
        return false;
    }
    for (size_t i = 0u; i < host->selection_count; ++i) {
        const char *uuid = host->selection_uuids[i];
        const VgJsonNode *entity = host_document_entity(host, uuid);
        if (entity == NULL || host_room_part(entity) != NULL) {
            host_error(error, error_capacity,
                       "La seleccion contiene una pieza de sala fija o un objeto ausente");
            return false;
        }
        if (!structural)
            continue;
        VgEntity selected;
        if (!host_find_instance_uuid(host, uuid, &selected))
            return false;
        for (size_t door = 0u; door < vg_document_instance_door_count(host->edit); ++door) {
            VgDocumentDoorBinding binding;
            if (!vg_document_instance_door_at(host->edit, door, &binding))
                return false;
            if (binding.panel.value != selected.value)
                continue;
            bool hinge_selected = false;
            for (size_t other = 0u; other < host->selection_count; ++other) {
                VgEntity parent;
                if (host_find_instance_uuid(host, host->selection_uuids[other], &parent) &&
                    parent.value == binding.hinge.value) {
                    hinge_selected = true;
                    break;
                }
            }
            if (!hinge_selected) {
                host_error(error, error_capacity,
                           "Selecciona tambien la bisagra para editar la puerta en lote");
                return false;
            }
        }
    }
    return true;
}

static VgMatrix host_identity_matrix(void) {
    VgMatrix result = {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f}};
    return result;
}

static VgVec3 host_gesture_axis(const VgGpuHost *host, size_t index) {
    VgVec3 axis = {0.0f, 0.0f, 0.0f};
    ((float *)&axis)[host->gesture.axis] = 1.0f;
    if (host->gesture.space == 0)
        return axis;
    size_t basis = host->gesture.pivot == 2 ? index : host->gesture.active_index;
    VgQuat q = host->gesture.world[basis].rotation;
    VgVec3 t = {2.0f * (q.y * axis.z - q.z * axis.y), 2.0f * (q.z * axis.x - q.x * axis.z),
                2.0f * (q.x * axis.y - q.y * axis.x)};
    return (VgVec3){axis.x + q.w * t.x + q.y * t.z - q.z * t.y,
                    axis.y + q.w * t.y + q.z * t.x - q.x * t.z,
                    axis.z + q.w * t.z + q.x * t.y - q.y * t.x};
}

static VgMatrix host_gesture_operation(const VgGpuHost *host, size_t index, float amount,
                                       VgVec3 pivot) {
    VgVec3 axis = host_gesture_axis(host, index);
    VgMatrix operation = host_identity_matrix();
    if (host->gesture.op == 0) {
        operation.m[3] = axis.x * amount;
        operation.m[7] = axis.y * amount;
        operation.m[11] = axis.z * amount;
        return operation;
    }
    if (host->gesture.op == 1) {
        float sine = sinf(amount * 0.5f), cosine = cosf(amount * 0.5f);
        VgTransform rotation = {
            {0}, {axis.x * sine, axis.y * sine, axis.z * sine, cosine}, {1.0f, 1.0f, 1.0f}};
        operation = vg_transform_matrix(rotation);
    } else {
        float vector[3] = {axis.x, axis.y, axis.z};
        for (size_t row = 0u; row < 3u; ++row)
            for (size_t column = 0u; column < 3u; ++column)
                operation.m[row * 4u + column] += amount * vector[row] * vector[column];
    }
    VgMatrix to_pivot = host_identity_matrix(), from_pivot = host_identity_matrix();
    to_pivot.m[3] = pivot.x;
    to_pivot.m[7] = pivot.y;
    to_pivot.m[11] = pivot.z;
    from_pivot.m[3] = -pivot.x;
    from_pivot.m[7] = -pivot.y;
    from_pivot.m[11] = -pivot.z;
    return vg_matrix_multiply(vg_matrix_multiply(to_pivot, operation), from_pivot);
}

static bool host_gesture_batch(VgGpuHost *host, float amount, VgToolBatch **out_batch, char *error,
                               size_t error_capacity) {
    VgDocumentDiagnostic diagnostic = {0};
    VgToolBatch *batch = NULL;
    if (!vg_tool_begin(host->document, host->gesture.revision, &batch, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgVec3 pivot = {0};
    for (size_t i = 0u; i < host->gesture.count; ++i) {
        pivot.x += host->gesture.world[i].position.x;
        pivot.y += host->gesture.world[i].position.y;
        pivot.z += host->gesture.world[i].position.z;
    }
    if (host->gesture.pivot == 1) {
        pivot = host->gesture.world[host->gesture.active_index].position;
    } else {
        float denominator = (float)host->gesture.count;
        pivot.x /= denominator;
        pivot.y /= denominator;
        pivot.z /= denominator;
    }
    VgMatrix desired[VG_TOOL_MAX_COMMANDS];
    for (size_t i = 0u; i < host->gesture.count; ++i) {
        VgVec3 center = host->gesture.pivot == 2 ? host->gesture.world[i].position : pivot;
        VgMatrix operation = host_gesture_operation(host, i, amount, center);
        desired[i] = vg_matrix_multiply(operation, vg_transform_matrix(host->gesture.world[i]));
    }
    for (size_t i = 0u; i < host->gesture.count; ++i) {
        VgMatrix parent = host->gesture.parent_matrix[i];
        for (size_t j = 0u; j < host->gesture.count; ++j) {
            if (host->gesture.parent_ids[i][0] != '\0' &&
                strcmp(host->gesture.parent_ids[i], host->gesture.ids[j]) == 0) {
                parent = desired[j];
                break;
            }
        }
        VgMatrix inverse;
        VgTransform local;
        if (!vg_matrix_inverse_affine(parent, &inverse) ||
            !vg_matrix_to_transform(vg_matrix_multiply(inverse, desired[i]), &local)) {
            host_error(error, error_capacity,
                       "El gesto produciria shear o escala no representable en la jerarquia");
            vg_tool_cancel(batch);
            return false;
        }
        VgDocumentTransform transform = {0};
        const float position[] = {local.position.x, local.position.y, local.position.z};
        const float rotation[] = {local.rotation.x, local.rotation.y, local.rotation.z,
                                  local.rotation.w};
        const float scale[] = {local.scale.x, local.scale.y, local.scale.z};
        for (size_t k = 0u; k < 3u; ++k) {
            transform.position[k] = position[k];
            transform.scale[k] = scale[k];
        }
        for (size_t k = 0u; k < 4u; ++k)
            transform.rotation[k] = rotation[k];
        if (!vg_tool_set_transform(batch, host->gesture.ids[i], &transform, &diagnostic)) {
            host_error(error, error_capacity, diagnostic.message);
            vg_tool_cancel(batch);
            return false;
        }
    }
    *out_batch = batch;
    return true;
}

int32_t vg_gpu_host_begin_gesture(VgGpuHost *host, int32_t op, int32_t space, int32_t pivot,
                                  int32_t axis, float snap, char *error, size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || host->edit == NULL ||
        host->play != NULL || host->gesture.active || op < 0 || op > 2 || space < 0 || space > 1 ||
        pivot < 0 || pivot > 2 || axis < 0 || axis > 2 || !isfinite(snap) || snap < 0.0f) {
        host_error(error, error_capacity, "Gesto o seleccion invalida");
        return false;
    }
    if (!host_selection_editable(host, false, error, error_capacity))
        return false;
    memset(&host->gesture, 0, sizeof(host->gesture));
    host->gesture.op = op;
    host->gesture.space = space;
    host->gesture.pivot = pivot;
    host->gesture.axis = axis;
    host->gesture.snap = snap;
    host->gesture.revision = vg_document_revision(host->document);
    host->gesture.count = host->selection_count;
    for (size_t i = 0u; i < host->selection_count; ++i) {
        if (strcmp(host->selected_uuid, host->selection_uuids[i]) == 0)
            host->gesture.active_index = i;
    }
    host->gesture.original_edit = host->edit;
    host->gesture.original_camera = host->edit_camera;
    for (size_t i = 0u; i < host->gesture.count; ++i) {
        const char *uuid = host->selection_uuids[i];
        VgEntity entity;
        const VgJsonNode *node = host_document_entity(host, uuid);
        const VgJsonNode *parent = vg_json_object_get(node, "parent");
        if (node == NULL || !host_find_instance_uuid(host, uuid, &entity) ||
            !vg_document_entity_transform(host->document, uuid, &host->gesture.local[i], NULL) ||
            vg_entity_get_world_transform(host->context, entity, &host->gesture.world[i]) !=
                VG_OK) {
            memset(&host->gesture, 0, sizeof(host->gesture));
            host_error(error, error_capacity, "No se pudo leer la pose original");
            return false;
        }
        (void)snprintf(host->gesture.ids[i], 37u, "%s", uuid);
        host->gesture.parent_matrix[i] = host_identity_matrix();
        if (parent != NULL && parent->type == VG_JSON_STRING) {
            VgEntity parent_entity;
            VgTransform parent_world;
            (void)snprintf(host->gesture.parent_ids[i], 37u, "%s", parent->as.string.data);
            if (!host_find_instance_uuid(host, parent->as.string.data, &parent_entity) ||
                vg_entity_get_world_transform(host->context, parent_entity, &parent_world) !=
                    VG_OK) {
                memset(&host->gesture, 0, sizeof(host->gesture));
                host_error(error, error_capacity, "No se pudo leer la jerarquia original");
                return false;
            }
            host->gesture.parent_matrix[i] = vg_transform_matrix(parent_world);
        }
    }
    host->gesture.active = true;
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_update_gesture(VgGpuHost *host, float amount, char *error,
                                   size_t error_capacity) {
    if (!host_is_current(host) || !host->gesture.active || !isfinite(amount)) {
        host_error(error, error_capacity, "No hay gesto activo o desplazamiento valido");
        return false;
    }
    if (vg_document_revision(host->document) != host->gesture.revision) {
        host_error(error, error_capacity, "El documento cambio durante el gesto");
        return false;
    }
    if (host->gesture.snap > 0.0f)
        amount = roundf(amount / host->gesture.snap) * host->gesture.snap;
    if (!isfinite(amount) || (host->gesture.op == 2 && amount <= -1.0f)) {
        host_error(error, error_capacity, "La escala resultante debe ser positiva");
        return false;
    }
    VgToolBatch *batch = NULL;
    if (!host_gesture_batch(host, amount, &batch, error, error_capacity))
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    char *json = NULL;
    size_t length = 0u;
    if (!vg_tool_preview(batch, &json, &length, NULL, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    vg_tool_cancel(batch);
    VgDocument *preview = NULL;
    bool opened = vg_document_open_memory("<gesture-preview>", json, length, &preview, &diagnostic);
    free(json);
    if (!opened) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera;
    bool prepared = host_prepare_instance(host, preview, &candidate, &camera, &diagnostic, true);
    vg_document_destroy(preview);
    if (!prepared) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    if (!host->gesture.has_preview)
        host->edit = NULL; /* Keep the committed instance for Escape. */
    host_swap_edit(host, candidate, camera);
    host->gesture.has_preview = true;
    host->gesture.amount = amount;
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_end_gesture(VgGpuHost *host, int32_t commit, char *error,
                                size_t error_capacity) {
    if (!host_is_current(host) || !host->gesture.active) {
        host_error(error, error_capacity, "No hay gesto activo");
        return false;
    }
    if (commit && vg_document_revision(host->document) != host->gesture.revision) {
        if (host->gesture.has_preview)
            host_swap_edit(host, host->gesture.original_edit, host->gesture.original_camera);
        memset(&host->gesture, 0, sizeof(host->gesture));
        host_error(error, error_capacity, "El documento cambio durante el gesto");
        return false;
    }
    if (commit && host->gesture.has_preview && host->gesture.amount != 0.0f) {
        VgToolBatch *batch = NULL;
        if (!host_gesture_batch(host, host->gesture.amount, &batch, error, error_capacity) ||
            !host_apply_batch(host, batch, NULL, error, error_capacity)) {
            char failure[256];
            (void)snprintf(failure, sizeof(failure), "%s",
                           error != NULL ? error : "Fallo del gesto");
            host_swap_edit(host, host->gesture.original_edit, host->gesture.original_camera);
            memset(&host->gesture, 0, sizeof(host->gesture));
            host_error(error, error_capacity, failure);
            return false;
        }
        vg_document_instance_destroy(host->gesture.original_edit);
    } else if (host->gesture.has_preview) {
        VgDocumentInstance *original = host->gesture.original_edit;
        VgEntity camera = host->gesture.original_camera;
        host_swap_edit(host, original, camera);
    }
    memset(&host->gesture, 0, sizeof(host->gesture));
    host_error(error, error_capacity, "");
    return true;
}

static bool host_can_structure(VgGpuHost *host, char *error, size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || host->edit == NULL ||
        host->play != NULL || host->gesture.active) {
        host_error(error, error_capacity, "La operacion requiere el modo Editar sin gesto activo");
        return false;
    }
    return host_selection_editable(host, true, error, error_capacity);
}

int32_t vg_gpu_host_duplicate_selection(VgGpuHost *host, char *error, size_t error_capacity) {
    if (!host_can_structure(host, error, error_capacity))
        return false;
    if (host->selection_count > VG_TOOL_MAX_COMMANDS / 2u) {
        host_error(error, error_capacity, "Demasiados objetos para duplicar en un lote");
        return false;
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    size_t count = host->selection_count;
    char ids[VG_TOOL_MAX_COMMANDS][37];
    memcpy(ids, host->selection_uuids, count * 37u);
    for (size_t i = 0u; i < count; ++i) {
        char temporary[32];
        (void)snprintf(temporary, sizeof(temporary), "$copy%zu", i);
        if (!vg_tool_duplicate_entity(batch, ids[i], temporary, NULL, &diagnostic)) {
            host_error(error, error_capacity, diagnostic.message);
            vg_tool_cancel(batch);
            return false;
        }
        const VgJsonNode *entity = host_document_entity(host, ids[i]);
        const VgJsonNode *parent = vg_json_object_get(entity, "parent");
        bool parent_selected = false;
        if (parent != NULL && parent->type == VG_JSON_STRING) {
            for (size_t j = 0u; j < count; ++j)
                parent_selected |= strcmp(parent->as.string.data, ids[j]) == 0;
        }
        if (!parent_selected) {
            VgDocumentTransform transform;
            if (!vg_document_entity_transform(host->document, ids[i], &transform, &diagnostic)) {
                host_error(error, error_capacity, diagnostic.message);
                vg_tool_cancel(batch);
                return false;
            }
            transform.position[0] += 1.5;
            if (!vg_tool_set_transform(batch, temporary, &transform, &diagnostic)) {
                host_error(error, error_capacity, diagnostic.message);
                vg_tool_cancel(batch);
                return false;
            }
        }
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    (void)vg_gpu_host_select(host, "");
    for (size_t i = 0u; i < count; ++i) {
        char temporary[32], duplicated[37];
        (void)snprintf(temporary, sizeof(temporary), "$copy%zu", i);
        if (host_result_id(&result, temporary, duplicated, sizeof(duplicated)))
            (void)vg_gpu_host_select_add(host, duplicated, i != 0u, false);
    }
    return true;
}

static bool host_descends_from(const VgGpuHost *host, const char *candidate, const char *ancestor) {
    const VgJsonNode *node = host_document_entity(host, candidate);
    for (size_t depth = 0u; node != NULL && depth < VG_CONTENT_MAX_ENTITIES; ++depth) {
        const VgJsonNode *parent = vg_json_object_get(node, "parent");
        if (parent == NULL || parent->type != VG_JSON_STRING)
            return false;
        if (strcmp(parent->as.string.data, ancestor) == 0)
            return true;
        node = host_document_entity(host, parent->as.string.data);
    }
    return false;
}

int32_t vg_gpu_host_delete_selection(VgGpuHost *host, char *error, size_t error_capacity) {
    if (!host_can_structure(host, error, error_capacity))
        return false;
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return false;
    char deletion[VG_TOOL_MAX_COMMANDS][37];
    size_t count = 0u;
    for (size_t i = 0u; i < entities->as.array.count; ++i) {
        const VgJsonNode *entity = entities->as.array.items[i];
        const VgJsonNode *id = vg_json_object_get(entity, "id");
        if (id == NULL || id->type != VG_JSON_STRING)
            return false;
        bool included = false;
        for (size_t j = 0u; j < host->selection_count; ++j)
            included |= strcmp(id->as.string.data, host->selection_uuids[j]) == 0 ||
                        host_descends_from(host, id->as.string.data, host->selection_uuids[j]);
        if (!included)
            continue;
        if (host_room_part(entity) != NULL || count >= VG_TOOL_MAX_COMMANDS) {
            host_error(error, error_capacity,
                       "La seleccion incluye una sala fija o supera el limite del lote");
            return false;
        }
        (void)snprintf(deletion[count++], 37u, "%s", id->as.string.data);
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    for (size_t i = count; i > 0u; --i) {
        if (!vg_tool_delete_entity(batch, deletion[i - 1u], &diagnostic)) {
            host_error(error, error_capacity, diagnostic.message);
            vg_tool_cancel(batch);
            return false;
        }
    }
    if (!host_apply_batch(host, batch, NULL, error, error_capacity))
        return false;
    (void)vg_gpu_host_select(host, "");
    return true;
}

int32_t vg_gpu_host_reparent_selection(VgGpuHost *host, const char *parent_uuid, char *error,
                                       size_t error_capacity) {
    if (!host_can_structure(host, error, error_capacity))
        return false;
    if (parent_uuid != NULL && parent_uuid[0] != '\0' &&
        host_document_entity(host, parent_uuid) == NULL) {
        host_error(error, error_capacity, "El nuevo padre no existe");
        return false;
    }
    VgMatrix parent = host_identity_matrix();
    if (parent_uuid != NULL && parent_uuid[0] != '\0') {
        VgEntity parent_entity;
        VgTransform parent_world;
        if (!host_find_instance_uuid(host, parent_uuid, &parent_entity) ||
            vg_entity_get_world_transform(host->context, parent_entity, &parent_world) != VG_OK) {
            host_error(error, error_capacity, "No se pudo leer el nuevo padre");
            return false;
        }
        parent = vg_transform_matrix(parent_world);
    }
    VgMatrix inverse;
    if (!vg_matrix_inverse_affine(parent, &inverse)) {
        host_error(error, error_capacity, "El nuevo padre tiene transformacion singular");
        return false;
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    for (size_t i = 0u; i < host->selection_count; ++i) {
        const char *uuid = host->selection_uuids[i];
        VgEntity entity;
        VgTransform world, local;
        if (!host_find_instance_uuid(host, uuid, &entity) ||
            vg_entity_get_world_transform(host->context, entity, &world) != VG_OK ||
            !vg_matrix_to_transform(vg_matrix_multiply(inverse, vg_transform_matrix(world)),
                                    &local)) {
            host_error(error, error_capacity,
                       "El nuevo padre produciria shear o escala no representable");
            vg_tool_cancel(batch);
            return false;
        }
        VgDocumentTransform transform = {
            {local.position.x, local.position.y, local.position.z},
            {local.rotation.x, local.rotation.y, local.rotation.z, local.rotation.w},
            {local.scale.x, local.scale.y, local.scale.z}};
        if (!vg_tool_reparent(batch, uuid,
                              parent_uuid == NULL || parent_uuid[0] == '\0' ? NULL : parent_uuid,
                              &diagnostic) ||
            !vg_tool_set_transform(batch, uuid, &transform, &diagnostic)) {
            host_error(error, error_capacity, diagnostic.message);
            vg_tool_cancel(batch);
            return false;
        }
    }
    return host_apply_batch(host, batch, NULL, error, error_capacity);
}

int32_t vg_gpu_host_entity_label(const VgGpuHost *host, const char *uuid, char *label,
                                 size_t label_capacity) {
    if (!host_is_current(host) || host->document == NULL || uuid == NULL || label == NULL ||
        label_capacity == 0u)
        return false;
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return false;
    for (size_t i = 0u; i < entities->as.array.count; ++i) {
        const VgJsonNode *entity = entities->as.array.items[i];
        const VgJsonNode *id = vg_json_object_get(entity, "id");
        if (id == NULL || id->type != VG_JSON_STRING || strcmp(id->as.string.data, uuid) != 0)
            continue;
        const char *part = host_room_part(entity);
        if (part == NULL)
            return false;
        for (size_t p = 0u; p < sizeof(host_room_pieces) / sizeof(host_room_pieces[0]); ++p) {
            if (strcmp(part, host_room_pieces[p].part) == 0) {
                (void)snprintf(label, label_capacity, "%s", host_room_pieces[p].label);
                return strlen(host_room_pieces[p].label) < label_capacity;
            }
        }
        return false;
    }
    return false;
}

int32_t vg_gpu_host_entity_editor_json(const VgGpuHost *host, const char *uuid, char *json,
                                       size_t capacity) {
    if (!host_is_current(host) || host->document == NULL || uuid == NULL || json == NULL ||
        capacity == 0u)
        return 0;
    const VgJsonNode *entity = host_document_entity(host, uuid);
    if (entity == NULL)
        return 0;
    const VgJsonNode *editor = vg_json_object_get(entity, "editor");
    const VgJsonNode *layer = vg_json_object_get(editor, "layer");
    const VgJsonNode *group = vg_json_object_get(editor, "group");
    const VgJsonNode *hidden = vg_json_object_get(editor, "hidden");
    const VgJsonNode *parent = vg_json_object_get(entity, "parent");
    const VgJsonNode *mesh =
        vg_json_object_get(vg_json_object_get(entity, "components"), "engine.mesh");
    const VgJsonNode *asset = vg_json_object_get(mesh, "asset");
    char *q_layer = host_json_quote(layer != NULL ? layer->as.string.data : "Default");
    char *q_group = host_json_quote(group != NULL ? group->as.string.data : "");
    char *q_parent = host_json_quote(
        parent != NULL && parent->type == VG_JSON_STRING ? parent->as.string.data : "");
    char *q_asset = host_json_quote(
        asset != NULL && asset->type == VG_JSON_STRING ? asset->as.string.data : "");
    if (q_layer == NULL || q_group == NULL || q_parent == NULL || q_asset == NULL) {
        free(q_layer);
        free(q_group);
        free(q_parent);
        free(q_asset);
        return 0;
    }
    int written = snprintf(
        json, capacity, "{\"layer\":%s,\"group\":%s,\"hidden\":%s,\"parent\":%s,\"asset\":%s}",
        q_layer, q_group,
        hidden != NULL && hidden->type == VG_JSON_BOOL && hidden->as.boolean ? "true" : "false",
        q_parent, q_asset);
    free(q_layer);
    free(q_group);
    free(q_parent);
    free(q_asset);
    return written > 0 && (size_t)written < capacity ? written : 0;
}

typedef struct VgHostFieldSpec {
    const char *path;
    const char *type;
    const char *unit;
    const char *minimum;
    const char *maximum;
    const char *component;
    const char *member;
    int transform_group;
    int transform_axis;
} VgHostFieldSpec;

static const VgHostFieldSpec host_fields[] = {
    {"transform.position.x", "number", "m", "-10000", "10000", NULL, NULL, 1, 0},
    {"transform.position.y", "number", "m", "-10000", "10000", NULL, NULL, 1, 1},
    {"transform.position.z", "number", "m", "-10000", "10000", NULL, NULL, 1, 2},
    {"transform.scale.x", "number", "factor", "0.001", "10000", NULL, NULL, 2, 0},
    {"transform.scale.y", "number", "factor", "0.001", "10000", NULL, NULL, 2, 1},
    {"transform.scale.z", "number", "factor", "0.001", "10000", NULL, NULL, 2, 2},
    {"engine.mesh.asset", "reference", "", "null", "null", "engine.mesh", "asset", 0, 0},
    {"engine.mesh.node_index", "integer", "", "0", "4095", "engine.mesh", "node_index", 0, 0},
    {"editor.layer", "string", "", "null", "null", "editor", "layer", 0, 0},
    {"editor.group", "string", "", "null", "null", "editor", "group", 0, 0},
    {"editor.hidden", "boolean", "", "null", "null", "editor", "hidden", 0, 0},
};

static const VgHostFieldSpec *host_field_spec(const char *path) {
    for (size_t index = 0u; index < sizeof(host_fields) / sizeof(host_fields[0]); ++index)
        if (strcmp(host_fields[index].path, path) == 0)
            return &host_fields[index];
    return NULL;
}

static const VgJsonNode *host_field_node(const VgJsonNode *entity, const VgHostFieldSpec *spec) {
    if (spec->transform_group != 0) {
        const VgJsonNode *transform = vg_json_object_get(entity, "transform");
        const VgJsonNode *array =
            vg_json_object_get(transform, spec->transform_group == 1 ? "position" : "scale");
        if (array == NULL || array->type != VG_JSON_ARRAY ||
            array->as.array.count <= (size_t)spec->transform_axis)
            return NULL;
        return array->as.array.items[spec->transform_axis];
    }
    const VgJsonNode *component =
        strcmp(spec->component, "editor") == 0
            ? vg_json_object_get(entity, "editor")
            : vg_json_object_get(vg_json_object_get(entity, "components"), spec->component);
    return vg_json_object_get(component, spec->member);
}

static char *host_field_value(const VgJsonNode *entity, const VgHostFieldSpec *spec) {
    const VgJsonNode *value = host_field_node(entity, spec);
    if (value != NULL) {
        char *json = NULL;
        size_t length = 0u;
        return vg_json_write_canonical(value, &json, &length) ? json : NULL;
    }
    const char *fallback = strcmp(spec->path, "engine.mesh.node_index") == 0 ? "0"
                           : strcmp(spec->path, "editor.layer") == 0         ? "\"Default\""
                           : strcmp(spec->path, "editor.group") == 0         ? "\"\""
                           : strcmp(spec->path, "editor.hidden") == 0        ? "false"
                                                                             : "null";
    char *copy = malloc(strlen(fallback) + 1u);
    if (copy != NULL)
        (void)strcpy(copy, fallback);
    return copy;
}

int32_t vg_gpu_host_selection_fields_json(const VgGpuHost *host, char *json, size_t capacity) {
    if (!host_is_current(host) || host->document == NULL || json == NULL || capacity < 14u)
        return 0;
    size_t length = 0u;
    json[0] = '\0';
    if (!host_json_append(json, capacity, &length, "{\"fields\":["))
        return 0;
    bool first = true;
    for (size_t field = 0u; field < sizeof(host_fields) / sizeof(host_fields[0]); ++field) {
        const VgHostFieldSpec *spec = &host_fields[field];
        if (host->selection_count == 0u)
            break;
        bool all_have_component = true;
        char *first_value = NULL;
        bool mixed = false;
        for (size_t selected = 0u; selected < host->selection_count; ++selected) {
            const VgJsonNode *entity = host_document_entity(host, host->selection_uuids[selected]);
            if (entity == NULL) {
                all_have_component = false;
                break;
            }
            if (spec->component != NULL && strcmp(spec->component, "editor") != 0 &&
                vg_json_object_get(vg_json_object_get(entity, "components"), spec->component) ==
                    NULL) {
                all_have_component = false;
                break;
            }
            char *value = host_field_value(entity, spec);
            if (value == NULL) {
                all_have_component = false;
                break;
            }
            if (first_value == NULL)
                first_value = value;
            else {
                mixed = mixed || strcmp(first_value, value) != 0;
                free(value);
            }
        }
        if (!all_have_component) {
            free(first_value);
            continue;
        }
        char field_json[2048];
        int written = snprintf(field_json, sizeof(field_json),
                               "%s{\"path\":\"%s\",\"type\":\"%s\",\"unit\":\"%s\",\"min\":%s,"
                               "\"max\":%s,\"value\":%s,\"mixed\":%s,\"editable\":true}",
                               first ? "" : ",", spec->path, spec->type, spec->unit, spec->minimum,
                               spec->maximum, first_value, mixed ? "true" : "false");
        free(first_value);
        if (written <= 0 || (size_t)written >= sizeof(field_json) ||
            !host_json_append(json, capacity, &length, field_json))
            return 0;
        first = false;
    }
    return host_json_append(json, capacity, &length, "]}") ? (int32_t)length : 0;
}

static bool host_field_valid(const VgGpuHost *host, const VgHostFieldSpec *spec,
                             const VgJsonNode *value, char *error, size_t error_capacity) {
    if (strcmp(spec->type, "number") == 0 || strcmp(spec->type, "integer") == 0) {
        if (value->type != VG_JSON_NUMBER || !isfinite(value->as.number.value))
            goto invalid;
        double minimum = strtod(spec->minimum, NULL);
        double maximum = strtod(spec->maximum, NULL);
        if (value->as.number.value < minimum || value->as.number.value > maximum ||
            (strcmp(spec->type, "integer") == 0 &&
             floor(value->as.number.value) != value->as.number.value))
            goto invalid;
        return true;
    }
    if (strcmp(spec->type, "boolean") == 0) {
        if (value->type != VG_JSON_BOOL)
            goto invalid;
        return true;
    }
    if (value->type != VG_JSON_STRING || value->as.string.length > 128u)
        goto invalid;
    if (strcmp(spec->type, "reference") == 0) {
        VgAssetId id;
        if (!host_asset_id_parse(value->as.string.data, &id) ||
            (host_asset_node(host->document, value->as.string.data) == NULL &&
             strcmp(value->as.string.data, "4a30312d-6174-7269-756d-2d6d6f64656c") != 0))
            goto invalid;
    } else if (strcmp(spec->path, "editor.layer") == 0 && value->as.string.length == 0u) {
        goto invalid;
    }
    return true;
invalid:
    (void)snprintf(error, error_capacity, "%s: valor fuera del tipo/rango o referencia ausente",
                   spec->path);
    return false;
}

int32_t vg_gpu_host_set_selection_fields_json(VgGpuHost *host, const char *json, char *error,
                                              size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || json == NULL ||
        host->selection_count == 0u || host->play != NULL) {
        host_error(error, error_capacity, "Selecciona objetos en modo Editar");
        return false;
    }
    VgJsonError parse_error = {0};
    VgJsonNode *root = vg_json_parse(json, strlen(json), &parse_error);
    const VgJsonNode *updates = vg_json_object_get(root, "updates");
    if (root == NULL || root->type != VG_JSON_OBJECT || updates == NULL ||
        updates->type != VG_JSON_ARRAY || updates->as.array.count == 0u ||
        updates->as.array.count > 32u) {
        host_error(error, error_capacity, "Se requiere updates con 1-32 campos");
        vg_json_destroy(root);
        return false;
    }
    const VgHostFieldSpec *specs[32];
    char *values[32] = {0};
    bool valid = true;
    for (size_t index = 0u; index < updates->as.array.count; ++index) {
        const VgJsonNode *update = updates->as.array.items[index];
        const VgJsonNode *path = vg_json_object_get(update, "path");
        const VgJsonNode *value = vg_json_object_get(update, "value");
        specs[index] = path != NULL && path->type == VG_JSON_STRING
                           ? host_field_spec(path->as.string.data)
                           : NULL;
        if (specs[index] == NULL || value == NULL ||
            !host_field_valid(host, specs[index], value, error, error_capacity)) {
            if (specs[index] == NULL)
                host_error(error, error_capacity, "Campo de inspector no soportado");
            valid = false;
            break;
        }
        size_t length = 0u;
        if (!vg_json_write_canonical(value, &values[index], &length)) {
            host_error(error, error_capacity, "Sin memoria para campo de inspector");
            valid = false;
            break;
        }
    }
    VgToolBatch *batch = valid ? host_begin_edit(host, error, error_capacity) : NULL;
    valid = valid && batch != NULL;
    VgDocumentDiagnostic diagnostic = {0};
    for (size_t selected = 0u; valid && selected < host->selection_count; ++selected) {
        const char *id = host->selection_uuids[selected];
        const VgJsonNode *entity = host_document_entity(host, id);
        VgDocumentTransform transform;
        if (entity == NULL ||
            !vg_document_entity_transform(host->document, id, &transform, &diagnostic)) {
            host_error(error, error_capacity, "Entidad seleccionada no existe");
            valid = false;
            break;
        }
        bool transform_changed = false;
        for (size_t index = 0u; valid && index < updates->as.array.count; ++index) {
            const VgHostFieldSpec *spec = specs[index];
            const VgJsonNode *value = vg_json_object_get(updates->as.array.items[index], "value");
            if (spec->transform_group != 0) {
                double *array = spec->transform_group == 1 ? transform.position : transform.scale;
                array[spec->transform_axis] = value->as.number.value;
                transform_changed = true;
            } else {
                valid = vg_tool_patch_field(batch, id, spec->component, spec->member, values[index],
                                            &diagnostic);
            }
        }
        if (valid && transform_changed)
            valid = vg_tool_set_transform(batch, id, &transform, &diagnostic);
    }
    if (!valid) {
        if (batch != NULL)
            vg_tool_cancel(batch);
        if (error != NULL && error_capacity != 0u && error[0] == '\0')
            host_error(error, error_capacity, diagnostic.message);
    }
    for (size_t index = 0u; index < 32u; ++index)
        free(values[index]);
    vg_json_destroy(root);
    return valid && host_apply_batch(host, batch, NULL, error, error_capacity);
}

static bool host_room_recipe_canonical(const char *recipe_json, char **out_canonical, char *error,
                                       size_t error_capacity) {
    if (recipe_json == NULL || strlen(recipe_json) > VG_CONTENT_MAX_FILE_BYTES) {
        host_error(error, error_capacity, "Receta de sala demasiado grande o ausente");
        return false;
    }
    VgJsonError json_error = {0};
    VgJsonNode *node = vg_json_parse(recipe_json, strlen(recipe_json), &json_error);
    VgRoomRecipe recipe;
    bool valid = node != NULL && vg_room_recipe_parse(node, &recipe, error, error_capacity);
    if (valid) {
        size_t length = 0u;
        valid = vg_json_write_canonical(node, out_canonical, &length);
        if (!valid)
            host_error(error, error_capacity, "Sin memoria para receta de sala");
    } else if (node == NULL) {
        host_error(error, error_capacity, "JSON de receta de sala invalido");
    }
    vg_json_destroy(node);
    return valid;
}

static char *host_room_components(const char *canonical, char *error, size_t error_capacity) {
    size_t length = strlen(canonical);
    char *components = malloc(length + sizeof("{\"vestigio.room\":}"));
    if (components == NULL) {
        host_error(error, error_capacity, "Sin memoria para sala");
        return NULL;
    }
    (void)snprintf(components, length + sizeof("{\"vestigio.room\":}"), "{\"vestigio.room\":%s}",
                   canonical);
    return components;
}

int32_t vg_gpu_host_cancel_room_preview(VgGpuHost *host) {
    if (!host_is_current(host))
        return false;
    host_cancel_room_preview_internal(host);
    return true;
}

int32_t vg_gpu_host_preview_room_recipe(VgGpuHost *host, const char *recipe_json, char *error,
                                        size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || host->play != NULL ||
        host->gesture.active) {
        host_error(error, error_capacity, "Vista previa requiere modo Editar");
        return false;
    }
    host_cancel_room_preview_internal(host);
    char *canonical = NULL;
    if (!host_room_recipe_canonical(recipe_json, &canonical, error, error_capacity))
        return false;
    const char *selected_room = NULL;
    if (host->selection_count == 1u) {
        const VgJsonNode *selected = host_document_entity(host, host->selection_uuids[0]);
        if (vg_json_object_get(vg_json_object_get(selected, "components"), "vestigio.room") != NULL)
            selected_room = host->selection_uuids[0];
    }
    char *components =
        selected_room == NULL ? host_room_components(canonical, error, error_capacity) : NULL;
    if (selected_room == NULL && components == NULL) {
        free(canonical);
        return false;
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL) {
        free(canonical);
        free(components);
        return false;
    }
    const VgDocumentTransform transform = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}};
    VgDocumentDiagnostic diagnostic = {0};
    bool queued =
        selected_room != NULL
            ? vg_tool_set_component(batch, selected_room, "vestigio.room", canonical, &diagnostic)
            : vg_tool_create_entity(batch, "$room-preview", NULL, &transform, components,
                                    &diagnostic);
    free(canonical);
    free(components);
    char *json = NULL;
    size_t length = 0u;
    bool prepared = queued && vg_tool_preview(batch, &json, &length, NULL, &diagnostic);
    vg_tool_cancel(batch);
    if (!prepared) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgDocument *preview_document = NULL;
    bool opened =
        vg_document_open_memory("<room-preview>", json, length, &preview_document, &diagnostic);
    free(json);
    if (!opened) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera = {VG_INVALID_HANDLE_VALUE};
    prepared =
        host_prepare_instance(host, preview_document, &candidate, &camera, &diagnostic, true);
    vg_document_destroy(preview_document);
    if (!prepared) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    host->room_preview_original_edit = host->edit;
    host->room_preview_original_camera = host->edit_camera;
    host->edit = candidate;
    host->edit_camera = camera;
    host->room_preview_active = true;
    host_rebind_selection(host);
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_create_room_recipe(VgGpuHost *host, const char *recipe_json, char *uuid,
                                       size_t uuid_capacity, char *error, size_t error_capacity) {
    if (!host_is_current(host) || uuid == NULL || uuid_capacity < 37u) {
        host_error(error, error_capacity, "Se requiere UUID de salida");
        return false;
    }
    host_cancel_room_preview_internal(host);
    char *canonical = NULL;
    if (!host_room_recipe_canonical(recipe_json, &canonical, error, error_capacity))
        return false;
    char *components = host_room_components(canonical, error, error_capacity);
    free(canonical);
    if (components == NULL)
        return false;
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL) {
        free(components);
        return false;
    }
    const VgDocumentTransform transform = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}};
    VgDocumentDiagnostic diagnostic = {0};
    bool queued =
        vg_tool_create_entity(batch, "$new-room-recipe", NULL, &transform, components, &diagnostic);
    free(components);
    if (!queued) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    if (!host_result_id(&result, "$new-room-recipe", uuid, uuid_capacity)) {
        host_error(error, error_capacity, "No se devolvio UUID de sala");
        return false;
    }
    (void)vg_gpu_host_select(host, uuid);
    return true;
}

int32_t vg_gpu_host_update_room_recipe(VgGpuHost *host, const char *uuid, const char *recipe_json,
                                       char *error, size_t error_capacity) {
    if (!host_is_current(host) || uuid == NULL) {
        host_error(error, error_capacity, "Se requiere sala existente");
        return false;
    }
    host_cancel_room_preview_internal(host);
    const VgJsonNode *entity = host_document_entity(host, uuid);
    const VgJsonNode *components = vg_json_object_get(entity, "components");
    if (vg_json_object_get(components, "vestigio.room") == NULL) {
        host_error(error, error_capacity, "La entidad no tiene receta de sala");
        return false;
    }
    char *canonical = NULL;
    if (!host_room_recipe_canonical(recipe_json, &canonical, error, error_capacity))
        return false;
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL) {
        free(canonical);
        return false;
    }
    VgDocumentDiagnostic diagnostic = {0};
    bool queued = vg_tool_set_component(batch, uuid, "vestigio.room", canonical, &diagnostic);
    free(canonical);
    if (!queued) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    return host_apply_batch(host, batch, NULL, error, error_capacity);
}

int32_t vg_gpu_host_room_recipe_json(const VgGpuHost *host, const char *uuid, char *json,
                                     size_t json_capacity) {
    if (!host_is_current(host) || uuid == NULL || json == NULL || json_capacity == 0u)
        return 0;
    const VgJsonNode *entity = host_document_entity(host, uuid);
    const VgJsonNode *recipe =
        vg_json_object_get(vg_json_object_get(entity, "components"), "vestigio.room");
    char *canonical = NULL;
    size_t length = 0u;
    if (recipe == NULL || !vg_json_write_canonical(recipe, &canonical, &length))
        return 0;
    bool fits = length < json_capacity && length <= INT32_MAX;
    if (fits)
        memcpy(json, canonical, length + 1u);
    free(canonical);
    return fits ? (int32_t)length : 0;
}

int32_t vg_gpu_host_set_room_editor_view(VgGpuHost *host, int32_t grid, int32_t ghost,
                                         float floor_z) {
    if (!host_is_current(host) || host->edit == NULL || (grid != 0 && grid != 1) ||
        (ghost != 0 && ghost != 1) || !isfinite(floor_z) || fabsf(floor_z) > 1000.0f)
        return false;
    host->room_view_grid = grid != 0;
    host->room_view_ghost = ghost != 0;
    host->room_view_floor_z = floor_z;
    return true;
}

int32_t vg_gpu_host_add_room(VgGpuHost *host, char *uuid, size_t uuid_capacity, char *error,
                             size_t error_capacity) {
    if (uuid == NULL || uuid_capacity < 37u) {
        host_error(error, error_capacity, "Se requiere un UUID de salida");
        return false;
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities != NULL && entities->type == VG_JSON_ARRAY) {
        for (size_t i = 0u; i < entities->as.array.count; ++i) {
            if (host_room_part(entities->as.array.items[i]) != NULL) {
                host_error(error, error_capacity, "La sala de ejemplo ya existe en el nivel");
                vg_tool_cancel(batch);
                return false;
            }
        }
    }
    VgDocumentDiagnostic diagnostic = {0};
    for (size_t i = 0u; i < sizeof(host_room_pieces) / sizeof(host_room_pieces[0]); ++i) {
        const VgHostRoomPiece *piece = &host_room_pieces[i];
        VgDocumentTransform transform = {{piece->x, piece->y, piece->z},
                                         {0.0, 0.0, 0.0, 1.0},
                                         {piece->sx, piece->sy, piece->sz}};
        char components[512];
        int length = snprintf(
            components, sizeof(components),
            "{\"engine.mesh\":{\"version\":1,\"asset\":\"4a30312d-6174-7269-756d-2d6d6f64656c\","
            "\"node_index\":2},\"engine.collider\":{\"version\":1,\"shape\":\"box\","
            "\"motion\":\"static\",\"center\":[0,0,0],\"half_extents\":[0.5,0.5,0.5]},"
            "\"vestigio.room_piece\":{\"version\":1,\"preset\":\"atrium-doorway-v1\",\"part\":\"%"
            "s\"}}",
            piece->part);
        if (length < 0 || (size_t)length >= sizeof(components) ||
            !vg_tool_create_entity(batch, piece->temporary, NULL, &transform, components,
                                   &diagnostic)) {
            host_error(error, error_capacity,
                       length < 0 || (size_t)length >= sizeof(components)
                           ? "No se pudo preparar la pieza de sala"
                           : diagnostic.message);
            vg_tool_cancel(batch);
            return false;
        }
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    if (!host_result_id(&result, host_room_pieces[0].temporary, uuid, uuid_capacity)) {
        host_error(error, error_capacity, "No se devolvio el UUID de la sala");
        return false;
    }
    (void)vg_gpu_host_select(host, uuid);
    return true;
}

int32_t vg_gpu_host_duplicate_selected(VgGpuHost *host, char *uuid, size_t uuid_capacity,
                                       char *error, size_t error_capacity) {
    if (uuid == NULL || uuid_capacity < 37u || !host_is_current(host) || !host->has_selection) {
        host_error(error, error_capacity, "Selecciona un objeto para duplicarlo");
        return false;
    }
    if (host_selected_room_piece(host)) {
        host_error(error, error_capacity,
                   "No se puede duplicar una pieza de sala; la plantilla fija quedaria incompleta");
        return false;
    }
    for (size_t i = 0u; i < vg_document_instance_door_count(host->edit); ++i) {
        VgDocumentDoorBinding binding;
        if (!vg_document_instance_door_at(host->edit, i, &binding)) {
            host_error(error, error_capacity, "No se pudo consultar la puerta seleccionada");
            return false;
        }
        if (binding.panel.value == host->selected_entity.value) {
            host_error(error, error_capacity,
                       "No se puede duplicar un panel de puerta sin una bisagra propia");
            return false;
        }
    }
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    VgDocumentTransform transform;
    if (!vg_document_entity_transform(host->document, host->selected_uuid, &transform,
                                      &diagnostic) ||
        !vg_tool_duplicate_entity(batch, host->selected_uuid, "$duplicate", NULL, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    transform.position[0] += 1.5;
    if (!vg_tool_set_transform(batch, "$duplicate", &transform, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    VgToolResult result;
    if (!host_apply_batch(host, batch, &result, error, error_capacity))
        return false;
    if (!host_result_id(&result, "$duplicate", uuid, uuid_capacity)) {
        host_error(error, error_capacity, "No se devolvio el UUID duplicado");
        return false;
    }
    (void)vg_gpu_host_select(host, uuid);
    return true;
}

int32_t vg_gpu_host_set_selected_transform(VgGpuHost *host, const float position[3],
                                           const float rotation[4], const float scale[3],
                                           char *error, size_t error_capacity) {
    if (!host_is_current(host) || !host->has_selection || position == NULL || rotation == NULL ||
        scale == NULL) {
        host_error(error, error_capacity, "Selecciona un objeto y transformacion valida");
        return false;
    }
    if (host_selected_room_piece(host)) {
        host_error(error, error_capacity,
                   "No se puede transformar una pieza de sala; la abertura perderia coherencia");
        return false;
    }
    VgDocumentTransform transform = {0};
    for (size_t i = 0u; i < 3u; ++i) {
        if (!isfinite(position[i]) || !isfinite(scale[i]) || scale[i] <= 0.0f) {
            host_error(error, error_capacity, "Posicion o escala invalida");
            return false;
        }
        transform.position[i] = position[i];
        transform.scale[i] = scale[i];
    }
    double norm = 0.0;
    for (size_t i = 0u; i < 4u; ++i) {
        if (!isfinite(rotation[i])) {
            host_error(error, error_capacity, "Rotacion invalida");
            return false;
        }
        norm += (double)rotation[i] * rotation[i];
    }
    if (norm < 1.0e-10) {
        host_error(error, error_capacity, "Rotacion nula");
        return false;
    }
    norm = sqrt(norm);
    for (size_t i = 0u; i < 4u; ++i)
        transform.rotation[i] = rotation[i] / norm;
    VgToolBatch *batch = host_begin_edit(host, error, error_capacity);
    if (batch == NULL)
        return false;
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_tool_set_transform(batch, host->selected_uuid, &transform, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        vg_tool_cancel(batch);
        return false;
    }
    return host_apply_batch(host, batch, NULL, error, error_capacity);
}

typedef struct VgHostSaveCopy {
    char path[4096];
    void *previous;
    uint64_t previous_bytes;
    bool existed;
} VgHostSaveCopy;

static bool host_rollback_save_copies(VgHostSaveCopy *copies, size_t count) {
    bool okay = true;
    for (size_t index = count; index > 0u; --index) {
        VgHostSaveCopy *copy = &copies[index - 1u];
        bool restored = copy->existed ? host_copy_model_file(copy->path, copy->previous,
                                                             (size_t)copy->previous_bytes)
                                      : host_remove_file(copy->path);
        okay = restored && okay;
    }
    return okay;
}

int32_t vg_gpu_host_save_level(VgGpuHost *host, const char *path, char *error,
                               size_t error_capacity) {
    if (!host_is_current(host) || host->document == NULL || host->play != NULL ||
        host->gesture.active || host->room_preview_active || path == NULL || path[0] == '\0') {
        host_error(error, error_capacity, "Guardar requiere una ruta y el modo Editar");
        return false;
    }
    VgDocumentDiagnostic diagnostic = {0};
    char target_directory[2048];
    if (!host_level_directory(path, target_directory)) {
        host_error(error, error_capacity, "Ruta de guardado invalida");
        return false;
    }
    uint64_t revision = vg_document_revision(host->document);
    char *canonical = NULL;
    size_t length = 0u;
    if (!vg_document_write_canonical(host->document, &canonical, &length, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgHostSaveCopy copies[32] = {0};
    size_t copy_count = 0u;
    if (strcmp(target_directory, host->level_directory) != 0) {
        const VgJsonNode *assets = vg_json_object_get(vg_document_root(host->document), "assets");
        if (assets != NULL && assets->type == VG_JSON_ARRAY) {
            for (size_t index = 0u; index < assets->as.array.count; ++index) {
                const VgJsonNode *source =
                    vg_json_object_get(assets->as.array.items[index], "source");
                char old_path[4096];
                void *data = NULL;
                uint64_t bytes = 0u;
                VgHostSaveCopy *copy = &copies[copy_count];
                bool prepared =
                    source != NULL && source->type == VG_JSON_STRING &&
                    host_asset_path(host, source->as.string.data, old_path) &&
                    host_read_model(old_path, &data, &bytes) &&
                    host_ensure_source_parents(target_directory, source->as.string.data) &&
                    snprintf(copy->path, sizeof(copy->path), "%s/%s", target_directory,
                             source->as.string.data) < (int)sizeof(copy->path);
                if (prepared) {
                    int state = host_settings_file_state(copy->path);
                    prepared = state >= 0;
                    copy->existed = state == 1;
                    if (copy->existed)
                        prepared =
                            host_read_model(copy->path, &copy->previous, &copy->previous_bytes);
                }
                if (prepared)
                    prepared = host_copy_model_file(copy->path, data, (size_t)bytes);
                free(data);
                if (!prepared) {
                    bool rolled_back = host_rollback_save_copies(copies, copy_count);
                    for (size_t old = 0u; old <= copy_count; ++old)
                        free(copies[old].previous);
                    free(canonical);
                    host_error(error, error_capacity,
                               rolled_back ? "No se pudieron copiar modelos de Guardar como"
                                           : "Fallo copia y rollback de modelos de Guardar como");
                    return false;
                }
                ++copy_count;
            }
        }
    }
    if (!vg_document_save_atomic(host->document, path, revision, &diagnostic)) {
        host_error(error, error_capacity, diagnostic.message);
        if (!host_rollback_save_copies(copies, copy_count))
            host_error(error, error_capacity,
                       "Fallo guardado y rollback de modelos de Guardar como");
        for (size_t index = 0u; index < copy_count; ++index)
            free(copies[index].previous);
        free(canonical);
        return false;
    }
    for (size_t index = 0u; index < copy_count; ++index)
        free(copies[index].previous);
    free(host->saved_json);
    host->saved_json = canonical;
    host->saved_length = length;
    host->saved_revision = revision;
    (void)snprintf(host->level_directory, sizeof(host->level_directory), "%s", target_directory);
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_is_dirty(const VgGpuHost *host) {
    if (!host_is_current(host) || host->document == NULL)
        return false;
    if (vg_document_revision(host->document) == host->saved_revision)
        return false;
    char *canonical = NULL;
    size_t length = 0u;
    if (!vg_document_write_canonical(host->document, &canonical, &length, NULL))
        return true;
    bool dirty = length != host->saved_length || host->saved_json == NULL ||
                 memcmp(canonical, host->saved_json, length) != 0;
    free(canonical);
    return dirty;
}

int32_t vg_gpu_host_reopen_level(VgGpuHost *host, const char *level_path, const char *model_path,
                                 char *error, size_t error_capacity) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        host->gesture.active || host->room_preview_active || level_path == NULL ||
        level_path[0] == '\0' || model_path == NULL || model_path[0] == '\0') {
        host_error(error, error_capacity, "Reabrir requiere rutas y el modo Editar");
        return false;
    }
    void *model = NULL;
    uint64_t model_size = 0u;
    if (!host_read_model(model_path, &model, &model_size)) {
        host_error(error, error_capacity, "No se pudo leer el modelo Atrium");
        return false;
    }
    bool same_model =
        model_size == host->model_size && memcmp(model, host->model_data, (size_t)model_size) == 0;
    free(model);
    if (!same_model) {
        host_error(error, error_capacity, "El modelo difiere del Atrium cargado");
        return false;
    }
    VgDocumentDiagnostic diagnostic = {0};
    char previous_directory[2048];
    (void)snprintf(previous_directory, sizeof(previous_directory), "%s", host->level_directory);
    if (!host_level_directory(level_path, host->level_directory)) {
        host_error(error, error_capacity, "Ruta de nivel invalida");
        return false;
    }
    VgDocument *document = NULL;
    if (!vg_document_open_file(level_path, &document, &diagnostic)) {
        (void)snprintf(host->level_directory, sizeof(host->level_directory), "%s",
                       previous_directory);
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    char *canonical = NULL;
    size_t length = 0u;
    if (!vg_document_write_canonical(document, &canonical, &length, &diagnostic)) {
        (void)snprintf(host->level_directory, sizeof(host->level_directory), "%s",
                       previous_directory);
        host_error(error, error_capacity, diagnostic.message);
        vg_document_destroy(document);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera;
    if (!host_prepare_instance(host, document, &candidate, &camera, &diagnostic, true)) {
        (void)snprintf(host->level_directory, sizeof(host->level_directory), "%s",
                       previous_directory);
        host_error(error, error_capacity, diagnostic.message);
        free(canonical);
        vg_document_destroy(document);
        return false;
    }
    host_swap_edit(host, candidate, camera);
    vg_document_destroy(host->document);
    host->document = document;
    free(host->saved_json);
    host->saved_json = canonical;
    host->saved_length = length;
    host->saved_revision = vg_document_revision(document);
    host_error(error, error_capacity, "");
    return true;
}

static int32_t host_history(VgGpuHost *host, bool undo, char *error, size_t error_capacity) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL ||
        host->gesture.active || host->room_preview_active) {
        host_error(error, error_capacity, "Historial requiere el modo Editar");
        return false;
    }
    VgDocumentDiagnostic diagnostic = {0};
    uint64_t revision = vg_document_revision(host->document);
    bool moved = undo ? vg_document_undo(host->document, revision, &diagnostic)
                      : vg_document_redo(host->document, revision, &diagnostic);
    if (!moved) {
        host_error(error, error_capacity, diagnostic.message);
        return false;
    }
    VgDocumentInstance *candidate = NULL;
    VgEntity camera;
    if (!host_prepare_instance(host, host->document, &candidate, &camera, &diagnostic, true)) {
        char failure[256];
        (void)snprintf(failure, sizeof(failure), "%s", diagnostic.message);
        uint64_t rollback_revision = vg_document_revision(host->document);
        bool rolled_back = undo ? vg_document_redo(host->document, rollback_revision, &diagnostic)
                                : vg_document_undo(host->document, rollback_revision, &diagnostic);
        host_error(error, error_capacity, rolled_back ? failure : diagnostic.message);
        return false;
    }
    host_swap_edit(host, candidate, camera);
    host_error(error, error_capacity, "");
    return true;
}

int32_t vg_gpu_host_undo(VgGpuHost *host, char *error, size_t error_capacity) {
    return host_history(host, true, error, error_capacity);
}

int32_t vg_gpu_host_redo(VgGpuHost *host, char *error, size_t error_capacity) {
    return host_history(host, false, error, error_capacity);
}

uint64_t vg_gpu_host_readbacks(const VgGpuHost *host) {
    return host_is_current(host) && host->renderer != NULL
               ? vg_gpu_renderer_stats(host->renderer).readbacks
               : 0u;
}

int32_t vg_gpu_host_camera_position(const VgGpuHost *host, float *x, float *y, float *z) {
    if (!host_is_current(host) || host->edit == NULL || x == NULL || y == NULL || z == NULL)
        return false;
    VgTransform camera;
    VgEntity entity = host->play != NULL ? host->play_camera : host->edit_camera;
    if (vg_entity_get_world_transform(host->context, entity, &camera) != VG_OK)
        return false;
    *x = camera.position.x;
    *y = camera.position.y;
    *z = camera.position.z;
    return true;
}

static VgVec3 host_rotate(VgQuat q, VgVec3 vector) {
    VgVec3 t = {2.0f * (q.y * vector.z - q.z * vector.y), 2.0f * (q.z * vector.x - q.x * vector.z),
                2.0f * (q.x * vector.y - q.y * vector.x)};
    return (VgVec3){vector.x + q.w * t.x + q.y * t.z - q.z * t.y,
                    vector.y + q.w * t.y + q.z * t.x - q.x * t.z,
                    vector.z + q.w * t.z + q.x * t.y - q.y * t.x};
}

static bool host_ray_bounds(VgVec3 origin, VgVec3 direction, VgVec3 center, VgVec3 extent,
                            float *out_distance) {
    float o[3] = {origin.x, origin.y, origin.z};
    float d[3] = {direction.x, direction.y, direction.z};
    float c[3] = {center.x, center.y, center.z};
    float e[3] = {extent.x, extent.y, extent.z};
    float near_t = 0.0f, far_t = 100000.0f;
    for (size_t axis = 0u; axis < 3u; ++axis) {
        if (fabsf(d[axis]) < 1.0e-7f) {
            if (o[axis] < c[axis] - e[axis] || o[axis] > c[axis] + e[axis])
                return false;
            continue;
        }
        float a = (c[axis] - e[axis] - o[axis]) / d[axis];
        float b = (c[axis] + e[axis] - o[axis]) / d[axis];
        if (a > b) {
            float swap = a;
            a = b;
            b = swap;
        }
        if (a > near_t)
            near_t = a;
        if (b < far_t)
            far_t = b;
        if (near_t > far_t)
            return false;
    }
    *out_distance = near_t;
    return true;
}

static int32_t host_pick_impl(VgGpuHost *host, float u, float v, char *uuid, size_t uuid_capacity,
                              bool update_selection) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL || uuid == NULL ||
        (update_selection && host->gesture.active) || uuid_capacity < 37u || !isfinite(u) ||
        !isfinite(v) || u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
        return false;
    uuid[0] = '\0';
    if (update_selection) {
        host->has_selection = false;
        host->selected_uuid[0] = '\0';
        host->selection_count = 0u;
    }
    unsigned int width = 0u, height = 0u;
    if (!vg_win32_embedded_size(host->window, &width, &height) || height == 0u)
        return false;
    float scale = fminf((float)width / 320.0f, (float)height / 180.0f);
    if (scale >= 1.0f)
        scale = floorf(scale);
    if (scale <= 0.0f)
        return false;
    float draw_width = 320.0f * scale, draw_height = 180.0f * scale;
    float x = u * (float)width - ((float)width - draw_width) * 0.5f;
    float y = v * (float)height - ((float)height - draw_height) * 0.5f;
    if (x < 0.0f || x > draw_width || y < 0.0f || y > draw_height)
        return false;
    u = x / draw_width;
    v = y / draw_height;
    VgTransform camera;
    VgCameraDesc camera_desc = {0};
    camera_desc.struct_size = sizeof(camera_desc);
    camera_desc.api_version = VG_API_VERSION;
    if (vg_entity_get_world_transform(host->context, host->edit_camera, &camera) != VG_OK ||
        vg_camera_get(host->context, host->edit_camera, &camera_desc) != VG_OK)
        return false;
    VgVec3 origin = camera.position;
    VgVec3 direction;
    if (camera_desc.projection == VG_CAMERA_ORTHOGRAPHIC) {
        float half_height = camera_desc.orthographic_height * 0.5f;
        float right_offset = (2.0f * u - 1.0f) * half_height * (320.0f / 180.0f);
        float up_offset = (1.0f - 2.0f * v) * half_height;
        VgVec3 lateral = host_rotate(camera.rotation, (VgVec3){right_offset, 0.0f, up_offset});
        origin.x += lateral.x;
        origin.y += lateral.y;
        origin.z += lateral.z;
        direction = host_rotate(camera.rotation, (VgVec3){0.0f, 1.0f, 0.0f});
    } else {
        float tan_half = tanf(camera_desc.vertical_fov_radians * 0.5f);
        VgVec3 camera_ray = {(2.0f * u - 1.0f) * tan_half * (320.0f / 180.0f), 1.0f,
                             (1.0f - 2.0f * v) * tan_half};
        float ray_length = sqrtf(camera_ray.x * camera_ray.x + camera_ray.y * camera_ray.y +
                                 camera_ray.z * camera_ray.z);
        camera_ray.x /= ray_length;
        camera_ray.y /= ray_length;
        camera_ray.z /= ray_length;
        direction = host_rotate(camera.rotation, camera_ray);
    }
    float nearest = 100000.0f;
    VgUuid chosen = {{0}};
    VgEntity chosen_entity = {0};
    VgVec3 chosen_center = {0}, chosen_extent = {0};
    bool found = false;
    size_t count = vg_document_instance_entity_count(host->edit);
    const VgJsonNode *entities = vg_json_object_get(vg_document_root(host->document), "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY || entities->as.array.count != count)
        return false;
    for (size_t index = 0u; index < count; ++index) {
        VgUuid id;
        VgEntity entity;
        VgTransform transform;
        if (!vg_document_instance_entity_at(host->edit, index, &id, &entity) ||
            entity.value == host->edit_camera.value)
            continue;
        const VgJsonNode *components =
            vg_json_object_get(entities->as.array.items[index], "components");
        uint32_t kind = 0u;
        if (vg_json_object_get(components, "engine.mesh") != NULL ||
            vg_json_object_get(components, "vestigio.room") != NULL)
            kind = VG_GPU_PICK_MESH;
        else if (vg_json_object_get(components, "engine.camera") != NULL)
            kind = VG_GPU_PICK_CAMERA;
        else if (vg_json_object_get(components, "engine.light") != NULL)
            kind = VG_GPU_PICK_LIGHT;
        else if (vg_json_object_get(components, "engine.trigger") != NULL)
            kind = VG_GPU_PICK_TRIGGER;
        if ((host->pick_mask & kind) == 0u ||
            vg_entity_get_world_transform(host->context, entity, &transform) != VG_OK)
            continue;
        VgVec3 center = {0};
        VgVec3 extent = {0.25f, 0.25f, 0.25f};
        if (kind == VG_GPU_PICK_MESH) {
            VgMeshRendererDesc mesh = {0};
            mesh.struct_size = sizeof(mesh);
            mesh.api_version = VG_API_VERSION;
            if (vg_mesh_renderer_get(host->context, entity, &mesh) != VG_OK)
                continue;
            center = mesh.bounds_center;
            extent = mesh.bounds_extent;
            (void)vg_asset_release(host->context, mesh.asset);
        }
        VgQuat inverse = {-transform.rotation.x, -transform.rotation.y, -transform.rotation.z,
                          transform.rotation.w};
        VgVec3 offset = {origin.x - transform.position.x, origin.y - transform.position.y,
                         origin.z - transform.position.z};
        VgVec3 local_origin = host_rotate(inverse, offset);
        VgVec3 local_direction = host_rotate(inverse, direction);
        if (transform.scale.x <= 0.0f || transform.scale.y <= 0.0f || transform.scale.z <= 0.0f)
            continue;
        local_origin.x /= transform.scale.x;
        local_origin.y /= transform.scale.y;
        local_origin.z /= transform.scale.z;
        local_direction.x /= transform.scale.x;
        local_direction.y /= transform.scale.y;
        local_direction.z /= transform.scale.z;
        float distance = 0.0f;
        if (host_ray_bounds(local_origin, local_direction, center, extent, &distance) &&
            distance < nearest) {
            nearest = distance;
            chosen = id;
            chosen_entity = entity;
            chosen_center = center;
            chosen_extent = extent;
            found = true;
        }
    }
    if (!found)
        return false;
    if (update_selection) {
        host->selected_entity = chosen_entity;
        host->has_selection = true;
        host->selected_center = chosen_center;
        host->selected_extent = chosen_extent;
    }
    (void)snprintf(uuid, uuid_capacity,
                   "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   chosen.bytes[0], chosen.bytes[1], chosen.bytes[2], chosen.bytes[3],
                   chosen.bytes[4], chosen.bytes[5], chosen.bytes[6], chosen.bytes[7],
                   chosen.bytes[8], chosen.bytes[9], chosen.bytes[10], chosen.bytes[11],
                   chosen.bytes[12], chosen.bytes[13], chosen.bytes[14], chosen.bytes[15]);
    if (update_selection) {
        (void)snprintf(host->selected_uuid, sizeof(host->selected_uuid), "%s", uuid);
        host->selection_count = 1u;
        (void)snprintf(host->selection_uuids[0], sizeof(host->selection_uuids[0]), "%s", uuid);
    }
    return true;
}

int32_t vg_gpu_host_pick(VgGpuHost *host, float u, float v, char *uuid, size_t uuid_capacity) {
    return host_pick_impl(host, u, v, uuid, uuid_capacity, true);
}

int32_t vg_gpu_host_peek(VgGpuHost *host, float u, float v, char *uuid, size_t uuid_capacity) {
    return host_pick_impl(host, u, v, uuid, uuid_capacity, false);
}

int32_t vg_gpu_host_frame_selection(VgGpuHost *host) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL || !host->has_selection)
        return false;
    VgTransform transform;
    if (vg_entity_get_world_transform(host->context, host->selected_entity, &transform) != VG_OK)
        return false;
    VgVec3 center = {host->selected_center.x * transform.scale.x,
                     host->selected_center.y * transform.scale.y,
                     host->selected_center.z * transform.scale.z};
    VgVec3 rotated_center = host_rotate(transform.rotation, center);
    host->orbit_target = transform.position;
    host->orbit_target.x += rotated_center.x;
    host->orbit_target.y += rotated_center.y;
    host->orbit_target.z += rotated_center.z;
    float extent_x = fabsf(host->selected_extent.x * transform.scale.x);
    float extent_y = fabsf(host->selected_extent.y * transform.scale.y);
    float extent_z = fabsf(host->selected_extent.z * transform.scale.z);
    float largest = fmaxf(extent_x, fmaxf(extent_y, extent_z));
    host->orbit_distance = fmaxf(2.0f, fminf(60.0f, largest * 3.0f));
    float cos_pitch = cosf(host->edit_pitch);
    VgVec3 eye = {host->orbit_target.x + sinf(host->edit_yaw) * cos_pitch * host->orbit_distance,
                  host->orbit_target.y - cosf(host->edit_yaw) * cos_pitch * host->orbit_distance,
                  host->orbit_target.z - sinf(host->edit_pitch) * host->orbit_distance};
    return host_set_camera(host, host->edit_camera, eye, host->edit_yaw, host->edit_pitch) == VG_OK;
}

int32_t vg_gpu_host_set_camera_mode(VgGpuHost *host, int32_t mode) {
    if (!host_is_current(host) || host->edit == NULL || host->play != NULL || mode < 0 || mode > 2)
        return false;
    VgCameraDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    if (vg_camera_get(host->context, host->edit_camera, &description) != VG_OK)
        return false;
    description.projection = mode == 2 ? VG_CAMERA_ORTHOGRAPHIC : VG_CAMERA_PERSPECTIVE;
    if (mode == 2)
        description.orthographic_height = 10.0f;
    if (vg_camera_set(host->context, host->edit_camera, &description) != VG_OK)
        return false;
    host->edit_camera_mode = mode;
    if (mode == 1 && host->has_selection)
        return vg_gpu_host_frame_selection(host);
    return true;
}

int32_t vg_gpu_host_set_pick_mask(VgGpuHost *host, uint32_t mask) {
    const uint32_t all =
        VG_GPU_PICK_MESH | VG_GPU_PICK_CAMERA | VG_GPU_PICK_LIGHT | VG_GPU_PICK_TRIGGER;
    if (!host_is_current(host) || (mask & ~all) != 0u)
        return false;
    host->pick_mask = mask;
    host->has_selection = false;
    host->selected_uuid[0] = '\0';
    return true;
}

int32_t vg_gpu_host_resize(VgGpuHost *host, unsigned int width, unsigned int height) {
    if (!host_is_current(host) || !host->window || width == 0u || height == 0u ||
        width > (unsigned int)INT_MAX || height > (unsigned int)INT_MAX)
        return false;
    return vg_win32_resize_embedded(host->window, (int)width, (int)height);
}

int32_t vg_gpu_host_size(VgGpuHost *host, unsigned int *width, unsigned int *height) {
    return host_is_current(host) && vg_win32_embedded_size(host->window, width, height);
}

int32_t vg_gpu_host_capture(VgGpuHost *host, const char *path) {
    return host_is_current(host) && host->renderer && path && path[0] != '\0' &&
           vg_gpu_renderer_capture(host->renderer, path);
}

int32_t vg_gpu_host_focus(VgGpuHost *host) {
    return host_is_current(host) && vg_win32_focus_embedded(host->window);
}

int32_t vg_gpu_host_has_focus(const VgGpuHost *host) {
    return host_is_current(host) && vg_win32_embedded_has_focus(host->window);
}

int32_t vg_gpu_host_destroy(VgGpuHost *host) {
    if (!host_is_current(host))
        return false;
    host_release_level(host);
    vg_gpu_renderer_destroy(host->renderer);
    vg_win32_unembed_window(host->window);
    CloseWindow();
    atomic_store_explicit(&active_host, NULL, memory_order_release);
    free(host);
    return true;
}
