using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;

namespace RetroForge.Studio;

/// <summary>Spike G02: aloja directamente la ventana/contexto OpenGL de raylib.
/// El contenido diagnóstico se sustituirá por EditWorld en E01; el camino normal
/// no copia el framebuffer a RAM ni a WriteableBitmap.</summary>
public sealed class GpuViewportHost : HwndHost
{
    private nint _nativeHost;
    private bool _subscribed;

    internal bool IsNativeReady => _nativeHost != 0;
    internal long RenderCount { get; private set; }
    internal string LastError { get; private set; } = string.Empty;

    public GpuViewportHost()
    {
        Focusable = true;
        ClipToBounds = true;
    }

    protected override HandleRef BuildWindowCore(HandleRef hwndParent)
    {
        DpiScale dpi = VisualTreeHelper.GetDpi(this);
        uint width = PixelSize(ActualWidth, dpi.DpiScaleX);
        uint height = PixelSize(ActualHeight, dpi.DpiScaleY);
        byte[] error = new byte[512];
        _nativeHost = GpuHostNative.vg_gpu_host_create(hwndParent.Handle, width, height,
            error, (nuint)error.Length);
        if (_nativeHost == 0)
        {
            LastError = GpuHostNative.Error(error);
            throw new Win32Exception(LastError);
        }
        nint child = GpuHostNative.vg_gpu_host_window(_nativeHost);
        if (child == 0)
        {
            GpuHostNative.vg_gpu_host_destroy(_nativeHost);
            _nativeHost = 0;
            throw new Win32Exception("El host GPU no devolvió una ventana hija.");
        }
        CompositionTarget.Rendering += RenderFrame;
        _subscribed = true;
        return new HandleRef(this, child);
    }

    protected override void DestroyWindowCore(HandleRef hwnd)
    {
        if (_subscribed)
        {
            CompositionTarget.Rendering -= RenderFrame;
            _subscribed = false;
        }
        if (_nativeHost != 0)
        {
            GpuHostNative.vg_gpu_host_destroy(_nativeHost);
            _nativeHost = 0;
        }
    }

    protected override void OnRenderSizeChanged(SizeChangedInfo sizeInfo)
    {
        base.OnRenderSizeChanged(sizeInfo);
        ResizeNative();
    }

    protected override void OnMouseDown(System.Windows.Input.MouseButtonEventArgs e)
    {
        Focus();
        if (_nativeHost != 0)
            _ = GpuHostNative.vg_gpu_host_focus(_nativeHost);
        base.OnMouseDown(e);
    }

    internal bool RenderForTest()
    {
        if (_nativeHost == 0 || GpuHostNative.vg_gpu_host_render(_nativeHost) == 0)
            return false;
        RenderCount++;
        return true;
    }

    internal bool NativeHasFocus =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_has_focus(_nativeHost) != 0;

    internal bool CaptureForTest(string path) =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_capture(_nativeHost, path) != 0;

    private void RenderFrame(object? sender, EventArgs e)
    {
        if (IsVisible)
            _ = RenderForTest();
    }

    private void ResizeNative()
    {
        if (_nativeHost == 0)
            return;
        DpiScale dpi = VisualTreeHelper.GetDpi(this);
        _ = GpuHostNative.vg_gpu_host_resize(_nativeHost,
            PixelSize(ActualWidth, dpi.DpiScaleX), PixelSize(ActualHeight, dpi.DpiScaleY));
    }

    private static uint PixelSize(double logical, double scale)
    {
        double value = Math.Ceiling(Math.Max(1.0, logical * scale));
        return (uint)Math.Min(value, 16384.0);
    }
}
