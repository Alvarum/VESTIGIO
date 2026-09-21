#include "content/document.h"
#include "tooling/tool_api.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define VG_TEST_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define VG_TEST_MKDIR(path) mkdir(path, 0700)
#endif

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);    \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void fixture_path(char *output, size_t capacity, const char *name) {
    (void)snprintf(output, capacity, "%s/%s", VG_CONTENT_FIXTURES, name);
}

static VgDocument *open_level(void) {
    char path[1024];
    fixture_path(path, sizeof(path), "valid-level.json");
    VgDocument *document = NULL;
    VgDocumentDiagnostic diagnostic;
    CHECK(vg_document_open_file(path, &document, &diagnostic));
    return document;
}

static char *canonical(VgDocument *document, size_t *out_length) {
    char *json = NULL;
    VgDocumentDiagnostic diagnostic;
    CHECK(vg_document_write_canonical(document, &json, out_length, &diagnostic));
    return json;
}

static bool same_document(VgDocument *left, VgDocument *right) {
    size_t left_length = 0u;
    size_t right_length = 0u;
    char *left_json = canonical(left, &left_length);
    char *right_json = canonical(right, &right_length);
    bool same = left_json != NULL && right_json != NULL && left_length == right_length &&
                memcmp(left_json, right_json, left_length) == 0;
    vg_content_string_destroy(left_json);
    vg_content_string_destroy(right_json);
    return same;
}

static const char *mapped_id(const VgToolResult *result, const char *temporary) {
    for (size_t index = 0u; index < result->mapping_count; ++index) {
        if (strcmp(result->mappings[index].temporary, temporary) == 0)
            return result->mappings[index].id;
    }
    return NULL;
}

static const VgDocumentTransform identity = {
    {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 1.0}};

static void test_batch_preview_temp_ids_and_unknown_fields(void) {
    VgDocument *document = open_level();
    if (document == NULL)
        return;
    size_t before_length = 0u;
    char *before = canonical(document, &before_length);
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *batch = NULL;
    CHECK(vg_tool_begin(document, 1u, &batch, &diagnostic));
    CHECK(vg_tool_create_entity(
        batch, "$parent", NULL, &identity,
        "{\"vendor.asset-ref\":{\"version\":3,\"asset\":"
        "\"50000000-0000-0000-0000-000000000001\",\"source\":\"models/heavy.glb\"}}",
        &diagnostic));
    VgDocumentTransform child_transform = identity;
    child_transform.position[0] = 3.25;
    CHECK(vg_tool_create_entity(batch, "$child", "$parent", &child_transform,
                                "{\"vendor.optional\":{\"version\":9,\"ordered\":[3,2,1]}}",
                                &diagnostic));
    VgToolResult validated;
    memset(&validated, 0xa5, sizeof(validated));
    CHECK(vg_tool_validate(batch, &validated, &diagnostic));
    CHECK(vg_document_revision(document) == 1u);
    CHECK(validated.mapping_count == 2u);
    const char *validated_parent = mapped_id(&validated, "$parent");
    CHECK(validated_parent != NULL);

    char *preview = NULL;
    size_t preview_length = 0u;
    VgToolResult preview_result;
    CHECK(vg_tool_preview(batch, &preview, &preview_length, &preview_result, &diagnostic));
    const char *preview_parent = mapped_id(&preview_result, "$parent");
    CHECK(preview != NULL && preview_parent != NULL);
    if (preview != NULL) {
        CHECK(preview_length == strlen(preview));
        CHECK(strstr(preview, "vendor.optional") != NULL);
        CHECK(strstr(preview, "models/heavy.glb") != NULL);
        if (preview_parent != NULL)
            CHECK(strstr(preview, preview_parent) != NULL);
    }
    size_t unchanged_length = 0u;
    char *unchanged = canonical(document, &unchanged_length);
    CHECK(before_length == unchanged_length && memcmp(before, unchanged, before_length) == 0);
    vg_content_string_destroy(unchanged);
    vg_content_string_destroy(preview);

    VgToolResult committed;
    CHECK(vg_tool_commit(batch, &committed, &diagnostic));
    CHECK(vg_document_revision(document) == 2u);
    CHECK(vg_document_undo_count(document) == 1u && vg_document_redo_count(document) == 0u);
    const char *committed_parent = mapped_id(&committed, "$parent");
    CHECK(committed_parent != NULL && validated_parent != NULL);
    if (committed_parent != NULL && validated_parent != NULL)
        CHECK(strcmp(committed_parent, validated_parent) == 0);
    size_t after_length = 0u;
    char *after = canonical(document, &after_length);
    CHECK(strstr(after, "vendor.optional") != NULL);
    CHECK(strstr(after, "\"ordered\": [\n            3,\n            2,\n            1") != NULL);
    vg_content_string_destroy(after);
    vg_content_string_destroy(before);
    vg_document_destroy(document);
}

static void test_failed_batches_preserve_document_and_redo(void) {
    VgDocument *document = open_level();
    if (document == NULL)
        return;
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *valid = NULL;
    CHECK(vg_tool_begin(document, 1u, &valid, &diagnostic));
    VgDocumentTransform moved = identity;
    moved.position[2] = 8.0;
    CHECK(
        vg_tool_set_transform(valid, "40000000-0000-0000-0000-000000000001", &moved, &diagnostic));
    CHECK(vg_tool_commit(valid, NULL, &diagnostic));
    size_t committed_length = 0u;
    char *committed = canonical(document, &committed_length);
    CHECK(vg_document_undo(document, 2u, &diagnostic));
    CHECK(vg_document_revision(document) == 3u);
    CHECK(vg_document_redo_count(document) == 1u);
    size_t baseline_length = 0u;
    char *baseline = canonical(document, &baseline_length);

    VgToolBatch *invalid = NULL;
    CHECK(vg_tool_begin(document, 3u, &invalid, &diagnostic));
    CHECK(vg_tool_reparent(invalid, "40000000-0000-0000-0000-000000000001",
                           "49999999-0000-0000-0000-000000000099", &diagnostic));
    VgToolResult sentinel;
    memset(&sentinel, 0x5a, sizeof(sentinel));
    VgToolResult unchanged_result = sentinel;
    char *preview_sentinel = (char *)(uintptr_t)1u;
    size_t preview_length_sentinel = 77u;
    CHECK(!vg_tool_preview(invalid, &preview_sentinel, &preview_length_sentinel, &sentinel,
                           &diagnostic));
    CHECK(preview_sentinel == (char *)(uintptr_t)1u && preview_length_sentinel == 77u);
    CHECK(memcmp(&sentinel, &unchanged_result, sizeof(sentinel)) == 0);
    CHECK(!vg_tool_commit(invalid, &sentinel, &diagnostic));
    CHECK(diagnostic.code == VG_DOCUMENT_VALIDATION);
    CHECK(memcmp(&sentinel, &unchanged_result, sizeof(sentinel)) == 0);
    CHECK(vg_document_revision(document) == 3u && vg_document_redo_count(document) == 1u);
    size_t current_length = 0u;
    char *current = canonical(document, &current_length);
    CHECK(current_length == baseline_length && memcmp(current, baseline, baseline_length) == 0);
    vg_content_string_destroy(current);
    vg_tool_cancel(invalid);
    CHECK(vg_document_redo_count(document) == 1u);

    VgToolBatch *oom = NULL;
    CHECK(vg_tool_begin(document, 3u, &oom, &diagnostic));
    CHECK(vg_tool_set_transform(oom, "40000000-0000-0000-0000-000000000001", &moved, &diagnostic));
    size_t undo_count = vg_document_undo_count(document);
    size_t redo_count = vg_document_redo_count(document);
    vg_document_test_fail_allocations_after(document, 7u);
    CHECK(!vg_tool_commit(oom, NULL, &diagnostic));
    CHECK(diagnostic.code == VG_DOCUMENT_OUT_OF_MEMORY);
    CHECK(vg_document_revision(document) == 3u);
    CHECK(vg_document_undo_count(document) == undo_count &&
          vg_document_redo_count(document) == redo_count);
    current = canonical(document, &current_length);
    CHECK(current_length == baseline_length && memcmp(current, baseline, baseline_length) == 0);
    vg_content_string_destroy(current);
    vg_document_test_fail_allocations_after(document, SIZE_MAX);
    vg_tool_cancel(oom);
    CHECK(vg_document_redo_count(document) == 1u);

    CHECK(vg_document_redo(document, 3u, &diagnostic));
    CHECK(vg_document_revision(document) == 4u && vg_document_redo_count(document) == 0u);
    current = canonical(document, &current_length);
    CHECK(current_length == committed_length && memcmp(current, committed, committed_length) == 0);
    vg_content_string_destroy(current);
    vg_content_string_destroy(committed);
    vg_content_string_destroy(baseline);
    vg_document_destroy(document);
}

static void test_revision_conflict_and_outputs_intact(void) {
    VgDocument *document = open_level();
    if (document == NULL)
        return;
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *stale = NULL;
    CHECK(vg_tool_begin(document, 1u, &stale, &diagnostic));
    CHECK(vg_tool_set_environment(
        stale, "{\"ambient_linear\":[0.2,0.2,0.2],\"clear_linear\":[0,0,0]}", &diagnostic));
    VgToolBatch *winner = NULL;
    CHECK(vg_tool_begin(document, 1u, &winner, &diagnostic));
    CHECK(vg_tool_set_environment(
        winner, "{\"ambient_linear\":[0.3,0.3,0.3],\"clear_linear\":[0,0,0]}", &diagnostic));
    CHECK(vg_tool_commit(winner, NULL, &diagnostic));
    VgToolResult result;
    memset(&result, 0x33, sizeof(result));
    VgToolResult result_before = result;
    CHECK(!vg_tool_commit(stale, &result, &diagnostic));
    CHECK(diagnostic.code == VG_DOCUMENT_CONFLICT && diagnostic.expected_revision == 1u &&
          diagnostic.actual_revision == 2u);
    CHECK(memcmp(&result, &result_before, sizeof(result)) == 0);
    vg_tool_cancel(stale);

    VgToolBatch *batch_sentinel = (VgToolBatch *)(uintptr_t)1u;
    CHECK(!vg_tool_begin(document, 1u, &batch_sentinel, &diagnostic));
    CHECK(batch_sentinel == (VgToolBatch *)(uintptr_t)1u);
    vg_document_destroy(document);
}

static void test_duplicate_group_remaps_internal_references(void) {
    VgDocument *document = open_level();
    if (document == NULL)
        return;
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *batch = NULL;
    CHECK(vg_tool_begin(document, 1u, &batch, &diagnostic));
    CHECK(vg_tool_duplicate_entity(batch, "40000000-0000-0000-0000-000000000002", "$parent", "",
                                   &diagnostic));
    CHECK(vg_tool_duplicate_entity(batch, "40000000-0000-0000-0000-000000000001", "$child", NULL,
                                   &diagnostic));
    VgToolResult result;
    CHECK(vg_tool_commit(batch, &result, &diagnostic));
    const char *parent = mapped_id(&result, "$parent");
    const char *child = mapped_id(&result, "$child");
    CHECK(parent != NULL && child != NULL);
    size_t length = 0u;
    char *json = canonical(document, &length);
    const char *child_location = NULL;
    if (json != NULL && child != NULL)
        child_location = strstr(json, child);
    CHECK(child_location != NULL);
    if (child_location != NULL && parent != NULL)
        CHECK(strstr(child_location, parent) != NULL);
    vg_content_string_destroy(json);
    vg_document_destroy(document);
}

static void test_batch_and_sequential_equivalence(void) {
    VgDocument *batched = open_level();
    VgDocument *sequential = open_level();
    if (batched == NULL || sequential == NULL) {
        vg_document_destroy(batched);
        vg_document_destroy(sequential);
        return;
    }
    VgDocumentTransform moved = identity;
    moved.position[0] = 9.5;
    static const char environment[] =
        "{\"ambient_linear\":[0.4,0.3,0.2],\"clear_linear\":[0.1,0.1,0.1],"
        "\"vendor.weather\":{\"preset\":\"ash\"}}";
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *batch = NULL;
    CHECK(vg_tool_begin(batched, 1u, &batch, &diagnostic));
    CHECK(
        vg_tool_set_transform(batch, "40000000-0000-0000-0000-000000000001", &moved, &diagnostic));
    CHECK(vg_tool_set_environment(batch, environment, &diagnostic));
    CHECK(vg_tool_commit(batch, NULL, &diagnostic));

    CHECK(vg_tool_begin(sequential, 1u, &batch, &diagnostic));
    CHECK(
        vg_tool_set_transform(batch, "40000000-0000-0000-0000-000000000001", &moved, &diagnostic));
    CHECK(vg_tool_commit(batch, NULL, &diagnostic));
    CHECK(vg_tool_begin(sequential, 2u, &batch, &diagnostic));
    CHECK(vg_tool_set_environment(batch, environment, &diagnostic));
    CHECK(vg_tool_commit(batch, NULL, &diagnostic));
    CHECK(same_document(batched, sequential));
    vg_document_destroy(batched);
    vg_document_destroy(sequential);
}

static bool copy_file(const char *source, const char *destination) {
    FILE *input = fopen(source, "rb");
    if (input == NULL)
        return false;
    FILE *output = fopen(destination, "wb");
    if (output == NULL) {
        (void)fclose(input);
        return false;
    }
    char buffer[4096];
    bool success = true;
    for (;;) {
        size_t count = fread(buffer, 1u, sizeof(buffer), input);
        if (count != 0u && fwrite(buffer, 1u, count, output) != count) {
            success = false;
            break;
        }
        if (count < sizeof(buffer)) {
            success = ferror(input) == 0;
            break;
        }
    }
    bool input_closed = fclose(input) == 0;
    bool output_closed = fclose(output) == 0;
    return input_closed && output_closed && success;
}

static char *read_file(const char *path, size_t *out_length) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return NULL;
    (void)fseek(file, 0, SEEK_END);
    long length = ftell(file);
    (void)fseek(file, 0, SEEK_SET);
    if (length < 0) {
        (void)fclose(file);
        return NULL;
    }
    char *data = malloc((size_t)length + 1u);
    if (data == NULL || fread(data, 1u, (size_t)length, file) != (size_t)length) {
        free(data);
        (void)fclose(file);
        return NULL;
    }
    data[(size_t)length] = '\0';
    *out_length = (size_t)length;
    (void)fclose(file);
    return data;
}

static void clean_save_files(const char *path) {
    char sidecar[1200];
    (void)remove(path);
    (void)snprintf(sidecar, sizeof(sidecar), "%s.vg-tmp", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, sizeof(sidecar), "%s.vg-bak", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, sizeof(sidecar), "%s.vg-journal", path);
    (void)remove(sidecar);
}

static void test_recoverable_save_and_future_guard(void) {
    (void)VG_TEST_MKDIR(VG_CONTENT_TEST_DIR);
    char fixture[1024];
    fixture_path(fixture, sizeof(fixture), "valid-level.json");
    char target[1024];
    (void)snprintf(target, sizeof(target), "%s/recovery.level.json", VG_CONTENT_TEST_DIR);
    clean_save_files(target);
    CHECK(copy_file(fixture, target));
    size_t original_length = 0u;
    char *original = read_file(target, &original_length);
    VgDocument *document = open_level();
    if (document == NULL || original == NULL) {
        vg_document_destroy(document);
        free(original);
        return;
    }
    VgDocumentDiagnostic diagnostic;
    VgToolBatch *batch = NULL;
    VgDocumentTransform moved = identity;
    moved.position[1] = 12.0;
    CHECK(vg_tool_begin(document, 1u, &batch, &diagnostic));
    CHECK(
        vg_tool_set_transform(batch, "40000000-0000-0000-0000-000000000001", &moved, &diagnostic));
    CHECK(vg_tool_commit(batch, NULL, &diagnostic));
    size_t changed_length = 0u;
    char *changed = canonical(document, &changed_length);

    vg_document_test_fail_save_at_step(document, 1u);
    CHECK(!vg_document_save_atomic(document, target, 2u, &diagnostic));
    VgDocument *recovered = NULL;
    CHECK(vg_document_open_file(target, &recovered, &diagnostic));
    VgDocument *original_document = NULL;
    CHECK(vg_document_open_memory("original.level.json", original, original_length,
                                  &original_document, &diagnostic));
    CHECK(same_document(recovered, original_document));
    vg_document_destroy(recovered);
    vg_document_destroy(original_document);

    vg_document_test_fail_save_at_step(document, 2u);
    CHECK(!vg_document_save_atomic(document, target, 2u, &diagnostic));
    recovered = NULL;
    CHECK(vg_document_open_file(target, &recovered, &diagnostic));
    size_t recovered_length = 0u;
    char *recovered_json = canonical(recovered, &recovered_length);
    CHECK(recovered_length == changed_length &&
          memcmp(recovered_json, changed, changed_length) == 0);
    vg_content_string_destroy(recovered_json);
    vg_document_destroy(recovered);

    char future[1024];
    (void)snprintf(future, sizeof(future), "%s/future.level.json", VG_CONTENT_TEST_DIR);
    clean_save_files(future);
    static const char future_json[] =
        "{\"format\":\"vestigio.level\",\"version\":2,\"sentinel\":\"do-not-overwrite\"}";
    FILE *file = fopen(future, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite(future_json, 1u, sizeof(future_json) - 1u, file) == sizeof(future_json) - 1u);
        CHECK(fclose(file) == 0);
    }
    vg_document_test_fail_save_at_step(document, 0u);
    CHECK(!vg_document_save_atomic(document, future, 2u, &diagnostic));
    CHECK(diagnostic.code == VG_DOCUMENT_VALIDATION &&
          diagnostic.content.code == VG_CONTENT_DIAGNOSTIC_VERSION);
    size_t future_length = 0u;
    char *future_after = read_file(future, &future_length);
    CHECK(future_after != NULL && future_length == sizeof(future_json) - 1u &&
          memcmp(future_after, future_json, future_length) == 0);
    free(future_after);

    char future_temporary[1200];
    char future_journal[1200];
    (void)snprintf(future_temporary, sizeof(future_temporary), "%s.vg-tmp", future);
    (void)snprintf(future_journal, sizeof(future_journal), "%s.vg-journal", future);
    CHECK(copy_file(fixture, future_temporary));
    file = fopen(future_journal, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite("publish", 1u, 7u, file) == 7u);
        CHECK(fclose(file) == 0);
    }
    recovered = (VgDocument *)(uintptr_t)1u;
    CHECK(!vg_document_open_file(future, &recovered, &diagnostic));
    CHECK(recovered == (VgDocument *)(uintptr_t)1u);
    future_after = read_file(future, &future_length);
    CHECK(future_after != NULL && future_length == sizeof(future_json) - 1u &&
          memcmp(future_after, future_json, future_length) == 0);
    free(future_after);

    char orphan[1024];
    char orphan_backup[1200];
    (void)snprintf(orphan, sizeof(orphan), "%s/orphan.level.json", VG_CONTENT_TEST_DIR);
    (void)snprintf(orphan_backup, sizeof(orphan_backup), "%s.vg-bak", orphan);
    clean_save_files(orphan);
    CHECK(copy_file(fixture, orphan));
    static const char recovery_sentinel[] = "manual-recovery-data";
    file = fopen(orphan_backup, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite(recovery_sentinel, 1u, sizeof(recovery_sentinel) - 1u, file) ==
              sizeof(recovery_sentinel) - 1u);
        CHECK(fclose(file) == 0);
    }
    CHECK(!vg_document_save_atomic(document, orphan, 2u, &diagnostic));
    size_t recovery_length = 0u;
    char *recovery_after = read_file(orphan_backup, &recovery_length);
    CHECK(recovery_after != NULL && recovery_length == sizeof(recovery_sentinel) - 1u &&
          memcmp(recovery_after, recovery_sentinel, recovery_length) == 0);
    free(recovery_after);

    free(original);
    vg_content_string_destroy(changed);
    vg_document_destroy(document);
    clean_save_files(target);
    clean_save_files(future);
    clean_save_files(orphan);
}

int main(void) {
    test_batch_preview_temp_ids_and_unknown_fields();
    test_failed_batches_preserve_document_and_redo();
    test_revision_conflict_and_outputs_intact();
    test_duplicate_group_remaps_internal_references();
    test_batch_and_sequential_equivalence();
    test_recoverable_save_and_future_guard();
    if (failures != 0)
        (void)fprintf(stderr, "%d document checks failed\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
