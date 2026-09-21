using System.Runtime.InteropServices;
using System.Text;

namespace RetroForge.Studio;

internal static class GpuHostNative
{
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
    internal static extern int vg_gpu_host_capture(nint host,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_focus(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int vg_gpu_host_has_focus(nint host);

    [DllImport("vestigio_gpu_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void vg_gpu_host_destroy(nint host);

    internal static string Error(byte[] bytes)
    {
        int end = Array.IndexOf(bytes, (byte)0);
        return Encoding.UTF8.GetString(bytes, 0, end < 0 ? bytes.Length : end);
    }
}
