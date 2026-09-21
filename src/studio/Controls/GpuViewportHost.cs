using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Input;
using System.Windows.Media;

namespace RetroForge.Studio;

/// <summary>Spike G02: aloja directamente la ventana/contexto OpenGL de raylib.
/// El contenido diagnóstico se sustituirá por EditWorld en E01; el camino normal
/// no copia el framebuffer a RAM ni a WriteableBitmap.</summary>
public sealed class GpuViewportHost : HwndHost
{
    private nint _nativeHost;
    private nint _fallbackWindow;
    private bool _subscribed;

    internal bool IsNativeReady => _nativeHost != 0;
    internal bool IsFallback => _fallbackWindow != 0;
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
        try
        {
            _nativeHost = GpuHostNative.vg_gpu_host_create(hwndParent.Handle, width, height,
                error, (nuint)error.Length);
        }
        catch (Exception exception) when (exception is DllNotFoundException or
            EntryPointNotFoundException or BadImageFormatException)
        {
            LastError = $"GPU no disponible: {exception.Message}";
            return BuildFallback(hwndParent);
        }
        if (_nativeHost == 0)
        {
            LastError = GpuHostNative.Error(error);
            return BuildFallback(hwndParent);
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
        if (_fallbackWindow != 0)
        {
            _ = GpuHostNative.DestroyWindow(_fallbackWindow);
            _fallbackWindow = 0;
        }
    }

    protected override void OnRenderSizeChanged(SizeChangedInfo sizeInfo)
    {
        base.OnRenderSizeChanged(sizeInfo);
        ResizeNative();
    }

    protected override void OnDpiChanged(DpiScale oldDpi, DpiScale newDpi)
    {
        base.OnDpiChanged(oldDpi, newDpi);
        ResizeNative();
    }

    protected override void OnMouseDown(System.Windows.Input.MouseButtonEventArgs e)
    {
        Focus();
        if (_nativeHost != 0)
            _ = GpuHostNative.vg_gpu_host_focus(_nativeHost);
        base.OnMouseDown(e);
    }

    protected override bool TabIntoCore(TraversalRequest request) =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_focus(_nativeHost) != 0;

    protected override bool HasFocusWithinCore() => NativeHasFocus;

    internal bool RenderForTest()
    {
        if (_nativeHost == 0 || GpuHostNative.vg_gpu_host_render(_nativeHost) == 0)
            return false;
        RenderCount++;
        return true;
    }

    internal bool NativeHasFocus =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_has_focus(_nativeHost) != 0;

    internal bool FocusNativeForTest() =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_focus(_nativeHost) != 0;

    internal bool CaptureForTest(string path) =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_capture(_nativeHost, path) != 0;

    private void RenderFrame(object? sender, EventArgs e)
    {
        Window? window = Window.GetWindow(this);
        if (IsVisible && PresentationSource.FromVisual(this) is not null &&
            window?.WindowState != WindowState.Minimized && ActualWidth > 0 && ActualHeight > 0)
        {
            if (!RenderForTest())
            {
                LastError = "La superficie GPU dej\u00f3 de responder.";
                CompositionTarget.Rendering -= RenderFrame;
                _subscribed = false;
            }
        }
    }

    private HandleRef BuildFallback(HandleRef hwndParent)
    {
        if (string.IsNullOrWhiteSpace(LastError))
            LastError = "No se pudo inicializar la superficie GPU.";
        _fallbackWindow = GpuHostNative.CreateFallbackWindow(hwndParent.Handle, LastError);
        if (_fallbackWindow == 0)
            throw new Win32Exception(Marshal.GetLastWin32Error(), LastError);
        return new HandleRef(this, _fallbackWindow);
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
