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
