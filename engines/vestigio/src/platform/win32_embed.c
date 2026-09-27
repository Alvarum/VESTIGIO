#define WIN32_LEAN_AND_MEAN
// commctrl.h depends on Win32 base types; keep this include order.
// clang-format off
#include <windows.h>
#include <commctrl.h>
// clang-format on

#include "platform/win32_embed.h"

static LRESULT CALLBACK embedded_window_proc(HWND window, UINT message, WPARAM w_param,
                                             LPARAM l_param, UINT_PTR subclass_id,
                                             DWORD_PTR reference_data) {
    (void)subclass_id;
    (void)reference_data;
    switch (message) {
    case WM_MOUSEACTIVATE:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        (void)SetFocus(window);
        break;
    default:
        break;
    }
    return DefSubclassProc(window, message, w_param, l_param);
}

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
    style |= WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous_style = SetWindowLongPtrW(child, GWL_STYLE, style);
    if (previous_style == 0 && GetLastError() != ERROR_SUCCESS)
        return false;
    if (!vg_win32_resize_embedded(child, width, height))
        return false;
    return SetWindowSubclass(child, embedded_window_proc, 1u, 0u) != 0;
}

void vg_win32_unembed_window(void *child_value) {
    HWND child = (HWND)child_value;
    if (child)
        (void)RemoveWindowSubclass(child, embedded_window_proc, 1u);
}

bool vg_win32_resize_embedded(void *child_value, int width, int height) {
    HWND child = (HWND)child_value;
    return child && width > 0 && height > 0 &&
           SetWindowPos(child, NULL, 0, 0, width, height,
                        SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW) != 0;
}

bool vg_win32_embedded_size(void *child_value, unsigned int *width, unsigned int *height) {
    HWND child = (HWND)child_value;
    RECT bounds = {0};
    if (!child || !width || !height || !GetClientRect(child, &bounds) || bounds.right < 0 ||
        bounds.bottom < 0)
        return false;
    *width = (unsigned int)bounds.right;
    *height = (unsigned int)bounds.bottom;
    return true;
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

unsigned long vg_win32_thread_id(void) {
    return GetCurrentThreadId();
}
