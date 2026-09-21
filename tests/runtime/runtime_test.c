#include "vestigio/vestigio.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression);                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct TestAllocator {
    size_t calls;
    size_t outstanding;
    size_t fail_call;
} TestAllocator;

static void *test_allocate(void *user, size_t size) {
    TestAllocator *allocator = user;
    ++allocator->calls;
    if (allocator->fail_call != 0u && allocator->calls == allocator->fail_call)
        return NULL;
    void *allocation = malloc(size);
    if (allocation != NULL)
        ++allocator->outstanding;
    return allocation;
}

static void test_deallocate(void *user, void *allocation) {
    TestAllocator *allocator = user;
    if (allocation != NULL) {
        if (allocator->outstanding == 0u) {
            fputs("allocator accounting underflow\n", stderr);
            abort();
        }
        --allocator->outstanding;
        free(allocation);
    }
}

static VgTransform transform(float x, float y, float z) {
    VgTransform value = {{x, y, z}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
    return value;
}

static int close_enough(float left, float right) {
    return fabsf(left - right) < 1.0e-4f;
}

static int test_context_world_and_handles(void) {
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    description.max_worlds = 2u;
    VgContext *first = NULL;
    VgContext *second = NULL;
    CHECK(vg_context_create(&description, &first) == VG_OK);
    CHECK(vg_context_create(&description, &second) == VG_OK);

    VgWorldDesc world_description = {sizeof(world_description), 2u, 8u};
    VgWorld world = {0};
    VgWorld other_world = {0};
    VgWorld second_context_world = {0};
    CHECK(vg_world_create(first, &world_description, &world) == VG_OK);
    CHECK(vg_world_create(first, &world_description, &other_world) == VG_OK);
    CHECK(vg_world_create(second, &world_description, &second_context_world) == VG_OK);

    VgEntity parent = {0};
    VgEntity child = {0};
    VgEntity foreign = {0};
    CHECK(vg_entity_create(first, world, &parent) == VG_OK);
    CHECK(vg_entity_create(first, world, &child) == VG_OK);
    CHECK(vg_entity_create(first, other_world, &foreign) == VG_OK);
    VgTransform output = transform(99.0f, 99.0f, 99.0f);
    CHECK(vg_entity_get_local_transform(second, child, &output) == VG_ERROR_WRONG_CONTEXT);
    CHECK(close_enough(output.position.x, 99.0f));
    CHECK(vg_entity_get_local_transform(first, (VgEntity){world.value}, &output) ==
          VG_ERROR_WRONG_TYPE);
    CHECK(vg_entity_set_parent(first, child, foreign, VG_REPARENT_KEEP_LOCAL) ==
          VG_ERROR_WRONG_WORLD);
    CHECK(vg_entity_get_parent(first, child, &(VgEntity){0}) == VG_OK);

    VgEntity stale = child;
    CHECK(vg_entity_destroy(first, child) == VG_OK);
    CHECK(vg_entity_get_local_transform(first, stale, &output) == VG_ERROR_INVALID_HANDLE);
    CHECK(vg_entity_create(first, world, &child) == VG_OK);
    CHECK(child.value != stale.value);

    VgWorld stale_world = world;
    CHECK(vg_world_destroy(first, world) == VG_OK);
    CHECK(vg_world_reserve_entities(first, stale_world, 4u) == VG_ERROR_INVALID_HANDLE);
    CHECK(vg_world_create(first, &world_description, &world) == VG_OK);
    CHECK(world.value != stale_world.value);

    VgWorld overflow_world = {UINT64_C(0xD00D)};
    CHECK(vg_world_create(first, &world_description, &overflow_world) == VG_ERROR_CAPACITY);
    CHECK(overflow_world.value == UINT64_C(0xD00D));

    CHECK(vg_world_destroy(first, world) == VG_OK);
    CHECK(vg_world_destroy(first, other_world) == VG_OK);
    CHECK(vg_world_destroy(second, second_context_world) == VG_OK);
    vg_context_destroy(second);
    vg_context_destroy(first);
    return 0;
}

static int test_transform_hierarchy(void) {
    VgContextDesc context_description = {0};
    context_description.struct_size = sizeof(context_description);
    context_description.api_version = VG_API_VERSION;
    VgContext *context = NULL;
    CHECK(vg_context_create(&context_description, &context) == VG_OK);
    VgWorld world = {0};
    CHECK(vg_world_create(context, NULL, &world) == VG_OK);
    VgEntity parent = {0}, child = {0}, grandchild = {0};
    CHECK(vg_entity_create(context, world, &parent) == VG_OK);
    CHECK(vg_entity_create(context, world, &child) == VG_OK);
    CHECK(vg_entity_create(context, world, &grandchild) == VG_OK);

    VgTransform parent_transform = transform(10.0f, 0.0f, 0.0f);
    VgTransform child_transform = transform(2.0f, 3.0f, 4.0f);
    CHECK(vg_entity_set_local_transform(context, parent, &parent_transform) == VG_OK);
    CHECK(vg_entity_set_local_transform(context, child, &child_transform) == VG_OK);
    CHECK(vg_entity_set_parent(context, child, parent, VG_REPARENT_KEEP_LOCAL) == VG_OK);
    VgTransform world_transform;
    CHECK(vg_entity_get_world_transform(context, child, &world_transform) == VG_OK);
    CHECK(close_enough(world_transform.position.x, 12.0f));
    CHECK(close_enough(world_transform.position.y, 3.0f));

    VgTransform desired_world = transform(20.0f, -2.0f, 1.0f);
    CHECK(vg_entity_set_world_transform(context, child, &desired_world) == VG_OK);
    CHECK(vg_entity_get_local_transform(context, child, &child_transform) == VG_OK);
    CHECK(close_enough(child_transform.position.x, 10.0f));
    CHECK(vg_entity_set_parent(context, child, (VgEntity){0}, VG_REPARENT_KEEP_WORLD) == VG_OK);
    CHECK(vg_entity_get_world_transform(context, child, &world_transform) == VG_OK);
    CHECK(close_enough(world_transform.position.x, 20.0f));
    CHECK(close_enough(world_transform.position.y, -2.0f));

    VgEntity scaled_parent = {0}, preserved_child = {0};
    CHECK(vg_entity_create(context, world, &scaled_parent) == VG_OK);
    CHECK(vg_entity_create(context, world, &preserved_child) == VG_OK);
    VgTransform scaled_rotated = transform(3.0f, -1.0f, 0.5f);
    scaled_rotated.rotation = (VgQuat){0.0f, 0.0f, 0.70710678118f, 0.70710678118f};
    scaled_rotated.scale = (VgVec3){2.0f, 2.0f, 2.0f};
    VgTransform preserved_world = transform(4.0f, 2.0f, 1.0f);
    CHECK(vg_entity_set_local_transform(context, scaled_parent, &scaled_rotated) == VG_OK);
    CHECK(vg_entity_set_local_transform(context, preserved_child, &preserved_world) == VG_OK);
    CHECK(vg_entity_set_parent(context, preserved_child, scaled_parent, VG_REPARENT_KEEP_WORLD) ==
          VG_OK);
    CHECK(vg_entity_get_world_transform(context, preserved_child, &world_transform) == VG_OK);
    CHECK(close_enough(world_transform.position.x, preserved_world.position.x));
    CHECK(close_enough(world_transform.position.y, preserved_world.position.y));
    CHECK(close_enough(world_transform.position.z, preserved_world.position.z));

    CHECK(vg_entity_set_parent(context, child, parent, VG_REPARENT_KEEP_WORLD) == VG_OK);
    CHECK(vg_entity_set_parent(context, grandchild, child, VG_REPARENT_KEEP_LOCAL) == VG_OK);
    VgEntity prior_parent = {0};
    CHECK(vg_entity_get_parent(context, parent, &prior_parent) == VG_OK);
    CHECK(prior_parent.value == 0u);
    CHECK(vg_entity_set_parent(context, parent, grandchild, VG_REPARENT_KEEP_LOCAL) ==
          VG_ERROR_CONFLICT);
    CHECK(vg_entity_get_parent(context, parent, &prior_parent) == VG_OK);
    CHECK(prior_parent.value == 0u);

    VgTransform prior_local;
    CHECK(vg_entity_get_local_transform(context, child, &prior_local) == VG_OK);
    VgTransform invalid = prior_local;
    invalid.position.x = NAN;
    CHECK(vg_entity_set_local_transform(context, child, &invalid) == VG_ERROR_INVALID_ARGUMENT);
    invalid = prior_local;
    invalid.scale.z = 0.0f;
    CHECK(vg_entity_set_local_transform(context, child, &invalid) == VG_ERROR_INVALID_ARGUMENT);
    CHECK(vg_entity_get_local_transform(context, child, &child_transform) == VG_OK);
    CHECK(memcmp(&prior_local, &child_transform, sizeof(prior_local)) == 0);

    VgEntity shear_child = {0};
    CHECK(vg_entity_create(context, world, &shear_child) == VG_OK);
    VgTransform non_uniform = transform(0.0f, 0.0f, 0.0f);
    non_uniform.scale = (VgVec3){2.0f, 1.0f, 1.0f};
    CHECK(vg_entity_set_local_transform(context, parent, &non_uniform) == VG_OK);
    VgTransform rotated = transform(0.0f, 0.0f, 0.0f);
    rotated.rotation = (VgQuat){0.0f, 0.0f, 0.38268343236f, 0.92387953251f};
    CHECK(vg_entity_set_local_transform(context, shear_child, &rotated) == VG_OK);
    CHECK(vg_entity_set_parent(context, shear_child, parent, VG_REPARENT_KEEP_LOCAL) ==
          VG_ERROR_UNSUPPORTED);
    CHECK(vg_entity_get_parent(context, shear_child, &prior_parent) == VG_OK);
    CHECK(prior_parent.value == 0u);

    VgTransform uniform = transform(0.0f, 0.0f, 0.0f);
    CHECK(vg_entity_set_local_transform(context, parent, &uniform) == VG_OK);
    CHECK(vg_entity_set_parent(context, shear_child, parent, VG_REPARENT_KEEP_LOCAL) == VG_OK);
    CHECK(vg_entity_set_local_transform(context, parent, &non_uniform) == VG_ERROR_UNSUPPORTED);
    CHECK(vg_entity_get_local_transform(context, parent, &parent_transform) == VG_OK);
    CHECK(close_enough(parent_transform.scale.x, 1.0f));

    CHECK(vg_entity_get_world_transform(context, child, &world_transform) == VG_OK);
    CHECK(vg_entity_destroy(context, parent) == VG_OK);
    CHECK(vg_entity_get_parent(context, child, &prior_parent) == VG_OK);
    CHECK(prior_parent.value == 0u);
    VgTransform after_destroy;
    CHECK(vg_entity_get_world_transform(context, child, &after_destroy) == VG_OK);
    CHECK(close_enough(after_destroy.position.x, world_transform.position.x));

    vg_context_destroy(context);
    return 0;
}

static int test_iteration_and_capacity(void) {
    VgContextDesc context_description = {0};
    context_description.struct_size = sizeof(context_description);
    context_description.api_version = VG_API_VERSION;
    VgContext *context = NULL;
    CHECK(vg_context_create(&context_description, &context) == VG_OK);
    VgWorldDesc description = {sizeof(description), 1u, 2u};
    VgWorld world = {0};
    CHECK(vg_world_create(context, &description, &world) == VG_OK);
    VgEntity first = {0};
    CHECK(vg_entity_create(context, world, &first) == VG_OK);
    uint32_t count = 0u;
    CHECK(vg_world_begin_iteration(context, world, &count) == VG_OK);
    CHECK(count == 1u);
    CHECK(vg_world_begin_iteration(context, world, &count) == VG_ERROR_REENTRANT);
    VgEntity created = {0};
    CHECK(vg_entity_create(context, world, &created) == VG_OK);
    CHECK(vg_entity_destroy(context, first) == VG_OK);
    VgEntity iterated = {0};
    CHECK(vg_world_entity_at(context, world, 0u, &iterated) == VG_OK);
    CHECK(iterated.value == first.value);
    CHECK(vg_world_entity_at(context, world, 1u, &iterated) == VG_ERROR_NOT_FOUND);
    CHECK(vg_world_destroy(context, world) == VG_ERROR_REENTRANT);
    CHECK(vg_world_end_iteration(context, world) == VG_OK);
    VgTransform output;
    CHECK(vg_entity_get_local_transform(context, first, &output) == VG_ERROR_INVALID_HANDLE);
    CHECK(vg_entity_get_local_transform(context, created, &output) == VG_OK);
    VgEntity second = {0};
    CHECK(vg_entity_create(context, world, &second) == VG_OK);
    VgEntity overflow = {UINT64_C(0xA5A5)};
    CHECK(vg_entity_create(context, world, &overflow) == VG_ERROR_CAPACITY);
    CHECK(overflow.value == UINT64_C(0xA5A5));
    CHECK(vg_world_reserve_entities(context, world, 3u) == VG_ERROR_CAPACITY);
    CHECK(vg_world_end_iteration(context, world) == VG_ERROR_REENTRANT);
    vg_context_destroy(context);
    return 0;
}

static int test_allocation_failures_and_cleanup(void) {
    TestAllocator partial_allocator = {0};
    VgContextDesc partial = {0};
    partial.struct_size =
        (uint32_t)(offsetof(VgContextDesc, deallocate) + sizeof(partial.deallocate));
    partial.api_version = VG_API_VERSION;
    partial.allocator_user = &partial_allocator;
    partial.allocate = test_allocate;
    partial.deallocate = test_deallocate;
    VgContext *partial_context = NULL;
    CHECK(vg_context_create(&partial, &partial_context) == VG_OK);
    vg_context_destroy(partial_context);
    CHECK(partial_allocator.outstanding == 0u);

    TestAllocator allocator = {0};
    VgContextDesc description = {0};
    description.struct_size = sizeof(description);
    description.api_version = VG_API_VERSION;
    description.allocator_user = &allocator;
    description.allocate = test_allocate;
    description.deallocate = test_deallocate;
    description.max_worlds = 1u;

    allocator.fail_call = 2u;
    VgContext *context = (VgContext *)(uintptr_t)1u;
    CHECK(vg_context_create(&description, &context) == VG_ERROR_OUT_OF_MEMORY);
    CHECK(context == (VgContext *)(uintptr_t)1u);
    CHECK(allocator.outstanding == 0u);

    allocator.fail_call = 0u;
    CHECK(vg_context_create(&description, &context) == VG_OK);
    CHECK(allocator.outstanding == 2u);
    VgWorldDesc world_description = {sizeof(world_description), 1u, 2u};
    VgWorld world = {UINT64_C(0xBEEF)};
    allocator.fail_call = allocator.calls + 2u;
    CHECK(vg_world_create(context, &world_description, &world) == VG_ERROR_OUT_OF_MEMORY);
    CHECK(world.value == UINT64_C(0xBEEF));
    CHECK(allocator.outstanding == 2u);

    allocator.fail_call = 0u;
    CHECK(vg_world_create(context, &world_description, &world) == VG_OK);
    VgEntity first = {0};
    CHECK(vg_entity_create(context, world, &first) == VG_OK);
    allocator.fail_call = allocator.calls + 1u;
    VgEntity failed = {UINT64_C(0xCAFE)};
    CHECK(vg_entity_create(context, world, &failed) == VG_ERROR_OUT_OF_MEMORY);
    CHECK(failed.value == UINT64_C(0xCAFE));
    VgTransform identity;
    CHECK(vg_entity_get_local_transform(context, first, &identity) == VG_OK);
    allocator.fail_call = 0u;
    CHECK(vg_entity_create(context, world, &failed) == VG_OK);
    CHECK(vg_world_destroy(context, world) == VG_OK);

    for (uint32_t iteration = 0u; iteration < 100u; ++iteration) {
        CHECK(vg_world_create(context, &world_description, &world) == VG_OK);
        CHECK(vg_entity_create(context, world, &first) == VG_OK);
        CHECK(vg_world_destroy(context, world) == VG_OK);
    }
    vg_context_destroy(context);
    CHECK(allocator.outstanding == 0u);
    return 0;
}

static int test_generation_retirement(void) {
    VgContextDesc context_description = {0};
    context_description.struct_size = sizeof(context_description);
    context_description.api_version = VG_API_VERSION;
    VgContext *context = NULL;
    CHECK(vg_context_create(&context_description, &context) == VG_OK);
    VgWorldDesc world_description = {sizeof(world_description), 1u, 1u};
    VgWorld world = {0};
    CHECK(vg_world_create(context, &world_description, &world) == VG_OK);
    VgEntity first = {0};
    for (uint32_t generation = 1u; generation <= 4095u; ++generation) {
        VgEntity entity = {0};
        CHECK(vg_entity_create(context, world, &entity) == VG_OK);
        if (generation == 1u)
            first = entity;
        CHECK(vg_entity_destroy(context, entity) == VG_OK);
    }
    VgEntity exhausted = {UINT64_C(0xBADC0DE)};
    CHECK(vg_entity_create(context, world, &exhausted) == VG_ERROR_CAPACITY);
    CHECK(exhausted.value == UINT64_C(0xBADC0DE));
    VgTransform output;
    CHECK(vg_entity_get_local_transform(context, first, &output) == VG_ERROR_INVALID_HANDLE);
    vg_context_destroy(context);
    return 0;
}

int main(void) {
    VgVersion version = {sizeof(version), 0u, 0u, 0u, 0u};
    CHECK(vg_get_version(&version) == VG_OK);
    CHECK(version.api_version == VG_API_VERSION);
    CHECK(test_context_world_and_handles() == 0);
    CHECK(test_transform_hierarchy() == 0);
    CHECK(test_iteration_and_capacity() == 0);
    CHECK(test_allocation_failures_and_cleanup() == 0);
    CHECK(test_generation_retirement() == 0);
    puts("PASS vestigio runtime lifecycle, handles, hierarchy, iteration, capacity and OOM");
    return 0;
}
