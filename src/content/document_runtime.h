#ifndef VESTIGIO_CONTENT_DOCUMENT_RUNTIME_H
#define VESTIGIO_CONTENT_DOCUMENT_RUNTIME_H

#include "content/document.h"
#include "vestigio/vestigio.h"

#include <stddef.h>

typedef struct VgDocumentInstance VgDocumentInstance;

/* A successful resolver transfers one public asset lease to the instance.
 * Failed resolvers must leave out_asset unchanged. */
typedef VgResult (*VgDocumentAssetResolverFn)(void *user, VgContext *context, VgAssetId id,
                                              VgAssetType type, VgAsset *out_asset);

typedef struct VgDocumentInstanceDesc {
    VgDocumentAssetResolverFn resolve_asset;
    void *resolver_user;
} VgDocumentInstanceDesc;

/* Builds a complete candidate and assigns out_instance only after every entity,
 * transform, hierarchy, camera and asset reference succeeds. */
VgResult vg_document_instantiate(VgContext *context, const VgDocument *document,
                                 const VgDocumentInstanceDesc *description,
                                 VgDocumentInstance **out_instance,
                                 VgDocumentDiagnostic *out_diagnostic);
void vg_document_instance_destroy(VgDocumentInstance *instance);

VgWorld vg_document_instance_world(const VgDocumentInstance *instance);
size_t vg_document_instance_entity_count(const VgDocumentInstance *instance);
bool vg_document_instance_find_entity(const VgDocumentInstance *instance, VgUuid id,
                                      VgEntity *out_entity);
size_t vg_document_instance_asset_count(const VgDocumentInstance *instance);
bool vg_document_instance_asset_at(const VgDocumentInstance *instance, size_t index,
                                   VgAssetId *out_id, VgAsset *out_asset);

#endif
