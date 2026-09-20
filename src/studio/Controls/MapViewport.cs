using System.Windows;
using System.Windows.Input;
using System.Windows.Media;

namespace RetroForge.Studio;

/// <summary>
/// Vista de planta independiente de raylib. Dibuja los mismos datos consultados
/// al motor C y prioriza objetos pequeños al seleccionar, evitando que una sala
/// grande oculte puertas, luces y personajes.
/// </summary>
public sealed class MapViewport : FrameworkElement
{
    public static readonly DependencyProperty ShowLightsProperty = DependencyProperty.Register(
        nameof(ShowLights), typeof(bool), typeof(MapViewport),
        new FrameworkPropertyMetadata(false, FrameworkPropertyMetadataOptions.AffectsRender));
    public static readonly DependencyProperty ShowTriggersProperty = DependencyProperty.Register(
        nameof(ShowTriggers), typeof(bool), typeof(MapViewport),
        new FrameworkPropertyMetadata(true, FrameworkPropertyMetadataOptions.AffectsRender));

    public bool ShowLights
    {
        get => (bool)GetValue(ShowLightsProperty);
        set => SetValue(ShowLightsProperty, value);
    }
    public bool ShowTriggers
    {
        get => (bool)GetValue(ShowTriggersProperty);
        set => SetValue(ShowTriggersProperty, value);
    }

    private StudioViewModel? _viewModel;
    private Point _pan;
    private Point _dragOrigin;
    private Point _panOrigin;
    private double _zoom = 36;
    private bool _panning;
    private MarkerModel? _draggingMarker;
    private Point _markerDragOrigin;
    private Point? _markerPreview;

    public MapViewport()
    {
        Focusable = true;
        ClipToBounds = true;
        AllowDrop = true;
        DataContextChanged += (_, _) => Attach(DataContext as StudioViewModel);
    }

    protected override void OnRender(DrawingContext drawing)
    {
        base.OnRender(drawing);
        drawing.DrawRectangle(new SolidColorBrush(Color.FromRgb(18, 21, 24)), null,
            new Rect(RenderSize));
        DrawGrid(drawing);
        if (_viewModel is null)
            return;

        foreach (SectorModel sector in _viewModel.Document.Sectors)
            DrawSector(drawing, sector);
        foreach (BarrierModel barrier in _viewModel.Document.Barriers)
            DrawBarrier(drawing, barrier);
        if (ShowTriggers)
            foreach (TriggerModel trigger in _viewModel.Document.Triggers)
                DrawTrigger(drawing, trigger);
        if (ShowLights)
            foreach (LightModel light in _viewModel.Document.Lights)
                DrawLight(drawing, light);
        foreach (MarkerModel marker in _viewModel.Document.Markers)
            DrawMarker(drawing, marker);
        if (_draggingMarker is not null && _markerPreview is Point preview)
        {
            Point screen = ToScreen(preview);
            var pen = new Pen(new SolidColorBrush(Color.FromRgb(255, 190, 92)), 2)
                { DashStyle = DashStyles.Dash };
            drawing.DrawEllipse(new SolidColorBrush(Color.FromArgb(80, 255, 190, 92)), pen,
                screen, 10, 10);
        }
    }

    private void DrawGrid(DrawingContext drawing)
    {
        var minor = new Pen(new SolidColorBrush(Color.FromRgb(34, 39, 43)), 1);
        var major = new Pen(new SolidColorBrush(Color.FromRgb(45, 51, 56)), 1);
        double spacing = _zoom;
        double originX = ActualWidth / 2 + _pan.X;
        double originY = ActualHeight / 2 + _pan.Y;
        int firstX = (int)Math.Floor(-originX / spacing) - 1;
        int lastX = (int)Math.Ceiling((ActualWidth - originX) / spacing) + 1;
        for (int x = firstX; x <= lastX; x++)
        {
            double screen = originX + x * spacing;
            drawing.DrawLine(x % 5 == 0 ? major : minor, new Point(screen, 0), new Point(screen, ActualHeight));
        }
        int firstY = (int)Math.Floor(-originY / spacing) - 1;
        int lastY = (int)Math.Ceiling((ActualHeight - originY) / spacing) + 1;
        for (int y = firstY; y <= lastY; y++)
        {
            double screen = originY + y * spacing;
            drawing.DrawLine(y % 5 == 0 ? major : minor, new Point(0, screen), new Point(ActualWidth, screen));
        }
    }

    private void DrawSector(DrawingContext drawing, SectorModel sector)
    {
        if (sector.Vertices.Count < 3)
            return;
        var geometry = new StreamGeometry();
        using (StreamGeometryContext context = geometry.Open())
        {
            context.BeginFigure(ToScreen(sector.Vertices[0]), true, true);
            context.PolyLineTo(sector.Vertices.Skip(1).Select(ToScreen).ToArray(), true, false);
        }
        bool selected = ReferenceEquals(_viewModel?.SelectedItem, sector) ||
                        _viewModel?.SelectedItem is SectorModel item && item.Index == sector.Index;
        var fill = new SolidColorBrush(selected ? Color.FromArgb(60, 232, 165, 75) : Color.FromArgb(24, 116, 147, 153));
        var stroke = new Pen(new SolidColorBrush(selected ? Color.FromRgb(232, 165, 75) : Color.FromRgb(105, 135, 142)), selected ? 2.4 : 1.5);
        drawing.DrawGeometry(fill, stroke, geometry);

        double area = PolygonArea(sector.Vertices);
        if (area >= 8 || selected)
        {
            Point center = new(sector.Vertices.Average(point => point.X), sector.Vertices.Average(point => point.Y));
            drawing.DrawText(CreateText($"H{sector.Index + 1}  ·  {sector.Floor:0.##} m", 12,
                selected ? Color.FromRgb(244, 194, 126) : Color.FromRgb(139, 151, 160)), ToScreen(center));
        }
    }

    private void DrawMarker(DrawingContext drawing, MarkerModel marker)
    {
        Point point = ToScreen(new Point(marker.X, marker.Y));
        bool selected = _viewModel?.SelectedItem is MarkerModel item && item.Index == marker.Index;
        Brush brush = new SolidColorBrush(selected ? Color.FromRgb(255, 190, 92) : Color.FromRgb(105, 195, 154));
        drawing.DrawEllipse(new SolidColorBrush(Color.FromRgb(22, 27, 30)), new Pen(brush, 2), point, 6, 6);
        drawing.DrawText(CreateText(marker.DisplayName, 11, ((SolidColorBrush)brush).Color), point + new Vector(9, -8));
    }

    private void DrawBarrier(DrawingContext drawing, BarrierModel barrier)
    {
        if (_viewModel is null || barrier.Sector < 0 || barrier.Sector >= _viewModel.Document.Sectors.Count)
            return;
        SectorModel sector = _viewModel.Document.Sectors[barrier.Sector];
        if (barrier.Edge < 0 || barrier.Edge >= sector.Vertices.Count)
            return;
        Point a = ToScreen(sector.Vertices[barrier.Edge]);
        Point b = ToScreen(sector.Vertices[(barrier.Edge + 1) % sector.Vertices.Count]);
        bool selected = _viewModel.SelectedItem is BarrierModel item && item.Index == barrier.Index;
        Color color = barrier.Native.Kind == 0 ? Color.FromRgb(214, 143, 74) : Color.FromRgb(94, 181, 205);
        drawing.DrawLine(new Pen(new SolidColorBrush(color), selected ? 6 : 4), a, b);
    }

    private void DrawTrigger(DrawingContext drawing, TriggerModel trigger)
    {
        Point center = ToScreen(new Point(trigger.Native.X, trigger.Native.Y));
        bool selected = _viewModel?.SelectedItem is TriggerModel item && item.Index == trigger.Index;
        var pen = new Pen(new SolidColorBrush(selected ? Color.FromRgb(255, 190, 92)
                                                       : Color.FromArgb(190, 177, 112, 212)),
                          selected ? 3 : 1.5)
            { DashStyle = DashStyles.Dash };
        if (trigger.Native.Shape == 1)
        {
            double radius = trigger.Native.Radius * _zoom;
            drawing.DrawEllipse(null, pen, center, radius, radius);
        }
        else if (trigger.Native.Shape == 0)
        {
            double width = trigger.Native.SizeX * 2 * _zoom;
            double height = trigger.Native.SizeY * 2 * _zoom;
            drawing.DrawRectangle(null, pen, new Rect(center.X - width / 2, center.Y - height / 2, width, height));
        }
    }

    private void DrawLight(DrawingContext drawing, LightModel light)
    {
        Point center = ToScreen(new Point(light.Native.X, light.Native.Y));
        var color = Color.FromArgb(35,
            (byte)(Math.Clamp(light.Native.Red, 0, 1) * 255),
            (byte)(Math.Clamp(light.Native.Green, 0, 1) * 255),
            (byte)(Math.Clamp(light.Native.Blue, 0, 1) * 255));
        double radius = light.Native.Radius * _zoom;
        bool selected = _viewModel?.SelectedItem is LightModel item && item.Index == light.Index;
        drawing.DrawEllipse(new SolidColorBrush(color),
            new Pen(new SolidColorBrush(selected ? Color.FromRgb(255, 190, 92)
                                                 : Color.FromArgb(130, 235, 191, 99)),
                    selected ? 3 : 1), center, radius, radius);
        drawing.DrawEllipse(new SolidColorBrush(Color.FromRgb(245, 196, 97)), null, center, 3.5, 3.5);
    }

    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        base.OnMouseLeftButtonDown(e);
        Focus();
        if (_viewModel is null)
            return;
        Point mouse = e.GetPosition(this);
        MarkerModel? marker = _viewModel.Document.Markers.LastOrDefault(item =>
            (ToScreen(new Point(item.X, item.Y)) - mouse).Length <= 12);
        if (marker is not null)
        {
            _viewModel.SelectedItem = marker;
            _draggingMarker = marker;
            _markerDragOrigin = mouse;
            _markerPreview = null;
            CaptureMouse();
            InvalidateVisual();
            return;
        }
        BarrierModel? barrier = _viewModel.Document.Barriers.LastOrDefault(item =>
            BarrierDistance(item, mouse) <= 9);
        if (barrier is not null)
        {
            _viewModel.SelectedItem = barrier;
            InvalidateVisual();
            return;
        }
        if (ShowLights)
        {
            LightModel? light = _viewModel.Document.Lights.LastOrDefault(item =>
                (ToScreen(new Point(item.Native.X, item.Native.Y)) - mouse).Length <= 11);
            if (light is not null)
            {
                _viewModel.SelectedItem = light;
                InvalidateVisual();
                return;
            }
        }
        Point world = ToWorld(mouse);
        if (ShowTriggers)
        {
            TriggerModel? trigger = _viewModel.Document.Triggers.LastOrDefault(item =>
                TriggerContains(item, world));
            if (trigger is not null)
            {
                _viewModel.SelectedItem = trigger;
                InvalidateVisual();
                return;
            }
        }
        SectorModel? sector = _viewModel.Document.Sectors.LastOrDefault(item => Contains(item.Vertices, world));
        _viewModel.SelectedItem = sector;
        InvalidateVisual();
    }

    protected override void OnMouseDown(MouseButtonEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.ChangedButton != MouseButton.Middle)
            return;
        _panning = true;
        _dragOrigin = e.GetPosition(this);
        _panOrigin = _pan;
        CaptureMouse();
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (_draggingMarker is not null && e.LeftButton == MouseButtonState.Pressed)
        {
            Point mouse = e.GetPosition(this);
            if ((mouse - _markerDragOrigin).Length >= SystemParameters.MinimumHorizontalDragDistance)
            {
                Point world = ToWorld(mouse);
                _markerPreview = new Point(Math.Round(world.X * 4) / 4, Math.Round(world.Y * 4) / 4);
                InvalidateVisual();
            }
            return;
        }
        if (!_panning)
            return;
        Vector delta = e.GetPosition(this) - _dragOrigin;
        _pan = _panOrigin + delta;
        InvalidateVisual();
    }

    protected override void OnMouseUp(MouseButtonEventArgs e)
    {
        base.OnMouseUp(e);
        if (e.ChangedButton == MouseButton.Left && _draggingMarker is not null)
        {
            MarkerModel marker = _draggingMarker;
            Point? destination = _markerPreview;
            _draggingMarker = null;
            _markerPreview = null;
            ReleaseMouseCapture();
            if (destination is Point world)
                _viewModel?.MoveMarker(marker, world.X, world.Y);
            InvalidateVisual();
            e.Handled = true;
            return;
        }
        if (e.ChangedButton != MouseButton.Middle)
            return;
        _panning = false;
        ReleaseMouseCapture();
    }

    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        Point before = ToWorld(e.GetPosition(this));
        _zoom = Math.Clamp(_zoom * (e.Delta > 0 ? 1.12 : 1 / 1.12), 8, 180);
        Point after = ToScreen(before);
        Vector correction = e.GetPosition(this) - after;
        _pan += correction;
        InvalidateVisual();
        e.Handled = true;
    }

    protected override void OnDragOver(DragEventArgs e)
    {
        base.OnDragOver(e);
        e.Effects = _viewModel is not null &&
                    e.Data.GetDataPresent(typeof(PlacementTool))
            ? DragDropEffects.Copy
            : DragDropEffects.None;
        e.Handled = true;
    }

    protected override void OnDrop(DragEventArgs e)
    {
        base.OnDrop(e);
        if (_viewModel is null || e.Data.GetData(typeof(PlacementTool)) is not PlacementTool tool)
            return;
        Point world = ToWorld(e.GetPosition(this));
        double x = Math.Round(world.X * 4) / 4;
        double y = Math.Round(world.Y * 4) / 4;
        StudioLog.Write($"Entidad soltada: {tool.Definition} en {x:0.##}, {y:0.##}");
        _viewModel.Place(tool, x, y);
        InvalidateVisual();
        e.Handled = true;
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        if (_viewModel is null)
            return;
        if (e.Key == Key.Delete && _viewModel.DeleteCommand.CanExecute(null))
        {
            _viewModel.DeleteCommand.Execute(null);
            e.Handled = true;
        }
        else if (e.Key == Key.D && Keyboard.Modifiers.HasFlag(ModifierKeys.Control) &&
                 _viewModel.DuplicateCommand.CanExecute(null))
        {
            _viewModel.DuplicateCommand.Execute(null);
            e.Handled = true;
        }
        else if (e.Key == Key.Escape && _draggingMarker is not null)
        {
            _draggingMarker = null;
            _markerPreview = null;
            ReleaseMouseCapture();
            InvalidateVisual();
            e.Handled = true;
        }
    }

    private void Attach(StudioViewModel? viewModel)
    {
        if (_viewModel is not null)
            _viewModel.Document.Changed -= DocumentChanged;
        _viewModel = viewModel;
        if (_viewModel is not null)
            _viewModel.Document.Changed += DocumentChanged;
        InvalidateVisual();
    }

    private void DocumentChanged(object? sender, EventArgs args) => InvalidateVisual();
    private Point ToScreen(Point world) => new(ActualWidth / 2 + _pan.X + world.X * _zoom,
        ActualHeight / 2 + _pan.Y - world.Y * _zoom);
    private Point ToWorld(Point screen) => new((screen.X - ActualWidth / 2 - _pan.X) / _zoom,
        -(screen.Y - ActualHeight / 2 - _pan.Y) / _zoom);

    private double BarrierDistance(BarrierModel barrier, Point mouse)
    {
        if (_viewModel is null || barrier.Sector < 0 || barrier.Sector >= _viewModel.Document.Sectors.Count)
            return double.PositiveInfinity;
        SectorModel sector = _viewModel.Document.Sectors[barrier.Sector];
        if (barrier.Edge < 0 || barrier.Edge >= sector.Vertices.Count)
            return double.PositiveInfinity;
        Point a = ToScreen(sector.Vertices[barrier.Edge]);
        Point b = ToScreen(sector.Vertices[(barrier.Edge + 1) % sector.Vertices.Count]);
        Vector edge = b - a;
        double lengthSquared = edge.LengthSquared;
        if (lengthSquared <= double.Epsilon)
            return (mouse - a).Length;
        double along = Math.Clamp(Vector.Multiply(mouse - a, edge) / lengthSquared, 0, 1);
        return (mouse - (a + edge * along)).Length;
    }

    private static bool TriggerContains(TriggerModel trigger, Point world)
    {
        double dx = world.X - trigger.Native.X;
        double dy = world.Y - trigger.Native.Y;
        return trigger.Native.Shape switch
        {
            1 => dx * dx + dy * dy <= trigger.Native.Radius * trigger.Native.Radius,
            0 => Math.Abs(dx) <= trigger.Native.SizeX && Math.Abs(dy) <= trigger.Native.SizeY,
            _ => false
        };
    }

    private FormattedText CreateText(string text, double size, Color color) => new(text,
        System.Globalization.CultureInfo.CurrentUICulture, FlowDirection.LeftToRight,
        new Typeface("Segoe UI Variable Text"), size, new SolidColorBrush(color),
        VisualTreeHelper.GetDpi(this).PixelsPerDip);

    private static bool Contains(IReadOnlyList<Point> polygon, Point point)
    {
        bool inside = false;
        for (int i = 0, previous = polygon.Count - 1; i < polygon.Count; previous = i++)
        {
            Point a = polygon[i], b = polygon[previous];
            bool crosses = (a.Y > point.Y) != (b.Y > point.Y) &&
                           point.X < (b.X - a.X) * (point.Y - a.Y) / (b.Y - a.Y) + a.X;
            if (crosses)
                inside = !inside;
        }
        return inside;
    }

    private static double PolygonArea(IReadOnlyList<Point> polygon)
    {
        double sum = 0;
        for (int i = 0; i < polygon.Count; i++)
        {
            Point a = polygon[i];
            Point b = polygon[(i + 1) % polygon.Count];
            sum += a.X * b.Y - b.X * a.Y;
        }
        return Math.Abs(sum) * .5;
    }
}
