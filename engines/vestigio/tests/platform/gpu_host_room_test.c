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

static int step(VgGpuHost *host, float move_x, float move_y, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        if (!vg_gpu_host_frame(host, 1.0 / 60.0, move_x, move_y, 0.0f, 0.0f, 0, 1))
            return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        (void)fprintf(stderr, "usage: gpu_host_room_test LEVEL MODEL SAVED_COPY\n");
        return 2;
    }
    char error[512] = {0};
    int failed = 0;
    HWND parent =
        CreateWindowExW(0, L"STATIC", L"VESTIGIO room test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 640, 360, NULL, NULL, GetModuleHandleW(NULL), NULL);
    VgGpuHost *host = NULL;
    CHECK(parent != NULL);
    host = vg_gpu_host_create(parent, 640u, 360u, error, sizeof(error));
    CHECK(host != NULL);
    CHECK(vg_gpu_host_set_audio_enabled(host, 0));
    CHECK(vg_gpu_host_open_level(host, argv[1], argv[2], error, sizeof(error)));
    size_t original = vg_gpu_host_entity_count(host);
    uint64_t initial_revision = vg_gpu_host_document_revision(host);
    char selected[37] = {0};
    CHECK(vg_gpu_host_add_room(host, selected, sizeof(selected), error, sizeof(error)));
    CHECK(strlen(selected) == 36u);
    CHECK(vg_gpu_host_entity_count(host) == original + 6u);
    CHECK(vg_gpu_host_document_revision(host) == initial_revision + 1u);
    CHECK(vg_gpu_host_is_dirty(host));
    char label[64] = {0};
    CHECK(vg_gpu_host_entity_label(host, selected, label, sizeof(label)));
    CHECK(strcmp(label, "Sala: jamba izquierda") == 0);
    size_t pieces = 0u;
    for (size_t i = 0u; i < vg_gpu_host_entity_count(host); ++i) {
        char id[37] = {0};
        CHECK(vg_gpu_host_entity_at(host, i, id, sizeof(id)));
        if (vg_gpu_host_entity_label(host, id, label, sizeof(label)))
            ++pieces;
    }
    CHECK(pieces == 6u);
    uint64_t revision = vg_gpu_host_document_revision(host);
    char rejected[37] = {0};
    CHECK(!vg_gpu_host_duplicate_selected(host, rejected, sizeof(rejected), error, sizeof(error)));
    CHECK(strstr(error, "pieza de sala") != NULL &&
          vg_gpu_host_document_revision(host) == revision &&
          vg_gpu_host_entity_count(host) == original + 6u);
    float position[3] = {0}, rotation[4] = {0}, scale[3] = {0};
    CHECK(vg_gpu_host_selected_transform(host, position, rotation, scale));
    position[0] += 0.5f;
    CHECK(
        !vg_gpu_host_set_selected_transform(host, position, rotation, scale, error, sizeof(error)));
    CHECK(strstr(error, "pieza de sala") != NULL &&
          vg_gpu_host_document_revision(host) == revision &&
          vg_gpu_host_entity_count(host) == original + 6u);
    char still_selected[37] = {0};
    CHECK(vg_gpu_host_selected_uuid(host, still_selected, sizeof(still_selected)) &&
          strcmp(still_selected, selected) == 0);
    CHECK(!vg_gpu_host_add_room(host, rejected, sizeof(rejected), error, sizeof(error)));
    CHECK(strstr(error, "ya existe") != NULL && vg_gpu_host_document_revision(host) == revision);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == original && !vg_gpu_host_is_dirty(host));
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == original + 6u);
    CHECK(vg_gpu_host_save_level(host, argv[3], error, sizeof(error)));
    CHECK(!vg_gpu_host_is_dirty(host));
    CHECK(vg_gpu_host_reopen_level(host, argv[3], argv[2], error, sizeof(error)));
    CHECK(vg_gpu_host_entity_count(host) == original + 6u);
    CHECK(vg_gpu_host_entity_label(host, selected, label, sizeof(label)));

    CHECK(vg_gpu_host_set_mode(host, 1));
    CHECK(!vg_gpu_host_add_room(host, rejected, sizeof(rejected), error, sizeof(error)));
    CHECK(step(host, 0.0f, 1.0f, 180));
    float x = 0.0f, y = 0.0f, z = 0.0f;
    CHECK(vg_gpu_host_camera_position(host, &x, &y, &z));
    CHECK(fabsf(x) < 0.5f && y > -3.5f && z > 1.0f);

    CHECK(vg_gpu_host_set_mode(host, 0));
    CHECK(vg_gpu_host_set_mode(host, 1));
    CHECK(step(host, 1.0f, 0.0f, 35));
    CHECK(vg_gpu_host_camera_position(host, &x, &y, &z));
    CHECK(x > 1.05f && x < 1.7f);
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
    puts("PASS W09 room batch/labels/save/reopen and controller doorway/wall GPU host");
    return 0;
}
