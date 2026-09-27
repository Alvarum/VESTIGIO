#ifndef VESTIGIO_CONTENT_DOCUMENT_H
#define VESTIGIO_CONTENT_DOCUMENT_H

#include "content/content.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum VgDocumentStatus {
    VG_DOCUMENT_OK = 0,
    VG_DOCUMENT_INVALID_ARGUMENT,
    VG_DOCUMENT_CONFLICT,
    VG_DOCUMENT_NOT_FOUND,
    VG_DOCUMENT_DUPLICATE,
    VG_DOCUMENT_INVALID_BATCH,
    VG_DOCUMENT_VALIDATION,
    VG_DOCUMENT_OUT_OF_MEMORY,
    VG_DOCUMENT_HISTORY_EMPTY,
    VG_DOCUMENT_IO
} VgDocumentStatus;

typedef struct VgDocumentDiagnostic {
    VgDocumentStatus code;
    uint64_t expected_revision;
    uint64_t actual_revision;
    char operation[32];
    char file[512];
    char path[512];
    char id[64];
    char message[256];
    VgContentDiagnostic content;
} VgDocumentDiagnostic;

typedef struct VgDocument VgDocument;

bool vg_document_open_memory(const char *file_name, const char *json, size_t length,
                             VgDocument **out_document, VgDocumentDiagnostic *out_diagnostic);
bool vg_document_open_file(const char *path, VgDocument **out_document,
                           VgDocumentDiagnostic *out_diagnostic);
void vg_document_destroy(VgDocument *document);
uint64_t vg_document_revision(const VgDocument *document);
bool vg_document_write_canonical(const VgDocument *document, char **out_json, size_t *out_length,
                                 VgDocumentDiagnostic *out_diagnostic);
/* Saves through a same-directory temporary, backup and journal. Recovery never
 * replaces an existing document whose version is newer than this library. */
bool vg_document_save_atomic(VgDocument *document, const char *path, uint64_t expected_revision,
                             VgDocumentDiagnostic *out_diagnostic);
bool vg_document_undo(VgDocument *document, uint64_t expected_revision,
                      VgDocumentDiagnostic *out_diagnostic);
bool vg_document_redo(VgDocument *document, uint64_t expected_revision,
                      VgDocumentDiagnostic *out_diagnostic);
size_t vg_document_undo_count(const VgDocument *document);
size_t vg_document_redo_count(const VgDocument *document);

/* History owns document trees only. Asset UUID/path references are retained as
 * JSON values; decoded blobs and CPU/GPU resident resources are never copied. */

/* Deterministic failure hooks for transaction/recovery tests. SIZE_MAX disables
 * allocation failure; save step 0 disables it and step 1 fails after backup. */
void vg_document_test_fail_allocations_after(VgDocument *document, size_t successes);
void vg_document_test_fail_save_at_step(VgDocument *document, unsigned int step);

#endif
