#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "content/document.h"
#include "content/json.h"
#include "platform/gpu_host.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #condition, error); \
            failed = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static int copy_file(const char *source, const char *destination) {
    FILE *input = fopen(source, "rb");
    FILE *output = fopen(destination, "wb");
    if (input == NULL || output == NULL) {
        if (input != NULL)
            (void)fclose(input);
        if (output != NULL)
            (void)fclose(output);
        return 0;
    }
    char buffer[8192];
    size_t length;
    int okay = 1;
    while ((length = fread(buffer, 1u, sizeof(buffer), input)) != 0u)
        if (fwrite(buffer, 1u, length, output) != length) {
            okay = 0;
            break;
        }
    int read_okay = ferror(input) == 0;
    int close_input = fclose(input) == 0;
    int close_output = fclose(output) == 0;
    okay = read_okay && close_input && close_output && okay;
    return okay;
}

static const VgJsonNode *find_asset(const VgJsonNode *root, const char *id) {
    const VgJsonNode *assets = vg_json_object_get(root, "assets");
    if (assets == NULL || assets->type != VG_JSON_ARRAY)
        return NULL;
    for (size_t index = 0u; index < assets->as.array.count; ++index) {
        const VgJsonNode *entry = assets->as.array.items[index];
        const VgJsonNode *entry_id = vg_json_object_get(entry, "id");
        if (entry_id != NULL && entry_id->type == VG_JSON_STRING &&
            strcmp(entry_id->as.string.data, id) == 0)
            return entry;
    }
    return NULL;
}

static int files_equal(const char *left, const char *right) {
    FILE *a = fopen(left, "rb"), *b = fopen(right, "rb");
    if (a == NULL || b == NULL) {
        if (a != NULL)
            (void)fclose(a);
        if (b != NULL)
            (void)fclose(b);
        return 0;
    }
    int equal = 1;
    int x, y;
    do {
        x = fgetc(a);
        y = fgetc(b);
        if (x != y)
            equal = 0;
    } while (equal && x != EOF && y != EOF);
    equal = equal && ferror(a) == 0 && ferror(b) == 0;
    (void)fclose(a);
    (void)fclose(b);
    return equal;
}

static int field_is_mixed(const char *json, const char *path) {
    VgJsonError parse_error = {0};
    VgJsonNode *root = vg_json_parse(json, strlen(json), &parse_error);
    const VgJsonNode *fields = vg_json_object_get(root, "fields");
    int result = 0;
    if (fields != NULL && fields->type == VG_JSON_ARRAY)
        for (size_t index = 0u; index < fields->as.array.count; ++index) {
            const VgJsonNode *field = fields->as.array.items[index];
            const VgJsonNode *candidate = vg_json_object_get(field, "path");
            const VgJsonNode *mixed = vg_json_object_get(field, "mixed");
            if (candidate != NULL && candidate->type == VG_JSON_STRING &&
                strcmp(candidate->as.string.data, path) == 0 && mixed != NULL &&
                mixed->type == VG_JSON_BOOL && mixed->as.boolean)
                result = 1;
        }
    vg_json_destroy(root);
    return result;
}

static int transform_matches(VgGpuHost *host, const char *id, float expected_x,
                             float expected_scale) {
    float position[3], rotation[4], scale[3];
    return vg_gpu_host_select(host, id) &&
           vg_gpu_host_selected_transform(host, position, rotation, scale) &&
           fabsf(position[0] - expected_x) < 0.0001f && fabsf(scale[0] - expected_scale) < 0.0001f;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        (void)fprintf(stderr, "usage: gpu_host_e03_test LEVEL MODEL GLB SCRATCH_DIR\n");
        return 2;
    }
    char error[512] = {0};
    int failed = 0;
    char level[4096], save_dir[4096], saved[4096], source_copy[4096];
    HWND parent = NULL;
    VgGpuHost *host = NULL;
    CHECK(snprintf(level, sizeof(level), "%s/e03-level.json", argv[4]) < (int)sizeof(level));
    CHECK(snprintf(save_dir, sizeof(save_dir), "%s/e03-save-as", argv[4]) < (int)sizeof(save_dir));
    CHECK(snprintf(saved, sizeof(saved), "%s/e03-level.json", save_dir) < (int)sizeof(saved));
    CHECK(copy_file(argv[1], level));
    CHECK(CreateDirectoryA(save_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    parent = CreateWindowExW(0, L"STATIC", L"VESTIGIO E03 test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                             CW_USEDEFAULT, 640, 360, NULL, NULL, GetModuleHandleW(NULL), NULL);
    CHECK(parent != NULL);
    host = vg_gpu_host_create(parent, 640u, 360u, error, sizeof(error));
    CHECK(host != NULL);
    CHECK(vg_gpu_host_set_audio_enabled(host, 0));
    CHECK(vg_gpu_host_open_level(host, level, argv[2], error, sizeof(error)));
    uint64_t before = vg_gpu_host_document_revision(host);
    char asset_id[37] = {0}, same_id[37] = {0};
    CHECK(
        vg_gpu_host_import_asset(host, argv[3], asset_id, sizeof(asset_id), error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == before + 1u);
    before = vg_gpu_host_document_revision(host);
    CHECK(vg_gpu_host_import_asset(host, argv[3], same_id, sizeof(same_id), error, sizeof(error)));
    CHECK(strcmp(asset_id, same_id) == 0 && vg_gpu_host_document_revision(host) == before);
    char assets[131072] = {0};
    CHECK(vg_gpu_host_assets_json(host, assets, sizeof(assets)) > 0);
    CHECK(strstr(assets, "\"status\":\"ready\"") != NULL);
    CHECK(strstr(assets, "\"path\":") != NULL);
    before = vg_gpu_host_document_revision(host);
    CHECK(vg_gpu_host_preview_asset(host, asset_id, error, sizeof(error)));
    CHECK(vg_gpu_host_render(host));
    CHECK(vg_gpu_host_preview_asset(host, "", error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == before);
    char first[37] = {0}, second[37] = {0};
    CHECK(vg_gpu_host_place_asset(host, asset_id, 0u, first, sizeof(first), error, sizeof(error)));
    CHECK(
        vg_gpu_host_place_asset(host, asset_id, 0u, second, sizeof(second), error, sizeof(error)));
    CHECK(strcmp(first, second) != 0);
    CHECK(vg_gpu_host_select(host, first));
    CHECK(vg_gpu_host_set_selection_fields_json(
        host, "{\"updates\":[{\"path\":\"transform.position.x\",\"value\":1}]}", error,
        sizeof(error)));
    CHECK(vg_gpu_host_select_add(host, first, 0, 0));
    CHECK(vg_gpu_host_select_add(host, second, 1, 0));
    char fields[32768] = {0};
    CHECK(vg_gpu_host_selection_fields_json(host, fields, sizeof(fields)) > 0);
    CHECK(strstr(fields, "engine.mesh.asset") != NULL);
    CHECK(field_is_mixed(fields, "transform.position.x"));
    CHECK(!field_is_mixed(fields, "transform.scale.x"));
    before = vg_gpu_host_document_revision(host);
    CHECK(!vg_gpu_host_set_selection_fields_json(
        host, "{\"updates\":[{\"path\":\"engine.mesh.asset\",\"value\":\"bad\"}]}", error,
        sizeof(error)));
    CHECK(strstr(error, "engine.mesh.asset") != NULL &&
          vg_gpu_host_document_revision(host) == before);
    CHECK(vg_gpu_host_set_selection_fields_json(
        host,
        "{\"updates\":[{\"path\":\"transform.position.x\",\"value\":2.25},"
        "{\"path\":\"editor.layer\",\"value\":\"Props\"},"
        "{\"path\":\"editor.group\",\"value\":\"Hall\"},"
        "{\"path\":\"editor.hidden\",\"value\":true}]}",
        error, sizeof(error)));
    CHECK(vg_gpu_host_document_revision(host) == before + 1u);
    CHECK(transform_matches(host, first, 2.25f, 1.0f));
    CHECK(transform_matches(host, second, 2.25f, 1.0f));
    char editor[2048] = {0};
    CHECK(vg_gpu_host_entity_editor_json(host, first, editor, sizeof(editor)) > 0);
    CHECK(strstr(editor, "\"layer\":\"Props\"") != NULL &&
          strstr(editor, "\"hidden\":true") != NULL && strstr(editor, asset_id) != NULL);
    CHECK(vg_gpu_host_undo(host, error, sizeof(error)));
    CHECK(transform_matches(host, first, 1.0f, 1.0f));
    CHECK(transform_matches(host, second, 0.0f, 1.0f));
    CHECK(vg_gpu_host_entity_editor_json(host, first, editor, sizeof(editor)) > 0);
    CHECK(strstr(editor, "\"hidden\":false") != NULL);
    CHECK(strstr(editor, "\"layer\":\"Default\"") != NULL &&
          strstr(editor, "\"group\":\"\"") != NULL);
    CHECK(vg_gpu_host_redo(host, error, sizeof(error)));
    CHECK(transform_matches(host, first, 2.25f, 1.0f));
    CHECK(transform_matches(host, second, 2.25f, 1.0f));
    CHECK(vg_gpu_host_entity_editor_json(host, first, editor, sizeof(editor)) > 0);
    CHECK(strstr(editor, "\"layer\":\"Props\"") != NULL &&
          strstr(editor, "\"group\":\"Hall\"") != NULL &&
          strstr(editor, "\"hidden\":true") != NULL);
    CHECK(vg_gpu_host_rename_asset(host, asset_id, "Triángulo renovado", error, sizeof(error)));
    CHECK(vg_gpu_host_reimport_asset(host, asset_id, argv[2], error, sizeof(error)));
    CHECK(vg_gpu_host_render(host));
    CHECK(vg_gpu_host_assets_json(host, assets, sizeof(assets)) > 0);
    CHECK(strstr(assets, asset_id) != NULL && strstr(assets, ".gltf") != NULL);
    CHECK(vg_gpu_host_reimport_asset(host, asset_id, argv[3], error, sizeof(error)));
    CHECK(vg_gpu_host_assets_json(host, assets, sizeof(assets)) > 0);
    CHECK(strstr(assets, asset_id) != NULL && strstr(assets, "ready") != NULL);
    /* Embedded .gltf works; external URI dependencies fail with a diagnostic
     * before either the document or copied source is changed. */
    char embedded_id[37] = {0};
    CHECK(vg_gpu_host_import_asset(host, argv[2], embedded_id, sizeof(embedded_id), error,
                                   sizeof(error)));
    CHECK(strcmp(embedded_id, asset_id) != 0);
    char external[4096];
    const char *last_slash = strrchr(argv[3], '/');
    const char *last_backslash = strrchr(argv[3], '\\');
    if (last_backslash != NULL && (last_slash == NULL || last_backslash > last_slash))
        last_slash = last_backslash;
    CHECK(last_slash != NULL &&
          snprintf(external, sizeof(external), "%.*s/external_dependencies.gltf",
                   (int)(last_slash - argv[3]), argv[3]) < (int)sizeof(external));
    before = vg_gpu_host_document_revision(host);
    CHECK(
        !vg_gpu_host_import_asset(host, external, same_id, sizeof(same_id), error, sizeof(error)));
    CHECK(error[0] != '\0' && vg_gpu_host_document_revision(host) == before);
    char manifest_before[131072], source_before[4096], backup_path[4096];
    CHECK(vg_gpu_host_assets_json(host, manifest_before, sizeof(manifest_before)) > 0);
    CHECK(snprintf(source_before, sizeof(source_before), "%s/assets/%s.glb", argv[4], asset_id) <
          (int)sizeof(source_before));
    CHECK(snprintf(backup_path, sizeof(backup_path), "%s/e03-before-failed-reimport.glb", argv[4]) <
          (int)sizeof(backup_path));
    CHECK(copy_file(source_before, backup_path));
    CHECK(!vg_gpu_host_reimport_asset(host, asset_id, external, error, sizeof(error)));
    CHECK(error[0] != '\0' && vg_gpu_host_document_revision(host) == before);
    CHECK(vg_gpu_host_assets_json(host, assets, sizeof(assets)) > 0);
    CHECK(strcmp(assets, manifest_before) == 0 && files_equal(source_before, backup_path));
    CHECK(vg_gpu_host_save_level(host, saved, error, sizeof(error)));
    VgDocument *document = NULL;
    VgDocumentDiagnostic diagnostic = {0};
    CHECK(vg_document_open_file(saved, &document, &diagnostic));
    char *canonical = NULL;
    size_t length = 0u;
    CHECK(vg_document_write_canonical(document, &canonical, &length, &diagnostic));
    VgJsonError parse_error = {0};
    VgJsonNode *root = vg_json_parse(canonical, length, &parse_error);
    CHECK(root != NULL);
    const VgJsonNode *entry = find_asset(root, asset_id);
    CHECK(entry != NULL);
    const VgJsonNode *source = vg_json_object_get(entry, "source");
    CHECK(source != NULL && source->type == VG_JSON_STRING);
    CHECK(snprintf(source_copy, sizeof(source_copy), "%s/%s", save_dir, source->as.string.data) <
          (int)sizeof(source_copy));
    FILE *copied = fopen(source_copy, "rb");
    CHECK(copied != NULL);
    (void)fclose(copied);
    char original_copy[4096];
    CHECK(snprintf(original_copy, sizeof(original_copy), "%s/%s", argv[4], source->as.string.data) <
          (int)sizeof(original_copy));
    CHECK(files_equal(original_copy, source_copy));
    vg_json_destroy(root);
    free(canonical);
    vg_document_destroy(document);
    CHECK(vg_gpu_host_reopen_level(host, saved, argv[2], error, sizeof(error)));
    CHECK(vg_gpu_host_assets_json(host, assets, sizeof(assets)) > 0);
    CHECK(strstr(assets, asset_id) != NULL);
    CHECK(vg_gpu_host_select(host, first));
    CHECK(vg_gpu_host_entity_editor_json(host, first, editor, sizeof(editor)) > 0);
    CHECK(strstr(editor, "\"hidden\":true") != NULL);
    CHECK(vg_gpu_host_set_mode(host, 1));
    CHECK(vg_gpu_host_render(host));
    CHECK(vg_gpu_host_set_mode(host, 0));
cleanup:
    if (host != NULL)
        (void)vg_gpu_host_destroy(host);
    if (parent != NULL)
        (void)DestroyWindow(parent);
    if (!failed)
        (void)puts("gpu_host_e03_test: PASS");
    return failed;
}
