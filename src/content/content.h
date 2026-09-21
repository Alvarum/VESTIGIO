#ifndef VESTIGIO_CONTENT_CONTENT_H
#define VESTIGIO_CONTENT_CONTENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    VG_CONTENT_MAX_FILE_BYTES = 1024 * 1024,
    VG_CONTENT_MAX_TOKENS = 32768,
    VG_CONTENT_MAX_DEPTH = 64,
    VG_CONTENT_MAX_STRING_BYTES = 65535,
    VG_CONTENT_MAX_ENTITIES = 1023,
    VG_CONTENT_MAX_SECTORS = 256,
    VG_CONTENT_MAX_VERTICES_PER_SECTOR = 64
};

typedef enum VgContentKind {
    VG_CONTENT_UNKNOWN = 0,
    VG_CONTENT_PROJECT = 1,
    VG_CONTENT_LEVEL = 2
} VgContentKind;

typedef enum VgContentDiagnosticCode {
    VG_CONTENT_DIAGNOSTIC_NONE = 0,
    VG_CONTENT_DIAGNOSTIC_IO,
    VG_CONTENT_DIAGNOSTIC_PARSE,
    VG_CONTENT_DIAGNOSTIC_LIMIT,
    VG_CONTENT_DIAGNOSTIC_TYPE,
    VG_CONTENT_DIAGNOSTIC_FORMAT,
    VG_CONTENT_DIAGNOSTIC_VERSION,
    VG_CONTENT_DIAGNOSTIC_REQUIRED,
    VG_CONTENT_DIAGNOSTIC_UUID,
    VG_CONTENT_DIAGNOSTIC_DUPLICATE,
    VG_CONTENT_DIAGNOSTIC_REFERENCE,
    VG_CONTENT_DIAGNOSTIC_TRANSFORM,
    VG_CONTENT_DIAGNOSTIC_CAPACITY
} VgContentDiagnosticCode;

typedef struct VgContentDiagnostic {
    VgContentDiagnosticCode code;
    size_t byte_offset;
    char file[512];
    char path[512];
    char id[37];
    char message[256];
} VgContentDiagnostic;

typedef struct VgContentDocument VgContentDocument;

bool vg_content_parse_memory(const char *file_name, const char *json, size_t length,
                             VgContentDocument **out_document, VgContentDiagnostic *out_diagnostic);
bool vg_content_parse_file(const char *path, VgContentDocument **out_document,
                           VgContentDiagnostic *out_diagnostic);
bool vg_content_write_canonical(const VgContentDocument *document, char **out_json,
                                size_t *out_length, VgContentDiagnostic *out_diagnostic);
void vg_content_document_destroy(VgContentDocument *document);
void vg_content_string_destroy(char *string);
VgContentKind vg_content_document_kind(const VgContentDocument *document);
const char *vg_content_document_id(const VgContentDocument *document);
size_t vg_content_level_entity_count(const VgContentDocument *document);

#endif
