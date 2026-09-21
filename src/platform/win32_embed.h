#ifndef VESTIGIO_WIN32_EMBED_H
#define VESTIGIO_WIN32_EMBED_H

#include <stdbool.h>

bool vg_win32_embed_window(void *child, void *parent, int width, int height);
void vg_win32_unembed_window(void *child);
bool vg_win32_resize_embedded(void *child, int width, int height);
bool vg_win32_embedded_size(void *child, unsigned int *width, unsigned int *height);
bool vg_win32_focus_embedded(void *child);
bool vg_win32_embedded_has_focus(void *child);
unsigned long vg_win32_thread_id(void);

#endif /* VESTIGIO_WIN32_EMBED_H */
