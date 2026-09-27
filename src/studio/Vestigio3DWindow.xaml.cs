using System.Globalization;
using System.Numerics;
using System.Text.Json;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Microsoft.Win32;

namespace RetroForge.Studio;

/// <summary>Edición del documento 3D nativo. WPF envía comandos al host y
/// mantiene un inspector, sin poseer una copia mutable de la escena.</summary>
public partial class Vestigio3DWindow : Window
{
    private readonly string _sourceLevelPath;
    private bool _savedWorkingCopy;
    private bool _refreshingHierarchy;
    private bool _refreshingAudio;
    private readonly Dictionary<string, string> _entityLabels = new(StringComparer.OrdinalIgnoreCase);
    private float[] _displayedPosition = [0, 0, 0];
    private float[] _displayedRotation = [0, 0, 0, 1];
    private float[] _displayedScale = [1, 1, 1];
    private Vector3 _displayedEuler;
    private readonly string[] _displayedTexts = new string[9];

    internal Vestigio3DWindow(string levelPath, string modelPath,
        string? visualSettingsPath = null, bool enableAudio = true)
    {
        if (!File.Exists(levelPath))
            throw new FileNotFoundException("No se encontró el nivel 3D.", levelPath);
        if (!File.Exists(modelPath))
            throw new FileNotFoundException("No se encontró el modelo 3D.", modelPath);
        InitializeComponent();
        _sourceLevelPath = Path.GetFullPath(levelPath);
        LoadEntityLabels(_sourceLevelPath);
        Viewport.LevelPath = _sourceLevelPath;
        Viewport.ModelPath = modelPath;
        Viewport.VisualSettingsPath = visualSettingsPath;
        Viewport.EnableAudio = enableAudio;
        Viewport.SelectionChanged += (_, _) => RefreshSelection();
        Viewport.GestureFinished += (_, _) =>
        {
            RefreshDocument();
            StatusText.Text = Viewport.LastGestureCommitted
                ? "Gesto aplicado · Deshacer disponible" : "Gesto cancelado";
        };
        LevelTitle.Text = DocumentName(levelPath);
        LevelPathText.Text = _sourceLevelPath;
        LevelPathText.ToolTip = _sourceLevelPath;
        Loaded += (_, _) =>
        {
            if (!Viewport.IsNativeReady)
                StatusText.Text = $"No se pudo abrir el viewport: {Viewport.LastError}";
            else
                RefreshDocument();
        };
        Closed += (_, _) => Viewport.Dispose();
        Closing += ConfirmClose;
    }

    internal GpuViewportHost GpuViewport => Viewport;
    internal string ActiveLevelPath => Viewport.LevelPath ?? _sourceLevelPath;
    internal void RefreshForTest() => RefreshDocument();
    internal int VisualProfileForTest => VisualProfile.SelectedIndex;
    internal string VisualSummaryForTest => VisualSummaryText.Text;
    internal void SelectVisualProfileForTest(int mode) => VisualProfile.SelectedIndex = mode;
    internal void SelectAudioGainForTest(uint bus, double percent) =>
        AudioSlider(bus).Value = percent;

    private Slider AudioSlider(uint bus) => bus switch
    {
        0 => MasterVolume,
        1 => MusicVolume,
        2 => SfxVolume,
        3 => AmbienceVolume,
        _ => throw new ArgumentOutOfRangeException(nameof(bus))
    };

    private void LoadEntityLabels(string levelPath)
    {
        _entityLabels.Clear();
        try
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(levelPath));
            if (!document.RootElement.TryGetProperty("entities", out JsonElement entities))
                return;
            foreach (JsonElement entity in entities.EnumerateArray())
            {
                string? id = entity.GetProperty("id").GetString();
                if (id is null) continue;
                if (!entity.TryGetProperty("components", out JsonElement components))
                    continue;
                if (components.TryGetProperty("engine.camera", out _))
                    _entityLabels[id] = "Cámara";
                else if (components.TryGetProperty("engine.mesh", out JsonElement mesh) &&
                         mesh.TryGetProperty("node_index", out JsonElement node))
                    _entityLabels[id] = node.GetInt32() switch
                    {
                        0 => "Suelo",
                        1 => "Monumento",
                        2 => "Pilar",
                        _ => "Modelo"
                    };
            }
        }
        catch (JsonException)
        {
            // La apertura nativa informa el error de estructura.
        }
    }

    private static string DocumentName(string levelPath)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(levelPath));
            if (document.RootElement.TryGetProperty("name", out JsonElement name) &&
                name.ValueKind == JsonValueKind.String &&
                !string.IsNullOrWhiteSpace(name.GetString()))
                return name.GetString()!;
        }
        catch (JsonException)
        {
            // El host nativo mostrará el diagnóstico estructural del documento.
        }
        return Path.GetFileNameWithoutExtension(levelPath);
    }

    private void RefreshDocument()
    {
        if (!Viewport.IsNativeReady) return;
        _refreshingHierarchy = true;
        HashSet<string> selected = Viewport.SelectedUuids().ToHashSet(
            StringComparer.OrdinalIgnoreCase);
        try
        {
            EntityList.Items.Clear();
            ParentTarget.Items.Clear();
            ParentTarget.Items.Add(new ComboBoxItem { Content = "Raíz", Tag = "" });
            IReadOnlyList<string> ids = Viewport.EntityUuids();
            for (int index = 0; index < ids.Count; ++index)
            {
                string id = ids[index];
                string label = Viewport.RoomPieceLabel(id) ??
                    (_entityLabels.TryGetValue(id, out string? known) ? known : "Objeto");
                var row = new ListBoxItem
                {
                    Content = $"{label} · {id[^8..]}",
                    Tag = id,
                    ToolTip = id
                };
                EntityList.Items.Add(row);
                ParentTarget.Items.Add(new ComboBoxItem
                {
                    Content = $"{label} · {id[^8..]}", Tag = id
                });
                if (selected.Contains(id))
                    EntityList.SelectedItems.Add(row);
            }
            ParentTarget.SelectedIndex = 0;
        }
        finally
        {
            _refreshingHierarchy = false;
        }
        RefreshSelectionFields();
        RefreshDirty();
        VisualSummaryText.Text = Viewport.VisualSummary;
        if (Viewport.VisualMode >= 0 && VisualProfile.SelectedIndex != Viewport.VisualMode)
            VisualProfile.SelectedIndex = Viewport.VisualMode;
        _refreshingAudio = true;
        try
        {
            for (uint bus = 0; bus < 4; ++bus)
                AudioSlider(bus).Value = Math.Round(Viewport.AudioGain(bus) * 100.0f);
        }
        finally { _refreshingAudio = false; }
    }

    private void AudioVolume_ValueChanged(object sender,
        RoutedPropertyChangedEventArgs<double> e)
    {
        if (_refreshingAudio || Viewport is null || !Viewport.IsNativeReady ||
            sender is not Slider slider || !uint.TryParse(slider.Tag?.ToString(), out uint bus))
            return;
        float gain = (float)(slider.Value / 100.0);
        if (!Viewport.TrySetAudioGain(bus, gain))
        {
            _refreshingAudio = true;
            slider.Value = Viewport.AudioGain(bus) * 100.0f;
            _refreshingAudio = false;
            StatusText.Text = $"No se pudo guardar volumen: {Viewport.LastError}";
            return;
        }
        StatusText.Text = $"Volumen guardado · {slider.Value:0}%";
    }

    private void VisualProfile_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (Viewport is null || !Viewport.IsNativeReady || VisualProfile.SelectedIndex < 0 ||
            VisualProfile.SelectedIndex == Viewport.VisualMode)
            return;
        if (!Viewport.TrySetVisualMode(VisualProfile.SelectedIndex))
        {
            VisualProfile.SelectedIndex = Viewport.VisualMode;
            StatusText.Text = $"No se pudo guardar perfil visual: {Viewport.LastError}";
            return;
        }
        StatusText.Text = VisualProfile.SelectedIndex == 0
            ? "Perfil limpio guardado" : "Perfil retro guardado";
    }

    private void RefreshSelection()
    {
        if (!Viewport.IsNativeReady) return;
        RefreshDocument();
        ConfigureGizmo();
    }

    private void RefreshSelectionFields()
    {
        IReadOnlyList<string> selection = Viewport.SelectedUuids();
        string selected = Viewport.SelectedUuid;
        string? roomPiece = selected == "Ninguno" ? null : Viewport.RoomPieceLabel(selected);
        bool editable = selection.Count > 0 && selection.All(id =>
            Viewport.RoomPieceLabel(id) is null);
        SelectedLabel.Text = selection.Count > 1
            ? $"{selection.Count} objetos seleccionados" : selected == "Ninguno"
            ? "Ningún objeto seleccionado" : roomPiece is null
                ? $"UUID {selected}" : $"{roomPiece} · plantilla fija";
        SelectedLabel.ToolTip = roomPiece is null ? null : selected;
        float[] position = [], rotation = [], scale = [];
        bool hasTransform = selection.Count == 1 && selected != "Ninguno" &&
            Viewport.TryGetSelectedTransform(out position, out rotation, out scale);
        TransformPanel.IsEnabled = hasTransform && editable && !Viewport.IsPlaying;
        DuplicateButton.IsEnabled = editable && !Viewport.IsPlaying;
        DeleteButton.IsEnabled = editable && !Viewport.IsPlaying;
        ReparentButton.IsEnabled = editable && !Viewport.IsPlaying;
        if (!hasTransform)
        {
            PositionX.Text = PositionY.Text = PositionZ.Text = string.Empty;
            RotationX.Text = RotationY.Text = RotationZ.Text = string.Empty;
            ScaleX.Text = ScaleY.Text = ScaleZ.Text = string.Empty;
            return;
        }
        _displayedPosition = (float[])position.Clone();
        _displayedRotation = (float[])rotation.Clone();
        _displayedScale = (float[])scale.Clone();
        Vector3 angles = ToEulerDegrees(rotation);
        _displayedEuler = angles;
        PositionX.Text = Format(position[0]); PositionY.Text = Format(position[1]);
        PositionZ.Text = Format(position[2]);
        RotationX.Text = Format(angles.X); RotationY.Text = Format(angles.Y);
        RotationZ.Text = Format(angles.Z);
        ScaleX.Text = Format(scale[0]); ScaleY.Text = Format(scale[1]);
        ScaleZ.Text = Format(scale[2]);
        TextBox[] fields = [PositionX, PositionY, PositionZ,
            RotationX, RotationY, RotationZ, ScaleX, ScaleY, ScaleZ];
        for (int index = 0; index < fields.Length; ++index)
            _displayedTexts[index] = fields[index].Text;
        FieldError.Text = string.Empty;
    }

    private void RefreshDirty()
    {
        bool dirty = Viewport.IsDocumentDirty;
        DirtyMark.Visibility = dirty ? Visibility.Visible : Visibility.Collapsed;
        Title = $"VESTIGIO Studio — {LevelTitle.Text}{(dirty ? " *" : "")}";
        LevelPathText.Text = ActiveLevelPath;
        LevelPathText.ToolTip = ActiveLevelPath;
    }

    private static string Format(float value) =>
        value.ToString("0.####", CultureInfo.CurrentCulture);

    private void SetEditingEnabled(bool enabled)
    {
        IReadOnlyList<string> selection = Viewport.SelectedUuids();
        bool editableSelection = selection.Count > 0 && selection.All(id =>
            Viewport.RoomPieceLabel(id) is null);
        PlayButton.IsEnabled = enabled;
        StopButton.IsEnabled = !enabled;
        CameraMode.IsEnabled = enabled;
        FrameButton.IsEnabled = enabled;
        AddButton.IsEnabled = enabled;
        AddRoomButton.IsEnabled = enabled;
        DuplicateButton.IsEnabled = enabled && editableSelection;
        DeleteButton.IsEnabled = enabled && editableSelection;
        ReparentButton.IsEnabled = enabled && editableSelection;
        ParentTarget.IsEnabled = enabled;
        GizmoTool.IsEnabled = enabled;
        GizmoAxis.IsEnabled = enabled;
        GizmoSpace.IsEnabled = enabled;
        GizmoPivot.IsEnabled = enabled;
        GizmoSnap.IsEnabled = enabled;
        UndoButton.IsEnabled = enabled;
        RedoButton.IsEnabled = enabled;
        SaveButton.IsEnabled = enabled;
        ReopenButton.IsEnabled = enabled;
        EntityList.IsEnabled = enabled;
        TransformPanel.IsEnabled = enabled && editableSelection;
    }

    private void Play_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TrySetPlaying(true))
        {
            StatusText.Text = "No se pudo iniciar la prueba 3D";
            return;
        }
        SetEditingEnabled(false);
        StatusText.Text = Viewport.EnableAudio && Viewport.AudioDeviceState != 1
            ? "Probar · audio no disponible; puedes seguir jugando"
            : "Probar · instancia aislada del documento";
    }

    private void Stop_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TrySetPlaying(false))
        {
            StatusText.Text = "No se pudo detener la prueba 3D";
            return;
        }
        SetEditingEnabled(true);
        RefreshDocument();
        StatusText.Text = "Editar · documento sin cambios por la prueba";
    }

    private void Add_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TryAddMesh())
        {
            ShowNativeError("No se pudo añadir el pilar");
            return;
        }
        _entityLabels[Viewport.SelectedUuid] = "Pilar nuevo";
        RefreshDocument();
        StatusText.Text = "Pilar añadido · guardar como para conservarlo";
    }

    private void AddRoom_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TryAddRoom())
        {
            ShowNativeError("No se pudo añadir la habitación");
            return;
        }
        FieldError.Text = string.Empty;
        RefreshDocument();
        StatusText.Text = "Habitación junto al punto inicial añadida · una por nivel · guardar para conservarla";
    }

    private void Duplicate_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TryDuplicateSelection())
        {
            ShowNativeError("No se pudo duplicar la selección");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Selección duplicada · Deshacer disponible";
    }

    private void Delete_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TryDeleteSelection())
        {
            ShowNativeError("No se pudo borrar la selección");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Selección borrada · Deshacer disponible";
    }

    private void Reparent_Click(object sender, RoutedEventArgs e)
    {
        string parent = (ParentTarget.SelectedItem as ComboBoxItem)?.Tag as string ?? "";
        if (!Viewport.TryReparentSelection(parent))
        {
            ShowNativeError("No se pudo cambiar el padre");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Jerarquía actualizada · Deshacer disponible";
    }

    private void Undo_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TryUndo())
        {
            ShowNativeError("No hay cambio que deshacer");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Cambio deshecho";
    }

    private void Redo_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TryRedo())
        {
            ShowNativeError("No hay cambio que rehacer");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Cambio rehecho";
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        string path;
        if (_savedWorkingCopy)
            path = ActiveLevelPath;
        else
        {
            var dialog = new SaveFileDialog
            {
                Title = "Guardar copia de trabajo VESTIGIO",
                Filter = "Nivel VESTIGIO (*.level.json)|*.level.json|JSON (*.json)|*.json",
                FileName = Path.GetFileNameWithoutExtension(_sourceLevelPath) +
                           "-editado.level.json",
                InitialDirectory = Path.GetDirectoryName(_sourceLevelPath)
            };
            if (dialog.ShowDialog(this) != true) return;
            path = dialog.FileName;
            if (Path.GetFullPath(path).Equals(_sourceLevelPath,
                    StringComparison.OrdinalIgnoreCase))
            {
                FieldError.Text = "Elige otra ruta para conservar intacto el Atrium de ejemplo.";
                return;
            }
        }
        if (TrySaveToPath(path))
            StatusText.Text = $"Guardado · {Path.GetFileName(path)}";
        else
            ShowNativeError("No se pudo guardar el nivel");
    }

    internal bool TrySaveToPath(string path)
    {
        if (!Viewport.TrySaveLevel(path)) return false;
        _savedWorkingCopy = true;
        SaveButton.Content = "Guardar";
        FieldError.Text = string.Empty;
        RefreshDirty();
        return true;
    }

    private void Reopen_Click(object sender, RoutedEventArgs e)
    {
        if (Viewport.IsDocumentDirty &&
            MessageBox.Show(this, "Hay cambios sin guardar. ¿Descartarlos y reabrir el archivo?",
                "Reabrir nivel", MessageBoxButton.YesNo, MessageBoxImage.Warning) !=
            MessageBoxResult.Yes)
            return;
        if (!TryReopen())
            ShowNativeError("No se pudo reabrir el nivel");
        else
            StatusText.Text = $"Reabierto · {Path.GetFileName(ActiveLevelPath)}";
    }

    internal bool TryReopen()
    {
        if (!Viewport.TryReopenLevel()) return false;
        LoadEntityLabels(ActiveLevelPath);
        CameraMode.SelectedIndex = 0;
        RefreshDocument();
        return true;
    }

    private void EntityList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_refreshingHierarchy)
            return;
        List<string> uuids = EntityList.SelectedItems.OfType<ListBoxItem>()
            .Select(item => item.Tag as string).OfType<string>().ToList();
        // El último elemento pulsado es el activo para pivot "Activo".
        if (e.AddedItems.OfType<ListBoxItem>().LastOrDefault()?.Tag is string active)
        {
            uuids.Remove(active);
            uuids.Add(active);
        }
        if (!Viewport.TrySelectMany(uuids))
        {
            ShowNativeError("No se pudo seleccionar los objetos");
            return;
        }
        RefreshSelectionFields();
        ConfigureGizmo();
    }

    private void GizmoOptions_Changed(object sender, SelectionChangedEventArgs e) =>
        ConfigureGizmo();

    private void GizmoSnap_LostFocus(object sender, RoutedEventArgs e) =>
        ConfigureGizmo();

    private void ConfigureGizmo()
    {
        if (Viewport is null || !Viewport.IsNativeReady || Viewport.IsPlaying ||
            GizmoTool is null || GizmoAxis is null || GizmoSpace is null ||
            GizmoPivot is null || GizmoSnap is null)
            return;
        string raw = GizmoSnap.Text.Trim();
        if ((!float.TryParse(raw, NumberStyles.Float, CultureInfo.CurrentCulture,
                 out float snap) &&
             !float.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture,
                 out snap)) || !float.IsFinite(snap) || snap < 0)
        {
            FieldError.Text = "Paso: introduce un número positivo o 0 para desactivar el ajuste.";
            return;
        }
        if (Viewport.TrySetGizmoOptions(GizmoTool.SelectedIndex,
                GizmoAxis.SelectedIndex, GizmoSpace.SelectedIndex,
                GizmoPivot.SelectedIndex, snap))
            FieldError.Text = string.Empty;
    }

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape && Viewport.IsGestureActive)
        {
            Viewport.TryEndGesture(false);
            e.Handled = true;
        }
    }

    private void ApplyTransform_Click(object sender, RoutedEventArgs e)
    {
        if (!TryReadTransform(out float[] position, out float[] rotation,
                out float[] scale))
            return;
        if (!Viewport.TrySetSelectedTransform(position, rotation, scale))
        {
            ShowNativeError("No se pudo aplicar la transformación");
            return;
        }
        FieldError.Text = string.Empty;
        RefreshDirty();
        RefreshSelectionFields();
        StatusText.Text = "Transformación aplicada · Deshacer disponible";
    }

    private bool TryReadTransform(out float[] position, out float[] rotation,
        out float[] scale)
    {
        position = new float[3]; rotation = new float[4]; scale = new float[3];
        TextBox[] fields = [PositionX, PositionY, PositionZ,
            RotationX, RotationY, RotationZ, ScaleX, ScaleY, ScaleZ];
        float[] values = new float[fields.Length];
        for (int index = 0; index < fields.Length; ++index)
        {
            string raw = fields[index].Text.Trim();
            if ((!float.TryParse(raw, NumberStyles.Float, CultureInfo.CurrentCulture,
                     out values[index]) &&
                 !float.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture,
                     out values[index])) || !float.IsFinite(values[index]))
            {
                FieldError.Text = $"{fields[index].ToolTip}: introduce un número válido.";
                fields[index].Focus();
                return false;
            }
        }
        if (values[6] <= 0.001f || values[7] <= 0.001f || values[8] <= 0.001f)
        {
            FieldError.Text = "La escala X, Y y Z debe ser mayor que 0,001.";
            return false;
        }
        for (int index = 0; index < 3; ++index)
        {
            if (fields[index].Text == _displayedTexts[index])
                values[index] = _displayedPosition[index];
            if (fields[index + 6].Text == _displayedTexts[index + 6])
                values[index + 6] = _displayedScale[index];
        }
        Array.Copy(values, position, 3);
        Array.Copy(values, 6, scale, 0, 3);
        if (fields[3].Text == _displayedTexts[3] &&
            fields[4].Text == _displayedTexts[4] &&
            fields[5].Text == _displayedTexts[5])
            Array.Copy(_displayedRotation, rotation, 4);
        else
        {
            float x = fields[3].Text == _displayedTexts[3]
                ? _displayedEuler.X : values[3];
            float y = fields[4].Text == _displayedTexts[4]
                ? _displayedEuler.Y : values[4];
            float z = fields[5].Text == _displayedTexts[5]
                ? _displayedEuler.Z : values[5];
            Quaternion q = Quaternion.CreateFromYawPitchRoll(ToRadians(y),
                ToRadians(x), ToRadians(z));
            rotation[0] = q.X; rotation[1] = q.Y; rotation[2] = q.Z;
            rotation[3] = q.W;
        }
        return true;
    }

    private static float ToRadians(float degrees) => degrees * MathF.PI / 180f;

    private static Vector3 ToEulerDegrees(float[] values)
    {
        Quaternion q = Quaternion.Normalize(new Quaternion(values[0], values[1],
            values[2], values[3]));
        float x = MathF.Asin(Math.Clamp(2f * (q.W * q.X - q.Y * q.Z), -1f, 1f));
        float y = MathF.Atan2(2f * (q.W * q.Y + q.X * q.Z),
            1f - 2f * (q.X * q.X + q.Y * q.Y));
        float z = MathF.Atan2(2f * (q.W * q.Z + q.X * q.Y),
            1f - 2f * (q.X * q.X + q.Z * q.Z));
        return new Vector3(x, y, z) * (180f / MathF.PI);
    }

    private void ShowNativeError(string prefix)
    {
        string detail = string.IsNullOrWhiteSpace(Viewport.LastError)
            ? prefix : $"{prefix}: {Viewport.LastError}";
        FieldError.Text = detail;
        StatusText.Text = detail;
    }

    private void ConfirmClose(object? sender, CancelEventArgs e)
    {
        if (!Viewport.IsDocumentDirty) return;
        MessageBoxResult choice = MessageBox.Show(this,
            "Hay cambios sin guardar. ¿Guardar una copia antes de cerrar?",
            "Cambios de VESTIGIO", MessageBoxButton.YesNoCancel,
            MessageBoxImage.Warning);
        if (choice == MessageBoxResult.Cancel)
            e.Cancel = true;
        else if (choice == MessageBoxResult.Yes)
        {
            Save_Click(this, new RoutedEventArgs());
            e.Cancel = Viewport.IsDocumentDirty;
        }
    }

    private void CameraMode_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (Viewport is null || sender is not ComboBox { SelectedIndex: >= 0 } selector)
            return;
        if (!Viewport.SetCameraMode(selector.SelectedIndex))
            StatusText.Text = "No se pudo cambiar la cámara 3D";
        else
            StatusText.Text = $"Editar · {((ComboBoxItem)selector.SelectedItem).Content}";
    }

    private void FrameSelection_Click(object sender, RoutedEventArgs e)
    {
        StatusText.Text = Viewport.FrameSelection()
            ? "Objeto seleccionado encuadrado"
            : "Selecciona un objeto en el viewport antes de encuadrar";
    }
}
