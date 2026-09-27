/* Regresión de la sesión compartida: dos anfitriones reciben idénticos frames;
 * editar el documento después de comenzar no altera la copia en ejecución. */
#include "retro/editor.h"
#include "retro/session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    ReEditorDocument *document = nullptr;
    ReGameSession *first = nullptr, *second = nullptr;
    ReError error = {0};
    int result = 1;
    if (!re_editor_open(RETRO_SOURCE_DIR "/assets/studio/haunted.retro", &document, &error))
        goto cleanup;
    if (!re_editor_set_property(document, RE_EDITOR_SECTOR, 0, "light", ".6", &error) ||
        !re_editor_start_session(document, &first, &error) ||
        !re_editor_start_session(document, &second, &error))
        goto cleanup;
    uint32_t width = 0u, height = 0u;
    if (!re_session_dimensions(first, &width, &height) || width != 480u || height != 270u ||
        re_session_binding_count(first) == 0u)
        goto cleanup;
    uint64_t action = 0u;
    uint32_t code = 0u;
    if (!re_session_binding(first, 0u, &action, &code) || action == 0u || code == 0u)
        goto cleanup;
    ReEditorOverview before = {0}, after = {0};
    if (!re_editor_overview(document, &before) || !before.dirty)
        goto cleanup;
    if (!re_editor_set_property(document, RE_EDITOR_SECTOR, 0, "light", ".1", &error))
        goto cleanup;
    for (int i = 0; i < 8; i++) {
        re_session_frame(first, RE_FIXED_DT, 0, .2f, 0, 0, 0, 0, 0, 1, 1);
        re_session_frame(second, RE_FIXED_DT, 0, .2f, 0, 0, 0, 0, 0, 1, 1);
    }
    const ReRenderer *a = re_session_renderer(first), *b = re_session_renderer(second);
    if (memcmp(a->pixels, b->pixels, 480u * 270u * 4u) != 0)
        goto cleanup;
    if (!re_editor_overview(document, &after) || after.revision != before.revision + 1u ||
        !after.dirty || !after.can_undo)
        goto cleanup;
    /* El exportado tiene que cargar también la DLL de sesión; esta prueba
     * enlaza la misma frontera que consume el ejecutable Player. */
    result = 0;
cleanup:
    if (result)
        (void)fprintf(stderr, "Sesión compartida: %s\n", error.message);
    re_session_destroy(first);
    re_session_destroy(second);
    re_editor_close(document);
    return result;
}
