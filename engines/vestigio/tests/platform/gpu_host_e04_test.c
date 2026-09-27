#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform/gpu_host.h"

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

static int room_json(char *out, size_t capacity, float right, float doorway) {
    int count = snprintf(out, capacity,
                         "{\"version\":1,\"vertices\":[[-2,-8],[%.4f,-8],[%.4f,-4],[-2,-4]],"
                         "\"floor_z\":0,\"wall_height\":3,\"wall_thickness\":0.2,"
                         "\"openings\":[{\"edge\":2,\"kind\":\"door\",\"offset\":%.4f,"
                         "\"width\":1.6,\"height\":2.2,\"sill\":0}]}",
                         (double)right, (double)right, (double)doorway);
    return count > 0 && (size_t)count < capacity;
}

static int step(VgGpuHost *host, float move_x, float move_y, int ticks) {
    for (int i = 0; i < ticks; ++i)
        if (!vg_gpu_host_frame(host, 1.0 / 60.0, move_x, move_y, 0.0f, 0.0f, 0, 1))
            return 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        (void)fprintf(stderr, "usage: gpu_host_e04_test LEVEL MODEL SAVED_COPY\n");
        return 2;
    }
    char error[512] = {0};
    int failed = 0;
    HWND parent =
        CreateWindowExW(0, L"STATIC", L"VESTIGIO E04 test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 640, 360, NULL, NULL, GetModuleHandleW(NULL), NULL);
    VgGpuHost *host = NULL;
    CHECK(parent != NULL);
    host = vg_gpu_host_create(parent, 640u, 360u, error, sizeof(error));
    CHECK(host != NULL);
    CHECK(vg_gpu_host_set_audio_enabled(host, 0));
    CHECK(vg_gpu_host_open_level(host, argv[1], argv[2], error, sizeof(error)));
    size_t original_count = vg_gpu_host_entity_count(host);
    uint64_t original_revision = vg_gpu_host_document_revision(host);
    char recipe[512], updated[512], invalid[512];
    CHECK(room_json(recipe, sizeof(recipe), 2.0f, 1.2f));
    CHECK(vg_gpu_host_set_room_editor_view(host, 1, 1, 0.0f));
    CHECK(vg_gpu_host_preview_room_recipe(host, recipe, error, sizeof(error)));
    CHECK(vg_gpu_host_frame(host, 1.0 / 60.0, 0.0f, 0.0f, 0.0f, 0.0f, 0, 1));
    CHECK(vg_gpu_host_document_revision(host) == original_revision);
    CHECK(vg_gpu_host_entity_count(host) == original_count);
    CHECK(vg_gpu_host_cancel_room_preview(host));
    CHECK(vg_gpu_host_document_revision(host) == original_revision);
    CHECK(vg_gpu_host_entity_count(host) == original_count);

    /* Each preview has a different generated asset. Exceed the default asset
     * catalog capacity in one Studio session to expose retained catalog slots. */
    for (unsigned int i = 0u; i < 270u; ++i) {
        CHECK(room_json(updated, sizeof(updated), 2.0f + (float)i * 0.001f, 1.2f));
        CHECK(vg_gpu_host_preview_room_recipe(host, updated, error, sizeof(error)));
        CHECK(vg_gpu_host_cancel_room_preview(host));
    }
    CHECK(vg_gpu_host_document_revision(host) == original_revision);
    CHECK(vg_gpu_host_entity_count(host) == original_count);

    char room_id[37] = {0};
    CHECK(vg_gpu_host_create_room_recipe(host, recipe, room_id, sizeof(room_id), error,
                                         sizeof(error)));
    CHECK(strlen(room_id) == 36u);
    CHECK(vg_gpu_host_entity_count(host) == original_count + 1u);
    CHECK(vg_gpu_host_document_revision(host) == original_revision + 1u);
    char stored[2048] = {0};
    CHECK(vg_gpu_host_room_recipe_json(host, room_id, stored, sizeof(stored)) > 0);
    CHECK(strstr(stored, "\"vestigio.room\"") == NULL);
    CHECK(strstr(stored, "\"vertices\"") != NULL);
    char stored_before_preview[2048];
    (void)snprintf(stored_before_preview, sizeof(stored_before_preview), "%s", stored);
    CHECK(vg_gpu_host_frame_selection(host));
    float before_x = 0.0f, before_y = 0.0f, before_z = 0.0f;
    CHECK(vg_gpu_host_camera_position(host, &before_x, &before_y, &before_z));
    char edited_preview[512];
    CHECK(room_json(edited_preview, sizeof(edited_preview), 6.0f, 1.2f));
    uint64_t before_preview_revision = vg_gpu_host_document_revision(host);
    CHECK(vg_gpu_host_preview_room_recipe(host, edited_preview, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == before_preview_revision);
    CHECK(vg_gpu_host_entity_count(host) == original_count + 1u);
    CHECK(vg_gpu_host_room_recipe_json(host, room_id, stored, sizeof(stored)) > 0);
    CHECK(strcmp(stored, stored_before_preview) == 0);
    CHECK(vg_gpu_host_frame_selection(host));
    float preview_x = 0.0f, preview_y = 0.0f, preview_z = 0.0f;
    CHECK(vg_gpu_host_camera_position(host, &preview_x, &preview_y, &preview_z));
    float preview_camera_delta = sqrtf((preview_x - before_x) * (preview_x - before_x) +
                                       (preview_y - before_y) * (preview_y - before_y) +
                                       (preview_z - before_z) * (preview_z - before_z));
    CHECK(preview_camera_delta > 3.0f);
    CHECK(vg_gpu_host_cancel_room_preview(host));
    CHECK(vg_gpu_host_document_revision(host) == before_preview_revision);
    CHECK(vg_gpu_host_entity_count(host) == original_count + 1u);
    CHECK(vg_gpu_host_room_recipe_json(host, room_id, stored, sizeof(stored)) > 0);
    CHECK(strcmp(stored, stored_before_preview) == 0);
    float restored_x = 0.0f, restored_y = 0.0f, restored_z = 0.0f;
    CHECK(vg_gpu_host_camera_position(host, &restored_x, &restored_y, &restored_z));
    CHECK(fabsf(restored_x - before_x) < 0.01f && fabsf(restored_y - before_y) < 0.01f &&
          fabsf(restored_z - before_z) < 0.01f);
    CHECK(room_json(updated, sizeof(updated), 2.5f, 1.3f));
    CHECK(vg_gpu_host_update_room_recipe(host, room_id, updated, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == original_revision + 2u);
    CHECK(vg_gpu_host_room_recipe_json(host, room_id, stored, sizeof(stored)) > 0);
    CHECK(strstr(stored, "2.5") != NULL);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_room_recipe_json(host, room_id, stored, sizeof(stored)) > 0);
    CHECK(strstr(stored, "2.5") == NULL);
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_room_recipe_json(host, room_id, stored, sizeof(stored)) > 0);
    CHECK(strstr(stored, "2.5") != NULL);
    CHECK(room_json(invalid, sizeof(invalid), 2.5f, 99.0f));
    uint64_t revision = vg_gpu_host_document_revision(host);
    CHECK(!vg_gpu_host_update_room_recipe(host, room_id, invalid, error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == revision);

    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    CHECK(vg_gpu_host_reopen_level(host, argv[3], argv[2], error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == original_count + 1u);
    CHECK(vg_gpu_host_room_recipe_json(host, room_id, stored, sizeof(stored)) > 0);
    CHECK(strstr(stored, "2.5") != NULL);
    CHECK(vg_gpu_host_select(host, room_id));
    CHECK(vg_gpu_host_set_mode(host, 1));
    CHECK(step(host, 0.0f, 1.0f, 180));
    float x = 0.0f, y = 0.0f, z = 0.0f;
    CHECK(vg_gpu_host_camera_position(host, &x, &y, &z));
    CHECK(y > -3.5f && z > 1.0f);
    CHECK(vg_gpu_host_set_mode(host, 0));
    CHECK(vg_gpu_host_set_mode(host, 1));
    CHECK(step(host, 1.0f, 0.0f, 35));
    CHECK(vg_gpu_host_camera_position(host, &x, &y, &z));
    CHECK(x > 1.5f && x < 2.25f);
    CHECK(step(host, 1.0f, 0.0f, 120));
    CHECK(vg_gpu_host_camera_position(host, &x, &y, &z));
    CHECK(x > 1.8f && x < 2.25f);
    CHECK(step(host, 0.0f, 1.0f, 180));
    CHECK(vg_gpu_host_camera_position(host, &x, &y, &z));
    CHECK(y < -4.25f);
    CHECK(vg_gpu_host_readbacks(host) == 0u);

cleanup:
    if (host != NULL && !vg_gpu_host_destroy(host))
        failed = 1;
    if (parent != NULL)
        (void)DestroyWindow(parent);
    if (failed)
        return 1;
    puts("PASS E04 GPU host recipe preview stress/undo/reopen and doorway/wall collision");
    return 0;
}
