#include "content/document_runtime.h"

#include "content/document_internal.h"
#include "content/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VgDocumentEntityBinding {
    VgUuid id;
    VgEntity entity;
} VgDocumentEntityBinding;

typedef struct VgDocumentAssetBinding {
    VgAssetId id;
    VgAsset asset;
} VgDocumentAssetBinding;

struct VgDocumentInstance {
    VgContext *context;
    VgWorld world;
    VgDocumentEntityBinding *entities;
    size_t entity_count;
    VgDocumentAssetBinding *assets;
    size_t asset_count;
};

static VgResult vg_document_runtime_fail(VgDocumentDiagnostic *diagnostic, VgDocumentStatus status,
                                         VgResult result, const char *path, const char *id,
                                         const char *action) {
    char message[256];
    (void)snprintf(message, sizeof(message), "%s failed with runtime result %d", action,
                   (int)result);
    (void)vg_document_fail(diagnostic, status, "instantiate", path, id, 0u, 0u, message);
    return result;
}

static VgResult vg_document_runtime_invalid(VgDocumentDiagnostic *diagnostic, const char *path,
                                            const char *id, const char *message) {
    (void)vg_document_fail(diagnostic, VG_DOCUMENT_VALIDATION, "instantiate", path, id, 0u, 0u,
                           message);
    return VG_ERROR_INVALID_ARGUMENT;
}

static int vg_document_hex(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static bool vg_document_parse_uuid(const char *text, uint8_t output[16]) {
    if (text == NULL || strlen(text) != 36u)
        return false;
    size_t source = 0u;
    size_t target = 0u;
    while (source < 36u) {
        if (source == 8u || source == 13u || source == 18u || source == 23u) {
            if (text[source++] != '-')
                return false;
            continue;
        }
        int high = vg_document_hex(text[source++]);
        int low = vg_document_hex(text[source++]);
        if (high < 0 || low < 0 || target >= 16u)
            return false;
        output[target++] = (uint8_t)((high << 4) | low);
    }
    return target == 16u;
}

static bool vg_document_uuid_equal(const uint8_t left[16], const uint8_t right[16]) {
    return memcmp(left, right, 16u) == 0;
}

static const char *vg_document_node_string(const VgJsonNode *node) {
    return node != NULL && node->type == VG_JSON_STRING ? node->as.string.data : NULL;
}

static bool vg_document_read_vector(const VgJsonNode *node, float *output, size_t count) {
    if (node == NULL || node->type != VG_JSON_ARRAY || node->as.array.count != count)
        return false;
    for (size_t index = 0u; index < count; ++index) {
        const VgJsonNode *item = node->as.array.items[index];
        if (item == NULL || item->type != VG_JSON_NUMBER)
            return false;
        output[index] = (float)item->as.number.value;
    }
    return true;
}

static bool vg_document_read_transform(const VgJsonNode *entity, VgTransform *output) {
    const VgJsonNode *transform = vg_json_object_get(entity, "transform");
    if (transform == NULL || transform->type != VG_JSON_OBJECT)
        return false;
    return vg_document_read_vector(vg_json_object_get(transform, "position"), &output->position.x,
                                   3u) &&
           vg_document_read_vector(vg_json_object_get(transform, "rotation"), &output->rotation.x,
                                   4u) &&
           vg_document_read_vector(vg_json_object_get(transform, "scale"), &output->scale.x, 3u);
}

static bool vg_document_find_binding(const VgDocumentInstance *instance, const uint8_t id[16],
                                     VgEntity *out_entity) {
    for (size_t index = 0u; index < instance->entity_count; ++index) {
        if (vg_document_uuid_equal(instance->entities[index].id.bytes, id)) {
            if (out_entity != NULL)
                *out_entity = instance->entities[index].entity;
            return true;
        }
    }
    return false;
}

static VgResult vg_document_resolve_mesh(VgDocumentInstance *instance,
                                         const VgDocumentInstanceDesc *description,
                                         const VgJsonNode *component, const char *entity_id,
                                         VgDocumentDiagnostic *diagnostic) {
    const char *asset_text = vg_document_node_string(vg_json_object_get(component, "asset"));
    VgAssetId asset_id = {{0}};
    if (!vg_document_parse_uuid(asset_text, asset_id.bytes))
        return vg_document_runtime_invalid(diagnostic, "$.entities[].components.engine.mesh.asset",
                                           entity_id, "mesh asset UUID is invalid");
    for (size_t index = 0u; index < instance->asset_count; ++index) {
        if (vg_document_uuid_equal(instance->assets[index].id.bytes, asset_id.bytes))
            return VG_OK;
    }
    if (description == NULL || description->resolve_asset == NULL)
        return vg_document_runtime_invalid(diagnostic, "$.entities[].components.engine.mesh.asset",
                                           entity_id, "mesh asset resolver is required");
    VgAsset asset = {VG_INVALID_HANDLE_VALUE};
    VgResult result = description->resolve_asset(description->resolver_user, instance->context,
                                                 asset_id, VG_ASSET_TYPE_MESH, &asset);
    if (result != VG_OK)
        return vg_document_runtime_fail(diagnostic,
                                        result == VG_ERROR_OUT_OF_MEMORY ? VG_DOCUMENT_OUT_OF_MEMORY
                                                                         : VG_DOCUMENT_VALIDATION,
                                        result, "$.entities[].components.engine.mesh.asset",
                                        entity_id, "mesh asset resolution");
    instance->assets[instance->asset_count++] = (VgDocumentAssetBinding){asset_id, asset};
    return VG_OK;
}

static VgResult vg_document_apply_components(VgDocumentInstance *instance,
                                             const VgDocumentInstanceDesc *description,
                                             const VgJsonNode *entity, VgEntity runtime_entity,
                                             const char *entity_id,
                                             VgDocumentDiagnostic *diagnostic) {
    const VgJsonNode *components = vg_json_object_get(entity, "components");
    if (components == NULL || components->type != VG_JSON_OBJECT)
        return VG_OK;
    const VgJsonNode *camera = vg_json_object_get(components, "engine.camera");
    if (camera != NULL) {
        const VgJsonNode *fov = vg_json_object_get(camera, "fov_y_radians");
        const VgJsonNode *near_plane = vg_json_object_get(camera, "near");
        const VgJsonNode *far_plane = vg_json_object_get(camera, "far");
        if (fov == NULL || near_plane == NULL || far_plane == NULL)
            return vg_document_runtime_invalid(diagnostic, "$.entities[].components.engine.camera",
                                               entity_id, "camera fields are missing");
        VgCameraDesc camera_description = {0};
        camera_description.struct_size = sizeof(camera_description);
        camera_description.api_version = VG_API_VERSION;
        camera_description.projection = VG_CAMERA_PERSPECTIVE;
        camera_description.vertical_fov_radians = (float)fov->as.number.value;
        camera_description.near_clip_metres = (float)near_plane->as.number.value;
        camera_description.far_clip_metres = (float)far_plane->as.number.value;
        VgResult result = vg_camera_set(instance->context, runtime_entity, &camera_description);
        if (result != VG_OK)
            return vg_document_runtime_fail(diagnostic, VG_DOCUMENT_VALIDATION, result,
                                            "$.entities[].components.engine.camera", entity_id,
                                            "camera creation");
    }
    const VgJsonNode *mesh = vg_json_object_get(components, "engine.mesh");
    if (mesh != NULL)
        return vg_document_resolve_mesh(instance, description, mesh, entity_id, diagnostic);
    return VG_OK;
}

static void vg_document_instance_cleanup(VgDocumentInstance *instance) {
    if (instance == NULL)
        return;
    for (size_t index = instance->asset_count; index > 0u; --index)
        (void)vg_asset_release(instance->context, instance->assets[index - 1u].asset);
    if (instance->world.value != VG_INVALID_HANDLE_VALUE)
        (void)vg_world_destroy(instance->context, instance->world);
    free(instance->assets);
    free(instance->entities);
    free(instance);
}

VgResult vg_document_instantiate(VgContext *context, const VgDocument *document,
                                 const VgDocumentInstanceDesc *description,
                                 VgDocumentInstance **out_instance,
                                 VgDocumentDiagnostic *out_diagnostic) {
    if (context == NULL || document == NULL || out_instance == NULL)
        return vg_document_runtime_invalid(out_diagnostic, "$", NULL,
                                           "context, document and output are required");

    char *canonical = NULL;
    size_t canonical_length = 0u;
    VgDocumentDiagnostic write_diagnostic;
    if (!vg_document_write_canonical(document, &canonical, &canonical_length, &write_diagnostic)) {
        if (out_diagnostic != NULL)
            *out_diagnostic = write_diagnostic;
        return write_diagnostic.code == VG_DOCUMENT_OUT_OF_MEMORY ? VG_ERROR_OUT_OF_MEMORY
                                                                  : VG_ERROR_INVALID_ARGUMENT;
    }
    VgContentDocument *validated = NULL;
    VgContentDiagnostic content_diagnostic;
    bool valid = vg_content_parse_memory("<runtime-candidate>", canonical, canonical_length,
                                         &validated, &content_diagnostic);
    vg_content_string_destroy(canonical);
    vg_content_document_destroy(validated);
    if (!valid) {
        if (out_diagnostic != NULL) {
            memset(out_diagnostic, 0, sizeof(*out_diagnostic));
            out_diagnostic->code = VG_DOCUMENT_VALIDATION;
            out_diagnostic->content = content_diagnostic;
            (void)snprintf(out_diagnostic->operation, sizeof(out_diagnostic->operation),
                           "instantiate");
            (void)snprintf(out_diagnostic->file, sizeof(out_diagnostic->file), "%s",
                           content_diagnostic.file);
            (void)snprintf(out_diagnostic->path, sizeof(out_diagnostic->path), "%s",
                           content_diagnostic.path);
            (void)snprintf(out_diagnostic->id, sizeof(out_diagnostic->id), "%s",
                           content_diagnostic.id);
            (void)snprintf(out_diagnostic->message, sizeof(out_diagnostic->message), "%s",
                           content_diagnostic.message);
        }
        return VG_ERROR_INVALID_ARGUMENT;
    }

    const VgJsonNode *root = vg_document_root(document);
    const char *format = vg_document_node_string(vg_json_object_get(root, "format"));
    if (format == NULL || strcmp(format, "vestigio.level") != 0)
        return vg_document_runtime_invalid(out_diagnostic, "$.format", NULL,
                                           "only level documents can be instantiated");
    const VgJsonNode *entities = vg_json_object_get(root, "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return vg_document_runtime_invalid(out_diagnostic, "$.entities", NULL,
                                           "level entities array is required");

    VgDocumentInstance *candidate = calloc(1u, sizeof(*candidate));
    if (candidate == NULL)
        return vg_document_runtime_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY,
                                        VG_ERROR_OUT_OF_MEMORY, "$", NULL, "instance allocation");
    candidate->context = context;
    candidate->entity_count = entities->as.array.count;
    if (candidate->entity_count != 0u) {
        candidate->entities = calloc(candidate->entity_count, sizeof(*candidate->entities));
        candidate->assets = calloc(candidate->entity_count, sizeof(*candidate->assets));
        if (candidate->entities == NULL || candidate->assets == NULL) {
            vg_document_instance_cleanup(candidate);
            return vg_document_runtime_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY,
                                            VG_ERROR_OUT_OF_MEMORY, "$", NULL,
                                            "instance tables allocation");
        }
    }

    VgWorldDesc world_description = {0};
    world_description.struct_size = sizeof(world_description);
    world_description.api_version = VG_API_VERSION;
    world_description.initial_entity_capacity = (uint32_t)candidate->entity_count;
    world_description.max_entities = (uint32_t)candidate->entity_count;
    VgResult result = vg_world_create(context, &world_description, &candidate->world);
    if (result != VG_OK) {
        vg_document_instance_cleanup(candidate);
        return vg_document_runtime_fail(out_diagnostic,
                                        result == VG_ERROR_OUT_OF_MEMORY ? VG_DOCUMENT_OUT_OF_MEMORY
                                                                         : VG_DOCUMENT_VALIDATION,
                                        result, "$.entities", NULL, "world creation");
    }

    for (size_t index = 0u; index < candidate->entity_count; ++index) {
        const VgJsonNode *entity = entities->as.array.items[index];
        const char *id = vg_document_node_string(vg_json_object_get(entity, "id"));
        if (!vg_document_parse_uuid(id, candidate->entities[index].id.bytes)) {
            result = vg_document_runtime_invalid(out_diagnostic, "$.entities[].id", id,
                                                 "entity UUID is invalid");
            goto fail;
        }
        result = vg_entity_create(context, candidate->world, &candidate->entities[index].entity);
        if (result != VG_OK) {
            result = vg_document_runtime_fail(out_diagnostic,
                                              result == VG_ERROR_OUT_OF_MEMORY
                                                  ? VG_DOCUMENT_OUT_OF_MEMORY
                                                  : VG_DOCUMENT_VALIDATION,
                                              result, "$.entities[]", id, "entity creation");
            goto fail;
        }
        VgTransform transform;
        if (!vg_document_read_transform(entity, &transform)) {
            result = vg_document_runtime_invalid(out_diagnostic, "$.entities[].transform", id,
                                                 "entity transform is invalid");
            goto fail;
        }
        result =
            vg_entity_set_local_transform(context, candidate->entities[index].entity, &transform);
        if (result != VG_OK) {
            result = vg_document_runtime_fail(out_diagnostic, VG_DOCUMENT_VALIDATION, result,
                                              "$.entities[].transform", id, "transform creation");
            goto fail;
        }
        result = vg_document_apply_components(
            candidate, description, entity, candidate->entities[index].entity, id, out_diagnostic);
        if (result != VG_OK)
            goto fail;
    }

    for (size_t index = 0u; index < candidate->entity_count; ++index) {
        const VgJsonNode *entity = entities->as.array.items[index];
        const VgJsonNode *parent_node = vg_json_object_get(entity, "parent");
        if (parent_node == NULL || parent_node->type == VG_JSON_NULL)
            continue;
        const char *parent_text = vg_document_node_string(parent_node);
        uint8_t parent_id[16];
        VgEntity parent = {VG_INVALID_HANDLE_VALUE};
        const char *id = vg_document_node_string(vg_json_object_get(entity, "id"));
        if (!vg_document_parse_uuid(parent_text, parent_id) ||
            !vg_document_find_binding(candidate, parent_id, &parent)) {
            result = vg_document_runtime_invalid(out_diagnostic, "$.entities[].parent", id,
                                                 "parent entity is not present");
            goto fail;
        }
        result = vg_entity_set_parent(context, candidate->entities[index].entity, parent,
                                      VG_REPARENT_KEEP_LOCAL);
        if (result != VG_OK) {
            result = vg_document_runtime_fail(out_diagnostic, VG_DOCUMENT_VALIDATION, result,
                                              "$.entities[].parent", id, "parent creation");
            goto fail;
        }
    }

    *out_instance = candidate;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return VG_OK;

fail:
    vg_document_instance_cleanup(candidate);
    return result;
}

void vg_document_instance_destroy(VgDocumentInstance *instance) {
    vg_document_instance_cleanup(instance);
}

VgWorld vg_document_instance_world(const VgDocumentInstance *instance) {
    return instance == NULL ? (VgWorld){VG_INVALID_HANDLE_VALUE} : instance->world;
}

size_t vg_document_instance_entity_count(const VgDocumentInstance *instance) {
    return instance == NULL ? 0u : instance->entity_count;
}

bool vg_document_instance_find_entity(const VgDocumentInstance *instance, VgUuid id,
                                      VgEntity *out_entity) {
    return instance != NULL && out_entity != NULL &&
           vg_document_find_binding(instance, id.bytes, out_entity);
}

size_t vg_document_instance_asset_count(const VgDocumentInstance *instance) {
    return instance == NULL ? 0u : instance->asset_count;
}

bool vg_document_instance_asset_at(const VgDocumentInstance *instance, size_t index,
                                   VgAssetId *out_id, VgAsset *out_asset) {
    if (instance == NULL || index >= instance->asset_count || out_id == NULL || out_asset == NULL)
        return false;
    *out_id = instance->assets[index].id;
    *out_asset = instance->assets[index].asset;
    return true;
}
