#ifndef VESTIGIO_ANIMATION_H
#define VESTIGIO_ANIMATION_H

#include "vestigio/vestigio.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental rigid clip: local offsets are evaluated against the authored
 * bind transform. Keyframes and their clip outlive each bound instance. */
typedef struct VgRigidKeyframe {
    float seconds;
    VgVec3 translation;
    VgQuat rotation;
    VgVec3 scale;
} VgRigidKeyframe;

typedef struct VgRigidClip {
    const VgRigidKeyframe *keys;
    size_t key_count;
    float duration_seconds;
    bool loop;
} VgRigidClip;

typedef struct VgRigidInstance {
    VgEntity entity;
    VgTransform bind_transform;
    const VgRigidClip *clip;
    double seconds;
    float speed;
    bool playing;
} VgRigidInstance;

VG_API VgResult vg_rigid_clip_validate(const VgRigidClip *clip);
VG_API VgResult vg_rigid_instance_bind(VgContext *context, VgEntity entity, const VgRigidClip *clip,
                                       double start_seconds, VgRigidInstance *out_instance);
VG_API VgResult vg_rigid_instance_step(VgContext *context, VgRigidInstance *instance,
                                       double elapsed_seconds);
VG_API VgResult vg_rigid_instance_pose(const VgRigidInstance *instance, VgTransform *out_transform);

#ifdef __cplusplus
}
#endif

#endif
