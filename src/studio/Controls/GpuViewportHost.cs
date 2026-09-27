using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Input;
using System.Windows.Media;

namespace RetroForge.Studio;

/// <summary>Aloja la superficie OpenGL. La escena 3D y su copia de juego viven
/// en el host nativo; WPF solo entrega entrada, tamaño y comandos editoriales.</summary>
public sealed class GpuViewportHost : HwndHost
{
    private const int VkLeft = 0x01;
    private const int VkRight = 0x02;
    private const int VkSpace = 0x20;
    private const int VkLeftControl = 0xA2;
    private const int VkA = 0x41;
    private const int VkD = 0x44;
    private const int VkE = 0x45;
    private const int VkS = 0x53;
    private const int VkW = 0x57;
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    private nint _nativeHost;
    private nint _fallbackWindow;
    private bool _subscribed;
    private bool _levelOpen;
    private bool _leftDown;
    private bool _spaceDown;
    private bool _eDown;
    private GpuHostNative.NativePoint? _lastCursor;
    private double _previousFrame;

    public static readonly DependencyProperty OpenAtriumProperty = DependencyProperty.Register(
        nameof(OpenAtrium), typeof(bool), typeof(GpuViewportHost), new PropertyMetadata(false));

    private static readonly DependencyPropertyKey SelectedUuidPropertyKey =
        DependencyProperty.RegisterReadOnly(nameof(SelectedUuid), typeof(string),
            typeof(GpuViewportHost), new PropertyMetadata("Ninguno"));

    public static readonly DependencyProperty SelectedUuidProperty =
        SelectedUuidPropertyKey.DependencyProperty;

    public bool OpenAtrium
    {
        get => (bool)GetValue(OpenAtriumProperty);
        set => SetValue(OpenAtriumProperty, value);
    }

    public bool IsPlaying { get; private set; }

    public string SelectedUuid => (string)GetValue(SelectedUuidProperty);

    internal event EventHandler? SelectionChanged;

    // Rutas opcionales para abrir un documento concreto en una prueba o proyecto.
    // Deben asignarse antes de que WPF cree el HWND.
    public string? LevelPath { get; set; }
    public string? ModelPath { get; set; }
    public string? VisualSettingsPath { get; set; }
    public bool EnableAudio { get; set; } = true;

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
        if (!EnableAudio)
            _ = GpuHostNative.vg_gpu_host_set_audio_enabled(_nativeHost, 0);
        if (OpenAtrium && !OpenLevel())
        {
            _ = GpuHostNative.vg_gpu_host_destroy(_nativeHost);
            _nativeHost = 0;
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
        _previousFrame = _clock.Elapsed.TotalSeconds;
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
        _levelOpen = false;
        IsPlaying = false;
        ClearInput();
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
        if (_nativeHost == 0)
            return false;
        double now = _clock.Elapsed.TotalSeconds;
        double elapsed = Math.Clamp(now - _previousFrame, 0.0, 0.1);
        _previousFrame = now;
        int result = _levelOpen ? FrameLevel(elapsed) : GpuHostNative.vg_gpu_host_render(_nativeHost);
        if (result == 0)
            return false;
        RenderCount++;
        return true;
    }

    internal int NativeModeForTest => _nativeHost != 0 && _levelOpen
        ? GpuHostNative.vg_gpu_host_mode(_nativeHost) : -1;

    internal nint NativeHandleForTest => _nativeHost;

    internal int VisualMode => _nativeHost != 0
        ? GpuHostNative.vg_gpu_host_visual_mode(_nativeHost) : -1;

    internal string VisualSummary => !_levelOpen || _nativeHost == 0 ? string.Empty :
        $"{GpuHostNative.vg_gpu_host_visual_light_count(_nativeHost)} luces · niebla " +
        (GpuHostNative.vg_gpu_host_visual_fog_enabled(_nativeHost) != 0 ? "lineal" : "apagada");

    internal int AudioDeviceState => _nativeHost != 0
        ? GpuHostNative.vg_gpu_host_audio_device_state(_nativeHost) : -1;

    internal float AudioGain(uint bus) => _nativeHost != 0
        ? GpuHostNative.vg_gpu_host_audio_gain(_nativeHost, bus) : -1;

    internal bool TrySetAudioGain(uint bus, float gain)
    {
        if (_nativeHost == 0) return false;
        float previous = AudioGain(bus);
        if (GpuHostNative.vg_gpu_host_set_audio_gain(_nativeHost, bus, gain) == 0)
        {
            LastError = "Volumen de audio inválido.";
            return false;
        }
        byte[] error = new byte[512];
        try
        {
            string path = Path.GetFullPath(ResolveVisualSettingsPath());
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            if (GpuHostNative.vg_gpu_host_save_audio_gains(_nativeHost, path,
                    error, (nuint)error.Length) != 0)
                return true;
            LastError = GpuHostNative.Error(error);
        }
        catch (IOException exception) { LastError = exception.Message; }
        catch (UnauthorizedAccessException exception) { LastError = exception.Message; }
        _ = GpuHostNative.vg_gpu_host_set_audio_gain(_nativeHost, bus, previous);
        return false;
    }

    internal bool TrySetVisualMode(int mode)
    {
        if (_nativeHost == 0) return false;
        byte[] error = new byte[512];
        int oldMode = VisualMode;
        if (GpuHostNative.vg_gpu_host_set_visual_mode(_nativeHost, mode, error,
                (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        try
        {
            string path = ResolveVisualSettingsPath();
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            if (GpuHostNative.vg_gpu_host_save_visual_profile(_nativeHost, path,
                    error, (nuint)error.Length) != 0)
                return true;
            LastError = GpuHostNative.Error(error);
        }
        catch (IOException exception) { LastError = exception.Message; }
        catch (UnauthorizedAccessException exception) { LastError = exception.Message; }
        _ = GpuHostNative.vg_gpu_host_set_visual_mode(_nativeHost, oldMode, error,
            (nuint)error.Length);
        return false;
    }

    private string ResolveVisualSettingsPath() => VisualSettingsPath ?? Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "VESTIGIO", "visual.settings");

    internal bool TrySetPlaying(bool play)
    {
        if (!_levelOpen || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_set_mode(_nativeHost, play ? 1 : 0) == 0 ||
            GpuHostNative.vg_gpu_host_mode(_nativeHost) != (play ? 1 : 0))
            return false;
        IsPlaying = play;
        ClearInput();
        if (!play)
        {
            byte[] uuid = new byte[80];
            SetSelection(GpuHostNative.vg_gpu_host_selected_uuid(_nativeHost, uuid,
                (nuint)uuid.Length) != 0 ? GpuHostNative.Error(uuid) : "Ninguno");
        }
        return true;
    }

    internal bool SetCameraMode(int mode) => _levelOpen && !IsPlaying &&
        GpuHostNative.vg_gpu_host_set_camera_mode(_nativeHost, mode) != 0;

    internal bool FrameSelection() => _levelOpen && !IsPlaying &&
        GpuHostNative.vg_gpu_host_frame_selection(_nativeHost) != 0;

    internal bool IsDocumentDirty => _levelOpen && _nativeHost != 0 &&
        GpuHostNative.vg_gpu_host_is_dirty(_nativeHost) != 0;

    internal IReadOnlyList<string> EntityUuids()
    {
        var ids = new List<string>();
        if (!_levelOpen || _nativeHost == 0) return ids;
        nuint count = GpuHostNative.vg_gpu_host_entity_count(_nativeHost);
        for (nuint index = 0; index < count; ++index)
        {
            byte[] uuid = new byte[80];
            if (GpuHostNative.vg_gpu_host_entity_at(_nativeHost, index, uuid,
                    (nuint)uuid.Length) != 0)
                ids.Add(GpuHostNative.Error(uuid));
        }
        return ids;
    }

    internal bool TrySelect(string uuid)
    {
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_select(_nativeHost, uuid) == 0)
            return false;
        SetSelection(uuid);
        return true;
    }

    internal bool TryAddMesh()
    {
        byte[] uuid = new byte[80], error = new byte[512];
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_add_mesh(_nativeHost, uuid, (nuint)uuid.Length,
                error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        SetSelection(GpuHostNative.Error(uuid));
        return true;
    }

    internal bool TryDuplicateSelected()
    {
        byte[] uuid = new byte[80], error = new byte[512];
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_duplicate_selected(_nativeHost, uuid,
                (nuint)uuid.Length, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        SetSelection(GpuHostNative.Error(uuid));
        return true;
    }

    internal bool TryGetSelectedTransform(out float[] position,
        out float[] rotation, out float[] scale)
    {
        position = new float[3];
        rotation = new float[4];
        scale = new float[3];
        return _levelOpen && !IsPlaying && _nativeHost != 0 &&
            GpuHostNative.vg_gpu_host_selected_transform(_nativeHost, position,
                rotation, scale) != 0;
    }

    internal bool TrySetSelectedTransform(float[] position, float[] rotation,
        float[] scale)
    {
        byte[] error = new byte[512];
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_set_selected_transform(_nativeHost, position,
                rotation, scale, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        return true;
    }

    internal bool TrySaveLevel(string path)
    {
        byte[] error = new byte[512];
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_save_level(_nativeHost, path, error,
                (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        LevelPath = path;
        return true;
    }

    internal bool TryReopenLevel()
    {
        if (!_levelOpen || IsPlaying || _nativeHost == 0) return false;
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_reopen_level(_nativeHost,
                LevelPath ?? ResolveDemoAsset("atrium.level.json"),
                ModelPath ?? ResolveDemoAsset("atrium.gltf"), error,
                (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        byte[] uuid = new byte[80];
        SetSelection(GpuHostNative.vg_gpu_host_selected_uuid(_nativeHost, uuid,
            (nuint)uuid.Length) != 0 ? GpuHostNative.Error(uuid) : "Ninguno");
        return true;
    }

    internal bool TryUndo() => TryHistory(false);
    internal bool TryRedo() => TryHistory(true);

    private bool TryHistory(bool redo)
    {
        byte[] error = new byte[512];
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            (redo ? GpuHostNative.vg_gpu_host_redo(_nativeHost, error,
                (nuint)error.Length) : GpuHostNative.vg_gpu_host_undo(_nativeHost,
                error, (nuint)error.Length)) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        byte[] uuid = new byte[80];
        SetSelection(GpuHostNative.vg_gpu_host_selected_uuid(_nativeHost, uuid,
            (nuint)uuid.Length) != 0 ? GpuHostNative.Error(uuid) : "Ninguno");
        return true;
    }

    private void SetSelection(string uuid)
    {
        SetValue(SelectedUuidPropertyKey, uuid);
        SelectionChanged?.Invoke(this, EventArgs.Empty);
    }

    internal bool NativeHasFocus =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_has_focus(_nativeHost) != 0;

    internal bool FocusNativeForTest() =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_focus(_nativeHost) != 0;

    internal bool CaptureForTest(string path) =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_capture(_nativeHost, path) != 0;

    internal bool InteractForTest() =>
        _nativeHost != 0 && GpuHostNative.vg_gpu_host_interact(_nativeHost) != 0;

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
        else
        {
            _previousFrame = _clock.Elapsed.TotalSeconds;
            ClearInput();
        }
    }

    private bool OpenLevel()
    {
        try
        {
            string level = LevelPath ?? ResolveDemoAsset("atrium.level.json");
            string model = ModelPath ?? ResolveDemoAsset("atrium.gltf");
            byte[] error = new byte[512];
            if (GpuHostNative.vg_gpu_host_open_level(_nativeHost, level, model,
                    error, (nuint)error.Length) == 0)
            {
                LastError = GpuHostNative.Error(error);
                return false;
            }
            _levelOpen = true;
            if (GpuHostNative.vg_gpu_host_load_visual_profile(_nativeHost,
                    ResolveVisualSettingsPath(), error, (nuint)error.Length) == 0)
            {
                LastError = GpuHostNative.Error(error);
                return false;
            }
            return true;
        }
        catch (Exception exception) when (exception is IOException or DllNotFoundException or
            EntryPointNotFoundException or BadImageFormatException)
        {
            LastError = $"No se pudo abrir Atrium 3D: {exception.Message}";
            return false;
        }
    }

    internal static string ResolveDemoAsset(string file)
    {
        string directory = AppContext.BaseDirectory;
        for (int depth = 0; depth < 8; ++depth)
        {
            string path = Path.Combine(directory, "assets", "demo", file);
            if (File.Exists(path))
                return path;
            DirectoryInfo? parent = Directory.GetParent(directory);
            if (parent is null)
                break;
            directory = parent.FullName;
        }
        throw new FileNotFoundException($"Falta assets/demo/{file} junto a Studio o en el repositorio.");
    }

    private int FrameLevel(double elapsed)
    {
        bool left = Down(VkLeft);
        bool right = Down(VkRight);
        bool space = Down(VkSpace);
        bool interact = Down(VkE);
        bool pointerKnown = GpuHostNative.GetCursorPos(out GpuHostNative.NativePoint cursor);
        uint width = 0, height = 0;
        bool inside = pointerKnown && GpuHostNative.ScreenToClient(
            GpuHostNative.vg_gpu_host_window(_nativeHost), ref cursor) &&
            GpuHostNative.vg_gpu_host_size(_nativeHost, out width, out height) != 0 &&
            cursor.X >= 0 && cursor.Y >= 0 && cursor.X < width && cursor.Y < height;
        bool activeWindow = Window.GetWindow(this)?.IsActive == true;
        if (activeWindow && inside && ((left && !_leftDown) || (right && _lastCursor is null)))
            _ = GpuHostNative.vg_gpu_host_focus(_nativeHost);
        bool focused = activeWindow && NativeHasFocus;
        if (!focused)
            _lastCursor = null;

        float lookX = 0, lookY = 0;
        if (focused && right && pointerKnown)
        {
            if (_lastCursor is GpuHostNative.NativePoint previous)
            {
                lookX = Math.Clamp(cursor.X - previous.X, -150, 150);
                lookY = Math.Clamp(cursor.Y - previous.Y, -150, 150);
            }
            _lastCursor = cursor;
        }
        else
            _lastCursor = null;

        if (focused && !IsPlaying && left && !_leftDown && inside)
        {
            byte[] uuid = new byte[80];
            float u = cursor.X / (float)width;
            float v = cursor.Y / (float)height;
            SetSelection(GpuHostNative.vg_gpu_host_pick(_nativeHost, u, v, uuid,
                (nuint)uuid.Length) != 0 ? GpuHostNative.Error(uuid) : "Ninguno");
        }
        _leftDown = left;
        if (focused && IsPlaying && interact && !_eDown)
            _ = GpuHostNative.vg_gpu_host_interact(_nativeHost);
        _eDown = interact;
        int jump = 0;
        if (focused)
            jump = IsPlaying ? (space && !_spaceDown ? 1 : 0) :
                (space ? 1 : 0) - (Down(VkLeftControl) ? 1 : 0);
        _spaceDown = space;
        float moveX = focused ? (Down(VkD) ? 1 : 0) - (Down(VkA) ? 1 : 0) : 0;
        float moveY = focused ? (Down(VkW) ? 1 : 0) - (Down(VkS) ? 1 : 0) : 0;
        return GpuHostNative.vg_gpu_host_frame(_nativeHost, elapsed, moveX, moveY,
            lookX, lookY, jump, focused ? 1 : 0);
    }

    private static bool Down(int key) => (GpuHostNative.GetAsyncKeyState(key) & 0x8000) != 0;

    private void ClearInput()
    {
        _lastCursor = null;
        _leftDown = Down(VkLeft);
        _spaceDown = Down(VkSpace);
        _eDown = Down(VkE);
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
