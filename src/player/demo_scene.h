#ifndef VESTIGIO_PLAYER_DEMO_SCENE_H
#define VESTIGIO_PLAYER_DEMO_SCENE_H

#include "vestigio/vestigio.h"
#include "vestigio/spatial.h"

typedef struct VgDemoScene VgDemoScene;

/* Game code uses only the installed C SDK. The Player host owns the GPU/window. */
VgResult vg_demo_scene_create(VgContext *context, const void *model_data, uint64_t model_size,
                              float look_sensitivity, VgDemoScene **out_scene);
void vg_demo_scene_destroy(VgDemoScene *scene);
VgResult vg_demo_scene_submit_input(VgDemoScene *scene, const VgInputSample *sample);
VgResult vg_demo_scene_step(VgDemoScene *scene, double elapsed_seconds);
VgWorld vg_demo_scene_world(const VgDemoScene *scene);
VgResult vg_demo_scene_camera_position(const VgDemoScene *scene, VgVec3 *out_position);
const VgSpatialScene *vg_demo_scene_spatial(const VgDemoScene *scene);

#endif
