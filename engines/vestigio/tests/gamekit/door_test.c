#include "vestigio/door.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); \
} } while (0)

typedef struct DoorFixture {
    VgContext *context;
    VgWorld world;
    VgSpatialScene *spatial;
    VgEntity parent;
    VgEntity hinge;
    VgEntity panel;
    VgSpatialCollider collider;
    VgDoor *door;
} DoorFixture;

static VgTransform identity(void) {
    VgTransform result = {0};
    result.rotation.w = 1.0f;
    result.scale = (VgVec3){1.0f, 1.0f, 1.0f};
    return result;
}

static DoorFixture fixture_create(float open_angle, bool rotated_parent) {
    DoorFixture fixture = {0};
    VgContextDesc context = {0};
    context.struct_size = sizeof(context);
    context.api_version = VG_API_VERSION;
    CHECK(vg_context_create(&context, &fixture.context) == VG_OK);
    CHECK(vg_world_create(fixture.context, NULL, &fixture.world) == VG_OK);
    CHECK(vg_spatial_scene_create(NULL, &fixture.spatial) == VG_OK);
    if (rotated_parent) {
        CHECK(vg_entity_create(fixture.context, fixture.world, &fixture.parent) == VG_OK);
        VgTransform parent = identity();
        parent.position.x = 3.0f;
        parent.rotation = (VgQuat){0.0f, 0.0f, 0.70710678f, 0.70710678f};
        CHECK(vg_entity_set_local_transform(fixture.context, fixture.parent, &parent) == VG_OK);
    }
    CHECK(vg_entity_create(fixture.context, fixture.world, &fixture.hinge) == VG_OK);
    CHECK(vg_entity_create(fixture.context, fixture.world, &fixture.panel) == VG_OK);
    if (rotated_parent)
        CHECK(vg_entity_set_parent(fixture.context, fixture.hinge, fixture.parent,
                                    VG_REPARENT_KEEP_LOCAL) == VG_OK);
    CHECK(vg_entity_set_parent(fixture.context, fixture.panel, fixture.hinge,
                                VG_REPARENT_KEEP_LOCAL) == VG_OK);
    VgTransform panel = identity();
    panel.position = (VgVec3){0.6f, 0.0f, 0.95f};
    panel.scale = (VgVec3){1.2f, 0.14f, 1.9f};
    CHECK(vg_entity_set_local_transform(fixture.context, fixture.panel, &panel) == VG_OK);
    VgSpatialColliderDesc collider = {0};
    collider.entity = fixture.panel;
    collider.layer_mask = 1u;
    collider.transform = identity();
    collider.shape_type = VG_SPATIAL_SHAPE_BOX;
    collider.enabled = true;
    collider.shape.box.half_extents = (VgVec3){0.5f, 0.5f, 0.5f};
    CHECK(vg_spatial_collider_create(fixture.spatial, &collider,
                                      &fixture.collider) == VG_OK);
    VgDoorDesc desc = {0};
    desc.struct_size = sizeof(desc);
    desc.api_version = VG_API_VERSION;
    desc.context = fixture.context;
    desc.spatial = fixture.spatial;
    desc.hinge = fixture.hinge;
    desc.panel = fixture.panel;
    desc.collider = fixture.collider;
    desc.collider_description = collider;
    desc.open_angle_radians = open_angle;
    desc.angular_speed_radians = 2.0f;
    CHECK(vg_door_create(&desc, &fixture.door) == VG_OK);
    CHECK(vg_door_state(fixture.door) == VG_DOOR_CLOSED);
    return fixture;
}

static void fixture_destroy(DoorFixture *fixture) {
    vg_door_destroy(fixture->door);
    CHECK(vg_spatial_collider_destroy(fixture->spatial, fixture->collider) == VG_OK);
    vg_spatial_scene_destroy(fixture->spatial);
    CHECK(vg_world_destroy(fixture->context, fixture->world) == VG_OK);
    vg_context_destroy(fixture->context);
}

static bool ray_hits(DoorFixture *fixture, VgVec3 origin, VgVec3 direction) {
    VgSpatialRayQuery query = {origin, direction, 3.0f, 1u, {0}};
    bool found = false;
    VgSpatialHit hit = {0};
    CHECK(vg_spatial_raycast(fixture->spatial, &query, &found, &hit) == VG_OK);
    if (found)
        CHECK(hit.entity.value == fixture->panel.value &&
              hit.collider.value == fixture->collider.value);
    return found;
}

static void step_until(DoorFixture *fixture, VgDoorState target,
                       const VgControllerConfig *config,
                       const VgControllerState *player) {
    for (unsigned int i = 0u; i < 120u && vg_door_state(fixture->door) != target; ++i)
        CHECK(vg_door_step(fixture->door, 1.0f / 60.0f, config, player) == VG_OK);
    CHECK(vg_door_state(fixture->door) == target);
}

static void test_door_pose_and_collider(void) {
    DoorFixture fixture = fixture_create(1.5707963f, false);
    CHECK(ray_hits(&fixture, (VgVec3){0.6f, -1.0f, 0.95f},
                   (VgVec3){0.0f, 1.0f, 0.0f}));
    CHECK(vg_door_open(fixture.door) == VG_OK);
    step_until(&fixture, VG_DOOR_OPEN, NULL, NULL);
    CHECK(fabsf(vg_door_angle(fixture.door) - 1.5707963f) < 0.001f);
    VgTransform panel;
    CHECK(vg_entity_get_world_transform(fixture.context, fixture.panel, &panel) == VG_OK);
    CHECK(fabsf(panel.position.x) < 0.001f && fabsf(panel.position.y - 0.6f) < 0.001f);
    CHECK(!ray_hits(&fixture, (VgVec3){0.6f, -1.0f, 0.95f},
                    (VgVec3){0.0f, 1.0f, 0.0f}));
    CHECK(ray_hits(&fixture, (VgVec3){-1.0f, 0.6f, 0.95f},
                   (VgVec3){1.0f, 0.0f, 0.0f}));
    VgSpatialSphereSweep movement = {{-1.0f, 0.6f, 0.95f}, 0.28f,
                                      {1.0f, 0.0f, 0.0f}, 2.0f, 1u, {0}};
    bool blocked = false;
    VgSpatialHit contact = {0};
    CHECK(vg_spatial_sweep_sphere(fixture.spatial, &movement, &blocked,
                                   &contact) == VG_OK);
    CHECK(blocked && contact.collider.value == fixture.collider.value);
    CHECK(vg_door_close(fixture.door) == VG_OK);
    step_until(&fixture, VG_DOOR_CLOSED, NULL, NULL);
    CHECK(ray_hits(&fixture, (VgVec3){0.6f, -1.0f, 0.95f},
                   (VgVec3){0.0f, 1.0f, 0.0f}));
    fixture_destroy(&fixture);
}

static void test_close_obstruction_and_retry(void) {
    DoorFixture fixture = fixture_create(1.5707963f, false);
    CHECK(vg_door_toggle(fixture.door) == VG_OK);
    step_until(&fixture, VG_DOOR_OPEN, NULL, NULL);
    VgControllerConfig config = vg_controller_default_config();
    VgControllerState player = {0};
    player.feet = (VgVec3){0.6f, 0.0f, 0.0f};
    CHECK(vg_door_close(fixture.door) == VG_OK);
    step_until(&fixture, VG_DOOR_BLOCKED, &config, &player);
    CHECK(vg_door_angle(fixture.door) > 0.05f);
    VgTransform panel;
    CHECK(vg_entity_get_world_transform(fixture.context, fixture.panel, &panel) == VG_OK);
    VgVec3 ray_origin = {panel.position.x, panel.position.y - 1.0f, 0.95f};
    CHECK(ray_hits(&fixture, ray_origin, (VgVec3){0.0f, 1.0f, 0.0f}));
    player.feet = (VgVec3){4.0f, 4.0f, 0.0f};
    step_until(&fixture, VG_DOOR_CLOSED, &config, &player);
    CHECK(vg_door_angle(fixture.door) < 0.001f);
    fixture_destroy(&fixture);
}

static void test_negative_swing_and_rotated_parent(void) {
    DoorFixture negative = fixture_create(-1.5707963f, false);
    CHECK(vg_door_open(negative.door) == VG_OK);
    step_until(&negative, VG_DOOR_OPEN, NULL, NULL);
    VgTransform panel;
    CHECK(vg_entity_get_world_transform(negative.context, negative.panel, &panel) == VG_OK);
    CHECK(fabsf(panel.position.y + 0.6f) < 0.001f);
    CHECK(ray_hits(&negative, (VgVec3){-1.0f, -0.6f, 0.95f},
                   (VgVec3){1.0f, 0.0f, 0.0f}));
    fixture_destroy(&negative);

    DoorFixture rotated = fixture_create(1.5707963f, true);
    CHECK(vg_door_open(rotated.door) == VG_OK);
    step_until(&rotated, VG_DOOR_OPEN, NULL, NULL);
    CHECK(vg_entity_get_world_transform(rotated.context, rotated.panel, &panel) == VG_OK);
    CHECK(fabsf(panel.position.x - 2.4f) < 0.001f &&
          fabsf(panel.position.y) < 0.001f);
    CHECK(ray_hits(&rotated, (VgVec3){2.4f, -1.0f, 0.95f},
                   (VgVec3){0.0f, 1.0f, 0.0f}));
    fixture_destroy(&rotated);
}

int main(void) {
    test_door_pose_and_collider();
    test_close_obstruction_and_retry();
    test_negative_swing_and_rotated_parent();
    (void)puts("PASS door hinge/collider/player obstruction");
    return 0;
}
