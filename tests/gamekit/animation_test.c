#include "vestigio/animation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                  \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

int main(void) {
    const VgRigidKeyframe keys[] = {
        {0.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
        {1.0f, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.70710678f, 0.70710678f}, {1.0f, 1.0f, 1.0f}},
        {2.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}};
    VgRigidClip clip = {keys, 3u, 2.0f, true};
    CHECK(vg_rigid_clip_validate(&clip) == VG_OK);
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    VgContext *context = NULL;
    VgWorld world = {0};
    VgEntity entities[2] = {{0}, {0}};
    CHECK(vg_context_create(&description, &context) == VG_OK);
    CHECK(vg_world_create(context, NULL, &world) == VG_OK);
    VgRigidInstance actors[2] = {0};
    for (size_t i = 0u; i < 2u; ++i) {
        CHECK(vg_entity_create(context, world, &entities[i]) == VG_OK);
        VgTransform bind = {
            {(float)i * 3.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
        CHECK(vg_entity_set_local_transform(context, entities[i], &bind) == VG_OK);
        CHECK(vg_rigid_instance_bind(context, entities[i], &clip, (double)i, &actors[i]) == VG_OK);
    }
    VgTransform first, second;
    CHECK(vg_rigid_instance_pose(&actors[0], &first) == VG_OK);
    CHECK(vg_rigid_instance_pose(&actors[1], &second) == VG_OK);
    CHECK(fabsf(first.position.z - 1.0f) < 0.001f);
    CHECK(fabsf(second.position.z - 2.0f) < 0.001f);
    CHECK(vg_rigid_instance_step(context, &actors[0], 0.5) == VG_OK);
    CHECK(vg_rigid_instance_step(context, &actors[1], 0.5) == VG_OK);
    CHECK(vg_entity_get_local_transform(context, entities[0], &first) == VG_OK);
    CHECK(vg_entity_get_local_transform(context, entities[1], &second) == VG_OK);
    CHECK(fabsf(first.position.z - 1.5f) < 0.001f);
    CHECK(fabsf(second.position.z - 1.5f) < 0.001f);
    CHECK(fabsf(first.rotation.z - second.rotation.z) > 0.2f);
    CHECK(vg_rigid_instance_step(context, &actors[0], 1.5) == VG_OK);
    CHECK(vg_rigid_instance_pose(&actors[0], &first) == VG_OK);
    CHECK(fabsf(first.position.z - 1.0f) < 0.001f);
    actors[0].playing = false;
    CHECK(vg_rigid_instance_step(context, &actors[0], 0.5) == VG_OK);
    CHECK(vg_rigid_instance_pose(&actors[0], &first) == VG_OK);
    CHECK(fabsf(first.position.z - 1.0f) < 0.001f);
    VgRigidClip once = {keys, 3u, 2.0f, false};
    actors[1].clip = &once;
    actors[1].seconds = 1.5;
    actors[1].playing = true;
    CHECK(vg_rigid_instance_step(context, &actors[1], 1.0) == VG_OK);
    CHECK(!actors[1].playing && actors[1].seconds == 2.0);
    CHECK(vg_entity_get_local_transform(context, entities[1], &second) == VG_OK);
    CHECK(fabsf(second.position.z - 1.0f) < 0.001f);
    VgRigidClip invalid = clip;
    invalid.duration_seconds = 3.0f;
    CHECK(vg_rigid_clip_validate(&invalid) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(vg_rigid_instance_step(context, &actors[1], NAN) == VG_ERROR_INVALID_ARGUMENT);
    actors[1].seconds = 1.5;
    actors[1].playing = true;
    CHECK(vg_entity_destroy(context, entities[1]) == VG_OK);
    CHECK(vg_rigid_instance_step(context, &actors[1], 1.0) != VG_OK);
    CHECK(actors[1].seconds == 1.5 && actors[1].playing);
    CHECK(vg_entity_destroy(context, entities[0]) == VG_OK);
    CHECK(vg_world_destroy(context, world) == VG_OK);
    vg_context_destroy(context);
    (void)printf("PASS rigid animation: shared clip, phases, loop, pause, rollback\n");
    return 0;
}
