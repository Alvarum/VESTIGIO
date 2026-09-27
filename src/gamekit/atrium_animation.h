#ifndef VESTIGIO_ATRIUM_ANIMATION_H
#define VESTIGIO_ATRIUM_ANIMATION_H

#include "content/document_runtime.h"

typedef struct VgAtriumAnimation VgAtriumAnimation;

/* Two runtime-only, non-colliding mesh instances. The authored level and its
 * edit instance never receive these transforms. */
VgResult vg_atrium_animation_create(VgContext *context, VgDocumentInstance *document,
                                    VgAtriumAnimation **out_animation);
void vg_atrium_animation_destroy(VgAtriumAnimation *animation);
VgResult vg_atrium_animation_step(VgAtriumAnimation *animation, double elapsed_seconds);
VgResult vg_atrium_animation_pose(const VgAtriumAnimation *animation, size_t index,
                                  VgTransform *out_pose);
size_t vg_atrium_animation_count(const VgAtriumAnimation *animation);

#endif
