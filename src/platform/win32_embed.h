#ifndef VESTIGIO_WIN32_EMBED_H
#define VESTIGIO_WIN32_EMBED_H

#include <stdbool.h>

bool vg_win32_embed_window(void *child, void *parent, int width, int height);
bool vg_win32_resize_embedded(void *child, int width, int height);
bool vg_win32_focus_embedded(void *child);
bool vg_win32_embedded_has_focus(void *child);

#endif /* VESTIGIO_WIN32_EMBED_H */
