using System.Diagnostics;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace RetroForge.Studio;

/// <summary>Presentación WPF de la sesión C compartida. Un único callback
/// consume entrada por fotograma; la sesión realiza los ticks fijos y descarta
/// el tiempo cuando la superficie pierde foco. No se guarda el documento.</summary>
public sealed class GameViewport : FrameworkElement
{

    private const uint MoveForward = 1u << 15;
    private const uint MoveBackward = 1u << 16;
    private const uint MoveLeft = 1u << 17;
    private const uint MoveRight = 1u << 18;
    private const uint MousePrimaryCode = 0x00010001;
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    private readonly Dictionary<uint, uint> _bindings = new();
    private WriteableBitmap? _bitmap;
    private byte[] _pixels = [];
    private int _pixelWidth;
    private int _pixelHeight;
    private nint _configuredSession;
    private double _previous;
    private uint _pressed;
    private uint _held;
    private uint _released;
    private Vector _look;
    private Point? _lastMouse;
    private bool _hasFrame;

    public GameViewport()
    {
        Focusable = true;
        ClipToBounds = true;
        RenderOptions.SetBitmapScalingMode(this, BitmapScalingMode.NearestNeighbor);
        Loaded += (_, _) => { _previous = _clock.Elapsed.TotalSeconds; CompositionTarget.Rendering += Frame; };
        Unloaded += (_, _) => { CompositionTarget.Rendering -= Frame; ClearInput(); };
        LostKeyboardFocus += (_, _) => { ClearInput(); _lastMouse = null; };
    }

    private void Frame(object? sender, EventArgs args)
    {
        double now = _clock.Elapsed.TotalSeconds, elapsed = now - _previous;
        _previous = now;
        if (DataContext is not StudioViewModel vm || vm.Session == 0 || !IsVisible)
        {
            if (_hasFrame) { _hasFrame = false; InvalidateVisual(); }
            return;
        }
        if (!EnsureSession(vm.Session)) return;
        bool active = IsKeyboardFocusWithin && Window.GetWindow(this)?.IsActive == true &&
                      !vm.SimulationPaused;
        uint sampled = active ? HeldActions() : 0;
        float x = active ? ((sampled & MoveRight) != 0 ? 1 : 0) - ((sampled & MoveLeft) != 0 ? 1 : 0) : 0;
        float y = active ? ((sampled & MoveForward) != 0 ? 1 : 0) - ((sampled & MoveBackward) != 0 ? 1 : 0) : 0;
        uint held = sampled & (MoveForward - 1u);
        uint released = active ? _released | (_held & ~held) : 0;
        SessionNative.re_session_frame(vm.Session, elapsed, x, y, (float)_look.X, (float)_look.Y,
            active ? _pressed : 0, held, released, active ? 1 : 0, vm.ConsumeStep() ? 1 : 0);
        _held = held;
        _pressed = 0;
        _released = 0;
        _look = default;
        if (SessionNative.re_session_copy_pixels(vm.Session, _pixels, (uint)_pixels.Length) != 0)
        {
            // El motor entrega RGBA; WPF exige BGRA. La conversión no toca el
            // framebuffer nativo y reutiliza este array durante toda la vista.
            for (int i = 0; i < _pixels.Length; i += 4)
                (_pixels[i], _pixels[i + 2]) = (_pixels[i + 2], _pixels[i]);
            _bitmap!.WritePixels(new Int32Rect(0, 0, _pixelWidth, _pixelHeight), _pixels,
                _pixelWidth * 4, 0);
            _hasFrame = true;
            InvalidateVisual();
        }
        if ((SessionNative.re_session_flags(vm.Session) & 1) != 0)
            vm.StopCommand.Execute(null);
    }

    protected override void OnRender(DrawingContext drawing)
    {
        drawing.DrawRectangle(Brushes.Black, null, new Rect(RenderSize));
        if (!_hasFrame || _bitmap is null) return;
        double scale = Math.Min(ActualWidth / _pixelWidth, ActualHeight / _pixelHeight);
        if (scale >= 1) scale = Math.Floor(scale);
        double width = _pixelWidth * scale, height = _pixelHeight * scale;
        drawing.DrawImage(_bitmap, new Rect((ActualWidth - width) / 2, (ActualHeight - height) / 2, width, height));
    }

    protected override void OnMouseDown(MouseButtonEventArgs e)
    {
        bool focused = IsKeyboardFocusWithin;
        Focus();
        if (e.ChangedButton == MouseButton.Right) { _lastMouse = e.GetPosition(this); CaptureMouse(); }
        if (e.ChangedButton == MouseButton.Left && focused) _pressed |= ActionForCode(MousePrimaryCode);
        e.Handled = true;
    }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        Point position = e.GetPosition(this);
        if (_lastMouse is Point last && e.RightButton == MouseButtonState.Pressed)
        { _look += position - last; _lastMouse = position; }
    }
    protected override void OnMouseUp(MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Right) { _lastMouse = null; ReleaseMouseCapture(); }
        if (e.ChangedButton == MouseButton.Left) _released |= ActionForCode(MousePrimaryCode);
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.IsRepeat) return;
        uint action = ActionForKey(e.Key);
        _pressed |= action;
        if (action != 0) e.Handled = true;
    }

    protected override void OnKeyUp(KeyEventArgs e)
    {
        uint action = ActionForKey(e.Key);
        _released |= action & (MoveForward - 1u);
        if (action != 0) e.Handled = true;
    }

    private uint HeldActions()
    {
        uint held = Mouse.LeftButton == MouseButtonState.Pressed ? ActionForCode(MousePrimaryCode) : 0;
        foreach ((uint code, uint action) in _bindings)
            if (code != MousePrimaryCode && KeyForCode(code) is Key key && Keyboard.IsKeyDown(key))
                held |= action;
        return held;
    }

    private uint ActionForKey(Key key) => CodeForKey(key) is uint code ? ActionForCode(code) : 0;
    private uint ActionForCode(uint code) => _bindings.TryGetValue(code, out uint action) ? action : 0;

    private static uint? CodeForKey(Key key) => key switch
    {
        Key.A => 0x04, Key.D => 0x07, Key.E => 0x08, Key.S => 0x16, Key.W => 0x1A,
        Key.Enter => 0x28, Key.Escape => 0x29, Key.Space => 0x2C,
        Key.F1 => 0x3A, Key.F2 => 0x3B, Key.F3 => 0x3C, Key.F4 => 0x3D,
        Key.F5 => 0x3E, Key.F9 => 0x42, Key.Right => 0x4F, Key.Left => 0x50,
        Key.Down => 0x51, Key.Up => 0x52, _ => null
    };

    private static Key? KeyForCode(uint code) => code switch
    {
        0x04 => Key.A, 0x07 => Key.D, 0x08 => Key.E, 0x16 => Key.S, 0x1A => Key.W,
        0x28 => Key.Enter, 0x29 => Key.Escape, 0x2C => Key.Space,
        0x3A => Key.F1, 0x3B => Key.F2, 0x3C => Key.F3, 0x3D => Key.F4,
        0x3E => Key.F5, 0x42 => Key.F9, 0x4F => Key.Right, 0x50 => Key.Left,
        0x51 => Key.Down, 0x52 => Key.Up, _ => null
    };

    private bool EnsureSession(nint session)
    {
        if (_configuredSession == session) return true;
        if (SessionNative.re_session_dimensions(session, out uint width, out uint height) == 0 ||
            width == 0 || height == 0 || width > 4096 || height > 4096) return false;
        _pixelWidth = (int)width;
        _pixelHeight = (int)height;
        _pixels = new byte[checked(_pixelWidth * _pixelHeight * 4)];
        _bitmap = new WriteableBitmap(_pixelWidth, _pixelHeight, 96, 96, PixelFormats.Bgra32, null);
        _bindings.Clear();
        uint count = SessionNative.re_session_binding_count(session);
        for (uint index = 0; index < count; ++index)
            if (SessionNative.re_session_binding(session, index, out ulong action, out uint code) != 0 &&
                action <= uint.MaxValue)
                _bindings[code] = (uint)action;
        _configuredSession = session;
        ClearInput();
        return true;
    }

    private void ClearInput()
    {
        _pressed = 0;
        _held = 0;
        _released = 0;
        _look = default;
    }
}
