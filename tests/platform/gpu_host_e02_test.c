#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "content/json.h"
#include "platform/gpu_host.h"
#include "tooling/tool_api.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #expression,        \
                          error);                                                                  \
            failed = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static int selected_position(VgGpuHost *host, const char *id, float result[3]) {
    float rotation[4], scale[3];
    return vg_gpu_host_select(host, id) &&
           vg_gpu_host_selected_transform(host, result, rotation, scale);
}

static void rotate_quaternion_z(double value[4], double radians, int world_space) {
    double sine = sin(radians * 0.5), cosine = cos(radians * 0.5);
    double original[4];
    memcpy(original, value, sizeof(original));
    value[0] = cosine * original[0] + (world_space ? -1.0 : 1.0) * sine * original[1];
    value[1] = cosine * original[1] + (world_space ? 1.0 : -1.0) * sine * original[0];
    value[2] = cosine * original[2] + sine * original[3];
    value[3] = cosine * original[3] - sine * original[2];
}

static int compare_with_tool(VgGpuHost *host, const char *path, const VgDocument *direct,
                             const char *const *ids, size_t count, char *error,
                             size_t error_capacity) {
    if (!vg_gpu_host_save_level(host, path, error, error_capacity))
        return 0;
    VgDocument *after = NULL;
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_document_open_file(path, &after, &diagnostic))
        return 0;
    int same = 1;
    for (size_t entity = 0u; entity < count; ++entity) {
        VgDocumentTransform tool, ui;
        if (!vg_document_entity_transform(direct, ids[entity], &tool, &diagnostic) ||
            !vg_document_entity_transform(after, ids[entity], &ui, &diagnostic)) {
            same = 0;
            break;
        }
        for (size_t axis = 0u; axis < 3u; ++axis)
            same &= fabs(tool.position[axis] - ui.position[axis]) < 0.0001 &&
                    fabs(tool.scale[axis] - ui.scale[axis]) < 0.0001;
        for (size_t axis = 0u; axis < 4u; ++axis)
            same &= fabs(tool.rotation[axis] - ui.rotation[axis]) < 0.0001;
    }
    vg_document_destroy(after);
    return same;
}

static int sample_drag_directions(VgGpuHost *host, float direction[3][2], int seen[3]) {
    for (int row = 0; row < 45; ++row) {
        for (int column = 0; column < 80; ++column) {
            float u = ((float)column + 0.5f) / 80.0f;
            float v = ((float)row + 0.5f) / 45.0f;
            int axis = vg_gpu_host_gizmo_hit(host, u, v) - 1;
            if (axis < 0 || axis > 2 || seen[axis])
                continue;
            if (!vg_gpu_host_gizmo_drag_direction(host, u, v, axis, &direction[axis][0],
                                                  &direction[axis][1]))
                continue;
            float length = hypotf(direction[axis][0], direction[axis][1]);
            if (!isfinite(length) || fabsf(length - 1.0f) > 0.001f)
                return 0;
            seen[axis] = 1;
        }
    }
    return seen[0] || seen[1] || seen[2];
}

static int child_has_parent(const char *path, const char *child, const char *parent) {
    VgDocument *document = NULL;
    VgDocumentDiagnostic diagnostic = {0};
    if (!vg_document_open_file(path, &document, &diagnostic))
        return 0;
    char *json = NULL;
    size_t length = 0u;
    int matches = 0;
    if (vg_document_write_canonical(document, &json, &length, &diagnostic)) {
        VgJsonError parse_error = {0};
        VgJsonNode *root = vg_json_parse(json, length, &parse_error);
        const VgJsonNode *entities = vg_json_object_get(root, "entities");
        if (entities != NULL && entities->type == VG_JSON_ARRAY) {
            for (size_t i = 0u; i < entities->as.array.count; ++i) {
                const VgJsonNode *entity = entities->as.array.items[i];
                const VgJsonNode *id = vg_json_object_get(entity, "id");
                const VgJsonNode *parent_node = vg_json_object_get(entity, "parent");
                if (id != NULL && id->type == VG_JSON_STRING &&
                    strcmp(id->as.string.data, child) == 0 && parent_node != NULL &&
                    parent_node->type == VG_JSON_STRING &&
                    strcmp(parent_node->as.string.data, parent) == 0)
                    matches = 1;
            }
        }
        vg_json_destroy(root);
    }
    vg_content_string_destroy(json);
    vg_document_destroy(document);
    return matches;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        (void)fprintf(stderr, "usage: gpu_host_e02_test LEVEL MODEL SAVED_COPY\n");
        return 2;
    }
    char error[512] = {0};
    int failed = 0;
    HWND parent =
        CreateWindowExW(0, L"STATIC", L"VESTIGIO E02 test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 640, 360, NULL, NULL, GetModuleHandleW(NULL), NULL);
    VgGpuHost *host = NULL;
    CHECK(parent != NULL);
    host = vg_gpu_host_create(parent, 640u, 360u, error, sizeof(error));
    CHECK(host != NULL);
    CHECK(vg_gpu_host_set_audio_enabled(host, 0));
    CHECK(vg_gpu_host_open_level(host, argv[1], argv[2], error, sizeof(error)));
    char first[37] = {0}, second[37] = {0};
    CHECK(vg_gpu_host_add_mesh(host, first, sizeof(first), error, sizeof(error)));
    CHECK(vg_gpu_host_add_mesh(host, second, sizeof(second), error, sizeof(error)));
    float first_start[3], second_start[3], position[3], rotation[4], scale[3];
    CHECK(selected_position(host, first, first_start));
    CHECK(selected_position(host, second, second_start));
    CHECK(vg_gpu_host_select_add(host, first, 0, 0));
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    CHECK(vg_gpu_host_selection_count(host) == 2u);
    CHECK(vg_gpu_host_gizmo_config(host, 0, 0, 0));
    CHECK(vg_gpu_host_set_camera_mode(host, 1));
    CHECK(vg_gpu_host_render(host));
    float initial_drag[3][2] = {{0}};
    int seen_initial[3] = {0};
    CHECK(sample_drag_directions(host, initial_drag, seen_initial));
    CHECK(vg_gpu_host_frame(host, 0.0, 0.0f, 0.0f, 180.0f, -55.0f, 0, 1));
    float perspective[3][2] = {{0}}, orthographic[3][2] = {{0}};
    int seen_perspective[3] = {0}, seen_orthographic[3] = {0};
    CHECK(sample_drag_directions(host, perspective, seen_perspective));
    int orbit_changed = 0;
    for (size_t axis = 0u; axis < 3u; ++axis) {
        if (seen_initial[axis] && seen_perspective[axis] &&
            hypotf(initial_drag[axis][0] - perspective[axis][0],
                   initial_drag[axis][1] - perspective[axis][1]) > 0.02f)
            orbit_changed = 1;
    }
    CHECK(orbit_changed);
    CHECK(vg_gpu_host_set_camera_mode(host, 2));
    CHECK(vg_gpu_host_render(host));
    CHECK(sample_drag_directions(host, orthographic, seen_orthographic));
    CHECK(vg_gpu_host_set_camera_mode(host, 1));
    char indexed[37] = {0};
    CHECK(vg_gpu_host_selection_at(host, 0u, indexed, sizeof(indexed)) &&
          strcmp(indexed, first) == 0);
    uint64_t revision = vg_gpu_host_document_revision(host);
    CHECK(vg_gpu_host_begin_gesture(host, 0, 0, 0, 0, 0.5f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, 0.62f, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision);
    CHECK(!vg_gpu_host_select(host, first));
    CHECK(!vg_gpu_host_select_add(host, first, 0, 0));
    CHECK(!vg_gpu_host_pick(host, 0.5f, 0.5f, indexed, sizeof(indexed)));
    CHECK(vg_gpu_host_selection_count(host) == 2u);
    CHECK(!vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 0, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision);
    CHECK(selected_position(host, first, position) && fabsf(position[0] - first_start[0]) < 0.001f);
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    CHECK(vg_gpu_host_begin_gesture(host, 0, 0, 0, 0, 0.5f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, 0.62f, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 1, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision + 1u);
    CHECK(selected_position(host, first, position) &&
          fabsf(position[0] - first_start[0] - 0.5f) < 0.001f);
    CHECK(selected_position(host, second, position) &&
          fabsf(position[0] - second_start[0] - 0.5f) < 0.001f);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(selected_position(host, first, position) && fabsf(position[0] - first_start[0]) < 0.001f);
    /* A canceled gesture must leave the redo branch intact. */
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    CHECK(vg_gpu_host_begin_gesture(host, 1, 0, 0, 2, 0.0f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, 0.25f, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 0, error, sizeof(error)));
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));

    /* Two roots rotated around their median: UI gesture and direct Tool API
     * must produce equal positions and quaternions. */
    CHECK(vg_gpu_host_select_add(host, first, 0, 0));
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    VgDocument *rotation_document = NULL;
    VgDocumentDiagnostic rotation_diagnostic = {0};
    CHECK(vg_document_open_file(argv[3], &rotation_document, &rotation_diagnostic));
    VgDocumentTransform rotated[2];
    const char *root_ids[] = {first, second};
    for (size_t i = 0u; i < 2u; ++i)
        CHECK(vg_document_entity_transform(rotation_document, root_ids[i], &rotated[i],
                                           &rotation_diagnostic));
    double median_x = (rotated[0].position[0] + rotated[1].position[0]) * 0.5;
    double median_y = (rotated[0].position[1] + rotated[1].position[1]) * 0.5;
    float median_angle = 0.75f;
    VgToolBatch *rotation_batch = NULL;
    CHECK(vg_tool_begin(rotation_document, vg_document_revision(rotation_document), &rotation_batch,
                        &rotation_diagnostic));
    for (size_t i = 0u; i < 2u; ++i) {
        double x = rotated[i].position[0] - median_x;
        double y = rotated[i].position[1] - median_y;
        rotated[i].position[0] =
            median_x + cos((double)median_angle) * x - sin((double)median_angle) * y;
        rotated[i].position[1] =
            median_y + sin((double)median_angle) * x + cos((double)median_angle) * y;
        rotate_quaternion_z(rotated[i].rotation, median_angle, 1);
        CHECK(
            vg_tool_set_transform(rotation_batch, root_ids[i], &rotated[i], &rotation_diagnostic));
    }
    CHECK(vg_tool_commit(rotation_batch, NULL, &rotation_diagnostic));
    CHECK(vg_gpu_host_begin_gesture(host, 1, 0, 0, 2, 0.0f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, median_angle, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 1, error, sizeof(error)));
    CHECK(compare_with_tool(host, argv[3], rotation_document, root_ids, 2u, error, sizeof(error)));
    vg_document_destroy(rotation_document);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));

    CHECK(vg_gpu_host_select_add(host, first, 0, 0));
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    size_t before_duplicate = vg_gpu_host_entity_count(host);
    CHECK(vg_gpu_host_duplicate_selection(host, error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == before_duplicate + 2u &&
          vg_gpu_host_selection_count(host) == 2u);
    CHECK(vg_gpu_host_delete_selection(host, error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == before_duplicate &&
          vg_gpu_host_selection_count(host) == 0u);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));

    /* Reparent under a rotated parent while preserving the child's world pose. */
    CHECK(vg_gpu_host_select(host, first));
    CHECK(vg_gpu_host_selected_transform(host, position, rotation, scale));
    rotation[2] = 0.38268343f;
    rotation[3] = 0.92387953f;
    CHECK(
        vg_gpu_host_set_selected_transform(host, position, rotation, scale, error, sizeof(error)));
    CHECK(vg_gpu_host_select(host, second));
    CHECK(vg_gpu_host_reparent_selection(host, first, error, sizeof(error)));
    CHECK(vg_gpu_host_selected_transform(host, position, rotation, scale));
    float child_local[3] = {position[0], position[1], position[2]};
    CHECK(vg_gpu_host_select(host, first));
    CHECK(vg_gpu_host_selected_transform(host, position, rotation, scale));
    float expected_x = position[0] + scale[0] * child_local[0] * 0.70710678f -
                       scale[1] * child_local[1] * 0.70710678f;
    float expected_y = position[1] + scale[0] * child_local[0] * 0.70710678f +
                       scale[1] * child_local[1] * 0.70710678f;
    CHECK(fabsf(expected_x - second_start[0] - 0.5f) < 0.002f &&
          fabsf(expected_y - second_start[1]) < 0.002f);
    revision = vg_gpu_host_document_revision(host);
    CHECK(vg_gpu_host_select(host, first));
    CHECK(!vg_gpu_host_reparent_selection(host, second, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision);
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    VgDocument *direct = NULL, *after = NULL;
    VgDocumentDiagnostic diagnostic = {0};
    CHECK(vg_document_open_file(argv[3], &direct, &diagnostic));
    VgDocumentTransform tool_parent, tool_child;
    CHECK(vg_document_entity_transform(direct, first, &tool_parent, &diagnostic));
    CHECK(vg_document_entity_transform(direct, second, &tool_child, &diagnostic));
    tool_parent.position[1] += (double)0.3f;
    VgToolBatch *tool_batch = NULL;
    CHECK(vg_tool_begin(direct, vg_document_revision(direct), &tool_batch, &diagnostic));
    CHECK(vg_tool_set_transform(tool_batch, first, &tool_parent, &diagnostic));
    CHECK(vg_tool_set_transform(tool_batch, second, &tool_child, &diagnostic));
    CHECK(vg_tool_commit(tool_batch, NULL, &diagnostic));
    CHECK(vg_gpu_host_begin_gesture(host, 0, 0, 0, 1, 0.0f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, 0.3f, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 1, error, sizeof(error)));
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    CHECK(vg_document_open_file(argv[3], &after, &diagnostic));
    VgDocumentTransform from_tool, from_ui;
    CHECK(vg_document_entity_transform(direct, first, &from_tool, &diagnostic));
    CHECK(vg_document_entity_transform(after, first, &from_ui, &diagnostic));
    for (size_t i = 0u; i < 3u; ++i) {
        CHECK(fabs(from_tool.position[i] - from_ui.position[i]) < 0.0001);
        CHECK(fabs(from_tool.scale[i] - from_ui.scale[i]) < 0.0001);
    }
    for (size_t i = 0u; i < 4u; ++i)
        CHECK(fabs(from_tool.rotation[i] - from_ui.rotation[i]) < 0.0001);
    CHECK(vg_document_entity_transform(direct, second, &from_tool, &diagnostic));
    CHECK(vg_document_entity_transform(after, second, &from_ui, &diagnostic));
    for (size_t i = 0u; i < 3u; ++i) {
        CHECK(fabs(from_tool.position[i] - from_ui.position[i]) < 0.0001);
        CHECK(fabs(from_tool.scale[i] - from_ui.scale[i]) < 0.0001);
    }
    for (size_t i = 0u; i < 4u; ++i)
        CHECK(fabs(from_tool.rotation[i] - from_ui.rotation[i]) < 0.0001);
    vg_document_destroy(direct);
    vg_document_destroy(after);
    CHECK(vg_gpu_host_reopen_level(host, argv[3], argv[2], error, sizeof(error)));
    CHECK(vg_gpu_host_select(host, second));
    CHECK(vg_gpu_host_selected_transform(host, position, rotation, scale));

    /* Individual local rotation under a rotated parent. */
    VgDocument *local_rotation = NULL;
    CHECK(vg_document_open_file(argv[3], &local_rotation, &diagnostic));
    VgDocumentTransform local_expected;
    CHECK(vg_document_entity_transform(local_rotation, second, &local_expected, &diagnostic));
    float local_angle = 0.4f;
    rotate_quaternion_z(local_expected.rotation, local_angle, 0);
    VgToolBatch *local_batch = NULL;
    CHECK(vg_tool_begin(local_rotation, vg_document_revision(local_rotation), &local_batch,
                        &diagnostic));
    CHECK(vg_tool_set_transform(local_batch, second, &local_expected, &diagnostic));
    CHECK(vg_tool_commit(local_batch, NULL, &diagnostic));
    CHECK(vg_gpu_host_begin_gesture(host, 1, 1, 2, 2, 0.0f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, local_angle, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 1, error, sizeof(error)));
    const char *hierarchy_ids[] = {first, second};
    CHECK(
        compare_with_tool(host, argv[3], local_rotation, hierarchy_ids, 2u, error, sizeof(error)));
    vg_document_destroy(local_rotation);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));

    /* Individual local Z scale keeps position and parent rotation intact. */
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    VgDocument *local_scale = NULL;
    CHECK(vg_document_open_file(argv[3], &local_scale, &diagnostic));
    CHECK(vg_document_entity_transform(local_scale, second, &local_expected, &diagnostic));
    local_expected.scale[2] *= 1.25;
    local_batch = NULL;
    CHECK(vg_tool_begin(local_scale, vg_document_revision(local_scale), &local_batch, &diagnostic));
    CHECK(vg_tool_set_transform(local_batch, second, &local_expected, &diagnostic));
    CHECK(vg_tool_commit(local_batch, NULL, &diagnostic));
    CHECK(vg_gpu_host_begin_gesture(host, 2, 1, 2, 2, 0.0f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, 0.25f, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 1, error, sizeof(error)));
    CHECK(compare_with_tool(host, argv[3], local_scale, hierarchy_ids, 2u, error, sizeof(error)));
    vg_document_destroy(local_scale);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));

    /* Rotating a selected parent and child about the active parent's pivot
     * must not double-rotate the child's local pose. */
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    VgDocument *active_pivot = NULL;
    CHECK(vg_document_open_file(argv[3], &active_pivot, &diagnostic));
    CHECK(vg_document_entity_transform(active_pivot, first, &tool_parent, &diagnostic));
    rotate_quaternion_z(tool_parent.rotation, 0.2f, 1);
    VgToolBatch *active_batch = NULL;
    CHECK(vg_tool_begin(active_pivot, vg_document_revision(active_pivot), &active_batch,
                        &diagnostic));
    CHECK(vg_tool_set_transform(active_batch, first, &tool_parent, &diagnostic));
    CHECK(vg_tool_commit(active_batch, NULL, &diagnostic));
    CHECK(vg_gpu_host_select_add(host, second, 0, 0));
    CHECK(vg_gpu_host_select_add(host, first, 1, 0));
    CHECK(vg_gpu_host_begin_gesture(host, 1, 0, 1, 2, 0.0f, error, sizeof(error)));
    CHECK(vg_gpu_host_update_gesture(host, 0.2f, error, sizeof(error)));
    CHECK(vg_gpu_host_end_gesture(host, 1, error, sizeof(error)));
    CHECK(compare_with_tool(host, argv[3], active_pivot, hierarchy_ids, 2u, error, sizeof(error)));
    vg_document_destroy(active_pivot);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));

    CHECK(vg_gpu_host_select_add(host, first, 0, 0));
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    size_t before_group = vg_gpu_host_entity_count(host);
    CHECK(vg_gpu_host_duplicate_selection(host, error, sizeof(error)));
    char copied_parent[37] = {0}, copied_child[37] = {0};
    CHECK(vg_gpu_host_selection_at(host, 0u, copied_parent, sizeof(copied_parent)));
    CHECK(vg_gpu_host_selection_at(host, 1u, copied_child, sizeof(copied_child)));
    CHECK(strcmp(copied_parent, first) != 0 && strcmp(copied_child, second) != 0);
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    CHECK(child_has_parent(argv[3], copied_child, copied_parent));
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == before_group);
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == before_group + 2u);
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    CHECK(child_has_parent(argv[3], copied_child, copied_parent));
    CHECK(vg_gpu_host_select_add(host, copied_parent, 0, 0));
    CHECK(vg_gpu_host_select_add(host, copied_child, 1, 0));
    CHECK(vg_gpu_host_delete_selection(host, error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == before_group);

    /* A rotated, nonuniform parent would introduce shear. Candidate failure
     * must not consume redo from an unrelated undone edit. */
    char sheared_parent[37] = {0}, redo_probe[37] = {0};
    CHECK(vg_gpu_host_add_mesh(host, sheared_parent, sizeof(sheared_parent), error, sizeof(error)));
    CHECK(vg_gpu_host_selected_transform(host, position, rotation, scale));
    rotation[2] = 0.38268343f;
    rotation[3] = 0.92387953f;
    scale[1] *= 2.0f;
    CHECK(
        vg_gpu_host_set_selected_transform(host, position, rotation, scale, error, sizeof(error)));
    CHECK(vg_gpu_host_add_mesh(host, redo_probe, sizeof(redo_probe), error, sizeof(error)));
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_select(host, second));
    revision = vg_gpu_host_document_revision(host);
    CHECK(!vg_gpu_host_reparent_selection(host, sheared_parent, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision);
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_select(host, sheared_parent));
    revision = vg_gpu_host_document_revision(host);
    CHECK(vg_gpu_host_begin_gesture(host, 2, 0, 2, 0, 0.0f, error, sizeof(error)));
    CHECK(!vg_gpu_host_update_gesture(host, 0.5f, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision);
    CHECK(vg_gpu_host_end_gesture(host, 0, error, sizeof(error)));
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));

    char room[37] = {0};
    CHECK(vg_gpu_host_add_room(host, room, sizeof(room), error, sizeof(error)));
    revision = vg_gpu_host_document_revision(host);
    CHECK(!vg_gpu_host_begin_gesture(host, 0, 0, 0, 0, 0.0f, error, sizeof(error)));
    CHECK(!vg_gpu_host_duplicate_selection(host, error, sizeof(error)));
    CHECK(!vg_gpu_host_delete_selection(host, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision);
    CHECK(vg_gpu_host_readbacks(host) == 0u);

cleanup:
    if (host != NULL && !vg_gpu_host_destroy(host))
        failed = 1;
    if (parent != NULL)
        (void)DestroyWindow(parent);
    if (failed)
        return 1;
    puts("PASS E02 multi-select gesture preview/cancel/commit, batch commands and hierarchy");
    return 0;
}
