#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform/win32_embed.h"

bool vg_win32_embed_window(void *child_value, void *parent_value, int width, int height) {
    HWND child = (HWND)child_value;
    HWND parent = (HWND)parent_value;
    if (!child || !parent || width < 1 || height < 1)
        return false;
    SetLastError(ERROR_SUCCESS);
    HWND previous_parent = SetParent(child, parent);
    if (!previous_parent && GetLastError() != ERROR_SUCCESS)
        return false;
    LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
    style &=
        ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous_style = SetWindowLongPtrW(child, GWL_STYLE, style);
    if (previous_style == 0 && GetLastError() != ERROR_SUCCESS)
        return false;
    return vg_win32_resize_embedded(child, width, height);
}

bool vg_win32_resize_embedded(void *child_value, int width, int height) {
    HWND child = (HWND)child_value;
    return child && width > 0 && height > 0 &&
           SetWindowPos(child, NULL, 0, 0, width, height,
                        SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW) != 0;
}

bool vg_win32_focus_embedded(void *child_value) {
    HWND child = (HWND)child_value;
    if (!child)
        return false;
    (void)SetFocus(child);
    return GetFocus() == child;
}

bool vg_win32_embedded_has_focus(void *child_value) {
    HWND child = (HWND)child_value;
    return child && GetFocus() == child;
}
