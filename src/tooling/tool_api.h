#ifndef VESTIGIO_TOOLING_TOOL_API_H
#define VESTIGIO_TOOLING_TOOL_API_H

#include "content/document.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { VG_TOOL_MAX_COMMANDS = 256, VG_TOOL_MAX_TEMP_ID_BYTES = 63 };

typedef struct VgDocumentTransform {
    double position[3];
    double rotation[4];
    double scale[3];
} VgDocumentTransform;

typedef struct VgToolIdMapping {
    char temporary[VG_TOOL_MAX_TEMP_ID_BYTES + 1];
    char id[37];
} VgToolIdMapping;

typedef struct VgToolResult {
    uint64_t base_revision;
    uint64_t resulting_revision;
    size_t mapping_count;
    VgToolIdMapping mappings[VG_TOOL_MAX_COMMANDS];
} VgToolResult;

typedef struct VgToolBatch VgToolBatch;

bool vg_tool_begin(VgDocument *document, uint64_t expected_revision, VgToolBatch **out_batch,
                   VgDocumentDiagnostic *out_diagnostic);
void vg_tool_cancel(VgToolBatch *batch);

bool vg_tool_create_entity(VgToolBatch *batch, const char *id_or_temporary,
                           const char *parent_id_or_temporary, const VgDocumentTransform *transform,
                           const char *components_json, VgDocumentDiagnostic *out_diagnostic);
/* NULL preserves the source parent; "" explicitly makes the duplicate a root.
 * References between entities duplicated in the same batch are remapped. */
bool vg_tool_duplicate_entity(VgToolBatch *batch, const char *source_id,
                              const char *id_or_temporary, const char *parent_id_or_temporary,
                              VgDocumentDiagnostic *out_diagnostic);
bool vg_tool_delete_entity(VgToolBatch *batch, const char *id_or_temporary,
                           VgDocumentDiagnostic *out_diagnostic);
bool vg_tool_set_transform(VgToolBatch *batch, const char *id_or_temporary,
                           const VgDocumentTransform *transform,
                           VgDocumentDiagnostic *out_diagnostic);
bool vg_tool_reparent(VgToolBatch *batch, const char *id_or_temporary,
                      const char *parent_id_or_temporary, VgDocumentDiagnostic *out_diagnostic);
bool vg_tool_set_component(VgToolBatch *batch, const char *id_or_temporary,
                           const char *component_name, const char *component_json,
                           VgDocumentDiagnostic *out_diagnostic);
bool vg_tool_set_environment(VgToolBatch *batch, const char *environment_json,
                             VgDocumentDiagnostic *out_diagnostic);

/* validate and preview are dry runs. commit consumes the batch only after a
 * successful publication; a failed candidate remains cancelable/retryable. */
bool vg_tool_validate(const VgToolBatch *batch, VgToolResult *out_result,
                      VgDocumentDiagnostic *out_diagnostic);
bool vg_tool_preview(const VgToolBatch *batch, char **out_json, size_t *out_length,
                     VgToolResult *out_result, VgDocumentDiagnostic *out_diagnostic);
bool vg_tool_commit(VgToolBatch *batch, VgToolResult *out_result,
                    VgDocumentDiagnostic *out_diagnostic);

#endif
