using System.Diagnostics;
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
    private readonly WriteableBitmap _bitmap = new(480, 270, 96, 96, PixelFormats.Bgra32, null);
    private readonly byte[] _pixels = new byte[480 * 270 * 4];
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    private double _previous;
    private uint _pressed;
    private Vector _look;
    private Point? _lastMouse;
    private bool _hasFrame;

    public GameViewport()
    {
        Focusable = true;
        ClipToBounds = true;
        RenderOptions.SetBitmapScalingMode(this, BitmapScalingMode.NearestNeighbor);
        Loaded += (_, _) => { _previous = _clock.Elapsed.TotalSeconds; CompositionTarget.Rendering += Frame; };
        Unloaded += (_, _) => { CompositionTarget.Rendering -= Frame; _pressed = 0; _look = default; };
        LostKeyboardFocus += (_, _) => { _pressed = 0; _look = default; _lastMouse = null; };
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
        bool active = IsKeyboardFocusWithin && Window.GetWindow(this)?.IsActive == true &&
                      !vm.SimulationPaused;
        float x = active ? (Keyboard.IsKeyDown(Key.D) ? 1 : 0) - (Keyboard.IsKeyDown(Key.A) ? 1 : 0) : 0;
        float y = active ? (Keyboard.IsKeyDown(Key.W) ? 1 : 0) - (Keyboard.IsKeyDown(Key.S) ? 1 : 0) : 0;
        SessionNative.re_session_frame(vm.Session, elapsed, x, y, (float)_look.X, (float)_look.Y,
            active ? _pressed : 0, 0, active ? 1 : 0, vm.ConsumeStep() ? 1 : 0);
        _pressed = 0;
        _look = default;
        if (SessionNative.re_session_copy_pixels(vm.Session, _pixels, (uint)_pixels.Length) != 0)
        {
            // El motor entrega RGBA; WPF exige BGRA. La conversión no toca el
            // framebuffer nativo y reutiliza este array durante toda la vista.
            for (int i = 0; i < _pixels.Length; i += 4)
                (_pixels[i], _pixels[i + 2]) = (_pixels[i + 2], _pixels[i]);
            _bitmap.WritePixels(new Int32Rect(0, 0, 480, 270), _pixels, 480 * 4, 0);
            _hasFrame = true;
            InvalidateVisual();
        }
        if ((SessionNative.re_session_flags(vm.Session) & 1) != 0)
            vm.StopCommand.Execute(null);
    }

    protected override void OnRender(DrawingContext drawing)
    {
        drawing.DrawRectangle(Brushes.Black, null, new Rect(RenderSize));
        if (!_hasFrame) return;
        double scale = Math.Min(ActualWidth / 480, ActualHeight / 270);
        if (scale >= 1) scale = Math.Floor(scale);
        double width = 480 * scale, height = 270 * scale;
        drawing.DrawImage(_bitmap, new Rect((ActualWidth - width) / 2, (ActualHeight - height) / 2, width, height));
    }

    protected override void OnMouseDown(MouseButtonEventArgs e)
    {
        bool focused = IsKeyboardFocusWithin;
        Focus();
        if (e.ChangedButton == MouseButton.Right) { _lastMouse = e.GetPosition(this); CaptureMouse(); }
        if (e.ChangedButton == MouseButton.Left && focused) _pressed |= 2;
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
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.IsRepeat) return;
        uint action = e.Key switch
        {
            Key.Space => 1, Key.E => 4, Key.Escape => 8, Key.Enter => 16,
            Key.Up => 32, Key.Down => 64, Key.Left => 128, Key.Right => 256,
            Key.F4 => 4096, _ => 0
        };
        _pressed |= action;
        if (action != 0 || e.Key is Key.W or Key.A or Key.S or Key.D) e.Handled = true;
    }
}
