using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using Microsoft.Win32;

namespace Vestigio.Studio;

public partial class Vestigio3DWindow
{
    private bool _unsaved;
    private bool _compact;
    private bool _short;
    private void ApplyCompactLayout(FrameworkElement content)
    {
        bool compact = content.ActualWidth < 1100;
        bool shortView = content.ActualHeight < 630;
        if (compact != _compact)
        {
            HierarchyWidth.Width = new GridLength(compact ? 150 : 210);
            InspectorWidth.Width = new GridLength(compact ? 280 : 330);
            VisualSummaryText.Visibility = compact ? Visibility.Collapsed : Visibility.Visible;
            RotationScaleExpander.IsExpanded = !compact;
            _compact = compact;
        }
        if (shortView != _short)
        {
            ResourcesHeight.Height = new GridLength(shortView ? 140 : 180);
            _short = shortView;
        }
    }
    internal Func<MessageBoxResult>? LeaveChoiceForTest { get; set; }
    internal Func<string?>? SavePathForTest { get; set; }

    internal void MarkUnsaved(bool value)
    {
        _unsaved = value;
        _savedWorkingCopy = !value && !LevelFiles.IsExample(ActiveLevelPath);
        if (Viewport.IsNativeReady) RefreshDirty();
    }

    private bool CanLeaveDocument()
    {
        if (!_unsaved && !Viewport.IsDocumentDirty) return true;
        MessageBoxResult choice = LeaveChoiceForTest?.Invoke() ?? MessageBox.Show(this,
            $"{LevelTitle.Text} tiene cambios sin guardar.\n\nSí: guardar · No: descartar · Cancelar: seguir editando.",
            "Guardar antes de continuar", MessageBoxButton.YesNoCancel, MessageBoxImage.Warning);
        return choice == MessageBoxResult.No || (choice == MessageBoxResult.Yes && SaveDocument(false));
    }

    private bool SaveDocument(bool saveAs)
    {
        if (Viewport.IsPlaying || !Viewport.IsNativeReady) return false;
        string? path = !saveAs && _savedWorkingCopy ? ActiveLevelPath : null;
        if (path is null)
        {
            if (SavePathForTest is not null) path = SavePathForTest();
            else
            {
                var dialog = new SaveFileDialog { Title = "Guardar nivel VESTIGIO",
                    Filter = "Nivel VESTIGIO (*.level.json)|*.level.json", DefaultExt = ".level.json",
                    FileName = _unsaved || LevelFiles.IsExample(ActiveLevelPath)
                        ? LevelTitle.Text + ".level.json" : Path.GetFileName(ActiveLevelPath),
                    InitialDirectory = _unsaved || LevelFiles.IsExample(ActiveLevelPath)
                        ? Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)
                        : Path.GetDirectoryName(ActiveLevelPath) };
                if (dialog.ShowDialog(this) != true) return false;
                path = dialog.FileName;
            }
        }
        if (string.IsNullOrWhiteSpace(path)) return false;
        if (!TrySaveToPath(path)) { ShowNativeError("No se pudo guardar el nivel"); return false; }
        StatusText.Text = $"Guardado · {Path.GetFileName(path)}";
        return true;
    }

    internal bool SwitchLevel(string path, bool unsaved = false)
    {
        if (Viewport.IsPlaying || !CanLeaveDocument()) return false;
        if (!Viewport.TryLoadLevel(Path.GetFullPath(path))) { ShowNativeError("No se pudo abrir el nivel"); return false; }
        _sourceLevelPath = Path.GetFullPath(path);
        _previewedAssetId = ""; _roomPreviewActive = false;
        PreviewAssetButton.Content = "Vista previa";
        _editingRoomId = ""; _loadedRoomRecipe = "";
        _entityLabels.Clear(); LoadEntityLabels(path);
        MarkUnsaved(unsaved);
        LevelTitle.Text = DocumentName(path);
        CameraMode.SelectedIndex = 0;
        ProblemsList.Items.Clear(); ProblemsHint.Text = "Sin problemas detectados."; ProblemsTab.Header = "Problemas";
        ResetRoomDraft();
        RefreshDocument();
        InspectorTabs.SelectedIndex = 0;
        StatusText.Text = unsaved ? "Nivel nuevo · construye tu primera habitación" : "Nivel abierto · listo para editar";
        return true;
    }

    private void New_Click(object sender, RoutedEventArgs e)
    {
        if (Viewport.IsPlaying) return;
        string? name = NewLevelDialog.GetName(this);
        if (name is not null) _ = SwitchLevel(LevelFiles.CreateNew(name), true);
    }

    private void Open_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog { Title = "Abrir nivel VESTIGIO",
            Filter = "Nivel VESTIGIO (*.level.json;*.json)|*.level.json;*.json" };
        if (dialog.ShowDialog(this) == true) _ = SwitchLevel(dialog.FileName);
    }

    private void Example_Click(object sender, RoutedEventArgs e) => _ = SwitchLevel(GpuViewportHost.ResolveDemoAsset("atrium.level.json"));
    private void SaveAs_Click(object sender, RoutedEventArgs e) => _ = SaveDocument(true);
    private void Close_Click(object sender, RoutedEventArgs e) => Close();
    private void Inspector_Click(object sender, RoutedEventArgs e) => InspectorTabs.SelectedIndex = 0;
    private void Resources_Click(object sender, RoutedEventArgs e) => ResourcesTabs.SelectedIndex = 0;
    private void ImportShortcut_Click(object sender, RoutedEventArgs e) { ResourcesTabs.SelectedIndex = 0; ImportAsset_Click(sender, e); }
    private void ResetLayout_Click(object sender, RoutedEventArgs e)
    {
        HierarchyWidth.Width = new GridLength(210); InspectorWidth.Width = new GridLength(330);
        ResourcesHeight.Height = new GridLength(180);
    }

    private void SelectSpawn_Click(object sender, RoutedEventArgs e)
    {
        string? id = _entityLabels.FirstOrDefault(entry => entry.Value == "Inicio del jugador").Key;
        if (id is not null) { _ = Viewport.TrySelectMany(new[] { id }); RefreshDocument(); InspectorTabs.SelectedIndex = 0; }
    }

    private void Help_Click(object sender, RoutedEventArgs e) => MessageBox.Show(this,
        "1. Habitación: traza un rectángulo, añade aberturas y pulsa Crear.\n2. Importar modelo: GLB/glTF; selecciona Colocar en Recursos.\n3. Selecciona en la escena o jerarquía; ajusta Transformación.\n4. Guarda y usa Probar / Detener (F5).\n\nNavegar: botón derecho + WASD, Espacio/Ctrl para subir/bajar.\nF: encuadrar selección. Ctrl+Z/Y: deshacer/rehacer.\nCtrl+N/O/S: nuevo/abrir/guardar. Ctrl+Mayús+S: guardar como.\nEscape: cancelar gesto o volver del modo Probar.\nEl inicio del jugador es independiente de la cámara de edición.",
        "Crear tu primer nivel", MessageBoxButton.OK, MessageBoxImage.Information);

    private void NewRoom_Click(object sender, RoutedEventArgs e)
    {
        if (_roomPreviewActive) _ = Viewport.TryCancelRoomPreview();
        _roomPreviewActive = false;
        _ = Viewport.TrySelectMany(Array.Empty<string>());
        ResetRoomDraft(); RefreshDocument(); OpenRoomEditor_Click(sender, e);
    }

    private void ResetRoomDraft()
    {
        _editingRoomId = ""; _loadedRoomRecipe = "";
        RoomRectX.Text = "-2"; RoomRectY.Text = "-3"; RoomRectWidth.Text = "4"; RoomRectDepth.Text = "4";
        RoomFloor.Text = "0"; RoomHeight.Text = "3"; RoomThickness.Text = "0.2";
        RoomOpenings.Items.Clear(); RoomRect_Click(this, new RoutedEventArgs());
        CommitRoomButton.Content = "Crear habitación";
    }

    private bool HandleShortcut(Key key, ModifierKeys modifiers)
    {
        if (key == Key.Escape && _roomPreviewActive && !Viewport.IsPlaying)
        {
            CancelRoomPreview_Click(this, new RoutedEventArgs()); return true;
        }
        if (key == Key.F5 || key == Key.Escape && Viewport.IsPlaying)
        {
            if (Viewport.IsPlaying) Stop_Click(this, new RoutedEventArgs());
            else Play_Click(this, new RoutedEventArgs());
            return true;
        }
        if (Viewport.IsPlaying) return false;
        bool ctrl = modifiers.HasFlag(ModifierKeys.Control);
        bool typing = Keyboard.FocusedElement is TextBox || Keyboard.FocusedElement is System.Windows.Controls.Primitives.TextBoxBase;
        if (ctrl && key == Key.N) New_Click(this, new RoutedEventArgs());
        else if (ctrl && key == Key.O) Open_Click(this, new RoutedEventArgs());
        else if (ctrl && key == Key.S) _ = SaveDocument(modifiers.HasFlag(ModifierKeys.Shift));
        else if (typing) return false;
        else if (ctrl && key == Key.Z) Undo_Click(this, new RoutedEventArgs());
        else if (ctrl && key == Key.Y) Redo_Click(this, new RoutedEventArgs());
        else if (ctrl && key == Key.D) Duplicate_Click(this, new RoutedEventArgs());
        else if (key == Key.Delete) Delete_Click(this, new RoutedEventArgs());
        else if (key == Key.F) FrameSelection_Click(this, new RoutedEventArgs());
        else return false;
        return true;
    }
}
