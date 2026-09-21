#ifndef VESTIGIO_CONTENT_JSON_H
#define VESTIGIO_CONTENT_JSON_H

#include <stdbool.h>
#include <stddef.h>

typedef enum VgJsonType {
    VG_JSON_NULL,
    VG_JSON_BOOL,
    VG_JSON_NUMBER,
    VG_JSON_STRING,
    VG_JSON_ARRAY,
    VG_JSON_OBJECT
} VgJsonType;

typedef struct VgJsonString {
    char *data;
    size_t length;
} VgJsonString;

typedef struct VgJsonNode VgJsonNode;
typedef struct VgJsonPair {
    VgJsonString key;
    VgJsonNode *value;
} VgJsonPair;

struct VgJsonNode {
    VgJsonType type;
    union {
        bool boolean;
        struct {
            char *lexeme;
            size_t length;
            double value;
        } number;
        VgJsonString string;
        struct {
            VgJsonNode **items;
            size_t count;
        } array;
        struct {
            VgJsonPair *pairs;
            size_t count;
        } object;
    } as;
};

typedef struct VgJsonError {
    size_t offset;
    const char *message;
} VgJsonError;

VgJsonNode *vg_json_parse(const char *json, size_t length, VgJsonError *error);
void vg_json_destroy(VgJsonNode *node);
bool vg_json_write_canonical(const VgJsonNode *node, char **out_json, size_t *out_length);
const VgJsonNode *vg_json_object_get(const VgJsonNode *object, const char *key);
const VgJsonPair *vg_json_object_pair(const VgJsonNode *object, const char *key);
bool vg_json_string_equals(const VgJsonString *string, const char *text);

#endif
