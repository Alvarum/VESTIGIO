#include "tooling/tool_api.h"

#include "content/document_internal.h"
#include "content/json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum VgToolCommandType {
    VG_TOOL_CREATE,
    VG_TOOL_DUPLICATE,
    VG_TOOL_DELETE,
    VG_TOOL_SET_TRANSFORM,
    VG_TOOL_REPARENT,
    VG_TOOL_SET_COMPONENT,
    VG_TOOL_SET_ENVIRONMENT
} VgToolCommandType;

typedef struct VgToolCommand {
    VgToolCommandType type;
    char primary[VG_TOOL_MAX_TEMP_ID_BYTES + 1];
    char secondary[VG_TOOL_MAX_TEMP_ID_BYTES + 1];
    char component[128];
    VgDocumentTransform transform;
    char *json;
    bool has_secondary;
} VgToolCommand;

struct VgToolBatch {
    VgDocument *document;
    uint64_t expected_revision;
    size_t command_count;
    VgToolCommand commands[VG_TOOL_MAX_COMMANDS];
    size_t mapping_count;
    VgToolIdMapping mappings[VG_TOOL_MAX_COMMANDS];
};

static bool vg_tool_fail(VgDocumentDiagnostic *diagnostic, VgDocumentStatus code,
                         const char *operation, const char *path, const char *id,
                         const VgToolBatch *batch, const char *message) {
    return vg_document_fail(diagnostic, code, operation, path, id,
                            batch != NULL ? batch->expected_revision : 0u,
                            batch != NULL ? vg_document_revision(batch->document) : 0u, message);
}

static bool vg_tool_uuid(const char *text) {
    if (text == NULL || strlen(text) != 36u)
        return false;
    for (size_t index = 0u; index < 36u; ++index) {
        bool dash = index == 8u || index == 13u || index == 18u || index == 23u;
        if (dash ? text[index] != '-'
                 : !((text[index] >= '0' && text[index] <= '9') ||
                     (text[index] >= 'a' && text[index] <= 'f')))
            return false;
    }
    return strcmp(text, "00000000-0000-0000-0000-000000000000") != 0;
}

static bool vg_tool_reference(const char *text) {
    return text != NULL && (vg_tool_uuid(text) || (text[0] == '$' && text[1] != '\0' &&
                                                   strlen(text) <= VG_TOOL_MAX_TEMP_ID_BYTES));
}

static uint64_t vg_tool_hash(uint64_t hash, const char *text) {
    while (*text != '\0') {
        hash ^= (unsigned char)*text++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void vg_tool_make_uuid(const VgToolBatch *batch, const char *temporary, size_t ordinal,
                              char output[37]) {
    const VgJsonNode *id = vg_json_object_get(vg_document_root(batch->document), "id");
    const char *document_id =
        id != NULL && id->type == VG_JSON_STRING ? id->as.string.data : "document";
    uint64_t high = vg_tool_hash(UINT64_C(1469598103934665603), document_id);
    high = vg_tool_hash(high, temporary);
    uint64_t low = vg_tool_hash(UINT64_C(7809847782465536322), temporary);
    low ^= batch->expected_revision * UINT64_C(0x9e3779b97f4a7c15);
    low ^= (uint64_t)ordinal * UINT64_C(0xbf58476d1ce4e5b9);
    unsigned char bytes[16];
    for (size_t index = 0u; index < 8u; ++index) {
        bytes[index] = (unsigned char)(high >> (index * 8u));
        bytes[index + 8u] = (unsigned char)(low >> (index * 8u));
    }
    bytes[6] = (unsigned char)((bytes[6] & 0x0fu) | 0x40u);
    bytes[8] = (unsigned char)((bytes[8] & 0x3fu) | 0x80u);
    (void)snprintf(output, 37u,
                   "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                   "%02x%02x%02x%02x%02x%02x",
                   bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                   bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14],
                   bytes[15]);
}

static const char *vg_tool_resolve(const VgToolBatch *batch, const char *reference) {
    if (reference == NULL || reference[0] != '$')
        return reference;
    for (size_t index = 0u; index < batch->mapping_count; ++index) {
        if (strcmp(batch->mappings[index].temporary, reference) == 0)
            return batch->mappings[index].id;
    }
    return NULL;
}

static bool vg_tool_register_id(VgToolBatch *batch, const char *id_or_temporary,
                                VgDocumentDiagnostic *diagnostic) {
    if (vg_tool_uuid(id_or_temporary))
        return true;
    if (!vg_tool_reference(id_or_temporary))
        return vg_tool_fail(diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$.id",
                            id_or_temporary, batch,
                            "entity id must be a UUID or a $temporary identifier");
    for (size_t index = 0u; index < batch->mapping_count; ++index) {
        if (strcmp(batch->mappings[index].temporary, id_or_temporary) == 0)
            return vg_tool_fail(diagnostic, VG_DOCUMENT_DUPLICATE, "queue", "$.id", id_or_temporary,
                                batch, "temporary identifier is already defined");
    }
    VgToolIdMapping *mapping = &batch->mappings[batch->mapping_count];
    (void)snprintf(mapping->temporary, sizeof(mapping->temporary), "%s", id_or_temporary);
    vg_tool_make_uuid(batch, id_or_temporary, batch->mapping_count, mapping->id);
    ++batch->mapping_count;
    return true;
}

static bool vg_tool_copy_reference(char output[VG_TOOL_MAX_TEMP_ID_BYTES + 1], const char *value,
                                   bool allow_null, VgToolBatch *batch, const char *path,
                                   VgDocumentDiagnostic *diagnostic) {
    if (value == NULL && allow_null) {
        output[0] = '\0';
        return true;
    }
    if (!vg_tool_reference(value))
        return vg_tool_fail(diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", path, value, batch,
                            "reference must be a UUID or defined $temporary identifier");
    (void)snprintf(output, VG_TOOL_MAX_TEMP_ID_BYTES + 1u, "%s", value);
    return true;
}

static bool vg_tool_json_object(const char *json) {
    if (json == NULL)
        return false;
    VgJsonError error = {0};
    VgJsonNode *node = vg_json_parse(json, strlen(json), &error);
    bool valid = node != NULL && node->type == VG_JSON_OBJECT;
    vg_json_destroy(node);
    return valid;
}

static VgToolCommand *vg_tool_queue(VgToolBatch *batch, VgDocumentDiagnostic *diagnostic) {
    if (batch == NULL) {
        (void)vg_tool_fail(diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$", NULL, batch,
                           "batch is required");
        return NULL;
    }
    if (batch->command_count >= VG_TOOL_MAX_COMMANDS) {
        (void)vg_tool_fail(diagnostic, VG_DOCUMENT_INVALID_BATCH, "queue", "$", NULL, batch,
                           "batch command limit exceeded");
        return NULL;
    }
    VgToolCommand *command = &batch->commands[batch->command_count++];
    memset(command, 0, sizeof(*command));
    return command;
}

bool vg_tool_begin(VgDocument *document, uint64_t expected_revision, VgToolBatch **out_batch,
                   VgDocumentDiagnostic *out_diagnostic) {
    if (out_batch == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "begin", "$", NULL,
                                expected_revision, vg_document_revision(document),
                                "batch output is required");
    if (!vg_document_check_revision(document, expected_revision, "begin", out_diagnostic))
        return false;
    VgToolBatch *batch = calloc(1u, sizeof(*batch));
    if (batch == NULL)
        return vg_document_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "begin", "$", NULL,
                                expected_revision, vg_document_revision(document),
                                "cannot allocate batch");
    batch->document = document;
    batch->expected_revision = expected_revision;
    *out_batch = batch;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

void vg_tool_cancel(VgToolBatch *batch) {
    if (batch == NULL)
        return;
    for (size_t index = 0u; index < batch->command_count; ++index)
        free(batch->commands[index].json);
    free(batch);
}

bool vg_tool_create_entity(VgToolBatch *batch, const char *id_or_temporary,
                           const char *parent_id_or_temporary, const VgDocumentTransform *transform,
                           const char *components_json, VgDocumentDiagnostic *out_diagnostic) {
    if (batch == NULL || transform == NULL)
        return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$.transform",
                            id_or_temporary, batch, "batch and transform are required");
    size_t mapping_count = batch->mapping_count;
    VgToolCommand *command = vg_tool_queue(batch, out_diagnostic);
    if (command == NULL)
        return false;
    if (!vg_tool_register_id(batch, id_or_temporary, out_diagnostic)) {
        --batch->command_count;
        return false;
    }
    command->type = VG_TOOL_CREATE;
    (void)snprintf(command->primary, sizeof(command->primary), "%s", id_or_temporary);
    if (!vg_tool_copy_reference(command->secondary, parent_id_or_temporary, true, batch, "$.parent",
                                out_diagnostic)) {
        --batch->command_count;
        batch->mapping_count = mapping_count;
        return false;
    }
    command->has_secondary = parent_id_or_temporary != NULL;
    command->transform = *transform;
    if (components_json != NULL) {
        if (!vg_tool_json_object(components_json)) {
            --batch->command_count;
            batch->mapping_count = mapping_count;
            return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue",
                                "$.components", id_or_temporary, batch,
                                "components must be one JSON object");
        }
        command->json = malloc(strlen(components_json) + 1u);
        if (command->json == NULL) {
            --batch->command_count;
            batch->mapping_count = mapping_count;
            return vg_tool_fail(out_diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "queue", "$.components",
                                id_or_temporary, batch, "cannot copy components");
        }
        (void)strcpy(command->json, components_json);
    }
    return true;
}

bool vg_tool_duplicate_entity(VgToolBatch *batch, const char *source_id,
                              const char *id_or_temporary, const char *parent_id_or_temporary,
                              VgDocumentDiagnostic *out_diagnostic) {
    if (batch == NULL || !vg_tool_reference(source_id))
        return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$.source",
                            source_id, batch, "duplicate source must be an entity reference");
    size_t mapping_count = batch->mapping_count;
    VgToolCommand *command = vg_tool_queue(batch, out_diagnostic);
    if (command == NULL)
        return false;
    if (!vg_tool_register_id(batch, id_or_temporary, out_diagnostic)) {
        --batch->command_count;
        return false;
    }
    command->type = VG_TOOL_DUPLICATE;
    (void)snprintf(command->primary, sizeof(command->primary), "%s", source_id);
    if (parent_id_or_temporary != NULL) {
        command->has_secondary = true;
        if (parent_id_or_temporary[0] == '\0') {
            command->secondary[0] = '\0';
        } else if (!vg_tool_copy_reference(command->secondary, parent_id_or_temporary, false, batch,
                                           "$.parent", out_diagnostic)) {
            --batch->command_count;
            batch->mapping_count = mapping_count;
            return false;
        }
    }
    (void)snprintf(command->component, sizeof(command->component), "%s", id_or_temporary);
    return true;
}

bool vg_tool_delete_entity(VgToolBatch *batch, const char *id_or_temporary,
                           VgDocumentDiagnostic *out_diagnostic) {
    VgToolCommand *command = NULL;
    if (batch == NULL || !vg_tool_reference(id_or_temporary))
        return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$.id",
                            id_or_temporary, batch, "delete requires an entity reference");
    command = vg_tool_queue(batch, out_diagnostic);
    if (command == NULL)
        return false;
    command->type = VG_TOOL_DELETE;
    (void)snprintf(command->primary, sizeof(command->primary), "%s", id_or_temporary);
    return true;
}

bool vg_tool_set_transform(VgToolBatch *batch, const char *id_or_temporary,
                           const VgDocumentTransform *transform,
                           VgDocumentDiagnostic *out_diagnostic) {
    VgToolCommand *command = NULL;
    if (batch == NULL || transform == NULL || !vg_tool_reference(id_or_temporary))
        return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$.transform",
                            id_or_temporary, batch, "set_transform requires an id and transform");
    command = vg_tool_queue(batch, out_diagnostic);
    if (command == NULL)
        return false;
    command->type = VG_TOOL_SET_TRANSFORM;
    (void)snprintf(command->primary, sizeof(command->primary), "%s", id_or_temporary);
    command->transform = *transform;
    return true;
}

bool vg_tool_reparent(VgToolBatch *batch, const char *id_or_temporary,
                      const char *parent_id_or_temporary, VgDocumentDiagnostic *out_diagnostic) {
    VgToolCommand *command = NULL;
    if (batch == NULL || !vg_tool_reference(id_or_temporary))
        return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$.parent",
                            id_or_temporary, batch, "reparent requires an entity reference");
    command = vg_tool_queue(batch, out_diagnostic);
    if (command == NULL)
        return false;
    command->type = VG_TOOL_REPARENT;
    (void)snprintf(command->primary, sizeof(command->primary), "%s", id_or_temporary);
    command->has_secondary = parent_id_or_temporary != NULL;
    if (!vg_tool_copy_reference(command->secondary, parent_id_or_temporary, true, batch, "$.parent",
                                out_diagnostic)) {
        --batch->command_count;
        return false;
    }
    return true;
}

static bool vg_tool_copy_json(char **output, const char *json, VgToolBatch *batch, const char *path,
                              const char *id, VgDocumentDiagnostic *diagnostic) {
    if (!vg_tool_json_object(json))
        return vg_tool_fail(diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", path, id, batch,
                            "value must be one JSON object");
    *output = malloc(strlen(json) + 1u);
    if (*output == NULL)
        return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "queue", path, id, batch,
                            "cannot copy JSON value");
    (void)strcpy(*output, json);
    return true;
}

bool vg_tool_set_component(VgToolBatch *batch, const char *id_or_temporary,
                           const char *component_name, const char *component_json,
                           VgDocumentDiagnostic *out_diagnostic) {
    VgToolCommand *command = NULL;
    if (batch == NULL || !vg_tool_reference(id_or_temporary) || component_name == NULL ||
        component_name[0] == '\0' || strlen(component_name) >= sizeof(command->component))
        return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "queue", "$.components",
                            id_or_temporary, batch, "component id, name and batch are required");
    command = vg_tool_queue(batch, out_diagnostic);
    if (command == NULL)
        return false;
    command->type = VG_TOOL_SET_COMPONENT;
    (void)snprintf(command->primary, sizeof(command->primary), "%s", id_or_temporary);
    (void)snprintf(command->component, sizeof(command->component), "%s", component_name);
    if (!vg_tool_copy_json(&command->json, component_json, batch, "$.components", id_or_temporary,
                           out_diagnostic)) {
        --batch->command_count;
        return false;
    }
    return true;
}

bool vg_tool_set_environment(VgToolBatch *batch, const char *environment_json,
                             VgDocumentDiagnostic *out_diagnostic) {
    VgToolCommand *command = vg_tool_queue(batch, out_diagnostic);
    if (command == NULL)
        return false;
    command->type = VG_TOOL_SET_ENVIRONMENT;
    if (!vg_tool_copy_json(&command->json, environment_json, batch, "$.environment", NULL,
                           out_diagnostic)) {
        --batch->command_count;
        return false;
    }
    return true;
}

static VgJsonNode *vg_tool_node(VgDocument *document, VgJsonType type) {
    VgJsonNode *node = vg_document_allocate(document, sizeof(*node));
    if (node != NULL) {
        memset(node, 0, sizeof(*node));
        node->type = type;
    }
    return node;
}

static VgJsonNode *vg_tool_string_node(VgDocument *document, const char *value) {
    VgJsonNode *node = vg_tool_node(document, VG_JSON_STRING);
    if (node == NULL)
        return NULL;
    node->as.string.length = strlen(value);
    node->as.string.data = vg_document_allocate(document, node->as.string.length + 1u);
    if (node->as.string.data == NULL) {
        vg_json_destroy(node);
        return NULL;
    }
    memcpy(node->as.string.data, value, node->as.string.length + 1u);
    return node;
}

static VgJsonNode *vg_tool_number_node(VgDocument *document, double value) {
    if (!isfinite(value))
        return NULL;
    char number[64];
    int length = snprintf(number, sizeof(number), "%.17g", value);
    if (length <= 0 || (size_t)length >= sizeof(number))
        return NULL;
    for (int index = 0; index < length; ++index) {
        if (number[index] == ',')
            number[index] = '.';
    }
    VgJsonNode *node = vg_tool_node(document, VG_JSON_NUMBER);
    if (node == NULL)
        return NULL;
    node->as.number.value = value;
    node->as.number.length = (size_t)length;
    node->as.number.lexeme = vg_document_allocate(document, (size_t)length + 1u);
    if (node->as.number.lexeme == NULL) {
        vg_json_destroy(node);
        return NULL;
    }
    memcpy(node->as.number.lexeme, number, (size_t)length + 1u);
    return node;
}

static bool vg_tool_object_set(VgDocument *document, VgJsonNode *object, const char *key,
                               VgJsonNode *value) {
    if (object == NULL || object->type != VG_JSON_OBJECT || value == NULL)
        return false;
    for (size_t index = 0u; index < object->as.object.count; ++index) {
        if (vg_json_string_equals(&object->as.object.pairs[index].key, key)) {
            vg_json_destroy(object->as.object.pairs[index].value);
            object->as.object.pairs[index].value = value;
            return true;
        }
    }
    size_t count = object->as.object.count;
    VgJsonPair *pairs = vg_document_allocate(document, (count + 1u) * sizeof(*pairs));
    if (pairs == NULL)
        return false;
    if (count != 0u)
        memcpy(pairs, object->as.object.pairs, count * sizeof(*pairs));
    memset(&pairs[count], 0, sizeof(pairs[count]));
    pairs[count].key.length = strlen(key);
    pairs[count].key.data = vg_document_allocate(document, pairs[count].key.length + 1u);
    if (pairs[count].key.data == NULL) {
        free(pairs);
        return false;
    }
    memcpy(pairs[count].key.data, key, pairs[count].key.length + 1u);
    pairs[count].value = value;
    free(object->as.object.pairs);
    object->as.object.pairs = pairs;
    object->as.object.count = count + 1u;
    return true;
}

static VgJsonNode *vg_tool_transform_node(VgDocument *document,
                                          const VgDocumentTransform *transform) {
    const double *sources[] = {transform->position, transform->rotation, transform->scale};
    const size_t counts[] = {3u, 4u, 3u};
    const char *keys[] = {"position", "rotation", "scale"};
    VgJsonNode *object = vg_tool_node(document, VG_JSON_OBJECT);
    if (object == NULL)
        return NULL;
    for (size_t group = 0u; group < 3u; ++group) {
        VgJsonNode *array = vg_tool_node(document, VG_JSON_ARRAY);
        if (array == NULL)
            goto fail;
        array->as.array.items =
            vg_document_allocate(document, counts[group] * sizeof(*array->as.array.items));
        if (array->as.array.items == NULL) {
            vg_json_destroy(array);
            goto fail;
        }
        memset(array->as.array.items, 0, counts[group] * sizeof(*array->as.array.items));
        array->as.array.count = counts[group];
        for (size_t index = 0u; index < counts[group]; ++index) {
            array->as.array.items[index] = vg_tool_number_node(document, sources[group][index]);
            if (array->as.array.items[index] == NULL) {
                vg_json_destroy(array);
                goto fail;
            }
        }
        if (!vg_tool_object_set(document, object, keys[group], array)) {
            vg_json_destroy(array);
            goto fail;
        }
    }
    return object;
fail:
    vg_json_destroy(object);
    return NULL;
}

static VgJsonNode *vg_tool_parse_object(const char *json) {
    VgJsonError error = {0};
    VgJsonNode *node = vg_json_parse(json, strlen(json), &error);
    if (node != NULL && node->type == VG_JSON_OBJECT)
        return node;
    vg_json_destroy(node);
    return NULL;
}

static VgJsonNode *vg_tool_entities(VgJsonNode *root) {
    return (VgJsonNode *)(uintptr_t)vg_json_object_get(root, "entities");
}

static VgJsonNode *vg_tool_find_entity(VgJsonNode *root, const char *id, size_t *out_index) {
    VgJsonNode *entities = vg_tool_entities(root);
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return NULL;
    for (size_t index = 0u; index < entities->as.array.count; ++index) {
        VgJsonNode *entity = entities->as.array.items[index];
        const VgJsonNode *entity_id = vg_json_object_get(entity, "id");
        if (entity_id != NULL && entity_id->type == VG_JSON_STRING &&
            strcmp(entity_id->as.string.data, id) == 0) {
            if (out_index != NULL)
                *out_index = index;
            return entity;
        }
    }
    return NULL;
}

static bool vg_tool_append_entity(VgDocument *document, VgJsonNode *root, VgJsonNode *entity) {
    VgJsonNode *entities = vg_tool_entities(root);
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return false;
    size_t count = entities->as.array.count;
    VgJsonNode **items = vg_document_allocate(document, (count + 1u) * sizeof(*items));
    if (items == NULL)
        return false;
    if (count != 0u)
        memcpy(items, entities->as.array.items, count * sizeof(*items));
    items[count] = entity;
    free(entities->as.array.items);
    entities->as.array.items = items;
    entities->as.array.count = count + 1u;
    return true;
}

static bool vg_tool_replace_string(VgDocument *document, VgJsonString *string,
                                   const char *replacement) {
    size_t length = strlen(replacement);
    char *copy = vg_document_allocate(document, length + 1u);
    if (copy == NULL)
        return false;
    memcpy(copy, replacement, length + 1u);
    free(string->data);
    string->data = copy;
    string->length = length;
    return true;
}

static bool vg_tool_remap_duplicate_refs(VgDocument *document, const VgToolBatch *batch,
                                         VgJsonNode *node) {
    if (node->type == VG_JSON_STRING) {
        for (size_t index = 0u; index < batch->command_count; ++index) {
            const VgToolCommand *command = &batch->commands[index];
            if (command->type != VG_TOOL_DUPLICATE)
                continue;
            const char *source = vg_tool_resolve(batch, command->primary);
            const char *target = vg_tool_resolve(batch, command->component);
            if (source != NULL && target != NULL && strcmp(node->as.string.data, source) == 0)
                return vg_tool_replace_string(document, &node->as.string, target);
        }
    } else if (node->type == VG_JSON_ARRAY) {
        for (size_t index = 0u; index < node->as.array.count; ++index) {
            if (!vg_tool_remap_duplicate_refs(document, batch, node->as.array.items[index]))
                return false;
        }
    } else if (node->type == VG_JSON_OBJECT) {
        for (size_t index = 0u; index < node->as.object.count; ++index) {
            if (!vg_tool_remap_duplicate_refs(document, batch, node->as.object.pairs[index].value))
                return false;
        }
    }
    return true;
}

static bool vg_tool_set_parent_node(VgDocument *document, VgJsonNode *entity, const char *parent) {
    VgJsonNode *node = parent == NULL || parent[0] == '\0' ? vg_tool_node(document, VG_JSON_NULL)
                                                           : vg_tool_string_node(document, parent);
    if (node == NULL)
        return false;
    if (!vg_tool_object_set(document, entity, "parent", node)) {
        vg_json_destroy(node);
        return false;
    }
    return true;
}

static bool vg_tool_apply_create(VgDocument *document, const VgToolBatch *batch,
                                 const VgToolCommand *command, VgJsonNode *root,
                                 VgDocumentDiagnostic *diagnostic) {
    const char *id = vg_tool_resolve(batch, command->primary);
    const char *parent = command->has_secondary ? vg_tool_resolve(batch, command->secondary) : NULL;
    if (id == NULL || (command->has_secondary && parent == NULL))
        return vg_tool_fail(diagnostic, VG_DOCUMENT_NOT_FOUND, "apply", "$.entities",
                            command->primary, batch, "temporary reference is not defined");
    if (vg_tool_find_entity(root, id, NULL) != NULL)
        return vg_tool_fail(diagnostic, VG_DOCUMENT_DUPLICATE, "apply", "$.entities[].id", id,
                            batch, "entity UUID already exists");
    VgJsonNode *entity = vg_tool_node(document, VG_JSON_OBJECT);
    VgJsonNode *id_node = vg_tool_string_node(document, id);
    VgJsonNode *transform_node = vg_tool_transform_node(document, &command->transform);
    VgJsonNode *components = command->json != NULL ? vg_tool_parse_object(command->json)
                                                   : vg_tool_node(document, VG_JSON_OBJECT);
    if (entity == NULL || id_node == NULL || transform_node == NULL || components == NULL)
        goto out_of_memory;
    if (!vg_tool_object_set(document, entity, "id", id_node))
        goto out_of_memory;
    id_node = NULL;
    if (!vg_tool_set_parent_node(document, entity, parent))
        goto out_of_memory;
    if (!vg_tool_object_set(document, entity, "transform", transform_node))
        goto out_of_memory;
    transform_node = NULL;
    if (!vg_tool_object_set(document, entity, "components", components))
        goto out_of_memory;
    components = NULL;
    if (!vg_tool_append_entity(document, root, entity))
        goto out_of_memory;
    return true;
out_of_memory:
    vg_json_destroy(id_node);
    vg_json_destroy(transform_node);
    vg_json_destroy(components);
    vg_json_destroy(entity);
    return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.entities", id, batch,
                        "cannot create candidate entity");
}

static bool vg_tool_apply_duplicate(VgDocument *document, const VgToolBatch *batch,
                                    const VgToolCommand *command, VgJsonNode *root,
                                    VgDocumentDiagnostic *diagnostic) {
    const char *source_id = vg_tool_resolve(batch, command->primary);
    const char *new_id = vg_tool_resolve(batch, command->component);
    VgJsonNode *source = source_id == NULL ? NULL : vg_tool_find_entity(root, source_id, NULL);
    if (source == NULL || new_id == NULL)
        return vg_tool_fail(diagnostic, VG_DOCUMENT_NOT_FOUND, "apply", "$.entities", source_id,
                            batch, "duplicate source or temporary id does not exist");
    if (vg_tool_find_entity(root, new_id, NULL) != NULL)
        return vg_tool_fail(diagnostic, VG_DOCUMENT_DUPLICATE, "apply", "$.entities[].id", new_id,
                            batch, "duplicate target UUID already exists");
    VgJsonNode *copy = vg_document_clone_json(document, source);
    if (copy == NULL || !vg_tool_remap_duplicate_refs(document, batch, copy)) {
        vg_json_destroy(copy);
        return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.entities", new_id,
                            batch, "cannot duplicate entity");
    }
    if (command->has_secondary) {
        const char *parent =
            command->secondary[0] == '\0' ? NULL : vg_tool_resolve(batch, command->secondary);
        if (command->secondary[0] != '\0' && parent == NULL) {
            vg_json_destroy(copy);
            return vg_tool_fail(diagnostic, VG_DOCUMENT_NOT_FOUND, "apply", "$.parent",
                                command->secondary, batch, "temporary parent is not defined");
        }
        if (!vg_tool_set_parent_node(document, copy, parent)) {
            vg_json_destroy(copy);
            return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.parent", new_id,
                                batch, "cannot set duplicate parent");
        }
    }
    if (!vg_tool_append_entity(document, root, copy)) {
        vg_json_destroy(copy);
        return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.entities", new_id,
                            batch, "cannot append duplicate entity");
    }
    return true;
}

static bool vg_tool_apply_command(VgDocument *document, const VgToolBatch *batch,
                                  const VgToolCommand *command, VgJsonNode *root,
                                  VgDocumentDiagnostic *diagnostic) {
    if (command->type == VG_TOOL_CREATE)
        return vg_tool_apply_create(document, batch, command, root, diagnostic);
    if (command->type == VG_TOOL_DUPLICATE)
        return vg_tool_apply_duplicate(document, batch, command, root, diagnostic);
    if (command->type == VG_TOOL_SET_ENVIRONMENT) {
        VgJsonNode *environment = vg_tool_parse_object(command->json);
        if (environment == NULL ||
            !vg_tool_object_set(document, root, "environment", environment)) {
            vg_json_destroy(environment);
            return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.environment",
                                NULL, batch, "cannot set environment");
        }
        return true;
    }
    const char *id = vg_tool_resolve(batch, command->primary);
    size_t index = 0u;
    VgJsonNode *entity = id == NULL ? NULL : vg_tool_find_entity(root, id, &index);
    if (entity == NULL)
        return vg_tool_fail(diagnostic, VG_DOCUMENT_NOT_FOUND, "apply", "$.entities", id, batch,
                            "entity does not exist");
    if (command->type == VG_TOOL_DELETE) {
        VgJsonNode *entities = vg_tool_entities(root);
        vg_json_destroy(entity);
        for (size_t current = index + 1u; current < entities->as.array.count; ++current)
            entities->as.array.items[current - 1u] = entities->as.array.items[current];
        --entities->as.array.count;
        return true;
    }
    if (command->type == VG_TOOL_SET_TRANSFORM) {
        VgJsonNode *transform = vg_tool_transform_node(document, &command->transform);
        if (transform == NULL || !vg_tool_object_set(document, entity, "transform", transform)) {
            vg_json_destroy(transform);
            return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.transform", id,
                                batch, "cannot set transform");
        }
        return true;
    }
    if (command->type == VG_TOOL_REPARENT) {
        const char *parent =
            command->has_secondary ? vg_tool_resolve(batch, command->secondary) : NULL;
        if (command->has_secondary && parent == NULL)
            return vg_tool_fail(diagnostic, VG_DOCUMENT_NOT_FOUND, "apply", "$.parent",
                                command->secondary, batch, "temporary parent is not defined");
        if (!vg_tool_set_parent_node(document, entity, parent))
            return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.parent", id,
                                batch, "cannot set parent");
        return true;
    }
    VgJsonNode *components = (VgJsonNode *)(uintptr_t)vg_json_object_get(entity, "components");
    if (components == NULL) {
        components = vg_tool_node(document, VG_JSON_OBJECT);
        if (components == NULL || !vg_tool_object_set(document, entity, "components", components)) {
            vg_json_destroy(components);
            return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.components", id,
                                batch, "cannot create component object");
        }
    }
    VgJsonNode *component = vg_tool_parse_object(command->json);
    if (component == NULL ||
        !vg_tool_object_set(document, components, command->component, component)) {
        vg_json_destroy(component);
        return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "apply", "$.components", id,
                            batch, "cannot set component");
    }
    return true;
}

static void vg_tool_result(const VgToolBatch *batch, uint64_t resulting_revision,
                           VgToolResult *result) {
    memset(result, 0, sizeof(*result));
    result->base_revision = batch->expected_revision;
    result->resulting_revision = resulting_revision;
    result->mapping_count = batch->mapping_count;
    if (batch->mapping_count != 0u)
        memcpy(result->mappings, batch->mappings, batch->mapping_count * sizeof(*batch->mappings));
}

static bool vg_tool_candidate(const VgToolBatch *batch, VgJsonNode **out_candidate, char **out_json,
                              size_t *out_length, VgDocumentDiagnostic *diagnostic) {
    if (batch == NULL || batch->command_count == 0u)
        return vg_tool_fail(diagnostic, VG_DOCUMENT_INVALID_BATCH, "validate", "$", NULL, batch,
                            "batch must contain at least one command");
    if (!vg_document_check_revision(batch->document, batch->expected_revision, "validate",
                                    diagnostic))
        return false;
    VgJsonNode *candidate =
        vg_document_clone_json(batch->document, vg_document_root(batch->document));
    if (candidate == NULL)
        return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "validate", "$", NULL, batch,
                            "cannot clone candidate document");
    for (size_t index = 0u; index < batch->command_count; ++index) {
        if (!vg_tool_apply_command(batch->document, batch, &batch->commands[index], candidate,
                                   diagnostic)) {
            vg_json_destroy(candidate);
            return false;
        }
    }
    char *json = NULL;
    size_t length = 0u;
    if (!vg_json_write_canonical(candidate, &json, &length)) {
        vg_json_destroy(candidate);
        return vg_tool_fail(diagnostic, VG_DOCUMENT_OUT_OF_MEMORY, "validate", "$", NULL, batch,
                            "cannot serialize candidate document");
    }
    VgContentDocument *validated = NULL;
    VgContentDiagnostic content_diagnostic;
    if (!vg_content_parse_memory("<tool-candidate>", json, length, &validated,
                                 &content_diagnostic)) {
        if (diagnostic != NULL) {
            memset(diagnostic, 0, sizeof(*diagnostic));
            diagnostic->code = VG_DOCUMENT_VALIDATION;
            diagnostic->expected_revision = batch->expected_revision;
            diagnostic->actual_revision = vg_document_revision(batch->document);
            (void)snprintf(diagnostic->operation, sizeof(diagnostic->operation), "validate");
            (void)snprintf(diagnostic->file, sizeof(diagnostic->file), "%s",
                           content_diagnostic.file);
            (void)snprintf(diagnostic->path, sizeof(diagnostic->path), "%s",
                           content_diagnostic.path);
            (void)snprintf(diagnostic->id, sizeof(diagnostic->id), "%s", content_diagnostic.id);
            (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s",
                           content_diagnostic.message);
            diagnostic->content = content_diagnostic;
        }
        free(json);
        vg_json_destroy(candidate);
        return false;
    }
    vg_content_document_destroy(validated);
    *out_candidate = candidate;
    if (out_json != NULL)
        *out_json = json;
    else
        free(json);
    if (out_length != NULL)
        *out_length = length;
    return true;
}

bool vg_tool_validate(const VgToolBatch *batch, VgToolResult *out_result,
                      VgDocumentDiagnostic *out_diagnostic) {
    VgJsonNode *candidate = NULL;
    if (!vg_tool_candidate(batch, &candidate, NULL, NULL, out_diagnostic))
        return false;
    vg_json_destroy(candidate);
    VgToolResult result;
    vg_tool_result(batch, batch->expected_revision + 1u, &result);
    if (out_result != NULL)
        *out_result = result;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

bool vg_tool_preview(const VgToolBatch *batch, char **out_json, size_t *out_length,
                     VgToolResult *out_result, VgDocumentDiagnostic *out_diagnostic) {
    if (out_json == NULL || out_length == NULL)
        return vg_tool_fail(out_diagnostic, VG_DOCUMENT_INVALID_ARGUMENT, "preview", "$", NULL,
                            batch, "preview output pointers are required");
    VgJsonNode *candidate = NULL;
    char *json = NULL;
    size_t length = 0u;
    if (!vg_tool_candidate(batch, &candidate, &json, &length, out_diagnostic))
        return false;
    vg_json_destroy(candidate);
    VgToolResult result;
    vg_tool_result(batch, batch->expected_revision + 1u, &result);
    *out_json = json;
    *out_length = length;
    if (out_result != NULL)
        *out_result = result;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    return true;
}

bool vg_tool_commit(VgToolBatch *batch, VgToolResult *out_result,
                    VgDocumentDiagnostic *out_diagnostic) {
    VgJsonNode *candidate = NULL;
    if (!vg_tool_candidate(batch, &candidate, NULL, NULL, out_diagnostic))
        return false;
    if (!vg_document_publish(batch->document, candidate, batch->expected_revision,
                             out_diagnostic)) {
        vg_json_destroy(candidate);
        return false;
    }
    VgToolResult result;
    vg_tool_result(batch, vg_document_revision(batch->document), &result);
    if (out_result != NULL)
        *out_result = result;
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    vg_tool_cancel(batch);
    return true;
}
