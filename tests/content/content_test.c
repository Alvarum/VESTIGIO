#include "content/content.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);    \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void fixture_path(char *output, size_t capacity, const char *name) {
    (void)snprintf(output, capacity, "%s/%s", VG_CONTENT_FIXTURES, name);
}

static void test_valid_documents(void) {
    char path[1024];
    VgContentDiagnostic diagnostic;
    VgContentDocument *project = NULL;
    fixture_path(path, sizeof(path), "valid-project.json");
    CHECK(vg_content_parse_file(path, &project, &diagnostic));
    CHECK(project != NULL);
    CHECK(vg_content_document_kind(project) == VG_CONTENT_PROJECT);
    CHECK(strcmp(vg_content_document_id(project), "10000000-0000-0000-0000-000000000001") == 0);
    vg_content_document_destroy(project);

    VgContentDocument *level = NULL;
    fixture_path(path, sizeof(path), "valid-level.json");
    CHECK(vg_content_parse_file(path, &level, &diagnostic));
    CHECK(level != NULL);
    CHECK(vg_content_document_kind(level) == VG_CONTENT_LEVEL);
    CHECK(vg_content_level_entity_count(level) == 2u);

    char *canonical = NULL;
    size_t canonical_length = 0u;
    CHECK(vg_content_write_canonical(level, &canonical, &canonical_length, &diagnostic));
    CHECK(canonical != NULL && canonical_length == strlen(canonical));
    CHECK(strstr(canonical, "vendor.marker") != NULL);
    CHECK(strstr(canonical, "optional survives") != NULL);
    CHECK(strstr(canonical, "caf\xC3\xA9") != NULL);
    const char *values = strstr(canonical, "\"values\"");
    CHECK(values != NULL);
    const char *three = values == NULL ? NULL : strstr(values, "3");
    const char *two = three == NULL ? NULL : strstr(three + 1, "2");
    const char *one = two == NULL ? NULL : strstr(two + 1, "1");
    CHECK(three != NULL && two != NULL && one != NULL && three < two && two < one);

    VgContentDocument *roundtrip = NULL;
    CHECK(vg_content_parse_memory("roundtrip.level.json", canonical, canonical_length, &roundtrip,
                                  &diagnostic));
    char *canonical_again = NULL;
    size_t canonical_again_length = 0u;
    CHECK(vg_content_write_canonical(roundtrip, &canonical_again, &canonical_again_length,
                                     &diagnostic));
    CHECK(canonical_length == canonical_again_length);
    CHECK(memcmp(canonical, canonical_again, canonical_length) == 0);
    vg_content_string_destroy(canonical_again);
    vg_content_document_destroy(roundtrip);
    vg_content_string_destroy(canonical);
    vg_content_document_destroy(level);
}

typedef struct InvalidCase {
    const char *file;
    VgContentDiagnosticCode code;
    const char *path_fragment;
    bool has_id;
} InvalidCase;

static void test_invalid_corpus_and_transaction(void) {
    const InvalidCase cases[] = {
        {"invalid-future.json", VG_CONTENT_DIAGNOSTIC_VERSION, "$.version", false},
        {"invalid-duplicate.json", VG_CONTENT_DIAGNOSTIC_DUPLICATE, ".id", true},
        {"invalid-reference.json", VG_CONTENT_DIAGNOSTIC_REFERENCE, ".parent", true},
        {"invalid-nonfinite.json", VG_CONTENT_DIAGNOSTIC_PARSE, "$", false},
    };
    char path[1024];
    VgContentDiagnostic diagnostic;
    VgContentDocument *sentinel = NULL;
    fixture_path(path, sizeof(path), "valid-project.json");
    CHECK(vg_content_parse_file(path, &sentinel, &diagnostic));
    VgContentDocument *output = sentinel;
    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        fixture_path(path, sizeof(path), cases[index].file);
        CHECK(!vg_content_parse_file(path, &output, &diagnostic));
        CHECK(output == sentinel);
        CHECK(diagnostic.code == cases[index].code);
        CHECK(strstr(diagnostic.path, cases[index].path_fragment) != NULL);
        CHECK(strstr(diagnostic.file, cases[index].file) != NULL);
        CHECK(!cases[index].has_id || diagnostic.id[0] != '\0');
    }
    CHECK(vg_content_document_kind(sentinel) == VG_CONTENT_PROJECT);
    vg_content_document_destroy(sentinel);
}

static void test_required_unknown_cycle_and_transform(void) {
    static const char unknown_required[] =
        "{\"format\":\"vestigio.level\",\"version\":1,"
        "\"id\":\"20000000-0000-0000-0000-000000000001\",\"name\":\"x\","
        "\"coordinates\":\"right-handed-z-up-meters\",\"required\":[\"vendor.future\"],"
        "\"entities\":[]}";
    static const char cycle[] =
        "{\"format\":\"vestigio.level\",\"version\":1,"
        "\"id\":\"20000000-0000-0000-0000-000000000001\",\"name\":\"x\","
        "\"coordinates\":\"right-handed-z-up-meters\",\"entities\":["
        "{\"id\":\"40000000-0000-0000-0000-000000000001\","
        "\"parent\":\"40000000-0000-0000-0000-000000000002\","
        "\"transform\":{\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}},"
        "{\"id\":\"40000000-0000-0000-0000-000000000002\","
        "\"parent\":\"40000000-0000-0000-0000-000000000001\","
        "\"transform\":{\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}}]}";
    static const char zero_scale[] =
        "{\"format\":\"vestigio.level\",\"version\":1,"
        "\"id\":\"20000000-0000-0000-0000-000000000001\",\"name\":\"x\","
        "\"coordinates\":\"right-handed-z-up-meters\",\"entities\":["
        "{\"id\":\"40000000-0000-0000-0000-000000000001\","
        "\"transform\":{\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,0,1]}}]}";
    static const char unknown_required_component[] =
        "{\"format\":\"vestigio.level\",\"version\":1,"
        "\"id\":\"20000000-0000-0000-0000-000000000001\",\"name\":\"x\","
        "\"coordinates\":\"right-handed-z-up-meters\",\"entities\":["
        "{\"id\":\"40000000-0000-0000-0000-000000000001\","
        "\"transform\":{\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]},"
        "\"components\":{\"vendor.future\":{\"version\":9}},"
        "\"required_components\":[\"vendor.future\"]}]}";
    static const char duplicate_key[] =
        "{\"format\":\"vestigio.project\",\"format\":\"vestigio.level\"}";
    static const char malformed_parent[] =
        "{\"format\":\"vestigio.level\",\"version\":1,"
        "\"id\":\"20000000-0000-0000-0000-000000000001\",\"name\":\"x\","
        "\"coordinates\":\"right-handed-z-up-meters\",\"entities\":["
        "{\"id\":\"40000000-0000-0000-0000-000000000001\",\"parent\":12,"
        "\"transform\":{\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}}]}";
    struct {
        const char *json;
        VgContentDiagnosticCode code;
    } cases[] = {{unknown_required, VG_CONTENT_DIAGNOSTIC_REQUIRED},
                 {cycle, VG_CONTENT_DIAGNOSTIC_REFERENCE},
                 {zero_scale, VG_CONTENT_DIAGNOSTIC_TRANSFORM},
                 {unknown_required_component, VG_CONTENT_DIAGNOSTIC_REQUIRED},
                 {duplicate_key, VG_CONTENT_DIAGNOSTIC_PARSE}};
    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        VgContentDocument *output = NULL;
        VgContentDiagnostic diagnostic;
        CHECK(!vg_content_parse_memory("inline.json", cases[index].json, strlen(cases[index].json),
                                       &output, &diagnostic));
        CHECK(output == NULL);
        CHECK(diagnostic.code == cases[index].code);
        CHECK(diagnostic.file[0] != '\0' && diagnostic.path[0] != '\0' &&
              diagnostic.message[0] != '\0');
    }
    VgContentDocument *output = NULL;
    VgContentDiagnostic diagnostic;
    CHECK(!vg_content_parse_memory("malformed-parent.json", malformed_parent,
                                   strlen(malformed_parent), &output, &diagnostic));
    CHECK(output == NULL);
    CHECK(diagnostic.code == VG_CONTENT_DIAGNOSTIC_TYPE);
    CHECK(strcmp(diagnostic.path, "$.entities[0].parent") == 0);
}

static void test_many_object_keys(void) {
    enum { KEY_COUNT = 4096, JSON_CAPACITY = 256 * 1024 };
    static const char prefix[] =
        "{\"format\":\"vestigio.project\",\"version\":1,"
        "\"id\":\"10000000-0000-0000-0000-000000000001\",\"name\":\"keys\","
        "\"engine\":{\"api_major\":0,\"min_minor\":1},\"entry_level\":\"x.json\","
        "\"extensions\":{\"synthetic\":{";
    char *json = malloc(JSON_CAPACITY);
    CHECK(json != NULL);
    if (json == NULL)
        return;
    memcpy(json, prefix, sizeof(prefix) - 1u);
    size_t length = sizeof(prefix) - 1u;
    for (size_t offset = 0u; offset < KEY_COUNT; ++offset) {
        size_t key = KEY_COUNT - offset - 1u;
        int written = snprintf(json + length, JSON_CAPACITY - length, "%s\"key%05zu\":%zu",
                               offset == 0u ? "" : ",", key, key);
        if (written < 0 || (size_t)written >= JSON_CAPACITY - length) {
            CHECK(false);
            free(json);
            return;
        }
        length += (size_t)written;
    }
    static const char suffix[] = "}}}";
    memcpy(json + length, suffix, sizeof(suffix));
    length += sizeof(suffix) - 1u;

    VgContentDiagnostic diagnostic;
    VgContentDocument *document = NULL;
    CHECK(vg_content_parse_memory("many-keys.project.json", json, length, &document, &diagnostic));
    free(json);
    if (document == NULL)
        return;
    char *canonical = NULL;
    size_t canonical_length = 0u;
    CHECK(vg_content_write_canonical(document, &canonical, &canonical_length, &diagnostic));
    const char *first = canonical == NULL ? NULL : strstr(canonical, "\"key00000\"");
    const char *middle = canonical == NULL ? NULL : strstr(canonical, "\"key02048\"");
    const char *last = canonical == NULL ? NULL : strstr(canonical, "\"key04095\"");
    CHECK(first != NULL && middle != NULL && last != NULL && first < middle && middle < last);

    VgContentDocument *roundtrip = NULL;
    CHECK(canonical != NULL &&
          vg_content_parse_memory("many-keys-roundtrip.project.json", canonical, canonical_length,
                                  &roundtrip, &diagnostic));
    char *canonical_again = NULL;
    size_t canonical_again_length = 0u;
    CHECK(roundtrip != NULL && vg_content_write_canonical(roundtrip, &canonical_again,
                                                          &canonical_again_length, &diagnostic));
    CHECK(canonical != NULL && canonical_again != NULL &&
          canonical_length == canonical_again_length &&
          memcmp(canonical, canonical_again, canonical_length) == 0);
    vg_content_string_destroy(canonical_again);
    vg_content_document_destroy(roundtrip);
    vg_content_string_destroy(canonical);
    vg_content_document_destroy(document);
}

static void test_locale_precision_and_limits(void) {
    static const char precise[] =
        "{\"format\":\"vestigio.project\",\"version\":1,"
        "\"id\":\"10000000-0000-0000-0000-000000000001\",\"name\":\"x\","
        "\"engine\":{\"api_major\":0,\"min_minor\":1},\"entry_level\":\"x.json\","
        "\"precision\":0.10000000000000001}";
    const char *old_locale_value = setlocale(LC_NUMERIC, NULL);
    char old_locale[128] = "C";
    if (old_locale_value != NULL)
        (void)snprintf(old_locale, sizeof(old_locale), "%s", old_locale_value);
    const char *locales[] = {"Spanish_Spain.1252", "es_ES.UTF-8", "de_DE.UTF-8"};
    for (size_t index = 0u; index < sizeof(locales) / sizeof(locales[0]); ++index) {
        if (setlocale(LC_NUMERIC, locales[index]) != NULL)
            break;
    }
    VgContentDiagnostic diagnostic;
    VgContentDocument *document = NULL;
    CHECK(vg_content_parse_memory("locale.project.json", precise, strlen(precise), &document,
                                  &diagnostic));
    char *canonical = NULL;
    size_t length = 0u;
    CHECK(vg_content_write_canonical(document, &canonical, &length, &diagnostic));
    CHECK(strstr(canonical, "0.10000000000000001") != NULL);
    CHECK(strstr(canonical, "0,10000000000000001") == NULL);
    vg_content_string_destroy(canonical);
    vg_content_document_destroy(document);
    (void)setlocale(LC_NUMERIC, old_locale);

    VgContentDocument *sentinel = (VgContentDocument *)(void *)&diagnostic;
    CHECK(!vg_content_parse_memory("large.json", "{}", VG_CONTENT_MAX_FILE_BYTES + 1u, &sentinel,
                                   &diagnostic));
    CHECK(sentinel == (VgContentDocument *)(void *)&diagnostic);
    CHECK(diagnostic.code == VG_CONTENT_DIAGNOSTIC_LIMIT);

    static const char string_prefix[] =
        "{\"format\":\"vestigio.project\",\"version\":1,"
        "\"id\":\"10000000-0000-0000-0000-000000000001\",\"name\":\"x\","
        "\"engine\":{\"api_major\":0,\"min_minor\":1},\"entry_level\":\"x.json\","
        "\"optional\":\"";
    static const char string_suffix[] = "\"}";
    size_t long_length =
        sizeof(string_prefix) - 1u + VG_CONTENT_MAX_STRING_BYTES + 1u + sizeof(string_suffix) - 1u;
    char *long_json = malloc(long_length + 1u);
    CHECK(long_json != NULL);
    if (long_json != NULL) {
        size_t offset = sizeof(string_prefix) - 1u;
        memcpy(long_json, string_prefix, offset);
        memset(long_json + offset, 'a', VG_CONTENT_MAX_STRING_BYTES + 1u);
        offset += VG_CONTENT_MAX_STRING_BYTES + 1u;
        memcpy(long_json + offset, string_suffix, sizeof(string_suffix));
        VgContentDocument *long_output = NULL;
        CHECK(!vg_content_parse_memory("long-string.json", long_json, long_length, &long_output,
                                       &diagnostic));
        CHECK(long_output == NULL);
        CHECK(diagnostic.code == VG_CONTENT_DIAGNOSTIC_LIMIT);
        free(long_json);
    }

    char *unchanged_json = (char *)(void *)&diagnostic;
    size_t unchanged_length = 42u;
    CHECK(!vg_content_write_canonical(NULL, &unchanged_json, &unchanged_length, &diagnostic));
    CHECK(unchanged_json == (char *)(void *)&diagnostic);
    CHECK(unchanged_length == 42u);
}

int main(void) {
    test_valid_documents();
    test_invalid_corpus_and_transaction();
    test_required_unknown_cycle_and_transform();
    test_many_object_keys();
    test_locale_precision_and_limits();
    if (failures != 0)
        (void)fprintf(stderr, "%d content checks failed\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
