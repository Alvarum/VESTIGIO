/* JSMN, a minimalistic JSON parser in C.
 *
 * Source: https://github.com/zserge/jsmn
 * This copy comes from cgltf 1.15 embedded in the pinned raylib 6.0 source at
 * .deps/raylib-dbc56a87da87d973a9c5baa4e7438a9d20121d28/src/external/cgltf.h.
 * Strict parsing and parent links match that reviewed copy.
 *
 * Copyright (c) 2010 Serge A. Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE. */

#ifndef VESTIGIO_CONTENT_THIRD_PARTY_JSMN_H
#define VESTIGIO_CONTENT_THIRD_PARTY_JSMN_H

#include <stddef.h>

typedef enum JsmnType {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1,
    JSMN_ARRAY = 2,
    JSMN_STRING = 3,
    JSMN_PRIMITIVE = 4
} jsmntype_t;

enum JsmnError { JSMN_ERROR_NOMEM = -1, JSMN_ERROR_INVAL = -2, JSMN_ERROR_PART = -3 };

typedef struct JsmnToken {
    jsmntype_t type;
    ptrdiff_t start;
    ptrdiff_t end;
    int size;
    int parent;
} jsmntok_t;

typedef struct JsmnParser {
    size_t pos;
    unsigned int toknext;
    int toksuper;
} jsmn_parser;

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens, size_t count) {
    if (parser->toknext >= count)
        return NULL;
    jsmntok_t *token = &tokens[parser->toknext++];
    token->start = -1;
    token->end = -1;
    token->size = 0;
    token->parent = -1;
    token->type = JSMN_UNDEFINED;
    return token;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, ptrdiff_t start, ptrdiff_t end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *parser, const char *json, size_t length,
                                jsmntok_t *tokens, size_t count) {
    ptrdiff_t start = (ptrdiff_t)parser->pos;
    for (; parser->pos < length && json[parser->pos] != '\0'; ++parser->pos) {
        char character = json[parser->pos];
        if (character == '\t' || character == '\r' || character == '\n' || character == ' ' ||
            character == ',' || character == ']' || character == '}')
            break;
        if ((unsigned char)character < 32u || (unsigned char)character >= 127u) {
            parser->pos = (size_t)start;
            return JSMN_ERROR_INVAL;
        }
    }
    if (parser->pos == length) {
        parser->pos = (size_t)start;
        return JSMN_ERROR_PART;
    }
    if (tokens != NULL) {
        jsmntok_t *token = jsmn_alloc_token(parser, tokens, count);
        if (token == NULL) {
            parser->pos = (size_t)start;
            return JSMN_ERROR_NOMEM;
        }
        jsmn_fill_token(token, JSMN_PRIMITIVE, start, (ptrdiff_t)parser->pos);
        token->parent = parser->toksuper;
    }
    --parser->pos;
    return 0;
}

static int jsmn_hex(char value) {
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'F') ||
           (value >= 'a' && value <= 'f');
}

static int jsmn_parse_string(jsmn_parser *parser, const char *json, size_t length,
                             jsmntok_t *tokens, size_t count) {
    ptrdiff_t start = (ptrdiff_t)parser->pos++;
    for (; parser->pos < length && json[parser->pos] != '\0'; ++parser->pos) {
        char character = json[parser->pos];
        if (character == '"') {
            if (tokens != NULL) {
                jsmntok_t *token = jsmn_alloc_token(parser, tokens, count);
                if (token == NULL) {
                    parser->pos = (size_t)start;
                    return JSMN_ERROR_NOMEM;
                }
                jsmn_fill_token(token, JSMN_STRING, start + 1, (ptrdiff_t)parser->pos);
                token->parent = parser->toksuper;
            }
            return 0;
        }
        if (character == '\\' && parser->pos + 1u < length) {
            character = json[++parser->pos];
            if (character == 'u') {
                for (int index = 0; index < 4; ++index) {
                    if (++parser->pos >= length || !jsmn_hex(json[parser->pos])) {
                        parser->pos = (size_t)start;
                        return JSMN_ERROR_INVAL;
                    }
                }
            } else if (character != '"' && character != '/' && character != '\\' &&
                       character != 'b' && character != 'f' && character != 'r' &&
                       character != 'n' && character != 't') {
                parser->pos = (size_t)start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
    parser->pos = (size_t)start;
    return JSMN_ERROR_PART;
}

static int jsmn_parse(jsmn_parser *parser, const char *json, size_t length, jsmntok_t *tokens,
                      size_t count) {
    int found = (int)parser->toknext;
    for (; parser->pos < length && json[parser->pos] != '\0'; ++parser->pos) {
        char character = json[parser->pos];
        jsmntok_t *token = NULL;
        switch (character) {
        case '{':
        case '[':
            ++found;
            if (tokens == NULL)
                break;
            token = jsmn_alloc_token(parser, tokens, count);
            if (token == NULL)
                return JSMN_ERROR_NOMEM;
            if (parser->toksuper != -1) {
                ++tokens[parser->toksuper].size;
                token->parent = parser->toksuper;
            }
            token->type = character == '{' ? JSMN_OBJECT : JSMN_ARRAY;
            token->start = (ptrdiff_t)parser->pos;
            parser->toksuper = (int)parser->toknext - 1;
            break;
        case '}':
        case ']': {
            if (tokens == NULL)
                break;
            jsmntype_t type = character == '}' ? JSMN_OBJECT : JSMN_ARRAY;
            if (parser->toknext < 1u)
                return JSMN_ERROR_INVAL;
            token = &tokens[parser->toknext - 1u];
            for (;;) {
                if (token->start != -1 && token->end == -1) {
                    if (token->type != type)
                        return JSMN_ERROR_INVAL;
                    token->end = (ptrdiff_t)parser->pos + 1;
                    parser->toksuper = token->parent;
                    break;
                }
                if (token->parent == -1) {
                    if (token->type != type || parser->toksuper == -1)
                        return JSMN_ERROR_INVAL;
                    break;
                }
                token = &tokens[token->parent];
            }
            break;
        }
        case '"': {
            int result = jsmn_parse_string(parser, json, length, tokens, count);
            if (result < 0)
                return result;
            ++found;
            if (parser->toksuper != -1 && tokens != NULL)
                ++tokens[parser->toksuper].size;
            break;
        }
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            break;
        case ':':
            parser->toksuper = (int)parser->toknext - 1;
            break;
        case ',':
            if (tokens != NULL && parser->toksuper != -1 &&
                tokens[parser->toksuper].type != JSMN_ARRAY &&
                tokens[parser->toksuper].type != JSMN_OBJECT)
                parser->toksuper = tokens[parser->toksuper].parent;
            break;
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 't':
        case 'f':
        case 'n': {
            if (tokens != NULL && parser->toksuper != -1) {
                jsmntok_t *superior = &tokens[parser->toksuper];
                if (superior->type == JSMN_OBJECT ||
                    (superior->type == JSMN_STRING && superior->size != 0))
                    return JSMN_ERROR_INVAL;
            }
            int result = jsmn_parse_primitive(parser, json, length, tokens, count);
            if (result < 0)
                return result;
            ++found;
            if (parser->toksuper != -1 && tokens != NULL)
                ++tokens[parser->toksuper].size;
            break;
        }
        default:
            return JSMN_ERROR_INVAL;
        }
    }
    if (tokens != NULL) {
        for (int index = (int)parser->toknext - 1; index >= 0; --index) {
            if (tokens[index].start != -1 && tokens[index].end == -1)
                return JSMN_ERROR_PART;
        }
    }
    return found;
}

static void jsmn_init(jsmn_parser *parser) {
    parser->pos = 0u;
    parser->toknext = 0u;
    parser->toksuper = -1;
}

#endif
