#ifndef VESTIGIO_PLAYER_DEMO_SCENE_H
#define VESTIGIO_PLAYER_DEMO_SCENE_H

#include "content/document_runtime.h"
#include "vestigio/spatial.h"
#include "vestigio/vestigio.h"

typedef struct VgDemoScene VgDemoScene;

/* Game code uses only the installed C SDK. The Player host owns the GPU/window. */
VgResult vg_demo_scene_create(VgContext *context, const char *level_path, const void *model_data,
                              uint64_t model_size, float look_sensitivity, VgDemoScene **out_scene);
void vg_demo_scene_destroy(VgDemoScene *scene);
VgResult vg_demo_scene_submit_input(VgDemoScene *scene, const VgInputSample *sample);
VgResult vg_demo_scene_step(VgDemoScene *scene, double elapsed_seconds);
VgWorld vg_demo_scene_world(const VgDemoScene *scene);
VgResult vg_demo_scene_camera_position(const VgDemoScene *scene, VgVec3 *out_position);
VgResult vg_demo_scene_camera_transform(const VgDemoScene *scene, VgTransform *out_transform);
bool vg_demo_scene_take_door_event(VgDemoScene *scene, VgVec3 *out_position);
const VgSpatialScene *vg_demo_scene_spatial(const VgDemoScene *scene);
const VgDocumentInstance *vg_demo_scene_document_instance(const VgDemoScene *scene);
/* NULL when the centered interaction ray does not hit an authored door. */
const char *vg_demo_scene_door_hint(const VgDemoScene *scene);
size_t vg_demo_scene_door_count(const VgDemoScene *scene);
VgResult vg_demo_scene_toggle_door(VgDemoScene *scene, size_t index);
float vg_demo_scene_door_angle(const VgDemoScene *scene, size_t index);
VgResult vg_demo_scene_door_pose(const VgDemoScene *scene, size_t index, VgTransform *out_panel,
                                 VgTransform *out_collider);
/* Hidden smoke harness: moves only its runtime player near the authored panel. */
VgResult vg_demo_scene_prepare_door_smoke(VgDemoScene *scene);
size_t vg_demo_scene_animation_count(const VgDemoScene *scene);
bool vg_demo_scene_is_example(const VgDemoScene *scene);
VgResult vg_demo_scene_animation_pose(const VgDemoScene *scene, size_t index,
                                      VgTransform *out_pose);

#endif
