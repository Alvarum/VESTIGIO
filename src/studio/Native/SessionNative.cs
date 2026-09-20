using System.Runtime.InteropServices;

namespace RetroForge.Studio;

/// <summary>La ABI de prueba comparte la sesión del Player. Los píxeles se
/// copian a memoria administrada; no sobreviven punteros nativos a un frame.</summary>
internal static class SessionNative
{
    [DllImport("retro_editor", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_start_session(nint document, out nint session,
        out EditorNative.Error error);
    [DllImport("retro_session", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void re_session_destroy(nint session);
    [DllImport("retro_session", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void re_session_frame(nint session, double elapsed, float moveX,
        float moveY, float lookX, float lookY, uint pressed, uint held, int focused, int singleStep);
    [DllImport("retro_session", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_session_copy_pixels(nint session, [Out] byte[] pixels, uint bytes);
    [DllImport("retro_session", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_session_flags(nint session);
}
