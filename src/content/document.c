#include "content/document_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VgDocumentHistory {
    VgJsonNode **items;
    size_t count;
    size_t capacity;
} VgDocumentHistory;

struct VgDocument {
    VgJsonNode *root;
    uint64_t revision;
    char file[512];
    VgDocumentHistory undo;
    VgDocumentHistory redo;
    size_t allocation_successes_remaining;
    unsigned int save_failure_step;
};

static bool vg_document_recover_path(const char *path, VgDocumentDiagnostic *out_diagnostic);

bool vg_document_fail(VgDocumentDiagnostic *diagnostic, VgDocumentStatus code,
                      const char *operation, const char *path, const char *id,
                      uint64_t expected_revision, uint64_t actual_revision, const char *message) {
    if (diagnostic != NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->expected_revision = expected_revision;
        diagnostic->actual_revision = actual_revision;
        (void)snprintf(diagnostic->operation, sizeof(diagnostic->operation), "%s",
                       operation != NULL ? operation : "document");
        (void)snprintf(diagnostic->path, sizeof(diagnostic->path), "%s", path != NULL ? path : "$");
        if (id != NULL)
            (void)snprintf(diagnostic->id, sizeof(diagnostic->id), "%s", id);
        (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
    return false;
}

static bool vg_document_content_fail(VgDocumentDiagnostic *diagnostic, const char *operation,
                                     const VgContentDiagnostic *content) {
    if (diagnostic != NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = VG_DOCUMENT_VALIDATION;
        (void)snprintf(diagnostic->operation, sizeof(diagnostic->operation), "%s", operation);
        (void)snprintf(diagnostic->file, sizeof(diagnostic->file), "%s", content->file);
        (void)snprintf(diagnostic->path, sizeof(diagnostic->path), "%s", content->path);
        (void)snprintf(diagnostic->id, sizeof(diagnostic->id), "%s", content->id);
        (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", content->message);
        diagnostic->content = *content;
    }
    return false;
}

void *vg_document_allocate(VgDocument *document, size_t size) {
    if (document->allocation_successes_remaining == 0u)
        return NULL;
    if (document->allocation_successes_remaining != SIZE_MAX)
        --document->allocation_successes_remaining;
    return malloc(size);
}

static char *vg_document_clone_chars(VgDocument *document, const char *source, size_t length) {
    char *copy = vg_document_allocate(document, length + 1u);
    if (copy == NULL)
        return NULL;
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

VgJsonNode *vg_document_clone_json(VgDocument *document, const VgJsonNode *source) {
    VgJsonNode *copy = vg_document_allocate(document, sizeof(*copy));
    if (copy == NULL)
        return NULL;
    memset(copy, 0, sizeof(*copy));
    copy->type = source->type;
    switch (source->type) {
    case VG_JSON_NULL:
        break;
    case VG_JSON_BOOL:
        copy->as.boolean = source->as.boolean;
        break;
    case VG_JSON_NUMBER:
        copy->as.number.value = source->as.number.value;
        copy->as.number.length = source->as.number.length;
        copy->as.number.lexeme =
            vg_document_clone_chars(document, source->as.number.lexeme, source->as.number.length);
        if (copy->as.number.lexeme == NULL)
            goto fail;
        break;
    case VG_JSON_STRING:
        copy->as.string.length = source->as.string.length;
        copy->as.string.data =
            vg_document_clone_chars(document, source->as.string.data, source->as.string.length);
        if (copy->as.string.data == NULL)
            goto fail;
        break;
    case VG_JSON_ARRAY:
        if (source->as.array.count != 0u) {
            copy->as.array.items = vg_document_allocate(
                document, source->as.array.count * sizeof(*copy->as.array.items));
            if (copy->as.array.items == NULL)
                goto fail;
            memset(copy->as.array.items, 0, source->as.array.count * sizeof(*copy->as.array.items));
        }
        copy->as.array.count = source->as.array.count;
        for (size_t index = 0u; index < source->as.array.count; ++index) {
            copy->as.array.items[index] =
                vg_document_clone_json(document, source->as.array.items[index]);
            if (copy->as.array.items[index] == NULL)
                goto fail;
        }
        break;
    case VG_JSON_OBJECT:
        if (source->as.object.count != 0u) {
            copy->as.object.pairs = vg_document_allocate(
                document, source->as.object.count * sizeof(*copy->as.object.pairs));
            if (copy->as.object.pairs == NULL)
                goto fail;
            memset(copy->as.object.pairs, 0,
                   source->as.object.count * sizeof(*copy->as.object.pairs));
        }
        copy->as.object.count = source->as.object.count;
        for (size_t index = 0u; index < source->as.object.count; ++index) {
            const VgJsonPair *source_pair = &source->as.object.pairs[index];
            VgJsonPair *copy_pair = &copy->as.object.pairs[index];
            copy_pair->key.length = source_pair->key.length;
            copy_pair->key.data =
                vg_document_clone_chars(document, source_pair->key.data, source_pair->key.length);
            if (copy_pair->key.data == NULL)
                goto fail;
            copy_pair->value = vg_document_clone_json(document, source_pair->value);
            if (copy_pair->value == NULL)
                goto fail;
        }
        break;
    }
    return copy;
fail:
    vg_json_destroy(copy);
    return NULL;
}

const VgJsonNode *vg_document_root(const VgDocument *document) {
    return document == NULL ? NULL : document->root;
}

static void vg_document_history_clear(VgDocumentHistory *history) {
    for (size_t index = 0u; index < history->count; ++index)
        vg_json_destroy(history->items[index]);
    free(history->items);
    memset(history, 0, sizeof(*history));
}

static bool vg_document_history_reserve(VgDocument *document, VgDocumentHistory *history) {
    if (history->count < history->capacity)
        return true;
    size_t capacity = history->capacity == 0u ? 8u : history->capacity * 2u;
    VgJsonNode **items = vg_document_allocate(document, capacity * sizeof(*history->items));
    if (items == NULL)
        return false;
    if (history->count != 0u)
        memcpy(items, history->items, history->count * sizeof(*history->items));
    free(history->items);
    history->items = items;
    history->capacity = capacity;
    return true;
}

bool vg_document_check_revision(const VgDocument *document, uint64_t expected_revision,
                                const char *operation, VgDocumentDiagnostic *out_diagnostic) {
    if (document == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, operation, "$", NULL,
                                expected_revision, 0u, "document is required");
    if (document->revision != expected_revision)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_CONFLICT, operation, "$", NULL,
                                expected_revision, document->revision, "document revision changed");
    return true;
}

bool vg_document_publish(VgDocument *document, VgJsonNode *candidate, uint64_t expected_revision,
                         VgDocumentDiagnostic *out_diagnostic) {
    if (candidate == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "commit", "$", NULL,
                                expected_revision, document != NULL ? document->revision : 0u,
                                "candidate is required");
    if (!vg_document_check_revision(document, expected_revision, "commit", out_diagnostic))
        return false;
    if (!vg_document_history_reserve(document, &document->undo))
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "commit", "$", NULL,
                                expected_revision, document->revision,
                                "cannot reserve undo history");
    document->undo.items[document->undo.count++] = document->root;
    document->root = candidate;
    ++document->revision;
    vg_document_history_clear(&document->redo);
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

static bool vg_document_create_from_content(const char *file_name, VgContentDocument *content,
                                            VgDocument **out_document,
                                            VgDocumentDiagnostic *out_diagnostic) {
    char *canonical = NULL;
    size_t length = 0u;
    VgContentDiagnostic content_diagnostic;
    if (!vg_content_write_canonical(content, &canonical, &length, &content_diagnostic))
        return vg_document_content_fail(out_diagnostic, "open", &content_diagnostic);
    VgJsonError json_error = {0};
    VgJsonNode *root = vg_json_parse(canonical, length, &json_error);
    vg_content_string_destroy(canonical);
    if (root == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_VALIDATION, "open", "$", NULL, 0u, 0u,
                                json_error.message != NULL ? json_error.message
                                                           : "canonical parse failed");
    VgDocument *candidate = calloc(1u, sizeof(*candidate));
    if (candidate == NULL) {
        vg_json_destroy(root);
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "open", "$", NULL, 0u,
                                0u, "cannot allocate document");
    }
    candidate->root = root;
    candidate->revision = 1u;
    candidate->allocation_successes_remaining = SIZE_MAX;
    (void)snprintf(candidate->file, sizeof(candidate->file), "%s",
                   file_name != NULL ? file_name : "<memory>");
    *out_document = candidate;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

bool vg_document_open_memory(const char *file_name, const char *json, size_t length,
                             VgDocument **out_document, VgDocumentDiagnostic *out_diagnostic) {
    if (out_document == NULL || json == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "open", "$", NULL, 0u,
                                0u, "input and output pointers are required");
    VgContentDocument *content = NULL;
    VgContentDiagnostic content_diagnostic;
    if (!vg_content_parse_memory(file_name, json, length, &content, &content_diagnostic))
        return vg_document_content_fail(out_diagnostic, "open", &content_diagnostic);
    bool success =
        vg_document_create_from_content(file_name, content, out_document, out_diagnostic);
    vg_content_document_destroy(content);
    return success;
}

bool vg_document_open_file(const char *path, VgDocument **out_document,
                           VgDocumentDiagnostic *out_diagnostic) {
    if (path == NULL || out_document == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "open", "$", NULL, 0u,
                                0u, "path and output pointer are required");
    if (!vg_document_recover_path(path, out_diagnostic))
        return false;
    VgContentDocument *content = NULL;
    VgContentDiagnostic content_diagnostic;
    if (!vg_content_parse_file(path, &content, &content_diagnostic))
        return vg_document_content_fail(out_diagnostic, "open", &content_diagnostic);
    bool success = vg_document_create_from_content(path, content, out_document, out_diagnostic);
    vg_content_document_destroy(content);
    return success;
}

void vg_document_destroy(VgDocument *document) {
    if (document == NULL)
        return;
    vg_json_destroy(document->root);
    vg_document_history_clear(&document->undo);
    vg_document_history_clear(&document->redo);
    free(document);
}

uint64_t vg_document_revision(const VgDocument *document) {
    return document == NULL ? 0u : document->revision;
}

bool vg_document_write_canonical(const VgDocument *document, char **out_json, size_t *out_length,
                                 VgDocumentDiagnostic *out_diagnostic) {
    if (document == NULL || out_json == NULL || out_length == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "write", "$", NULL,
                                0u, document != NULL ? document->revision : 0u,
                                "document and output pointers are required");
    char *json = NULL;
    size_t length = 0u;
    if (!vg_json_write_canonical(document->root, &json, &length))
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "write", "$", NULL, 0u,
                                document->revision, "cannot serialize document");
    *out_json = json;
    *out_length = length;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

static bool vg_document_file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    (void)fclose(file);
    return true;
}

static bool vg_document_sidecar_paths(const char *path, char temporary[1024], char backup[1024],
                                      char journal[1024], VgDocumentDiagnostic *out_diagnostic) {
    int temporary_length = snprintf(temporary, 1024u, "%s.vg-tmp", path);
    int backup_length = snprintf(backup, 1024u, "%s.vg-bak", path);
    int journal_length = snprintf(journal, 1024u, "%s.vg-journal", path);
    if (temporary_length < 0 || temporary_length >= 1024 || backup_length < 0 ||
        backup_length >= 1024 || journal_length < 0 || journal_length >= 1024)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "recovery", "$", NULL,
                                0u, 0u, "document path is too long");
    return true;
}

static bool vg_document_write_stage(const char *journal, const char *stage) {
    FILE *file = fopen(journal, "wb");
    if (file == NULL)
        return false;
    size_t length = strlen(stage);
    bool wrote = fwrite(stage, 1u, length, file) == length && fflush(file) == 0;
    bool closed = fclose(file) == 0;
    return wrote && closed;
}

static bool vg_document_read_stage(const char *journal, char stage[16]) {
    FILE *file = fopen(journal, "rb");
    if (file == NULL)
        return false;
    size_t length = fread(stage, 1u, 15u, file);
    bool okay = ferror(file) == 0;
    bool closed = fclose(file) == 0;
    stage[length] = '\0';
    return okay && closed && length != 0u;
}

typedef enum VgDocumentFileState {
    VG_DOCUMENT_FILE_MISSING,
    VG_DOCUMENT_FILE_CURRENT,
    VG_DOCUMENT_FILE_FUTURE,
    VG_DOCUMENT_FILE_INVALID
} VgDocumentFileState;

static VgDocumentFileState vg_document_file_state(const char *path) {
    if (!vg_document_file_exists(path))
        return VG_DOCUMENT_FILE_MISSING;
    VgContentDocument *content = NULL;
    VgContentDiagnostic diagnostic;
    bool valid = vg_content_parse_file(path, &content, &diagnostic);
    vg_content_document_destroy(content);
    if (valid)
        return VG_DOCUMENT_FILE_CURRENT;
    if (diagnostic.code == VG_CONTENT_DIAGNOSTIC_VERSION)
        return VG_DOCUMENT_FILE_FUTURE;
    return VG_DOCUMENT_FILE_INVALID;
}

static bool vg_document_valid_file(const char *path) {
    return vg_document_file_state(path) == VG_DOCUMENT_FILE_CURRENT;
}

static bool vg_document_restore_backup(const char *path, const char *backup) {
    if (!vg_document_file_exists(backup) || !vg_document_valid_file(backup))
        return false;
    if (vg_document_file_exists(path) && remove(path) != 0)
        return false;
    return rename(backup, path) == 0;
}

static bool vg_document_recover_path(const char *path, VgDocumentDiagnostic *out_diagnostic) {
    char temporary[1024];
    char backup[1024];
    char journal[1024];
    if (!vg_document_sidecar_paths(path, temporary, backup, journal, out_diagnostic))
        return false;
    if (!vg_document_file_exists(journal))
        return true;
    char stage[16];
    if (!vg_document_read_stage(journal, stage))
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u, 0u,
                                "recovery journal cannot be read");
    if (vg_document_file_state(path) == VG_DOCUMENT_FILE_FUTURE)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_VALIDATION, "recovery", "$.version",
                                NULL, 0u, 0u,
                                "future document version cannot be replaced during recovery");
    if (strcmp(stage, "prepared") == 0) {
        if (!vg_document_file_exists(path)) {
            if (!vg_document_restore_backup(path, backup)) {
                if (!vg_document_file_exists(temporary) || !vg_document_valid_file(temporary) ||
                    rename(temporary, path) != 0)
                    return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL,
                                            0u, 0u,
                                            "prepared save has no valid backup or temporary");
            }
        }
        (void)remove(temporary);
        if (remove(journal) != 0)
            return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u, 0u,
                                    "cannot finalize restored save");
        return true;
    }
    if (strcmp(stage, "publish") == 0) {
        if (vg_document_file_exists(temporary) && vg_document_valid_file(temporary)) {
            if (vg_document_file_exists(path) && remove(path) != 0)
                return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u,
                                        0u, "cannot replace interrupted target");
            if (rename(temporary, path) != 0)
                return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u,
                                        0u, "cannot publish recovered temporary document");
        } else if (!vg_document_file_exists(path) && !vg_document_restore_backup(path, backup)) {
            return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u, 0u,
                                    "publish recovery has no valid document");
        }
    } else if (strcmp(stage, "published") != 0) {
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u, 0u,
                                "unknown recovery journal stage");
    }
    if (!vg_document_file_exists(path) || !vg_document_valid_file(path)) {
        if (!vg_document_restore_backup(path, backup))
            return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u, 0u,
                                    "published document and backup are invalid");
    }
    if (vg_document_file_exists(backup) && remove(backup) != 0)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u, 0u,
                                "cannot remove completed recovery backup");
    (void)remove(temporary);
    if (remove(journal) != 0)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "recovery", "$", NULL, 0u, 0u,
                                "cannot remove completed recovery journal");
    return true;
}

bool vg_document_save_atomic(VgDocument *document, const char *path, uint64_t expected_revision,
                             VgDocumentDiagnostic *out_diagnostic) {
    if (path == NULL || path[0] == '\0')
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "save", "$", NULL,
                                expected_revision, document != NULL ? document->revision : 0u,
                                "save path is required");
    if (!vg_document_check_revision(document, expected_revision, "save", out_diagnostic))
        return false;
    if (!vg_document_recover_path(path, out_diagnostic))
        return false;
    char temporary[1024];
    char backup[1024];
    char journal[1024];
    if (!vg_document_sidecar_paths(path, temporary, backup, journal, out_diagnostic))
        return false;
    if (vg_document_file_exists(backup))
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "orphan recovery backup exists; refusing to overwrite it");
    bool target_exists = vg_document_file_exists(path);
    if (target_exists) {
        VgContentDocument *existing = NULL;
        VgContentDiagnostic content_diagnostic;
        if (!vg_content_parse_file(path, &existing, &content_diagnostic))
            return vg_document_content_fail(out_diagnostic, "save", &content_diagnostic);
        vg_content_document_destroy(existing);
    }
    char *json = NULL;
    size_t length = 0u;
    if (!vg_document_write_canonical(document, &json, &length, out_diagnostic))
        return false;
    FILE *file = fopen(temporary, "wb");
    if (file == NULL) {
        free(json);
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "cannot create temporary document");
    }
    bool wrote = fwrite(json, 1u, length, file) == length && fflush(file) == 0;
    free(json);
    bool closed = fclose(file) == 0;
    if (!wrote || !closed) {
        (void)remove(temporary);
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "cannot write temporary document");
    }
    if (!vg_document_write_stage(journal, "prepared")) {
        (void)remove(temporary);
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "cannot create recovery journal");
    }
    bool moved_original = false;
    if (target_exists) {
        if (rename(path, backup) != 0) {
            (void)remove(temporary);
            (void)remove(journal);
            return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                    expected_revision, document->revision,
                                    "cannot create recovery backup");
        }
        moved_original = true;
    }
    if (document->save_failure_step == 1u)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "injected interruption after backup");
    if (!vg_document_write_stage(journal, "publish"))
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "cannot advance recovery journal");
    if (document->save_failure_step == 2u)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "injected interruption before publication");
    if (rename(temporary, path) != 0)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "cannot publish temporary document");
    if (!vg_document_write_stage(journal, "published"))
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "document published but recovery marker could not advance");
    if (moved_original && remove(backup) != 0)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "document published but backup cleanup failed");
    if (remove(journal) != 0)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_IO, "save", "$", NULL,
                                expected_revision, document->revision,
                                "document published but journal cleanup failed");
    (void)snprintf(document->file, sizeof(document->file), "%s", path);
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

static bool vg_document_move_history(VgDocument *document, VgDocumentHistory *source,
                                     VgDocumentHistory *destination, const char *operation,
                                     uint64_t expected_revision,
                                     VgDocumentDiagnostic *out_diagnostic) {
    if (!vg_document_check_revision(document, expected_revision, operation, out_diagnostic))
        return false;
    if (source->count == 0u)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_HISTORY_EMPTY, operation, "$", NULL,
                                expected_revision, document->revision, "history is empty");
    if (!vg_document_history_reserve(document, destination))
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, operation, "$", NULL,
                                expected_revision, document->revision,
                                "cannot reserve history destination");
    VgJsonNode *replacement = source->items[--source->count];
    destination->items[destination->count++] = document->root;
    document->root = replacement;
    ++document->revision;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

bool vg_document_undo(VgDocument *document, uint64_t expected_revision,
                      VgDocumentDiagnostic *out_diagnostic) {
    if (document == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "undo", "$", NULL,
                                expected_revision, 0u, "document is required");
    return vg_document_move_history(document, &document->undo, &document->redo, "undo",
                                    expected_revision, out_diagnostic);
}

bool vg_document_redo(VgDocument *document, uint64_t expected_revision,
                      VgDocumentDiagnostic *out_diagnostic) {
    if (document == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "redo", "$", NULL,
                                expected_revision, 0u, "document is required");
    return vg_document_move_history(document, &document->redo, &document->undo, "redo",
                                    expected_revision, out_diagnostic);
}

size_t vg_document_undo_count(const VgDocument *document) {
    return document == NULL ? 0u : document->undo.count;
}

size_t vg_document_redo_count(const VgDocument *document) {
    return document == NULL ? 0u : document->redo.count;
}

void vg_document_test_fail_allocations_after(VgDocument *document, size_t successes) {
    if (document != NULL)
        document->allocation_successes_remaining = successes;
}

void vg_document_test_fail_save_at_step(VgDocument *document, unsigned int step) {
    if (document != NULL)
        document->save_failure_step = step;
}
