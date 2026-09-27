using System.Globalization;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;

namespace Vestigio.Studio;

public partial class Vestigio3DWindow
{
    private sealed record RoomOpening(int Edge, string Kind, float Offset,
        float Width, float Height, float Sill)
    {
        public override string ToString() =>
            $"{Kind switch { "window" => "Ventana", "gap" => "Hueco", _ => "Puerta" }}" +
            $" · arista {Edge} · {Offset:0.##}–{Offset + Width:0.##} m";
    }

    private string _editingRoomId = "";
    private string _loadedRoomRecipe = "";
    private bool _refreshingRoomEditor;
    private bool _roomPreviewActive;
    private bool _refreshingRoomView;

    private void RefreshRoomEditor()
    {
        if (!Viewport.IsNativeReady || RoomVertices is null) return;
        IReadOnlyList<string> selected = Viewport.SelectedUuids();
        string recipe = "";
        string id = selected.Count == 1 &&
            Viewport.TryGetRoomRecipe(selected[0], out recipe)
            ? selected[0] : "";
        if (id == _editingRoomId && recipe == _loadedRoomRecipe) return;
        _editingRoomId = id;
        _loadedRoomRecipe = recipe;
        CommitRoomButton.Content = id.Length == 0
            ? "Crear habitación" : "Actualizar habitación";
        if (id.Length == 0) return;
        try
        {
            using JsonDocument document = JsonDocument.Parse(recipe);
            JsonElement root = document.RootElement;
            _refreshingRoomEditor = true;
            RoomVertices.Text = string.Join(Environment.NewLine,
                root.GetProperty("vertices").EnumerateArray().Select(vertex =>
                    $"{vertex[0].GetSingle().ToString("0.####", CultureInfo.InvariantCulture)}," +
                    vertex[1].GetSingle().ToString("0.####", CultureInfo.InvariantCulture)));
            RoomFloor.Text = FormatRoomNumber(root.GetProperty("floor_z").GetSingle());
            RoomHeight.Text = FormatRoomNumber(root.GetProperty("wall_height").GetSingle());
            RoomThickness.Text = FormatRoomNumber(root.GetProperty("wall_thickness").GetSingle());
            RoomOpenings.Items.Clear();
            foreach (JsonElement opening in root.GetProperty("openings").EnumerateArray())
                RoomOpenings.Items.Add(new RoomOpening(
                    opening.GetProperty("edge").GetInt32(),
                    opening.GetProperty("kind").GetString() ?? "door",
                    opening.GetProperty("offset").GetSingle(),
                    opening.GetProperty("width").GetSingle(),
                    opening.GetProperty("height").GetSingle(),
                    opening.GetProperty("sill").GetSingle()));
            RoomEditorError.Text = "";
        }
        catch (Exception exception) when (exception is JsonException or
            KeyNotFoundException or InvalidOperationException)
        {
            RoomEditorError.Text = $"No se pudo leer la receta: {exception.Message}";
            RecordProblem(RoomEditorError.Text);
        }
        finally { _refreshingRoomEditor = false; }
        RefreshRoomDimensions();
        UpdateRoomPlan();
    }

    private static string FormatRoomNumber(float value) =>
        value.ToString("0.####", CultureInfo.InvariantCulture);

    private void OpenRoomEditor_Click(object sender, RoutedEventArgs e)
    {
        Point position = RoomSectionHeading.TransformToAncestor(InspectorScroll)
            .Transform(new Point(0, 0));
        InspectorScroll.ScrollToVerticalOffset(
            InspectorScroll.VerticalOffset + position.Y - 8);
        StatusText.Text = "Constructor de salas · traza el contorno y comprueba la vista previa";
    }

    private void RoomRect_Click(object sender, RoutedEventArgs e)
    {
        if (!ReadRoomNumber(RoomRectX, out float x) ||
            !ReadRoomNumber(RoomRectY, out float y) ||
            !ReadRoomNumber(RoomRectWidth, out float width) || width <= 0 ||
            !ReadRoomNumber(RoomRectDepth, out float depth) || depth <= 0)
        {
            RoomEditorError.Text = "Rectángulo: X/Y finitos y ancho/largo positivos.";
            return;
        }
        RoomVertices.Text = string.Join(Environment.NewLine,
            $"{FormatRoomNumber(x)},{FormatRoomNumber(y)}",
            $"{FormatRoomNumber(x + width)},{FormatRoomNumber(y)}",
            $"{FormatRoomNumber(x + width)},{FormatRoomNumber(y + depth)}",
            $"{FormatRoomNumber(x)},{FormatRoomNumber(y + depth)}");
        RoomEditorError.Text = "";
    }

    private static bool TryReadRoomVertices(string text, out List<Point> vertices)
    {
        vertices = [];
        foreach (string rawLine in text.Split(
                     ["\r\n", "\n", "\r"], StringSplitOptions.RemoveEmptyEntries))
        {
            string[] coordinates = rawLine.Trim().Split(',', 2);
            if (coordinates.Length != 2 ||
                !float.TryParse(coordinates[0], NumberStyles.Float,
                    CultureInfo.InvariantCulture, out float x) ||
                !float.TryParse(coordinates[1], NumberStyles.Float,
                    CultureInfo.InvariantCulture, out float y) ||
                !float.IsFinite(x) || !float.IsFinite(y)) return false;
            vertices.Add(new Point(x, y));
        }
        return vertices.Count is >= 3 and <= 24;
    }

    private void RoomVertices_Changed(object sender, TextChangedEventArgs e)
    {
        RefreshRoomDimensions();
        if (!_refreshingRoomEditor) UpdateRoomPlan();
    }

    private void RefreshRoomDimensions()
    {
        if (RoomDimensions is null || RoomPlan is null) return;
        if (!TryReadRoomVertices(RoomVertices.Text, out List<Point> vertices))
            RoomDimensions.Text = "Contorno: 3–24 vértices x,y con punto decimal.";
        else
        {
            double area = 0, perimeter = 0;
            for (int index = 0; index < vertices.Count; ++index)
            {
                Point a = vertices[index], b = vertices[(index + 1) % vertices.Count];
                area += a.X * b.Y - b.X * a.Y;
                perimeter += (b - a).Length;
            }
            RoomDimensions.Text =
                $"{vertices.Count} lados · {Math.Abs(area * 0.5):0.##} m² · perímetro {perimeter:0.##} m" +
                (area <= 0 ? " · invierte el orden a antihorario" : "");
        }
    }

    private void RoomFloor_Changed(object sender, TextChangedEventArgs e)
    {
        if (!_refreshingRoomEditor) UpdateRoomPlan();
    }

    private static bool ReadRoomNumber(TextBox input, out float value) =>
        float.TryParse(input.Text.Trim(), NumberStyles.Float,
            CultureInfo.InvariantCulture, out value) && float.IsFinite(value);

    private bool ReadRoomOpening(out RoomOpening opening)
    {
        opening = new RoomOpening(0, "door", 0, 0, 0, 0);
        if (!int.TryParse(OpeningEdge.Text.Trim(), NumberStyles.None,
                CultureInfo.InvariantCulture, out int edge) || edge < 0 ||
            !ReadRoomNumber(OpeningOffset, out float offset) || offset < 0 ||
            !ReadRoomNumber(OpeningWidth, out float width) || width <= 0 ||
            !ReadRoomNumber(OpeningHeight, out float height) || height <= 0 ||
            !ReadRoomNumber(OpeningSill, out float sill) || sill < 0)
        {
            RoomEditorError.Text = "Abertura: arista entera y dimensiones positivas en metros.";
            return false;
        }
        string kind = (OpeningKind.SelectedItem as ComboBoxItem)?.Tag as string ?? "door";
        opening = new RoomOpening(edge, kind, offset, width, height, sill);
        return true;
    }

    private void RoomOpenings_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_refreshingRoomEditor || RoomOpenings.SelectedItem is not RoomOpening opening)
            return;
        OpeningEdge.Text = opening.Edge.ToString(CultureInfo.InvariantCulture);
        OpeningKind.SelectedIndex = opening.Kind switch
        {
            "window" => 1, "gap" => 2, _ => 0
        };
        OpeningOffset.Text = FormatRoomNumber(opening.Offset);
        OpeningWidth.Text = FormatRoomNumber(opening.Width);
        OpeningHeight.Text = FormatRoomNumber(opening.Height);
        OpeningSill.Text = FormatRoomNumber(opening.Sill);
    }

    private void AddOpening_Click(object sender, RoutedEventArgs e)
    {
        if (Viewport.IsPlaying || !ReadRoomOpening(out RoomOpening opening)) return;
        RoomOpenings.Items.Add(opening);
        RoomOpenings.SelectedIndex = RoomOpenings.Items.Count - 1;
        RoomEditorError.Text = "";
        UpdateRoomPlan();
    }

    private void UpdateOpening_Click(object sender, RoutedEventArgs e)
    {
        if (Viewport.IsPlaying || RoomOpenings.SelectedIndex < 0 ||
            !ReadRoomOpening(out RoomOpening opening)) return;
        int index = RoomOpenings.SelectedIndex;
        RoomOpenings.Items[index] = opening;
        RoomOpenings.SelectedIndex = index;
        RoomEditorError.Text = "";
        UpdateRoomPlan();
    }

    private void RemoveOpening_Click(object sender, RoutedEventArgs e)
    {
        if (Viewport.IsPlaying || RoomOpenings.SelectedIndex < 0) return;
        RoomOpenings.Items.RemoveAt(RoomOpenings.SelectedIndex);
        UpdateRoomPlan();
    }

    private bool TryBuildRoomRecipe(out string json)
    {
        json = "";
        if (!TryReadRoomVertices(RoomVertices.Text, out List<Point> roomVertices) ||
            !ReadRoomNumber(RoomFloor, out float floor) ||
            !ReadRoomNumber(RoomHeight, out float height) || height <= 0 ||
            !ReadRoomNumber(RoomThickness, out float thickness) || thickness <= 0)
        {
            RoomEditorError.Text = "La sala necesita 3–24 vértices y alto/grosor positivos.";
            return false;
        }
        var openings = RoomOpenings.Items.OfType<RoomOpening>().ToArray();
        if (openings.Any(opening => opening.Edge >= roomVertices.Count))
        {
            RoomEditorError.Text = "Una abertura refiere una arista fuera del contorno.";
            return false;
        }
        foreach (RoomOpening opening in openings)
        {
            Point a = roomVertices[opening.Edge];
            Point b = roomVertices[(opening.Edge + 1) % roomVertices.Count];
            double edgeLength = (b - a).Length;
            if (opening.Offset < thickness * 0.5f ||
                opening.Offset + opening.Width > edgeLength - thickness * 0.5f ||
                opening.Sill + opening.Height > height + 0.0001f ||
                (opening.Kind == "window" && opening.Sill < 0.2f) ||
                (opening.Kind != "window" && opening.Sill != 0))
            {
                RoomEditorError.Text =
                    $"Abertura de arista {opening.Edge}: revisa margen, ancho, alto y umbral.";
                return false;
            }
        }
        float[][] vertices = roomVertices.Select(point =>
            new[] { (float)point.X, (float)point.Y }).ToArray();
        json = JsonSerializer.Serialize(new
        {
            version = 1, vertices, floor_z = floor,
            wall_height = height, wall_thickness = thickness,
            openings = openings.Select(opening => new
            {
                edge = opening.Edge, kind = opening.Kind,
                offset = opening.Offset, width = opening.Width,
                height = opening.Height, sill = opening.Sill
            }).ToArray()
        });
        RoomEditorError.Text = "";
        return true;
    }

    private void PreviewRoom_Click(object sender, RoutedEventArgs e)
    {
        if (!TryBuildRoomRecipe(out string recipe)) return;
        if (!Viewport.TryPreviewRoomRecipe(recipe))
        {
            RoomEditorError.Text = Viewport.LastError;
            RecordProblem(RoomEditorError.Text);
            return;
        }
        _roomPreviewActive = true;
        StatusText.Text = "Vista previa de habitación · no modifica el documento";
    }

    private void CancelRoomPreview_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TryCancelRoomPreview()) return;
        _roomPreviewActive = false;
        RoomEditorError.Text = "";
        StatusText.Text = "Vista previa cancelada · documento intacto";
    }

    private void CommitRoom_Click(object sender, RoutedEventArgs e)
    {
        if (!TryBuildRoomRecipe(out string recipe)) return;
        bool creating = _editingRoomId.Length == 0;
        bool result = creating
            ? Viewport.TryCreateRoomRecipe(recipe, out _)
            : Viewport.TryUpdateRoomRecipe(_editingRoomId, recipe);
        if (!result)
        {
            RoomEditorError.Text = Viewport.LastError;
            RecordProblem(RoomEditorError.Text);
            return;
        }
        _roomPreviewActive = false;
        RefreshDocument();
        _ = Viewport.FrameSelection();
        StatusText.Text = creating
            ? "Habitación creada · Deshacer disponible"
            : "Receta actualizada · Deshacer disponible";
    }

    private void RoomView_Changed(object sender, RoutedEventArgs e)
    {
        if (_refreshingRoomView || Viewport is null || !Viewport.IsNativeReady ||
            RoomPlan is null || RoomFloorView is null) return;
        UpdateRoomPlan();
        if (!ApplyRoomEditorView())
        {
            RoomEditorError.Text = "No se pudo actualizar la vista 3D de autoría.";
            RecordProblem(RoomEditorError.Text);
            return;
        }
        StatusText.Text = "Vistas 2D y 3D de autoría actualizadas · juego sin cambios";
    }

    private bool ApplyRoomEditorView()
    {
        if (!double.TryParse(RoomFloorView.Text, NumberStyles.Float,
                CultureInfo.InvariantCulture, out double floor) ||
            !double.IsFinite(floor) || floor < -1000 || floor > 1000)
        {
            RoomEditorError.Text = "Planta activa: introduce una altura Z válida en metros.";
            return false;
        }
        return Viewport.TrySetRoomEditorView(RoomGrid.IsChecked == true,
            RoomGhost.IsChecked == true, (float)floor);
    }

    private void UpdateRoomPlan()
    {
        if (RoomPlan is null || RoomFloorView is null || Viewport is null ||
            !Viewport.IsNativeReady) return;
        var rooms = new List<RoomPlanView.Outline>();
        foreach (string id in Viewport.EntityUuids())
        {
            if (!Viewport.TryGetRoomRecipe(id, out string json)) continue;
            try
            {
                using JsonDocument document = JsonDocument.Parse(json);
                JsonElement root = document.RootElement;
                Point[] vertices = root.GetProperty("vertices").EnumerateArray()
                    .Select(vertex => new Point(vertex[0].GetDouble(),
                        vertex[1].GetDouble())).ToArray();
                RoomPlanView.Gap[] gaps = root.GetProperty("openings").EnumerateArray()
                    .Select(opening => new RoomPlanView.Gap(
                        opening.GetProperty("edge").GetInt32(),
                        opening.GetProperty("offset").GetDouble(),
                        opening.GetProperty("width").GetDouble(),
                        opening.GetProperty("kind").GetString() ?? "gap")).ToArray();
                rooms.Add(new RoomPlanView.Outline(vertices,
                    root.GetProperty("floor_z").GetDouble(), gaps));
            }
            catch (Exception exception) when (exception is JsonException or
                KeyNotFoundException or InvalidOperationException)
            {
                RecordProblem($"Plano editorial: {exception.Message}");
            }
        }
        string activeText = RoomFloorView.Text;
        _refreshingRoomView = true;
        try
        {
            RoomFloorView.Items.Clear();
            foreach (double floor in rooms.Select(room => room.Floor)
                         .Append(0).Distinct().OrderBy(value => value))
                RoomFloorView.Items.Add(floor.ToString("0.####",
                    CultureInfo.InvariantCulture));
            RoomFloorView.Text = activeText.Length > 0 ? activeText : "0";
        }
        finally { _refreshingRoomView = false; }
        RoomPlan.Rooms = rooms;
        RoomPlan.Draft = TryReadRoomVertices(RoomVertices.Text,
            out List<Point> draft) ? draft : [];
        RoomPlan.DraftOpenings = RoomOpenings.Items.OfType<RoomOpening>()
            .Select(opening => new RoomPlanView.Gap(opening.Edge,
                opening.Offset, opening.Width, opening.Kind)).ToArray();
        RoomPlan.DraftFloor = ReadRoomNumber(RoomFloor, out float draftFloor)
            ? draftFloor : 0;
        RoomPlan.ShowGrid = RoomGrid.IsChecked == true;
        RoomPlan.ShowGhost = RoomGhost.IsChecked == true;
        RoomPlan.ActiveFloor = double.TryParse(RoomFloorView.Text,
            NumberStyles.Float, CultureInfo.InvariantCulture, out double floorZ) &&
            double.IsFinite(floorZ) ? floorZ : 0;
        RoomPlan.InvalidateVisual();
    }
}
