#include "content/content.h"

#include "content/json.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct VgContentDocument {
    VgJsonNode *root;
    VgContentKind kind;
    char id[37];
    size_t entity_count;
    char file[512];
};

typedef struct VgContentValidation {
    const char *file;
    VgContentDiagnostic *diagnostic;
} VgContentValidation;

static bool vg_content_fail(VgContentValidation *validation, VgContentDiagnosticCode code,
                            const char *path, const char *id, const char *format, ...)
#if defined(__MINGW32__)
    __attribute__((format(gnu_printf, 5, 6)))
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;

static bool vg_content_fail(VgContentValidation *validation, VgContentDiagnosticCode code,
                            const char *path, const char *id, const char *format, ...) {
    VgContentDiagnostic *diagnostic = validation->diagnostic;
    if (diagnostic != NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        (void)snprintf(diagnostic->file, sizeof(diagnostic->file), "%s",
                       validation->file != NULL ? validation->file : "<memory>");
        (void)snprintf(diagnostic->path, sizeof(diagnostic->path), "%s", path);
        if (id != NULL)
            (void)snprintf(diagnostic->id, sizeof(diagnostic->id), "%s", id);
        va_list arguments;
        va_start(arguments, format);
        (void)vsnprintf(diagnostic->message, sizeof(diagnostic->message), format, arguments);
        va_end(arguments);
    }
    return false;
}

static bool vg_content_string(const VgJsonNode *node, const char **out) {
    if (node == NULL || node->type != VG_JSON_STRING ||
        memchr(node->as.string.data, '\0', node->as.string.length) != NULL)
        return false;
    if (out != NULL)
        *out = node->as.string.data;
    return true;
}

static bool vg_content_number(const VgJsonNode *node, double *out) {
    if (node == NULL || node->type != VG_JSON_NUMBER)
        return false;
    if (out != NULL)
        *out = node->as.number.value;
    return true;
}

static bool vg_content_integer(const VgJsonNode *node, uint32_t *out) {
    double value = 0.0;
    if (!vg_content_number(node, &value) || value < 0.0 || value > (double)UINT32_MAX ||
        floor(value) != value)
        return false;
    if (out != NULL)
        *out = (uint32_t)value;
    return true;
}

static bool vg_content_uuid(const char *text) {
    if (text == NULL || strlen(text) != 36u)
        return false;
    for (size_t index = 0u; index < 36u; ++index) {
        if (index == 8u || index == 13u || index == 18u || index == 23u) {
            if (text[index] != '-')
                return false;
        } else if (!((text[index] >= '0' && text[index] <= '9') ||
                     (text[index] >= 'a' && text[index] <= 'f'))) {
            return false;
        }
    }
    return strcmp(text, "00000000-0000-0000-0000-000000000000") != 0;
}

static bool vg_content_relative_path(const char *path) {
    if (path == NULL || path[0] == '\0' || path[0] == '/' || path[0] == '\\' ||
        strchr(path, '\\') != NULL || strchr(path, ':') != NULL)
        return false;
    const char *part = path;
    while (*part != '\0') {
        const char *end = strchr(part, '/');
        size_t length = end == NULL ? strlen(part) : (size_t)(end - part);
        if (length == 0u || (length == 1u && part[0] == '.') ||
            (length == 2u && part[0] == '.' && part[1] == '.'))
            return false;
        if (end == NULL)
            break;
        part = end + 1;
    }
    return true;
}

static const VgJsonNode *vg_content_required(VgContentValidation *validation,
                                             const VgJsonNode *object, const char *member,
                                             const char *path, const char *id) {
    const VgJsonNode *node = vg_json_object_get(object, member);
    if (node == NULL)
        (void)vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REQUIRED, path, id,
                              "required member '%s' is missing", member);
    return node;
}

static bool vg_content_validate_uuid_node(VgContentValidation *validation, const VgJsonNode *node,
                                          const char *path, const char *id, const char **out) {
    const char *text = NULL;
    if (!vg_content_string(node, &text))
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, id,
                               "expected a UUID string");
    if (!vg_content_uuid(text))
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_UUID, path, id,
                               "UUID must be lowercase 8-4-4-4-12 and non-zero");
    if (out != NULL)
        *out = text;
    return true;
}

static bool vg_content_validate_vec(VgContentValidation *validation, const VgJsonNode *node,
                                    size_t count, const char *path, const char *id,
                                    double *values) {
    if (node == NULL || node->type != VG_JSON_ARRAY || node->as.array.count != count)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, id,
                               "expected an array of %zu finite numbers", count);
    for (size_t index = 0u; index < count; ++index) {
        if (!vg_content_number(node->as.array.items[index], &values[index]))
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, id,
                                   "array element %zu must be a finite number", index);
    }
    return true;
}

static bool vg_content_validate_string_array(VgContentValidation *validation,
                                             const VgJsonNode *node, size_t maximum,
                                             const char *path) {
    if (node == NULL)
        return true;
    if (node->type != VG_JSON_ARRAY)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, NULL,
                               "expected an array of strings");
    if (node->as.array.count > maximum)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_CAPACITY, path, NULL,
                               "array exceeds limit of %zu items", maximum);
    for (size_t index = 0u; index < node->as.array.count; ++index) {
        const char *value = NULL;
        if (!vg_content_string(node->as.array.items[index], &value) || value[0] == '\0')
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, NULL,
                                   "item %zu must be a non-empty string", index);
        for (size_t prior = 0u; prior < index; ++prior) {
            const char *other = NULL;
            (void)vg_content_string(node->as.array.items[prior], &other);
            if (strcmp(value, other) == 0)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_DUPLICATE, path, NULL,
                                       "duplicate value '%s'", value);
        }
    }
    return true;
}

static bool vg_content_validate_capabilities(VgContentValidation *validation,
                                             const VgJsonNode *root, VgContentKind kind) {
    const VgJsonNode *required = vg_json_object_get(root, "required");
    if (!vg_content_validate_string_array(validation, required, 32u, "$.required"))
        return false;
    if (required != NULL) {
        for (size_t index = 0u; index < required->as.array.count; ++index) {
            const char *capability = required->as.array.items[index]->as.string.data;
            bool known = strcmp(capability, "core") == 0;
            if (kind == VG_CONTENT_LEVEL)
                known = known || strcmp(capability, "transform") == 0 ||
                        strcmp(capability, "environment") == 0 ||
                        strcmp(capability, "legacy-geometry") == 0 ||
                        strcmp(capability, "component.engine.camera.v1") == 0 ||
                        strcmp(capability, "component.engine.mesh.v1") == 0 ||
                        strcmp(capability, "component.engine.collider.v1") == 0;
            if (!known) {
                char path[96];
                (void)snprintf(path, sizeof(path), "$.required[%zu]", index);
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REQUIRED, path, NULL,
                                       "required capability '%s' is not supported", capability);
            }
        }
    }
    const VgJsonNode *extensions = vg_json_object_get(root, "extensions");
    if (extensions != NULL && extensions->type != VG_JSON_OBJECT)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$.extensions", NULL,
                               "extensions must be an object");
    const VgJsonNode *required_extensions = vg_json_object_get(root, "required_extensions");
    if (!vg_content_validate_string_array(validation, required_extensions, 32u,
                                          "$.required_extensions"))
        return false;
    if (required_extensions != NULL && required_extensions->as.array.count != 0u) {
        const char *extension = required_extensions->as.array.items[0]->as.string.data;
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REQUIRED,
                               "$.required_extensions[0]", NULL,
                               "required extension '%s' is not implemented", extension);
    }
    return true;
}

static bool vg_content_validate_project(VgContentValidation *validation, const VgJsonNode *root,
                                        VgContentDocument *document) {
    const char *id = NULL;
    if (!vg_content_validate_uuid_node(validation,
                                       vg_content_required(validation, root, "id", "$.id", NULL),
                                       "$.id", NULL, &id))
        return false;
    const VgJsonNode *name = vg_content_required(validation, root, "name", "$.name", id);
    const char *name_text = NULL;
    if (!vg_content_string(name, &name_text) || name_text[0] == '\0' ||
        name->as.string.length > 128u)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$.name", id,
                               "name must be a non-empty UTF-8 string of at most 128 bytes");
    const VgJsonNode *engine = vg_content_required(validation, root, "engine", "$.engine", id);
    if (engine == NULL || engine->type != VG_JSON_OBJECT)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$.engine", id,
                               "engine must be an object");
    uint32_t major = 0u, minor = 0u;
    if (!vg_content_integer(vg_json_object_get(engine, "api_major"), &major) || major != 0u)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_VERSION, "$.engine.api_major", id,
                               "supported API major is 0");
    if (!vg_content_integer(vg_json_object_get(engine, "min_minor"), &minor) || minor > 1u)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_VERSION, "$.engine.min_minor", id,
                               "supported API minor is at most 1");
    const VgJsonNode *entry =
        vg_content_required(validation, root, "entry_level", "$.entry_level", id);
    const char *entry_path = NULL;
    if (!vg_content_string(entry, &entry_path) || !vg_content_relative_path(entry_path))
        return vg_content_fail(
            validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.entry_level", id,
            "entry_level must be a relative forward-slash path without traversal");
    const char *array_names[] = {"asset_roots", "definition_roots"};
    for (size_t array_index = 0u; array_index < 2u; ++array_index) {
        char path[64];
        (void)snprintf(path, sizeof(path), "$.%s", array_names[array_index]);
        const VgJsonNode *array = vg_json_object_get(root, array_names[array_index]);
        if (!vg_content_validate_string_array(validation, array, 16u, path))
            return false;
        if (array != NULL) {
            for (size_t index = 0u; index < array->as.array.count; ++index) {
                if (!vg_content_relative_path(array->as.array.items[index]->as.string.data))
                    return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, id,
                                           "root %zu must be a relative path without traversal",
                                           index);
            }
        }
    }
    const VgJsonNode *defaults = vg_json_object_get(root, "defaults");
    const char *defaults_path = NULL;
    if (defaults != NULL &&
        (!vg_content_string(defaults, &defaults_path) || !vg_content_relative_path(defaults_path)))
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.defaults", id,
                               "defaults must be a relative path without traversal");
    const VgJsonNode *game = vg_json_object_get(root, "game");
    if (game != NULL) {
        const char *kind = NULL;
        const char *module = NULL;
        if (game->type != VG_JSON_OBJECT ||
            !vg_content_string(vg_json_object_get(game, "kind"), &kind) ||
            !vg_content_string(vg_json_object_get(game, "module"), &module) ||
            strcmp(kind, "builtin") != 0 || module[0] == '\0')
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.game", id,
                                   "game requires kind 'builtin' and a non-empty module");
    }
    if (!vg_content_validate_capabilities(validation, root, VG_CONTENT_PROJECT))
        return false;
    (void)snprintf(document->id, sizeof(document->id), "%s", id);
    return true;
}

static bool vg_content_validate_transform(VgContentValidation *validation,
                                          const VgJsonNode *transform, const char *path,
                                          const char *id) {
    if (transform == NULL || transform->type != VG_JSON_OBJECT)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TRANSFORM, path, id,
                               "transform must be an object");
    double position[3], rotation[4], scale[3];
    char child[512];
    (void)snprintf(child, sizeof(child), "%s.position", path);
    if (!vg_content_validate_vec(validation, vg_json_object_get(transform, "position"), 3u, child,
                                 id, position))
        return false;
    (void)snprintf(child, sizeof(child), "%s.rotation", path);
    if (!vg_content_validate_vec(validation, vg_json_object_get(transform, "rotation"), 4u, child,
                                 id, rotation))
        return false;
    (void)snprintf(child, sizeof(child), "%s.scale", path);
    if (!vg_content_validate_vec(validation, vg_json_object_get(transform, "scale"), 3u, child, id,
                                 scale))
        return false;
    double length_squared = rotation[0] * rotation[0] + rotation[1] * rotation[1] +
                            rotation[2] * rotation[2] + rotation[3] * rotation[3];
    if (fabs(length_squared - 1.0) > 0.0001)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TRANSFORM, path, id,
                               "rotation quaternion [x,y,z,w] must be normalized");
    if (scale[0] <= 0.0 || scale[1] <= 0.0 || scale[2] <= 0.0)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TRANSFORM, path, id,
                               "scale components must be strictly positive");
    return true;
}

static bool vg_content_component_known(const char *name) {
    return strcmp(name, "engine.camera") == 0 || strcmp(name, "engine.mesh") == 0 ||
           strcmp(name, "engine.collider") == 0;
}

static bool vg_content_component_version(VgContentValidation *validation, const VgJsonNode *node,
                                         const char *path, const char *id) {
    uint32_t version = 0u;
    if (node == NULL || node->type != VG_JSON_OBJECT ||
        !vg_content_integer(vg_json_object_get(node, "version"), &version))
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, id,
                               "component must be an object with integer version");
    if (version != 1u)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_VERSION, path, id,
                               "supported component version is 1");
    return true;
}

static bool vg_content_validate_component(VgContentValidation *validation, const char *name,
                                          const VgJsonNode *node, const char *path,
                                          const char *id) {
    if (!vg_content_component_known(name))
        return true;
    if (!vg_content_component_version(validation, node, path, id))
        return false;
    if (strcmp(name, "engine.camera") == 0) {
        double fov = 0.0, near_plane = 0.0, far_plane = 0.0;
        if (!vg_content_number(vg_json_object_get(node, "fov_y_radians"), &fov) || fov <= 0.0 ||
            fov >= 3.14159265358979323846 ||
            !vg_content_number(vg_json_object_get(node, "near"), &near_plane) ||
            !vg_content_number(vg_json_object_get(node, "far"), &far_plane) || near_plane <= 0.0 ||
            far_plane <= near_plane)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, id,
                                   "camera requires 0<fov_y_radians<pi and 0<near<far");
    } else if (strcmp(name, "engine.mesh") == 0) {
        char asset_path[512];
        size_t path_length = strlen(path);
        if (path_length > sizeof(asset_path) - sizeof(".asset"))
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_LIMIT, path, id,
                                   "component diagnostic path exceeds limit");
        memcpy(asset_path, path, path_length);
        memcpy(asset_path + path_length, ".asset", sizeof(".asset"));
        if (!vg_content_validate_uuid_node(validation, vg_json_object_get(node, "asset"),
                                           asset_path, id, NULL))
            return false;
    } else {
        const char *shape = NULL;
        const char *motion = NULL;
        double extents[3], center[3];
        if (!vg_content_string(vg_json_object_get(node, "shape"), &shape) ||
            strcmp(shape, "box") != 0 ||
            !vg_content_string(vg_json_object_get(node, "motion"), &motion) ||
            (strcmp(motion, "static") != 0 && strcmp(motion, "kinematic") != 0) ||
            !vg_content_validate_vec(validation, vg_json_object_get(node, "half_extents"), 3u, path,
                                     id, extents) ||
            !vg_content_validate_vec(validation, vg_json_object_get(node, "center"), 3u, path, id,
                                     center))
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, id,
                                   "collider v1 supports box with center, positive half_extents "
                                   "and static/kinematic motion");
        if (extents[0] <= 0.0 || extents[1] <= 0.0 || extents[2] <= 0.0)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, id,
                                   "collider half_extents must be positive");
    }
    return true;
}

static bool vg_content_validate_components(VgContentValidation *validation,
                                           const VgJsonNode *entity, const char *path,
                                           const char *id) {
    const VgJsonNode *components = vg_json_object_get(entity, "components");
    if (components != NULL && components->type != VG_JSON_OBJECT)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, id,
                               "components must be an object");
    if (components != NULL) {
        for (size_t index = 0u; index < components->as.object.count; ++index) {
            const VgJsonPair *pair = &components->as.object.pairs[index];
            if (memchr(pair->key.data, '\0', pair->key.length) != NULL)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, id,
                                       "component name contains NUL");
            char component_name[128];
            if (pair->key.length >= sizeof(component_name))
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_LIMIT, path, id,
                                       "component name exceeds 127 bytes");
            memcpy(component_name, pair->key.data, pair->key.length);
            component_name[pair->key.length] = '\0';
            char component_path[512];
            (void)snprintf(component_path, sizeof(component_path), "%s.%s", path, component_name);
            if (!vg_content_validate_component(validation, component_name, pair->value,
                                               component_path, id))
                return false;
        }
    }
    const VgJsonNode *required = vg_json_object_get(entity, "required_components");
    char required_path[512];
    (void)snprintf(required_path, sizeof(required_path), "%s.required_components", path);
    if (!vg_content_validate_string_array(validation, required, 32u, required_path))
        return false;
    if (required != NULL) {
        for (size_t index = 0u; index < required->as.array.count; ++index) {
            const char *name = required->as.array.items[index]->as.string.data;
            if (!vg_content_component_known(name))
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REQUIRED, required_path,
                                       id, "required component '%s' is not supported", name);
            if (components == NULL || vg_json_object_get(components, name) == NULL)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REFERENCE, required_path,
                                       id, "required component '%s' is missing", name);
        }
    }
    return true;
}

static bool vg_content_validate_environment(VgContentValidation *validation,
                                            const VgJsonNode *environment, const char *id) {
    if (environment == NULL)
        return true;
    if (environment->type != VG_JSON_OBJECT)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$.environment", id,
                               "environment must be an object");
    const char *colors[] = {"ambient_linear", "clear_linear"};
    for (size_t color_index = 0u; color_index < 2u; ++color_index) {
        const VgJsonNode *color = vg_json_object_get(environment, colors[color_index]);
        if (color == NULL)
            continue;
        double values[3];
        char path[96];
        (void)snprintf(path, sizeof(path), "$.environment.%s", colors[color_index]);
        if (!vg_content_validate_vec(validation, color, 3u, path, id, values))
            return false;
        for (size_t index = 0u; index < 3u; ++index) {
            if (values[index] < 0.0 || values[index] > 1.0)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, id,
                                       "linear color components must be in [0,1]");
        }
    }
    const VgJsonNode *fog = vg_json_object_get(environment, "fog");
    if (fog != NULL) {
        const char *mode = NULL;
        double color[3];
        if (fog->type != VG_JSON_OBJECT ||
            !vg_content_string(vg_json_object_get(fog, "mode"), &mode) ||
            !vg_content_validate_vec(validation, vg_json_object_get(fog, "color_linear"), 3u,
                                     "$.environment.fog.color_linear", id, color))
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.environment.fog",
                                   id, "fog requires mode and color_linear");
        for (size_t index = 0u; index < 3u; ++index) {
            if (color[index] < 0.0 || color[index] > 1.0)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT,
                                       "$.environment.fog.color_linear", id,
                                       "linear color components must be in [0,1]");
        }
        if (strcmp(mode, "linear") == 0) {
            double start = 0.0, end = 0.0;
            if (!vg_content_number(vg_json_object_get(fog, "start"), &start) ||
                !vg_content_number(vg_json_object_get(fog, "end"), &end) || start < 0.0 ||
                end <= start)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT,
                                       "$.environment.fog", id, "linear fog requires 0<=start<end");
        } else if (strcmp(mode, "exponential") == 0) {
            double density = 0.0;
            if (!vg_content_number(vg_json_object_get(fog, "density"), &density) || density <= 0.0)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT,
                                       "$.environment.fog.density", id,
                                       "exponential fog density must be positive");
        } else if (strcmp(mode, "none") != 0) {
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT,
                                   "$.environment.fog.mode", id,
                                   "fog mode must be none, linear or exponential");
        }
    }
    return true;
}

static bool vg_content_find_id(const char *id, const char *const *ids, size_t count,
                               size_t *out_index) {
    for (size_t index = 0u; index < count; ++index) {
        if (strcmp(id, ids[index]) == 0) {
            if (out_index != NULL)
                *out_index = index;
            return true;
        }
    }
    return false;
}

static bool vg_content_validate_geometry(VgContentValidation *validation,
                                         const VgJsonNode *geometry, const char *level_id) {
    if (geometry == NULL)
        return true;
    const char *kind = NULL;
    uint32_t version = 0u;
    if (geometry->type != VG_JSON_OBJECT ||
        !vg_content_string(vg_json_object_get(geometry, "kind"), &kind) ||
        strcmp(kind, "legacy.sectors") != 0 ||
        !vg_content_integer(vg_json_object_get(geometry, "version"), &version) || version != 1u)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.geometry", level_id,
                               "geometry must be legacy.sectors version 1");
    const VgJsonNode *sectors = vg_json_object_get(geometry, "sectors");
    if (sectors == NULL || sectors->type != VG_JSON_ARRAY)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$.geometry.sectors",
                               level_id, "sectors must be an array");
    if (sectors->as.array.count > VG_CONTENT_MAX_SECTORS)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_CAPACITY, "$.geometry.sectors",
                               level_id, "sector count exceeds %u", VG_CONTENT_MAX_SECTORS);
    const char *ids[VG_CONTENT_MAX_SECTORS];
    size_t vertex_counts[VG_CONTENT_MAX_SECTORS];
    for (size_t index = 0u; index < sectors->as.array.count; ++index) {
        const VgJsonNode *sector = sectors->as.array.items[index];
        char path[128];
        (void)snprintf(path, sizeof(path), "$.geometry.sectors[%zu]", index);
        if (sector->type != VG_JSON_OBJECT)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, level_id,
                                   "sector must be an object");
        char id_path[160];
        (void)snprintf(id_path, sizeof(id_path), "%s.id", path);
        if (!vg_content_validate_uuid_node(validation, vg_json_object_get(sector, "id"), id_path,
                                           level_id, &ids[index]))
            return false;
        if (strcmp(ids[index], level_id) == 0)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_DUPLICATE, id_path, ids[index],
                                   "sector UUID duplicates level UUID");
        for (size_t prior = 0u; prior < index; ++prior) {
            if (strcmp(ids[index], ids[prior]) == 0)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_DUPLICATE, id_path,
                                       ids[index], "duplicate sector UUID");
        }
        double floor_height = 0.0, ceiling = 0.0;
        if (!vg_content_number(vg_json_object_get(sector, "floor"), &floor_height) ||
            !vg_content_number(vg_json_object_get(sector, "ceiling"), &ceiling) ||
            ceiling <= floor_height)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, ids[index],
                                   "sector requires finite floor < ceiling");
        const VgJsonNode *vertices = vg_json_object_get(sector, "vertices");
        if (vertices == NULL || vertices->type != VG_JSON_ARRAY || vertices->as.array.count < 3u ||
            vertices->as.array.count > VG_CONTENT_MAX_VERTICES_PER_SECTOR)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_CAPACITY, path, ids[index],
                                   "sector vertices count must be in [3,%u]",
                                   VG_CONTENT_MAX_VERTICES_PER_SECTOR);
        vertex_counts[index] = vertices->as.array.count;
        for (size_t vertex = 0u; vertex < vertices->as.array.count; ++vertex) {
            double point[2];
            if (!vg_content_validate_vec(validation, vertices->as.array.items[vertex], 2u, path,
                                         ids[index], point))
                return false;
        }
    }
    for (size_t index = 0u; index < sectors->as.array.count; ++index) {
        const VgJsonNode *portals = vg_json_object_get(sectors->as.array.items[index], "portals");
        if (portals == NULL)
            continue;
        char path[160];
        (void)snprintf(path, sizeof(path), "$.geometry.sectors[%zu].portals", index);
        if (portals->type != VG_JSON_ARRAY || portals->as.array.count > vertex_counts[index])
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_CAPACITY, path, ids[index],
                                   "portals must be an array with at most one entry per edge");
        bool used_edges[VG_CONTENT_MAX_VERTICES_PER_SECTOR] = {false};
        for (size_t portal_index = 0u; portal_index < portals->as.array.count; ++portal_index) {
            const VgJsonNode *portal = portals->as.array.items[portal_index];
            uint32_t edge = 0u;
            const char *target = NULL;
            double opening[2];
            if (portal->type != VG_JSON_OBJECT ||
                !vg_content_integer(vg_json_object_get(portal, "edge"), &edge) ||
                edge >= vertex_counts[index] || used_edges[edge] ||
                !vg_content_validate_uuid_node(validation, vg_json_object_get(portal, "target"),
                                               path, ids[index], &target) ||
                !vg_content_validate_vec(validation, vg_json_object_get(portal, "opening"), 2u,
                                         path, ids[index], opening) ||
                opening[0] < 0.0 || opening[1] > 1.0 || opening[1] <= opening[0])
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, path, ids[index],
                                       "portal requires unique valid edge, target sector and "
                                       "0<=opening[0]<opening[1]<=1");
            used_edges[edge] = true;
            if (!vg_content_find_id(target, ids, sectors->as.array.count, NULL))
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REFERENCE, path,
                                       ids[index], "portal target '%s' does not exist", target);
        }
    }
    return true;
}

static bool vg_content_validate_level(VgContentValidation *validation, const VgJsonNode *root,
                                      VgContentDocument *document) {
    const char *level_id = NULL;
    if (!vg_content_validate_uuid_node(validation,
                                       vg_content_required(validation, root, "id", "$.id", NULL),
                                       "$.id", NULL, &level_id))
        return false;
    const char *coordinates = NULL;
    if (!vg_content_string(vg_json_object_get(root, "coordinates"), &coordinates) ||
        strcmp(coordinates, "right-handed-z-up-meters") != 0)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.coordinates", level_id,
                               "coordinates must be right-handed-z-up-meters");
    const char *name = NULL;
    const VgJsonNode *name_node = vg_json_object_get(root, "name");
    if (!vg_content_string(name_node, &name) || name[0] == '\0' ||
        name_node->as.string.length > 128u)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$.name", level_id,
                               "name must be a non-empty UTF-8 string of at most 128 bytes");
    if (!vg_content_validate_capabilities(validation, root, VG_CONTENT_LEVEL) ||
        !vg_content_validate_environment(validation, vg_json_object_get(root, "environment"),
                                         level_id) ||
        !vg_content_validate_geometry(validation, vg_json_object_get(root, "geometry"), level_id))
        return false;
    const VgJsonNode *entities = vg_json_object_get(root, "entities");
    if (entities == NULL || entities->type != VG_JSON_ARRAY)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$.entities", level_id,
                               "entities must be an array");
    if (entities->as.array.count > VG_CONTENT_MAX_ENTITIES)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_CAPACITY, "$.entities", level_id,
                               "entity count exceeds %u", VG_CONTENT_MAX_ENTITIES);
    const char *ids[VG_CONTENT_MAX_ENTITIES];
    const char *parents[VG_CONTENT_MAX_ENTITIES] = {0};
    for (size_t index = 0u; index < entities->as.array.count; ++index) {
        const VgJsonNode *entity = entities->as.array.items[index];
        char path[128];
        (void)snprintf(path, sizeof(path), "$.entities[%zu]", index);
        if (entity->type != VG_JSON_OBJECT)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, path, level_id,
                                   "entity must be an object");
        char member_path[160];
        (void)snprintf(member_path, sizeof(member_path), "%s.id", path);
        if (!vg_content_validate_uuid_node(validation, vg_json_object_get(entity, "id"),
                                           member_path, level_id, &ids[index]))
            return false;
        if (strcmp(ids[index], level_id) == 0)
            return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_DUPLICATE, member_path,
                                   ids[index], "entity UUID duplicates level UUID");
        const VgJsonNode *geometry = vg_json_object_get(root, "geometry");
        const VgJsonNode *sectors = vg_json_object_get(geometry, "sectors");
        if (sectors != NULL) {
            for (size_t sector_index = 0u; sector_index < sectors->as.array.count; ++sector_index) {
                const char *sector_id = NULL;
                (void)vg_content_string(
                    vg_json_object_get(sectors->as.array.items[sector_index], "id"), &sector_id);
                if (sector_id != NULL && strcmp(ids[index], sector_id) == 0)
                    return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_DUPLICATE, member_path,
                                           ids[index], "entity UUID duplicates sector UUID");
            }
        }
        for (size_t prior = 0u; prior < index; ++prior) {
            if (strcmp(ids[index], ids[prior]) == 0)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_DUPLICATE, member_path,
                                       ids[index], "duplicate entity UUID");
        }
        const VgJsonNode *parent = vg_json_object_get(entity, "parent");
        (void)snprintf(member_path, sizeof(member_path), "%s.parent", path);
        if (parent != NULL && parent->type != VG_JSON_NULL &&
            !vg_content_validate_uuid_node(validation, parent, member_path, ids[index],
                                           &parents[index]))
            return false;
        (void)snprintf(member_path, sizeof(member_path), "%s.transform", path);
        if (!vg_content_validate_transform(validation, vg_json_object_get(entity, "transform"),
                                           member_path, ids[index]))
            return false;
        (void)snprintf(member_path, sizeof(member_path), "%s.components", path);
        if (!vg_content_validate_components(validation, entity, member_path, ids[index]))
            return false;
    }
    uint8_t state[VG_CONTENT_MAX_ENTITIES] = {0};
    for (size_t start = 0u; start < entities->as.array.count; ++start) {
        size_t current = start;
        while (parents[current] != NULL) {
            size_t parent_index = 0u;
            if (!vg_content_find_id(parents[current], ids, entities->as.array.count, &parent_index))
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REFERENCE,
                                       "$.entities[].parent", ids[current],
                                       "parent entity '%s' does not exist", parents[current]);
            if (state[parent_index] == 1u)
                return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_REFERENCE,
                                       "$.entities[].parent", ids[current],
                                       "entity parent cycle detected");
            if (state[parent_index] == 2u)
                break;
            state[current] = 1u;
            current = parent_index;
        }
        current = start;
        while (state[current] == 1u) {
            state[current] = 2u;
            if (parents[current] == NULL)
                break;
            size_t parent_index = 0u;
            if (!vg_content_find_id(parents[current], ids, entities->as.array.count, &parent_index))
                break;
            current = parent_index;
        }
    }
    (void)snprintf(document->id, sizeof(document->id), "%s", level_id);
    document->entity_count = entities->as.array.count;
    return true;
}

static bool vg_content_validate_root(VgContentValidation *validation, const VgJsonNode *root,
                                     VgContentDocument *document) {
    if (root == NULL || root->type != VG_JSON_OBJECT)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$", NULL,
                               "document root must be an object");
    const char *format = NULL;
    if (!vg_content_string(vg_json_object_get(root, "format"), &format))
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.format", NULL,
                               "format must be a string");
    uint32_t version = 0u;
    if (!vg_content_integer(vg_json_object_get(root, "version"), &version))
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_VERSION, "$.version", NULL,
                               "version must be an integer");
    if (version != 1u)
        return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_VERSION, "$.version", NULL,
                               "supported document version is 1");
    if (strcmp(format, "vestigio.project") == 0) {
        document->kind = VG_CONTENT_PROJECT;
        return vg_content_validate_project(validation, root, document);
    }
    if (strcmp(format, "vestigio.level") == 0) {
        document->kind = VG_CONTENT_LEVEL;
        return vg_content_validate_level(validation, root, document);
    }
    return vg_content_fail(validation, VG_CONTENT_DIAGNOSTIC_FORMAT, "$.format", NULL,
                           "format must be vestigio.project or vestigio.level");
}

bool vg_content_parse_memory(const char *file_name, const char *json, size_t length,
                             VgContentDocument **out_document,
                             VgContentDiagnostic *out_diagnostic) {
    VgContentValidation validation = {file_name != NULL ? file_name : "<memory>", out_diagnostic};
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    if (out_document == NULL || json == NULL)
        return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$", NULL,
                               "input and output pointers are required");
    if (length == 0u || length > VG_CONTENT_MAX_FILE_BYTES)
        return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_LIMIT, "$", NULL,
                               "document size must be in [1,%u] bytes", VG_CONTENT_MAX_FILE_BYTES);
    VgJsonError json_error = {0};
    VgJsonNode *root = vg_json_parse(json, length, &json_error);
    if (root == NULL) {
        const char *message = json_error.message != NULL ? json_error.message : "invalid JSON";
        VgContentDiagnosticCode code = strstr(message, "limit exceeded") != NULL
                                           ? VG_CONTENT_DIAGNOSTIC_LIMIT
                                           : VG_CONTENT_DIAGNOSTIC_PARSE;
        bool result = vg_content_fail(&validation, code, "$", NULL, "%s", message);
        if (out_diagnostic != NULL)
            out_diagnostic->byte_offset = json_error.offset;
        return result;
    }
    VgContentDocument *candidate = calloc(1u, sizeof(*candidate));
    if (candidate == NULL) {
        vg_json_destroy(root);
        return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_LIMIT, "$", NULL,
                               "out of memory");
    }
    candidate->root = root;
    (void)snprintf(candidate->file, sizeof(candidate->file), "%s", validation.file);
    if (!vg_content_validate_root(&validation, root, candidate)) {
        vg_content_document_destroy(candidate);
        return false;
    }
    *out_document = candidate;
    return true;
}

bool vg_content_parse_file(const char *path, VgContentDocument **out_document,
                           VgContentDiagnostic *out_diagnostic) {
    VgContentValidation validation = {path != NULL ? path : "<null>", out_diagnostic};
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    if (path == NULL || out_document == NULL)
        return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$", NULL,
                               "path and output pointer are required");
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_IO, "$", NULL,
                               "cannot open document");
    bool success = false;
    if (fseek(file, 0, SEEK_END) != 0)
        goto io_error;
    long file_length = ftell(file);
    if (file_length <= 0 || (unsigned long)file_length > VG_CONTENT_MAX_FILE_BYTES)
        goto limit_error;
    if (fseek(file, 0, SEEK_SET) != 0)
        goto io_error;
    char *json = malloc((size_t)file_length);
    if (json == NULL)
        goto memory_error;
    if (fread(json, 1u, (size_t)file_length, file) != (size_t)file_length) {
        free(json);
        goto io_error;
    }
    success =
        vg_content_parse_memory(path, json, (size_t)file_length, out_document, out_diagnostic);
    free(json);
    (void)fclose(file);
    return success;
memory_error:
    (void)fclose(file);
    return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_LIMIT, "$", NULL, "out of memory");
limit_error:
    (void)fclose(file);
    return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_LIMIT, "$", NULL,
                           "document size must be in [1,%u] bytes", VG_CONTENT_MAX_FILE_BYTES);
io_error:
    (void)fclose(file);
    return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_IO, "$", NULL,
                           "cannot read document");
}

bool vg_content_write_canonical(const VgContentDocument *document, char **out_json,
                                size_t *out_length, VgContentDiagnostic *out_diagnostic) {
    VgContentValidation validation = {document != NULL ? document->file : "<null>", out_diagnostic};
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    if (document == NULL || out_json == NULL || out_length == NULL)
        return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_TYPE, "$", NULL,
                               "document and output pointers are required");
    char *json = NULL;
    size_t length = 0u;
    if (!vg_json_write_canonical(document->root, &json, &length))
        return vg_content_fail(&validation, VG_CONTENT_DIAGNOSTIC_LIMIT, "$", document->id,
                               "canonical document exceeds output limit or memory is exhausted");
    *out_json = json;
    *out_length = length;
    return true;
}

void vg_content_document_destroy(VgContentDocument *document) {
    if (document == NULL)
        return;
    vg_json_destroy(document->root);
    free(document);
}

void vg_content_string_destroy(char *string) {
    free(string);
}

VgContentKind vg_content_document_kind(const VgContentDocument *document) {
    return document == NULL ? VG_CONTENT_UNKNOWN : document->kind;
}

const char *vg_content_document_id(const VgContentDocument *document) {
    return document == NULL ? NULL : document->id;
}

size_t vg_content_level_entity_count(const VgContentDocument *document) {
    return document != NULL && document->kind == VG_CONTENT_LEVEL ? document->entity_count : 0u;
}
