#ifndef VESTIGIO_CONTENT_DOCUMENT_INTERNAL_H
#define VESTIGIO_CONTENT_DOCUMENT_INTERNAL_H

#include "content/document.h"
#include "content/json.h"

void *vg_document_allocate(VgDocument *document, size_t size);
VgJsonNode *vg_document_clone_json(VgDocument *document, const VgJsonNode *node);
const VgJsonNode *vg_document_root(const VgDocument *document);
bool vg_document_publish(VgDocument *document, VgJsonNode *candidate, uint64_t expected_revision,
                         VgDocumentDiagnostic *out_diagnostic);
bool vg_document_check_revision(const VgDocument *document, uint64_t expected_revision,
                                const char *operation, VgDocumentDiagnostic *out_diagnostic);
bool vg_document_fail(VgDocumentDiagnostic *diagnostic, VgDocumentStatus code,
                      const char *operation, const char *path, const char *id,
                      uint64_t expected_revision, uint64_t actual_revision, const char *message);

#endif
