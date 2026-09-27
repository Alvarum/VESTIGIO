using System.Runtime.InteropServices;
using System.Text;

namespace RetroForge.Studio;

internal static class GpuHostNative
{
    private const uint WsChildVisible = 0x50000000;
    private const uint SsCenter = 0x00000001;

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint vg_gpu_host_create(nint parentWindow, uint width, uint height,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint vg_gpu_host_window(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_render(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_visual_mode(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_set_visual_mode(nint host, int mode,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_load_visual_profile(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_save_visual_profile(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nuint vg_gpu_host_visual_light_count(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_visual_fog_enabled(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_set_audio_enabled(nint host, int enabled);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_audio_device_state(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong vg_gpu_host_audio_voice_starts(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint vg_gpu_host_audio_active_voices(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint vg_gpu_host_audio_music_streams(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong vg_gpu_host_audio_stream_updates(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_audio_focus_paused(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern float vg_gpu_host_audio_gain(nint host, uint bus);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_set_audio_gain(nint host, uint bus, float gain);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_save_audio_gains(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_open_level(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string levelPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modelPath,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_set_mode(nint host, int play);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_interact(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nuint vg_gpu_host_door_count(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_door_angle(nint host, nuint index,
        out float angle);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_mode(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_frame(nint host, double elapsed, float moveX,
        float moveY, float lookX, float lookY, int jump, int focused);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_pick(nint host, float u, float v,
        [Out] byte[] uuid, nuint uuidCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_set_pick_mask(nint host, uint mask);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_add_mesh(nint host, [Out] byte[] uuid,
        nuint uuidCapacity, [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_duplicate_selected(nint host, [Out] byte[] uuid,
        nuint uuidCapacity, [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_select(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string uuid);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_selected_uuid(nint host, [Out] byte[] uuid,
        nuint uuidCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_selected_transform(nint host,
        [Out] float[] position, [Out] float[] rotation, [Out] float[] scale);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_set_selected_transform(nint host,
        [In] float[] position, [In] float[] rotation, [In] float[] scale,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_save_level(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_reopen_level(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string levelPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string modelPath,
        [Out] byte[] error, nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_is_dirty(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_undo(nint host, [Out] byte[] error,
        nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_redo(nint host, [Out] byte[] error,
        nuint errorCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nuint vg_gpu_host_entity_count(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_entity_at(nint host, nuint index,
        [Out] byte[] uuid, nuint uuidCapacity);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong vg_gpu_host_document_revision(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern nuint vg_gpu_host_animation_count(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_animation_height(nint host, nuint index,
        out float height);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong vg_gpu_host_readbacks(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_camera_position(nint host,
        out float x, out float y, out float z);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_set_camera_mode(nint host, int mode);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_frame_selection(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_resize(nint host, uint width, uint height);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_size(nint host, out uint width, out uint height);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_capture(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_focus(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_has_focus(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_destroy(nint host);

    [DllImport("user32")]
    internal static extern short GetAsyncKeyState(int virtualKey);

    [DllImport("user32", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetCursorPos(out NativePoint point);

    [DllImport("user32", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool ScreenToClient(nint window, ref NativePoint point);

    [StructLayout(LayoutKind.Sequential)]
    internal struct NativePoint
    {
        public int X;
        public int Y;
    }

    [DllImport("user32", EntryPoint = "CreateWindowExW", CharSet = CharSet.Unicode,
        SetLastError = true)]
    private static extern nint CreateWindowEx(uint exStyle, string className, string windowName,
        uint style, int x, int y, int width, int height, nint parent, nint menu,
        nint instance, nint parameter);

    [DllImport("user32", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DestroyWindow(nint window);

    internal static nint CreateFallbackWindow(nint parent, string message) =>
        CreateWindowEx(0, "STATIC", message, WsChildVisible | SsCenter,
            0, 0, 1, 1, parent, 0, 0, 0);

    internal static string Error(byte[] bytes)
    {
        int end = Array.IndexOf(bytes, (byte)0);
        return Encoding.UTF8.GetString(bytes, 0, end < 0 ? bytes.Length : end);
    }
}
