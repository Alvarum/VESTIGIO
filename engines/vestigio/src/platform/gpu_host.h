#ifndef VESTIGIO_GPU_HOST_H
#define VESTIGIO_GPU_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(VG_GPU_HOST_BUILD)
#define VG_GPU_HOST_API __declspec(dllexport)
#elif defined(_WIN32)
#define VG_GPU_HOST_API __declspec(dllimport)
#else
#define VG_GPU_HOST_API
#endif

typedef struct VgGpuHost VgGpuHost;

VG_GPU_HOST_API VgGpuHost *vg_gpu_host_create(void *parent_window, unsigned int width,
                                              unsigned int height, char *error,
                                              size_t error_capacity);
VG_GPU_HOST_API void *vg_gpu_host_window(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_render(VgGpuHost *host);
/* Profile is a user preference; environment and lights come from the level. */
VG_GPU_HOST_API int32_t vg_gpu_host_visual_mode(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_set_visual_mode(VgGpuHost *host, int32_t mode, char *error,
                                                    size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_load_visual_profile(VgGpuHost *host, const char *path,
                                                        char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_save_visual_profile(VgGpuHost *host, const char *path,
                                                        char *error, size_t error_capacity);
VG_GPU_HOST_API size_t vg_gpu_host_visual_light_count(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_visual_fog_enabled(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_set_audio_enabled(VgGpuHost *host, int32_t enabled);
VG_GPU_HOST_API int32_t vg_gpu_host_audio_device_state(const VgGpuHost *host);
VG_GPU_HOST_API uint64_t vg_gpu_host_audio_voice_starts(const VgGpuHost *host);
VG_GPU_HOST_API uint32_t vg_gpu_host_audio_active_voices(const VgGpuHost *host);
VG_GPU_HOST_API uint32_t vg_gpu_host_audio_music_streams(const VgGpuHost *host);
VG_GPU_HOST_API uint64_t vg_gpu_host_audio_stream_updates(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_audio_focus_paused(const VgGpuHost *host);
VG_GPU_HOST_API float vg_gpu_host_audio_gain(const VgGpuHost *host, uint32_t bus);
VG_GPU_HOST_API int32_t vg_gpu_host_set_audio_gain(VgGpuHost *host, uint32_t bus, float gain);
VG_GPU_HOST_API int32_t vg_gpu_host_save_audio_gains(VgGpuHost *host, const char *path, char *error,
                                                     size_t error_capacity);
/* E01: load a native level and its referenced Atrium model. Edit owns the
 * document; Play instantiates a separate world and Stop discards it. */
VG_GPU_HOST_API int32_t vg_gpu_host_open_level(VgGpuHost *host, const char *level_path,
                                               const char *model_path, char *error,
                                               size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_set_mode(VgGpuHost *host, int32_t play);
VG_GPU_HOST_API int32_t vg_gpu_host_mode(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_frame(VgGpuHost *host, double elapsed_seconds, float move_x,
                                          float move_y, float look_x, float look_y, int32_t jump,
                                          int32_t focused);
/* Queue one Play-mode interaction; consumed by the next fixed tick. */
VG_GPU_HOST_API int32_t vg_gpu_host_interact(VgGpuHost *host);
VG_GPU_HOST_API size_t vg_gpu_host_door_count(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_door_angle(const VgGpuHost *host, size_t index,
                                               float *out_angle);
VG_GPU_HOST_API size_t vg_gpu_host_animation_count(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_animation_height(const VgGpuHost *host, size_t index,
                                                     float *out_height);
/* Normalized viewport point, selecting mesh bounds by UUID. No hit returns 0. */
VG_GPU_HOST_API int32_t vg_gpu_host_pick(VgGpuHost *host, float u, float v, char *uuid,
                                         size_t uuid_capacity);
/* Hit-test without changing selection, used for additive viewport selection. */
VG_GPU_HOST_API int32_t vg_gpu_host_peek(VgGpuHost *host, float u, float v, char *uuid,
                                         size_t uuid_capacity);
/* Editor gizmo draws into the GPU scene target. Operation -1 hides it;
 * 0/1/2 are move/rotate/scale. Hit returns 0 or axis X/Y/Z as 1/2/3. */
VG_GPU_HOST_API int32_t vg_gpu_host_gizmo_config(VgGpuHost *host, int32_t operation, int32_t space,
                                                 int32_t pivot);
VG_GPU_HOST_API int32_t vg_gpu_host_gizmo_hit(VgGpuHost *host, float u, float v);
VG_GPU_HOST_API int32_t vg_gpu_host_gizmo_drag_direction(VgGpuHost *host, float u, float v,
                                                         int32_t axis, float *out_x, float *out_y);
VG_GPU_HOST_API uint64_t vg_gpu_host_document_revision(const VgGpuHost *host);
/* Wave 4 document editing. All mutations require Edit mode on the owner thread.
 * Quaternions use XYZW and transforms are local to the entity parent. */
VG_GPU_HOST_API int32_t vg_gpu_host_add_mesh(VgGpuHost *host, char *uuid, size_t uuid_capacity,
                                             char *error, size_t error_capacity);
/* Adds the fixed six-piece Atrium room shell as one undoable document batch.
 * The north opening is 1.6 m wide and 2.2 m high; output selects its left jamb. */
VG_GPU_HOST_API int32_t vg_gpu_host_add_room(VgGpuHost *host, char *uuid, size_t uuid_capacity,
                                             char *error, size_t error_capacity);
/* E04: bounded room recipe. Preview owns a temporary GPU world and never changes
 * the document or history. Coordinates are XY floor, Z up, in metres. */
VG_GPU_HOST_API int32_t vg_gpu_host_preview_room_recipe(VgGpuHost *host, const char *recipe_json,
                                                        char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_cancel_room_preview(VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_create_room_recipe(VgGpuHost *host, const char *recipe_json,
                                                       char *uuid, size_t uuid_capacity,
                                                       char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_update_room_recipe(VgGpuHost *host, const char *uuid,
                                                       const char *recipe_json, char *error,
                                                       size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_room_recipe_json(const VgGpuHost *host, const char *uuid,
                                                     char *json, size_t json_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_set_room_editor_view(VgGpuHost *host, int32_t grid,
                                                         int32_t ghost, float floor_z);
/* Optional stable label for an authored room piece. Returns 0 for other entities. */
VG_GPU_HOST_API int32_t vg_gpu_host_entity_label(const VgGpuHost *host, const char *uuid,
                                                 char *label, size_t label_capacity);
/* E03: document-owned model manifest. All strings are UTF-8; JSON queries return
 * 0 on failure or the written byte count (excluding NUL). Imported sources are
 * copied beside the level under assets/<stable-id>.glb/.gltf. */
VG_GPU_HOST_API int32_t vg_gpu_host_assets_json(const VgGpuHost *host, char *json, size_t capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_import_asset(VgGpuHost *host, const char *path, char *id,
                                                 size_t id_capacity, char *error,
                                                 size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_reimport_asset(VgGpuHost *host, const char *id,
                                                   const char *path, char *error,
                                                   size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_rename_asset(VgGpuHost *host, const char *id, const char *name,
                                                 char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_place_asset(VgGpuHost *host, const char *id,
                                                uint32_t node_index, char *uuid,
                                                size_t uuid_capacity, char *error,
                                                size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_preview_asset(VgGpuHost *host, const char *id, char *error,
                                                  size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_entity_editor_json(const VgGpuHost *host, const char *uuid,
                                                       char *json, size_t capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_selection_fields_json(const VgGpuHost *host, char *json,
                                                          size_t capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_set_selection_fields_json(VgGpuHost *host, const char *json,
                                                              char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_duplicate_selected(VgGpuHost *host, char *uuid,
                                                       size_t uuid_capacity, char *error,
                                                       size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_select(VgGpuHost *host, const char *uuid);
/* E02: stable UUID selection. Empty UUID with additive=0 clears it. */
VG_GPU_HOST_API int32_t vg_gpu_host_select_add(VgGpuHost *host, const char *uuid, int32_t additive,
                                               int32_t toggle);
VG_GPU_HOST_API size_t vg_gpu_host_selection_count(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_selection_at(const VgGpuHost *host, size_t index, char *uuid,
                                                 size_t uuid_capacity);
/* One gesture previews without publishing history. amount is absolute from the
 * start of the gesture; snap applies in metres, radians or scale factor.
 * op: 0 move, 1 rotate, 2 scale; space: 0 world, 1 local;
 * pivot: 0 median, 1 active, 2 individual; axis: 0 X, 1 Y, 2 Z. */
VG_GPU_HOST_API int32_t vg_gpu_host_begin_gesture(VgGpuHost *host, int32_t op, int32_t space,
                                                  int32_t pivot, int32_t axis, float snap,
                                                  char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_update_gesture(VgGpuHost *host, float amount, char *error,
                                                   size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_end_gesture(VgGpuHost *host, int32_t commit, char *error,
                                                size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_duplicate_selection(VgGpuHost *host, char *error,
                                                        size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_delete_selection(VgGpuHost *host, char *error,
                                                     size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_reparent_selection(VgGpuHost *host, const char *parent_uuid,
                                                       char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_selected_uuid(const VgGpuHost *host, char *uuid,
                                                  size_t uuid_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_selected_transform(const VgGpuHost *host, float position[3],
                                                       float rotation[4], float scale[3]);
VG_GPU_HOST_API int32_t vg_gpu_host_set_selected_transform(VgGpuHost *host, const float position[3],
                                                           const float rotation[4],
                                                           const float scale[3], char *error,
                                                           size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_save_level(VgGpuHost *host, const char *path, char *error,
                                               size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_reopen_level(VgGpuHost *host, const char *level_path,
                                                 const char *model_path, char *error,
                                                 size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_is_dirty(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_undo(VgGpuHost *host, char *error, size_t error_capacity);
VG_GPU_HOST_API int32_t vg_gpu_host_redo(VgGpuHost *host, char *error, size_t error_capacity);
VG_GPU_HOST_API size_t vg_gpu_host_entity_count(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_entity_at(const VgGpuHost *host, size_t index, char *uuid,
                                              size_t uuid_capacity);
VG_GPU_HOST_API uint64_t vg_gpu_host_readbacks(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_camera_position(const VgGpuHost *host, float *x, float *y,
                                                    float *z);
VG_GPU_HOST_API int32_t vg_gpu_host_set_camera_mode(VgGpuHost *host, int32_t mode);
VG_GPU_HOST_API int32_t vg_gpu_host_frame_selection(VgGpuHost *host);
enum {
    VG_GPU_PICK_MESH = 1u,
    VG_GPU_PICK_CAMERA = 2u,
    VG_GPU_PICK_LIGHT = 4u,
    VG_GPU_PICK_TRIGGER = 8u
};
VG_GPU_HOST_API int32_t vg_gpu_host_set_pick_mask(VgGpuHost *host, uint32_t mask);
VG_GPU_HOST_API int32_t vg_gpu_host_resize(VgGpuHost *host, unsigned int width,
                                           unsigned int height);
VG_GPU_HOST_API int32_t vg_gpu_host_size(VgGpuHost *host, unsigned int *width,
                                         unsigned int *height);
VG_GPU_HOST_API int32_t vg_gpu_host_capture(VgGpuHost *host, const char *path);
VG_GPU_HOST_API int32_t vg_gpu_host_focus(VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_has_focus(const VgGpuHost *host);
VG_GPU_HOST_API int32_t vg_gpu_host_destroy(VgGpuHost *host);

#endif /* VESTIGIO_GPU_HOST_H */
