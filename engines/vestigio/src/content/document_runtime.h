#ifndef VESTIGIO_CONTENT_DOCUMENT_RUNTIME_H
#define VESTIGIO_CONTENT_DOCUMENT_RUNTIME_H

#include "content/document.h"
#include "vestigio/spatial.h"
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
    /* Extra world slots for runtime-only entities. Zero preserves an exact
     * document-sized world for editor and existing consumers. */
    uint32_t runtime_entity_capacity;
} VgDocumentInstanceDesc;

/* The panel local transform is its closed pose. Rotate the hinge about local Z;
 * refresh the panel collider from its world transform after each pose update. */
typedef struct VgDocumentDoorBinding {
    VgUuid id;
    VgEntity panel;
    VgEntity hinge;
    VgSpatialCollider collider;
    VgSpatialColliderDesc collider_description;
    float open_angle_radians;
    float speed_radians_per_second;
} VgDocumentDoorBinding;

/* Scene values are authored in the level, independent from the user's visual
 * profile preference. Colors are linear RGB in [0,1], distances in metres. */
typedef struct VgDocumentEnvironment {
    float ambient_linear[3];
    float clear_linear[3];
    float fog_color_linear[3];
    float fog_start;
    float fog_end;
    bool fog_enabled;
} VgDocumentEnvironment;

typedef struct VgDocumentLightBinding {
    VgUuid id;
    VgEntity entity;
    float color_linear[3];
    float intensity;
    float range;
} VgDocumentLightBinding;

/* Builds a complete candidate and assigns out_instance only after every entity,
 * transform, hierarchy, component and asset reference succeeds. */
VgResult vg_document_instantiate(VgContext *context, const VgDocument *document,
                                 const VgDocumentInstanceDesc *description,
                                 VgDocumentInstance **out_instance,
                                 VgDocumentDiagnostic *out_diagnostic);
void vg_document_instance_destroy(VgDocumentInstance *instance);

VgWorld vg_document_instance_world(const VgDocumentInstance *instance);
/* Owned by the instance; NULL when the document has no colliders. */
VgSpatialScene *vg_document_instance_spatial(VgDocumentInstance *instance);
size_t vg_document_instance_entity_count(const VgDocumentInstance *instance);
bool vg_document_instance_entity_at(const VgDocumentInstance *instance, size_t index,
                                    VgUuid *out_id, VgEntity *out_entity);
bool vg_document_instance_find_entity(const VgDocumentInstance *instance, VgUuid id,
                                      VgEntity *out_entity);
bool vg_document_instance_find_collider(const VgDocumentInstance *instance, VgUuid id,
                                        VgSpatialCollider *out_collider,
                                        VgSpatialColliderDesc *out_description);
size_t vg_document_instance_door_count(const VgDocumentInstance *instance);
bool vg_document_instance_door_at(const VgDocumentInstance *instance, size_t index,
                                  VgDocumentDoorBinding *out_door);
bool vg_document_instance_environment(const VgDocumentInstance *instance,
                                      VgDocumentEnvironment *out_environment);
size_t vg_document_instance_light_count(const VgDocumentInstance *instance);
bool vg_document_instance_light_at(const VgDocumentInstance *instance, size_t index,
                                   VgDocumentLightBinding *out_light);
size_t vg_document_instance_asset_count(const VgDocumentInstance *instance);
bool vg_document_instance_asset_at(const VgDocumentInstance *instance, size_t index,
                                   VgAssetId *out_id, VgAsset *out_asset);

#endif
