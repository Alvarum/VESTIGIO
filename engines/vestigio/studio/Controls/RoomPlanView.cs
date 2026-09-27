using System.Globalization;
using System.Windows;
using System.Windows.Media;

namespace Vestigio.Studio;

/// <summary>Plano editorial 2D. Lee recetas nativas sin poseer ni guardar geometría.</summary>
public sealed class RoomPlanView : FrameworkElement
{
    internal sealed record Gap(int Edge, double Offset, double Width, string Kind);
    internal sealed record Outline(IReadOnlyList<Point> Vertices, double Floor,
        IReadOnlyList<Gap> Openings);

    private static readonly Brush BackgroundBrush = new SolidColorBrush(
        Color.FromRgb(9, 20, 29));
    private static readonly Pen GridPen = new(new SolidColorBrush(
        Color.FromRgb(37, 53, 66)), 0.6);
    private static readonly Pen ActivePen = new(new SolidColorBrush(
        Color.FromRgb(64, 211, 226)), 2);
    private static readonly Pen GhostPen = new(new SolidColorBrush(
        Color.FromArgb(95, 130, 157, 176)), 1);
    private static readonly Pen DraftPen = new(new SolidColorBrush(
        Color.FromRgb(255, 190, 94)), 2)
    {
        DashStyle = DashStyles.Dash
    };
    private static readonly Pen DoorPen = new(Brushes.Orange, 2);
    private static readonly Pen WindowPen = new(Brushes.LightSkyBlue, 2);
    private static readonly Brush ActiveFill = new SolidColorBrush(
        Color.FromArgb(32, 64, 211, 226));
    private static readonly Brush GhostFill = new SolidColorBrush(
        Color.FromArgb(14, 130, 157, 176));

    internal IReadOnlyList<Outline> Rooms { get; set; } = [];
    internal IReadOnlyList<Point> Draft { get; set; } = [];
    internal IReadOnlyList<Gap> DraftOpenings { get; set; } = [];
    internal double DraftFloor { get; set; }
    internal double ActiveFloor { get; set; }
    internal bool ShowGrid { get; set; } = true;
    internal bool ShowGhost { get; set; }

    protected override void OnRender(DrawingContext drawing)
    {
        base.OnRender(drawing);
        double width = ActualWidth, height = ActualHeight;
        drawing.DrawRectangle(BackgroundBrush, null, new Rect(0, 0, width, height));
        if (width < 40 || height < 40) return;
        List<Point> all = Rooms.Where(room => ShowGhost ||
                Math.Abs(room.Floor - ActiveFloor) < 0.01)
            .SelectMany(room => room.Vertices).Concat(ShowGhost ||
                Math.Abs(DraftFloor - ActiveFloor) < 0.01 ? Draft : []).ToList();
        if (all.Count == 0)
            all.AddRange([new Point(-5, -5), new Point(5, 5)]);
        double minX = all.Min(point => point.X), maxX = all.Max(point => point.X);
        double minY = all.Min(point => point.Y), maxY = all.Max(point => point.Y);
        double centerX = (minX + maxX) * 0.5, centerY = (minY + maxY) * 0.5;
        double spanX = Math.Max(8, maxX - minX + 3), spanY = Math.Max(8, maxY - minY + 3);
        double pixels = Math.Min((width - 24) / spanX, (height - 24) / spanY);
        Point Screen(Point point) => new(width * 0.5 + (point.X - centerX) * pixels,
            height * 0.5 - (point.Y - centerY) * pixels);
        if (ShowGrid && pixels >= 7)
        {
            int lowX = Math.Max(-100, (int)Math.Floor(centerX - width / pixels));
            int highX = Math.Min(100, (int)Math.Ceiling(centerX + width / pixels));
            int lowY = Math.Max(-100, (int)Math.Floor(centerY - height / pixels));
            int highY = Math.Min(100, (int)Math.Ceiling(centerY + height / pixels));
            for (int x = lowX; x <= highX; ++x)
                drawing.DrawLine(GridPen, Screen(new Point(x, lowY)),
                    Screen(new Point(x, highY)));
            for (int y = lowY; y <= highY; ++y)
                drawing.DrawLine(GridPen, Screen(new Point(lowX, y)),
                    Screen(new Point(highX, y)));
        }
        foreach (Outline room in Rooms.Where(room =>
                     Math.Abs(room.Floor - ActiveFloor) >= 0.01 && ShowGhost))
            DrawOutline(drawing, room.Vertices, room.Openings, Screen,
                GhostPen, GhostFill, false);
        foreach (Outline room in Rooms.Where(room =>
                     Math.Abs(room.Floor - ActiveFloor) < 0.01))
            DrawOutline(drawing, room.Vertices, room.Openings, Screen,
                ActivePen, ActiveFill, true);
        if (Math.Abs(DraftFloor - ActiveFloor) < 0.01)
            DrawOutline(drawing, Draft, DraftOpenings, Screen,
                DraftPen, null, true);
        else if (ShowGhost)
            DrawOutline(drawing, Draft, DraftOpenings, Screen,
                GhostPen, null, false);
        DrawText(drawing, $"PLANTA Z {ActiveFloor:0.##} m", new Point(8, 5),
            Brushes.LightGray, 11);
        if (Rooms.Count == 0)
            DrawText(drawing, "Vista previa del contorno", new Point(8, height - 21),
                Brushes.LightGray, 11);
    }

    private void DrawOutline(DrawingContext drawing, IReadOnlyList<Point> vertices,
        IReadOnlyList<Gap> openings, Func<Point, Point> screen,
        Pen pen, Brush? fill, bool dimension)
    {
        if (vertices.Count < 2) return;
        var geometry = new StreamGeometry();
        using (StreamGeometryContext context = geometry.Open())
        {
            context.BeginFigure(screen(vertices[0]), fill is not null,
                vertices.Count >= 3);
            context.PolyLineTo(vertices.Skip(1).Select(screen).ToArray(), true, false);
        }
        geometry.Freeze();
        drawing.DrawGeometry(fill, null, geometry);
        int edgeCount = vertices.Count >= 3 ? vertices.Count : vertices.Count - 1;
        for (int edge = 0; edge < edgeCount; ++edge)
        {
            Point a = vertices[edge], b = vertices[(edge + 1) % vertices.Count];
            Vector direction = b - a;
            double length = direction.Length;
            if (length <= 0.001) continue;
            direction.Normalize();
            double cursor = 0;
            foreach (Gap gap in openings.Where(item => item.Edge == edge)
                         .OrderBy(item => item.Offset))
            {
                double start = Math.Clamp(gap.Offset, cursor, length);
                double end = Math.Clamp(gap.Offset + gap.Width, start, length);
                if (start > cursor)
                    drawing.DrawLine(pen, screen(a + direction * cursor),
                        screen(a + direction * start));
                if (end > start && gap.Kind != "gap")
                    drawing.DrawLine(!dimension ? GhostPen :
                        gap.Kind == "window" ? WindowPen : DoorPen,
                        screen(a + direction * start), screen(a + direction * end));
                cursor = end;
            }
            if (cursor < length)
                drawing.DrawLine(pen, screen(a + direction * cursor), screen(b));
        }
        if (!dimension || vertices.Count < 3) return;
        for (int index = 0; index < vertices.Count; ++index)
        {
            Point a = vertices[index], b = vertices[(index + 1) % vertices.Count];
            double length = (b - a).Length;
            if ((screen(b) - screen(a)).Length < 24)
                continue;
            Point mid = screen(new Point((a.X + b.X) * 0.5, (a.Y + b.Y) * 0.5));
            DrawText(drawing, $"{length:0.##} m", new Point(mid.X + 3, mid.Y - 14),
                Brushes.WhiteSmoke, 10);
        }
    }

    private void DrawText(DrawingContext drawing, string text, Point origin,
        Brush brush, double size)
    {
        var formatted = new FormattedText(text, CultureInfo.InvariantCulture,
            FlowDirection.LeftToRight, new Typeface("Segoe UI"), size,
            brush, VisualTreeHelper.GetDpi(this).PixelsPerDip);
        drawing.DrawText(formatted, origin);
    }
}
