using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Input;
using System.Windows.Media;

namespace Vestigio.Studio;

/// <summary>Aloja la superficie OpenGL. La escena 3D y su copia de juego viven
/// en el host nativo; WPF solo entrega entrada, tamaño y comandos editoriales.</summary>
public sealed class GpuViewportHost : HwndHost
{
    private const int VkLeft = 0x01;
    private const int VkRight = 0x02;
    private const int VkSpace = 0x20;
    private const int VkLeftControl = 0xA2;
    private const int VkRightControl = 0xA3;
    private const int VkLeftShift = 0xA0;
    private const int VkRightShift = 0xA1;
    private const int VkEscape = 0x1B;
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
    private bool _escapeDown;
    private bool _gestureActive;
    private GpuHostNative.NativePoint _gestureStart;
    private float _gestureDirectionX;
    private float _gestureDirectionY;
    private int _gestureOp;
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
    internal event EventHandler? GestureFinished;
    internal event Action<Key, ModifierKeys>? ShortcutPressed;
    private readonly HashSet<Key> _shortcutKeysDown = [];
    private static readonly Key[] ShortcutKeys = [Key.F5, Key.Escape, Key.F, Key.Delete, Key.N, Key.O, Key.S, Key.Z, Key.Y, Key.D];
    internal int GizmoTool { get; set; }
    internal int GizmoAxis { get; set; }
    internal int GizmoSpace { get; set; }
    internal int GizmoPivot { get; set; }
    internal float GizmoSnap { get; set; }
    internal bool IsGestureActive => _gestureActive;
    internal bool LastGestureCommitted { get; private set; }

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
        if (_gestureActive && !TryEndGesture(false)) return false;
        if (play && _nativeHost != 0)
            _ = GpuHostNative.vg_gpu_host_cancel_room_preview(_nativeHost);
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

    internal string? RoomPieceLabel(string uuid)
    {
        if (!_levelOpen || _nativeHost == 0) return null;
        byte[] label = new byte[128];
        return GpuHostNative.vg_gpu_host_entity_label(_nativeHost, uuid, label,
            (nuint)label.Length) != 0 ? GpuHostNative.Error(label) : null;
    }

    internal bool TrySelect(string uuid)
    {
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_select_add(_nativeHost, uuid, 0, 0) == 0)
            return false;
        SetSelection(uuid);
        return true;
    }

    internal IReadOnlyList<string> SelectedUuids()
    {
        var ids = new List<string>();
        if (!_levelOpen || _nativeHost == 0) return ids;
        nuint count = GpuHostNative.vg_gpu_host_selection_count(_nativeHost);
        for (nuint index = 0; index < count; ++index)
        {
            byte[] uuid = new byte[80];
            if (GpuHostNative.vg_gpu_host_selection_at(_nativeHost, index, uuid,
                    (nuint)uuid.Length) != 0)
                ids.Add(GpuHostNative.Error(uuid));
        }
        return ids;
    }

    internal bool TrySelectMany(IReadOnlyList<string> uuids)
    {
        if (!_levelOpen || IsPlaying || _nativeHost == 0) return false;
        if (uuids.Count == 0)
        {
            if (GpuHostNative.vg_gpu_host_select_add(_nativeHost, "", 0, 0) == 0)
                return false;
        }
        else
        {
            for (int index = 0; index < uuids.Count; ++index)
                if (GpuHostNative.vg_gpu_host_select_add(_nativeHost, uuids[index],
                        index == 0 ? 0 : 1, 0) == 0)
                    return false;
        }
        SyncSelection();
        return true;
    }

    internal bool TrySetGizmoOptions(int tool, int axis, int space, int pivot,
        float snap)
    {
        if (!_levelOpen || _nativeHost == 0 || _gestureActive || IsPlaying ||
            tool is < 0 or > 3 || axis is < 0 or > 2 || space is < 0 or > 1 ||
            pivot is < 0 or > 2 || !float.IsFinite(snap) || snap < 0 ||
            GpuHostNative.vg_gpu_host_gizmo_config(_nativeHost, tool - 1,
                space, pivot) == 0)
            return false;
        GizmoTool = tool; GizmoAxis = axis; GizmoSpace = space;
        GizmoPivot = pivot; GizmoSnap = snap;
        return true;
    }

    internal bool TryBeginGesture(int axis)
    {
        if (!_levelOpen || _nativeHost == 0 || IsPlaying || _gestureActive ||
            GizmoTool is < 1 or > 3 || axis is < 0 or > 2)
            return false;
        byte[] error = new byte[512];
        float snap = GizmoTool == 2 ? GizmoSnap * MathF.PI / 180f : GizmoSnap;
        if (GpuHostNative.vg_gpu_host_begin_gesture(_nativeHost, GizmoTool - 1,
                GizmoSpace, GizmoPivot, axis, snap, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        _gestureActive = true;
        _gestureOp = GizmoTool - 1;
        return true;
    }

    private void SetGestureDragDirection(float u, float v, int axis)
    {
        if (GpuHostNative.vg_gpu_host_gizmo_drag_direction(_nativeHost,
                u, v, axis, out float dx, out float dy) != 0 &&
            float.IsFinite(dx) && float.IsFinite(dy) &&
            dx * dx + dy * dy > 0.25f)
        {
            float length = MathF.Sqrt(dx * dx + dy * dy);
            _gestureDirectionX = dx / length;
            _gestureDirectionY = dy / length;
        }
        else
        {
            // Eje casi alineado con la mirada: conserva control del gesto.
            _gestureDirectionX = axis == 2 ? 0f : 1f;
            _gestureDirectionY = axis == 2 ? -1f : 0f;
        }
    }

    internal static float ProjectDragPixels(int startX, int startY, int cursorX,
        int cursorY, float directionX, float directionY) =>
        (cursorX - startX) * directionX +
        (cursorY - startY) * directionY;

    internal bool TryBeginPointerGesture(float u, float v, int cursorX, int cursorY)
    {
        if (_nativeHost == 0 || GizmoTool <= 0) return false;
        int hit = GpuHostNative.vg_gpu_host_gizmo_hit(_nativeHost, u, v);
        if (hit is < 1 or > 3 || !TryBeginGesture(hit - 1)) return false;
        GizmoAxis = hit - 1;
        _gestureStart = new GpuHostNative.NativePoint { X = cursorX, Y = cursorY };
        SetGestureDragDirection(u, v, hit - 1);
        return true;
    }

    internal bool TryUpdatePointerGesture(int cursorX, int cursorY)
    {
        if (!_gestureActive) return false;
        float pixels = ProjectDragPixels(_gestureStart.X,
            _gestureStart.Y, cursorX, cursorY,
            _gestureDirectionX, _gestureDirectionY);
        float amount = _gestureOp switch
        {
            0 => pixels * 0.02f,
            1 => pixels * (MathF.PI / 360f),
            _ => pixels * 0.01f
        };
        return TryUpdateGesture(amount);
    }

    internal bool TryUpdateGesture(float amount)
    {
        if (!_gestureActive || _nativeHost == 0) return false;
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_update_gesture(_nativeHost, amount,
                error, (nuint)error.Length) != 0)
            return true;
        LastError = GpuHostNative.Error(error);
        return false;
    }

    internal bool TryEndGesture(bool commit)
    {
        if (!_gestureActive || _nativeHost == 0) return false;
        byte[] error = new byte[512];
        bool ok = GpuHostNative.vg_gpu_host_end_gesture(_nativeHost,
            commit ? 1 : 0, error, (nuint)error.Length) != 0;
        if (!ok)
        {
            LastError = GpuHostNative.Error(error);
            // Un commit fallido no debe dejar un preview activo ni una sesión
            // de arrastre bloqueando los siguientes comandos editoriales.
            _ = GpuHostNative.vg_gpu_host_end_gesture(_nativeHost, 0,
                new byte[512], 512);
        }
        LastGestureCommitted = ok && commit;
        _gestureActive = false;
        SyncSelection();
        GestureFinished?.Invoke(this, EventArgs.Empty);
        return ok;
    }

    internal bool TryDuplicateSelection() => TrySelectionCommand(
        GpuHostNative.vg_gpu_host_duplicate_selection);

    internal bool TryDeleteSelection() => TrySelectionCommand(
        GpuHostNative.vg_gpu_host_delete_selection);

    private delegate int SelectionCommand(nint host, byte[] error,
        nuint errorCapacity);

    private bool TrySelectionCommand(SelectionCommand command)
    {
        if (!_levelOpen || _nativeHost == 0 || IsPlaying || _gestureActive)
            return false;
        byte[] error = new byte[512];
        if (command(_nativeHost, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        SyncSelection();
        return true;
    }

    internal bool TryReparentSelection(string parentUuid)
    {
        if (!_levelOpen || _nativeHost == 0 || IsPlaying || _gestureActive)
            return false;
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_reparent_selection(_nativeHost,
                parentUuid, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        SyncSelection();
        return true;
    }

    private void SyncSelection()
    {
        byte[] uuid = new byte[80];
        SetSelection(GpuHostNative.vg_gpu_host_selected_uuid(_nativeHost, uuid,
            (nuint)uuid.Length) != 0 ? GpuHostNative.Error(uuid) : "Ninguno");
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

    internal bool TryAddRoom()
    {
        byte[] uuid = new byte[80], error = new byte[512];
        if (!_levelOpen || IsPlaying || _nativeHost == 0 ||
            GpuHostNative.vg_gpu_host_add_room(_nativeHost, uuid, (nuint)uuid.Length,
                error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        SetSelection(GpuHostNative.Error(uuid));
        return true;
    }

    internal bool TryPreviewRoomRecipe(string recipeJson)
    {
        if (!_levelOpen || IsPlaying || _nativeHost == 0 || _gestureActive)
            return false;
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_preview_room_recipe(_nativeHost, recipeJson,
                error, (nuint)error.Length) != 0)
            return true;
        LastError = GpuHostNative.Error(error);
        return false;
    }

    internal bool TryCancelRoomPreview() => _nativeHost != 0 &&
        GpuHostNative.vg_gpu_host_cancel_room_preview(_nativeHost) != 0;

    internal bool TryCreateRoomRecipe(string recipeJson, out string uuid)
    {
        uuid = "";
        if (!_levelOpen || IsPlaying || _nativeHost == 0 || _gestureActive)
            return false;
        byte[] nativeId = new byte[80], error = new byte[512];
        if (GpuHostNative.vg_gpu_host_create_room_recipe(_nativeHost, recipeJson,
                nativeId, (nuint)nativeId.Length, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        uuid = GpuHostNative.Error(nativeId);
        SetSelection(uuid);
        return true;
    }

    internal bool TryUpdateRoomRecipe(string uuid, string recipeJson)
    {
        if (!_levelOpen || IsPlaying || _nativeHost == 0 || _gestureActive)
            return false;
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_update_room_recipe(_nativeHost, uuid,
                recipeJson, error, (nuint)error.Length) != 0)
            return true;
        LastError = GpuHostNative.Error(error);
        return false;
    }

    internal bool TryGetRoomRecipe(string uuid, out string recipeJson)
    {
        recipeJson = "";
        if (!_levelOpen || _nativeHost == 0 || uuid == "Ninguno") return false;
        byte[] json = new byte[16384];
        if (GpuHostNative.vg_gpu_host_room_recipe_json(_nativeHost, uuid,
                json, (nuint)json.Length) == 0) return false;
        recipeJson = GpuHostNative.Error(json);
        return true;
    }

    internal bool TrySetRoomEditorView(bool grid, bool ghost, float floorZ) =>
        _levelOpen && !IsPlaying && _nativeHost != 0 && float.IsFinite(floorZ) &&
        GpuHostNative.vg_gpu_host_set_room_editor_view(_nativeHost,
            grid ? 1 : 0, ghost ? 1 : 0, floorZ) != 0;

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

    internal IReadOnlyList<GpuAssetInfo> AssetLibrary()
    {
        if (!_levelOpen || _nativeHost == 0) return [];
        byte[] json = new byte[131072];
        if (GpuHostNative.vg_gpu_host_assets_json(_nativeHost, json,
                (nuint)json.Length) == 0)
        {
            LastError = "No se pudo leer la biblioteca de assets.";
            return [];
        }
        try { return GpuAssetInfo.Parse(GpuHostNative.Error(json)); }
        catch (System.Text.Json.JsonException exception)
        {
            LastError = $"Biblioteca de assets inválida: {exception.Message}";
            return [];
        }
    }

    internal IReadOnlyList<GpuInspectorField> SelectionFields()
    {
        if (!_levelOpen || _nativeHost == 0 || IsPlaying) return [];
        byte[] json = new byte[65536];
        if (GpuHostNative.vg_gpu_host_selection_fields_json(_nativeHost, json,
                (nuint)json.Length) == 0)
        {
            LastError = "No se pudo leer el esquema del inspector.";
            return [];
        }
        try { return GpuInspectorField.Parse(GpuHostNative.Error(json)); }
        catch (System.Text.Json.JsonException exception)
        {
            LastError = $"Esquema de inspector inválido: {exception.Message}";
            return [];
        }
    }

    internal bool TrySetSelectionFields(IReadOnlyDictionary<string, object?> changes)
    {
        if (!_levelOpen || _nativeHost == 0 || IsPlaying || _gestureActive ||
            changes.Count == 0) return false;
        string json = System.Text.Json.JsonSerializer.Serialize(new
        {
            updates = changes.Select(change => new { path = change.Key,
                value = change.Value }).ToArray()
        });
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_set_selection_fields_json(_nativeHost,
                json, error, (nuint)error.Length) != 0)
            return true;
        LastError = GpuHostNative.Error(error);
        return false;
    }

    internal bool TryImportAsset(string path, out string id)
    {
        id = "";
        if (!_levelOpen || _nativeHost == 0 || IsPlaying || _gestureActive)
            return false;
        byte[] nativeId = new byte[80], error = new byte[512];
        if (GpuHostNative.vg_gpu_host_import_asset(_nativeHost, path, nativeId,
                (nuint)nativeId.Length, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        id = GpuHostNative.Error(nativeId);
        return true;
    }

    internal bool TryPlaceAsset(string id, out string entityId)
    {
        entityId = "";
        if (!_levelOpen || _nativeHost == 0 || IsPlaying || _gestureActive)
            return false;
        byte[] uuid = new byte[80], error = new byte[512];
        if (GpuHostNative.vg_gpu_host_place_asset(_nativeHost, id, 0,
                uuid, (nuint)uuid.Length, error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        entityId = GpuHostNative.Error(uuid);
        SetSelection(entityId);
        return true;
    }

    internal bool TryPreviewAsset(string id)
    {
        if (!_levelOpen || _nativeHost == 0 || IsPlaying) return false;
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_preview_asset(_nativeHost, id,
                error, (nuint)error.Length) != 0)
            return true;
        LastError = GpuHostNative.Error(error);
        return false;
    }

    internal bool TryReimportAsset(string id, string path) => TryAssetCommand(id,
        (native, asset, error, length) =>
            GpuHostNative.vg_gpu_host_reimport_asset(native, asset, path, error, length));

    internal bool TryGetEntityEditor(string id, out string json)
    {
        json = "";
        if (!_levelOpen || _nativeHost == 0) return false;
        byte[] buffer = new byte[2048];
        if (GpuHostNative.vg_gpu_host_entity_editor_json(_nativeHost, id,
                buffer, (nuint)buffer.Length) == 0)
            return false;
        json = GpuHostNative.Error(buffer);
        return true;
    }

    internal bool TryRenameAsset(string id, string name) => TryAssetCommand(id,
        (native, asset, error, length) =>
            GpuHostNative.vg_gpu_host_rename_asset(native, asset, name, error, length));

    private bool TryAssetCommand(string id,
        Func<nint, string, byte[], nuint, int> command)
    {
        if (!_levelOpen || _nativeHost == 0 || IsPlaying || _gestureActive)
            return false;
        byte[] error = new byte[512];
        if (command(_nativeHost, id, error, (nuint)error.Length) != 0)
            return true;
        LastError = GpuHostNative.Error(error);
        return false;
    }

    internal bool TrySaveLevel(string path)
    {
        if (_nativeHost != 0) _ = TryCancelRoomPreview();
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
        _ = TryCancelRoomPreview();
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

    internal bool TryLoadLevel(string path)
    {
        if (!_levelOpen || IsPlaying || _nativeHost == 0) return false;
        _ = TryCancelRoomPreview();
        if (IsGestureActive) _ = TryEndGesture(false);
        byte[] error = new byte[512];
        if (GpuHostNative.vg_gpu_host_load_level(_nativeHost, path,
                ModelPath ?? ResolveDemoAsset("atrium.gltf"), error, (nuint)error.Length) == 0)
        {
            LastError = GpuHostNative.Error(error);
            return false;
        }
        LevelPath = path;
        SetSelection("Ninguno");
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
            string source = Path.Combine(directory, "engines", "vestigio", "assets",
                "demo", file);
            if (File.Exists(source))
                return source;
            DirectoryInfo? parent = Directory.GetParent(directory);
            if (parent is null)
                break;
            directory = parent.FullName;
        }
        throw new FileNotFoundException($"Falta engines/vestigio/assets/demo/{file} junto a Studio o en el repositorio.");
    }

    private int FrameLevel(double elapsed)
    {
        bool left = Down(VkLeft);
        bool right = Down(VkRight);
        bool space = Down(VkSpace);
        bool interact = Down(VkE);
        bool escape = Down(VkEscape);
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
        // HwndHost owns native focus: WPF PreviewKeyDown cannot see these keys.
        ModifierKeys modifiers = (Down(VkLeftControl) || Down(VkRightControl) ? ModifierKeys.Control : ModifierKeys.None) |
            (Down(VkLeftShift) || Down(VkRightShift) ? ModifierKeys.Shift : ModifierKeys.None);
        foreach (Key key in ShortcutKeys)
        {
            bool down = Down(KeyInterop.VirtualKeyFromKey(key));
            if (down && _shortcutKeysDown.Add(key) && focused && !_gestureActive &&
                (key is Key.F5 or Key.Escape or Key.Delete || key == Key.F && !right || modifiers.HasFlag(ModifierKeys.Control)))
                ShortcutPressed?.Invoke(key, modifiers);
            if (!down) _shortcutKeysDown.Remove(key);
        }
        if (!focused)
        {
            _lastCursor = null;
            if (_gestureActive) TryEndGesture(false);
        }

        if (_gestureActive)
        {
            if (escape && !_escapeDown)
                TryEndGesture(false);
            else if (!left)
                TryEndGesture(true);
            else if (pointerKnown)
            {
                if (!TryUpdatePointerGesture(cursor.X, cursor.Y))
                    TryEndGesture(false);
            }
        }
        _escapeDown = escape;

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

        if (focused && !IsPlaying && !_gestureActive && left && !_leftDown && inside)
        {
            float u = cursor.X / (float)width;
            float v = cursor.Y / (float)height;
            if (!TryBeginPointerGesture(u, v, cursor.X, cursor.Y))
            {
                byte[] uuid = new byte[80];
                string picked = GpuHostNative.vg_gpu_host_peek(_nativeHost, u, v,
                    uuid, (nuint)uuid.Length) != 0
                    ? GpuHostNative.Error(uuid) : "";
                bool additive = Down(VkLeftControl) || Down(VkRightControl) ||
                    Down(VkLeftShift) || Down(VkRightShift);
                bool toggle = Down(VkLeftControl) || Down(VkRightControl);
                if (!string.IsNullOrEmpty(picked))
                {
                    if (GpuHostNative.vg_gpu_host_select_add(_nativeHost, picked,
                            additive ? 1 : 0, toggle ? 1 : 0) != 0)
                        SyncSelection();
                }
                else if (!additive && GpuHostNative.vg_gpu_host_select_add(
                             _nativeHost, "", 0, 0) != 0)
                    SyncSelection();
            }
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
        if (_gestureActive) TryEndGesture(false);
        _lastCursor = null;
        _leftDown = Down(VkLeft);
        _spaceDown = Down(VkSpace);
        _eDown = Down(VkE);
        _escapeDown = Down(VkEscape);
        _shortcutKeysDown.Clear();
        foreach (Key key in ShortcutKeys)
            if (Down(KeyInterop.VirtualKeyFromKey(key))) _shortcutKeysDown.Add(key);
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
