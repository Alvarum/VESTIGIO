#include "content/json.h"

#include "content/third_party/jsmn.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    VG_JSON_MAX_TOKENS = 32768,
    VG_JSON_MAX_DEPTH = 64,
    VG_JSON_MAX_STRING_BYTES = 65535,
    VG_JSON_MAX_OUTPUT = 8 * 1024 * 1024
};

typedef struct VgJsonBuild {
    const char *source;
    size_t source_length;
    const jsmntok_t *tokens;
    size_t token_count;
    size_t next;
    VgJsonError *error;
} VgJsonBuild;

typedef struct VgJsonBuffer {
    char *data;
    size_t length;
    size_t capacity;
} VgJsonBuffer;

static void vg_json_error(VgJsonError *error, size_t offset, const char *message) {
    if (error != NULL) {
        error->offset = offset;
        error->message = message;
    }
}

static bool vg_json_utf8_valid(const unsigned char *text, size_t length) {
    for (size_t index = 0u; index < length;) {
        unsigned char first = text[index++];
        if (first < 0x80u)
            continue;
        uint32_t codepoint = 0u;
        size_t remaining = 0u;
        if (first >= 0xC2u && first <= 0xDFu) {
            codepoint = first & 0x1Fu;
            remaining = 1u;
        } else if (first >= 0xE0u && first <= 0xEFu) {
            codepoint = first & 0x0Fu;
            remaining = 2u;
        } else if (first >= 0xF0u && first <= 0xF4u) {
            codepoint = first & 0x07u;
            remaining = 3u;
        } else {
            return false;
        }
        if (index + remaining > length)
            return false;
        for (size_t part = 0u; part < remaining; ++part) {
            unsigned char continuation = text[index++];
            if ((continuation & 0xC0u) != 0x80u)
                return false;
            codepoint = (codepoint << 6u) | (continuation & 0x3Fu);
        }
        if ((remaining == 2u && codepoint < 0x800u) || (remaining == 3u && codepoint < 0x10000u) ||
            codepoint > 0x10FFFFu || (codepoint >= 0xD800u && codepoint <= 0xDFFFu))
            return false;
    }
    return true;
}

static uint32_t vg_json_hex4(const char *text, bool *valid) {
    uint32_t value = 0u;
    for (size_t index = 0u; index < 4u; ++index) {
        char character = text[index];
        uint32_t digit = 0u;
        if (character >= '0' && character <= '9')
            digit = (uint32_t)(character - '0');
        else if (character >= 'a' && character <= 'f')
            digit = (uint32_t)(character - 'a') + 10u;
        else if (character >= 'A' && character <= 'F')
            digit = (uint32_t)(character - 'A') + 10u;
        else {
            *valid = false;
            return 0u;
        }
        value = (value << 4u) | digit;
    }
    return value;
}

static size_t vg_json_encode_utf8(uint32_t codepoint, char *output) {
    if (codepoint <= 0x7Fu) {
        output[0] = (char)codepoint;
        return 1u;
    }
    if (codepoint <= 0x7FFu) {
        output[0] = (char)(0xC0u | (codepoint >> 6u));
        output[1] = (char)(0x80u | (codepoint & 0x3Fu));
        return 2u;
    }
    if (codepoint <= 0xFFFFu) {
        output[0] = (char)(0xE0u | (codepoint >> 12u));
        output[1] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
        output[2] = (char)(0x80u | (codepoint & 0x3Fu));
        return 3u;
    }
    output[0] = (char)(0xF0u | (codepoint >> 18u));
    output[1] = (char)(0x80u | ((codepoint >> 12u) & 0x3Fu));
    output[2] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
    output[3] = (char)(0x80u | (codepoint & 0x3Fu));
    return 4u;
}

static bool vg_json_decode_string(const char *source, size_t length, VgJsonString *out) {
    char *decoded = malloc(length + 1u);
    if (decoded == NULL)
        return false;
    size_t output = 0u;
    for (size_t index = 0u; index < length; ++index) {
        unsigned char character = (unsigned char)source[index];
        if (character != '\\') {
            decoded[output++] = (char)character;
            continue;
        }
        if (++index >= length) {
            free(decoded);
            return false;
        }
        character = (unsigned char)source[index];
        switch (character) {
        case '"':
        case '\\':
        case '/':
            decoded[output++] = (char)character;
            break;
        case 'b':
            decoded[output++] = '\b';
            break;
        case 'f':
            decoded[output++] = '\f';
            break;
        case 'n':
            decoded[output++] = '\n';
            break;
        case 'r':
            decoded[output++] = '\r';
            break;
        case 't':
            decoded[output++] = '\t';
            break;
        case 'u': {
            if (index + 4u >= length) {
                free(decoded);
                return false;
            }
            bool valid = true;
            uint32_t codepoint = vg_json_hex4(source + index + 1u, &valid);
            index += 4u;
            if (valid && codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
                if (index + 6u >= length || source[index + 1u] != '\\' ||
                    source[index + 2u] != 'u') {
                    valid = false;
                } else {
                    uint32_t low = vg_json_hex4(source + index + 3u, &valid);
                    if (low < 0xDC00u || low > 0xDFFFu)
                        valid = false;
                    else {
                        codepoint = 0x10000u + ((codepoint - 0xD800u) << 10u) + (low - 0xDC00u);
                        index += 6u;
                    }
                }
            } else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
                valid = false;
            }
            if (!valid) {
                free(decoded);
                return false;
            }
            output += vg_json_encode_utf8(codepoint, decoded + output);
            break;
        }
        default:
            free(decoded);
            return false;
        }
    }
    if (!vg_json_utf8_valid((const unsigned char *)decoded, output)) {
        free(decoded);
        return false;
    }
    decoded[output] = '\0';
    out->data = decoded;
    out->length = output;
    return true;
}

static bool vg_json_parse_number(const char *text, size_t length, double *out_value) {
    size_t index = 0u;
    bool negative = index < length && text[index] == '-';
    if (negative)
        ++index;
    if (index >= length)
        return false;
    double value = 0.0;
    if (text[index] == '0') {
        ++index;
        if (index < length && text[index] >= '0' && text[index] <= '9')
            return false;
    } else if (text[index] >= '1' && text[index] <= '9') {
        while (index < length && text[index] >= '0' && text[index] <= '9')
            value = value * 10.0 + (double)(text[index++] - '0');
    } else {
        return false;
    }
    if (index < length && text[index] == '.') {
        ++index;
        if (index >= length || text[index] < '0' || text[index] > '9')
            return false;
        double place = 0.1;
        while (index < length && text[index] >= '0' && text[index] <= '9') {
            value += (double)(text[index++] - '0') * place;
            place *= 0.1;
        }
    }
    int exponent = 0;
    bool exponent_negative = false;
    if (index < length && (text[index] == 'e' || text[index] == 'E')) {
        ++index;
        if (index < length && (text[index] == '+' || text[index] == '-'))
            exponent_negative = text[index++] == '-';
        if (index >= length || text[index] < '0' || text[index] > '9')
            return false;
        while (index < length && text[index] >= '0' && text[index] <= '9') {
            if (exponent < 10000)
                exponent = exponent * 10 + (text[index] - '0');
            ++index;
        }
    }
    if (index != length)
        return false;
    if (exponent != 0)
        value *= pow(10.0, exponent_negative ? -(double)exponent : (double)exponent);
    if (negative)
        value = -value;
    if (!isfinite(value))
        return false;
    *out_value = value;
    return true;
}

void vg_json_destroy(VgJsonNode *node) {
    if (node == NULL)
        return;
    if (node->type == VG_JSON_STRING) {
        free(node->as.string.data);
    } else if (node->type == VG_JSON_NUMBER) {
        free(node->as.number.lexeme);
    } else if (node->type == VG_JSON_ARRAY) {
        for (size_t index = 0u; index < node->as.array.count; ++index)
            vg_json_destroy(node->as.array.items[index]);
        free(node->as.array.items);
    } else if (node->type == VG_JSON_OBJECT) {
        for (size_t index = 0u; index < node->as.object.count; ++index) {
            free(node->as.object.pairs[index].key.data);
            vg_json_destroy(node->as.object.pairs[index].value);
        }
        free(node->as.object.pairs);
    }
    free(node);
}

bool vg_json_string_equals(const VgJsonString *string, const char *text) {
    size_t length = strlen(text);
    return string != NULL && string->length == length && memcmp(string->data, text, length) == 0;
}

static VgJsonNode *vg_json_build_node(VgJsonBuild *build, size_t depth) {
    if (build->next >= build->token_count || depth > VG_JSON_MAX_DEPTH) {
        vg_json_error(build->error, build->source_length,
                      depth > VG_JSON_MAX_DEPTH ? "JSON nesting limit exceeded"
                                                : "incomplete token tree");
        return NULL;
    }
    const jsmntok_t *token = &build->tokens[build->next++];
    VgJsonNode *node = calloc(1u, sizeof(*node));
    if (node == NULL) {
        vg_json_error(build->error, (size_t)token->start, "out of memory");
        return NULL;
    }
    size_t start = (size_t)token->start;
    size_t length = (size_t)(token->end - token->start);
    if (token->type == JSMN_STRING) {
        node->type = VG_JSON_STRING;
        if (!vg_json_decode_string(build->source + start, length, &node->as.string))
            goto invalid_string;
        if (node->as.string.length > VG_JSON_MAX_STRING_BYTES) {
            vg_json_error(build->error, start, "decoded JSON string limit exceeded");
            vg_json_destroy(node);
            return NULL;
        }
        return node;
    }
    if (token->type == JSMN_PRIMITIVE) {
        if (length == 4u && memcmp(build->source + start, "null", 4u) == 0) {
            node->type = VG_JSON_NULL;
        } else if (length == 4u && memcmp(build->source + start, "true", 4u) == 0) {
            node->type = VG_JSON_BOOL;
            node->as.boolean = true;
        } else if (length == 5u && memcmp(build->source + start, "false", 5u) == 0) {
            node->type = VG_JSON_BOOL;
        } else {
            node->type = VG_JSON_NUMBER;
            node->as.number.lexeme = malloc(length + 1u);
            if (node->as.number.lexeme == NULL)
                goto out_of_memory;
            memcpy(node->as.number.lexeme, build->source + start, length);
            node->as.number.lexeme[length] = '\0';
            node->as.number.length = length;
            if (!vg_json_parse_number(node->as.number.lexeme, length, &node->as.number.value)) {
                vg_json_error(build->error, start, "invalid or non-finite JSON number");
                vg_json_destroy(node);
                return NULL;
            }
        }
        return node;
    }
    if (token->type == JSMN_ARRAY) {
        node->type = VG_JSON_ARRAY;
        node->as.array.count = (size_t)token->size;
        if (node->as.array.count != 0u) {
            node->as.array.items = calloc(node->as.array.count, sizeof(*node->as.array.items));
            if (node->as.array.items == NULL)
                goto out_of_memory;
        }
        for (size_t index = 0u; index < node->as.array.count; ++index) {
            node->as.array.items[index] = vg_json_build_node(build, depth + 1u);
            if (node->as.array.items[index] == NULL) {
                vg_json_destroy(node);
                return NULL;
            }
        }
        return node;
    }
    if (token->type == JSMN_OBJECT) {
        node->type = VG_JSON_OBJECT;
        node->as.object.count = (size_t)token->size;
        if (node->as.object.count != 0u) {
            node->as.object.pairs = calloc(node->as.object.count, sizeof(*node->as.object.pairs));
            if (node->as.object.pairs == NULL)
                goto out_of_memory;
        }
        for (size_t index = 0u; index < node->as.object.count; ++index) {
            if (build->next >= build->token_count ||
                build->tokens[build->next].type != JSMN_STRING) {
                vg_json_error(build->error, start, "object key must be a string");
                vg_json_destroy(node);
                return NULL;
            }
            const jsmntok_t *key = &build->tokens[build->next++];
            size_t key_start = (size_t)key->start;
            size_t key_length = (size_t)(key->end - key->start);
            if (!vg_json_decode_string(build->source + key_start, key_length,
                                       &node->as.object.pairs[index].key))
                goto invalid_object_string;
            if (node->as.object.pairs[index].key.length > VG_JSON_MAX_STRING_BYTES) {
                vg_json_error(build->error, key_start, "decoded JSON object key limit exceeded");
                vg_json_destroy(node);
                return NULL;
            }
            for (size_t prior = 0u; prior < index; ++prior) {
                VgJsonString *left = &node->as.object.pairs[prior].key;
                VgJsonString *right = &node->as.object.pairs[index].key;
                if (left->length == right->length &&
                    memcmp(left->data, right->data, left->length) == 0) {
                    vg_json_error(build->error, key_start, "duplicate object key");
                    vg_json_destroy(node);
                    return NULL;
                }
            }
            node->as.object.pairs[index].value = vg_json_build_node(build, depth + 1u);
            if (node->as.object.pairs[index].value == NULL) {
                vg_json_destroy(node);
                return NULL;
            }
        }
        return node;
    }
    vg_json_error(build->error, start, "undefined JSON token");
    vg_json_destroy(node);
    return NULL;

invalid_object_string:
    vg_json_error(build->error, start, "invalid UTF-8 or Unicode escape in object key");
    vg_json_destroy(node);
    return NULL;
invalid_string:
    vg_json_error(build->error, start, "invalid UTF-8 or Unicode escape in string");
    vg_json_destroy(node);
    return NULL;
out_of_memory:
    vg_json_error(build->error, start, "out of memory");
    vg_json_destroy(node);
    return NULL;
}

VgJsonNode *vg_json_parse(const char *json, size_t length, VgJsonError *error) {
    if (json == NULL || length == 0u) {
        vg_json_error(error, 0u, "empty JSON input");
        return NULL;
    }
    if (memchr(json, '\0', length) != NULL) {
        vg_json_error(error, 0u, "embedded NUL is not allowed");
        return NULL;
    }
    jsmn_parser parser;
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json, length, NULL, 0u);
    if (token_count <= 0 || token_count > VG_JSON_MAX_TOKENS) {
        vg_json_error(error, parser.pos,
                      token_count > VG_JSON_MAX_TOKENS ? "JSON token limit exceeded"
                                                       : "invalid or incomplete JSON");
        return NULL;
    }
    jsmntok_t *tokens = calloc((size_t)token_count, sizeof(*tokens));
    if (tokens == NULL) {
        vg_json_error(error, 0u, "out of memory");
        return NULL;
    }
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, json, length, tokens, (size_t)token_count);
    if (parsed != token_count) {
        free(tokens);
        vg_json_error(error, parser.pos, "invalid or incomplete JSON");
        return NULL;
    }
    VgJsonBuild build = {json, length, tokens, (size_t)parsed, 0u, error};
    VgJsonNode *root = vg_json_build_node(&build, 0u);
    if (root != NULL && build.next != build.token_count) {
        vg_json_error(error, (size_t)tokens[build.next].start, "multiple JSON root values");
        vg_json_destroy(root);
        root = NULL;
    }
    free(tokens);
    return root;
}

const VgJsonPair *vg_json_object_pair(const VgJsonNode *object, const char *key) {
    if (object == NULL || object->type != VG_JSON_OBJECT)
        return NULL;
    for (size_t index = 0u; index < object->as.object.count; ++index) {
        if (vg_json_string_equals(&object->as.object.pairs[index].key, key))
            return &object->as.object.pairs[index];
    }
    return NULL;
}

const VgJsonNode *vg_json_object_get(const VgJsonNode *object, const char *key) {
    const VgJsonPair *pair = vg_json_object_pair(object, key);
    return pair == NULL ? NULL : pair->value;
}

static bool vg_json_buffer_append(VgJsonBuffer *buffer, const char *text, size_t length) {
    if (length > VG_JSON_MAX_OUTPUT - buffer->length)
        return false;
    size_t required = buffer->length + length + 1u;
    if (required > buffer->capacity) {
        size_t capacity = buffer->capacity == 0u ? 256u : buffer->capacity;
        while (capacity < required) {
            if (capacity > VG_JSON_MAX_OUTPUT / 2u) {
                capacity = VG_JSON_MAX_OUTPUT + 1u;
                break;
            }
            capacity *= 2u;
        }
        if (capacity > VG_JSON_MAX_OUTPUT)
            return false;
        char *data = realloc(buffer->data, capacity);
        if (data == NULL)
            return false;
        buffer->data = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool vg_json_write_indent(VgJsonBuffer *buffer, size_t depth) {
    for (size_t index = 0u; index < depth; ++index) {
        if (!vg_json_buffer_append(buffer, "  ", 2u))
            return false;
    }
    return true;
}

static bool vg_json_write_string(VgJsonBuffer *buffer, const VgJsonString *string) {
    if (!vg_json_buffer_append(buffer, "\"", 1u))
        return false;
    for (size_t index = 0u; index < string->length; ++index) {
        unsigned char character = (unsigned char)string->data[index];
        const char *escape = NULL;
        switch (character) {
        case '"':
            escape = "\\\"";
            break;
        case '\\':
            escape = "\\\\";
            break;
        case '\b':
            escape = "\\b";
            break;
        case '\f':
            escape = "\\f";
            break;
        case '\n':
            escape = "\\n";
            break;
        case '\r':
            escape = "\\r";
            break;
        case '\t':
            escape = "\\t";
            break;
        default:
            break;
        }
        if (escape != NULL) {
            if (!vg_json_buffer_append(buffer, escape, 2u))
                return false;
        } else if (character < 0x20u) {
            char encoded[7];
            int count = snprintf(encoded, sizeof(encoded), "\\u%04x", (unsigned int)character);
            if (count != 6 || !vg_json_buffer_append(buffer, encoded, 6u))
                return false;
        } else if (!vg_json_buffer_append(buffer, (const char *)&string->data[index], 1u)) {
            return false;
        }
    }
    return vg_json_buffer_append(buffer, "\"", 1u);
}

static int vg_json_pair_pointer_compare(const void *left, const void *right) {
    const VgJsonPair *const *a = left;
    const VgJsonPair *const *b = right;
    size_t common = (*a)->key.length < (*b)->key.length ? (*a)->key.length : (*b)->key.length;
    int comparison = memcmp((*a)->key.data, (*b)->key.data, common);
    if (comparison != 0)
        return comparison;
    return ((*a)->key.length > (*b)->key.length) - ((*a)->key.length < (*b)->key.length);
}

static size_t vg_json_count_object_pairs(const VgJsonNode *node) {
    size_t total = node->type == VG_JSON_OBJECT ? node->as.object.count : 0u;
    if (node->type == VG_JSON_ARRAY) {
        for (size_t index = 0u; index < node->as.array.count; ++index)
            total += vg_json_count_object_pairs(node->as.array.items[index]);
    } else if (node->type == VG_JSON_OBJECT) {
        for (size_t index = 0u; index < node->as.object.count; ++index)
            total += vg_json_count_object_pairs(node->as.object.pairs[index].value);
    }
    return total;
}

static bool vg_json_write_node(VgJsonBuffer *buffer, const VgJsonNode *node, size_t depth,
                               const VgJsonPair **sorted_pairs, size_t *next_pair) {
    switch (node->type) {
    case VG_JSON_NULL:
        return vg_json_buffer_append(buffer, "null", 4u);
    case VG_JSON_BOOL:
        return node->as.boolean ? vg_json_buffer_append(buffer, "true", 4u)
                                : vg_json_buffer_append(buffer, "false", 5u);
    case VG_JSON_NUMBER:
        return vg_json_buffer_append(buffer, node->as.number.lexeme, node->as.number.length);
    case VG_JSON_STRING:
        return vg_json_write_string(buffer, &node->as.string);
    case VG_JSON_ARRAY: {
        if (!vg_json_buffer_append(buffer, "[", 1u))
            return false;
        bool success = true;
        for (size_t index = 0u; success && index < node->as.array.count; ++index) {
            success =
                vg_json_buffer_append(buffer, index == 0u ? "\n" : ",\n", index == 0u ? 1u : 2u) &&
                vg_json_write_indent(buffer, depth + 1u) &&
                vg_json_write_node(buffer, node->as.array.items[index], depth + 1u, sorted_pairs,
                                   next_pair);
        }
        if (!success)
            return false;
        if (node->as.array.count != 0u &&
            (!vg_json_buffer_append(buffer, "\n", 1u) || !vg_json_write_indent(buffer, depth)))
            return false;
        return vg_json_buffer_append(buffer, "]", 1u);
    }
    case VG_JSON_OBJECT: {
        if (!vg_json_buffer_append(buffer, "{", 1u))
            return false;
        size_t first_pair = *next_pair;
        *next_pair += node->as.object.count;
        for (size_t index = 0u; index < node->as.object.count; ++index)
            sorted_pairs[first_pair + index] = &node->as.object.pairs[index];
        if (node->as.object.count > 1u)
            qsort(sorted_pairs + first_pair, node->as.object.count, sizeof(*sorted_pairs),
                  vg_json_pair_pointer_compare);
        bool success = true;
        for (size_t index = 0u; success && index < node->as.object.count; ++index) {
            const VgJsonPair *pair = sorted_pairs[first_pair + index];
            success =
                vg_json_buffer_append(buffer, index == 0u ? "\n" : ",\n", index == 0u ? 1u : 2u) &&
                vg_json_write_indent(buffer, depth + 1u) &&
                vg_json_write_string(buffer, &pair->key) &&
                vg_json_buffer_append(buffer, ": ", 2u) &&
                vg_json_write_node(buffer, pair->value, depth + 1u, sorted_pairs, next_pair);
        }
        if (!success)
            return false;
        if (node->as.object.count != 0u &&
            (!vg_json_buffer_append(buffer, "\n", 1u) || !vg_json_write_indent(buffer, depth)))
            return false;
        return vg_json_buffer_append(buffer, "}", 1u);
    }
    }
    return false;
}

bool vg_json_write_canonical(const VgJsonNode *node, char **out_json, size_t *out_length) {
    if (node == NULL || out_json == NULL || out_length == NULL)
        return false;
    VgJsonBuffer buffer = {0};
    size_t pair_count = vg_json_count_object_pairs(node);
    const VgJsonPair **sorted_pairs = NULL;
    if (pair_count != 0u) {
        sorted_pairs = malloc(pair_count * sizeof(*sorted_pairs));
        if (sorted_pairs == NULL)
            return false;
    }
    size_t next_pair = 0u;
    bool success = vg_json_write_node(&buffer, node, 0u, sorted_pairs, &next_pair) &&
                   vg_json_buffer_append(&buffer, "\n", 1u);
    free(sorted_pairs);
    if (!success) {
        free(buffer.data);
        return false;
    }
    *out_json = buffer.data;
    *out_length = buffer.length;
    return true;
}
