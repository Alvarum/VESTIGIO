using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Globalization;
using System.Windows;
using System.Windows.Input;

namespace RetroForge.Studio;

internal sealed class StudioViewModel : ObservableObject, IDisposable
{
    private IEditorItem? _selectedItem;
    private string _workspace = "Construir";
    private string _status = "Proyecto cargado y validado";
    private string _searchText = string.Empty;
    private bool _isPlaying;

    public StudioViewModel(EditorDocument document)
    {
        Document = document;
        Document.Changed += DocumentChanged;
        SaveCommand = new RelayCommand(Save);
        UndoCommand = new RelayCommand(Undo, () => Document.Overview.CanUndo && !IsPlaying);
        RedoCommand = new RelayCommand(Redo, () => Document.Overview.CanRedo && !IsPlaying);
        PlayCommand = new RelayCommand(Play, () => !IsPlaying);
        StopCommand = new RelayCommand(Stop, () => IsPlaying);
        SelectCommand = new RelayCommand<IEditorItem>(item => SelectedItem = item);
        WorkspaceCommand = new RelayCommand<string>(name => Workspace = name ?? "Construir");
        ApplyFieldCommand = new RelayCommand<InspectorEdit>(ApplyField);
        RefreshPresentation();
    }

    public EditorDocument Document { get; }
    public ProjectOverview Overview => Document.Overview;
    public ObservableCollection<SceneGroup> SceneGroups { get; } = [];
    public ObservableCollection<InspectorField> InspectorFields { get; } = [];
    public ObservableCollection<string> Problems { get; } = [];
    public ObservableCollection<ResourceItem> Resources { get; } = [];

    public ICommand SaveCommand { get; }
    public RelayCommand UndoCommand { get; }
    public RelayCommand RedoCommand { get; }
    public RelayCommand PlayCommand { get; }
    public RelayCommand StopCommand { get; }
    public ICommand SelectCommand { get; }
    public ICommand WorkspaceCommand { get; }
    public ICommand ApplyFieldCommand { get; }

    public IEditorItem? SelectedItem
    {
        get => _selectedItem;
        set
        {
            if (!Set(ref _selectedItem, value))
                return;
            BuildInspector();
            Raise(nameof(SelectionTitle));
            Raise(nameof(SelectionSubtitle));
        }
    }

    public string SelectionTitle => SelectedItem?.DisplayName ?? "Nada seleccionado";
    public string SelectionSubtitle => SelectedItem?.Subtitle ?? "Selecciona un elemento en la escena o en el mapa.";

    public string Workspace
    {
        get => _workspace;
        set
        {
            if (Set(ref _workspace, value))
                Status = $"Espacio {value}";
        }
    }

    public string Status
    {
        get => _status;
        private set => Set(ref _status, value);
    }

    public string SearchText
    {
        get => _searchText;
        set => Set(ref _searchText, value);
    }

    public bool IsPlaying
    {
        get => _isPlaying;
        private set
        {
            if (!Set(ref _isPlaying, value))
                return;
            Raise(nameof(EditEnabled));
            PlayCommand.Notify();
            StopCommand.Notify();
            UndoCommand.Notify();
            RedoCommand.Notify();
        }
    }

    public bool EditEnabled => !IsPlaying;
    public string DirtyMark => Overview.Dirty ? "●" : string.Empty;

    private void DocumentChanged(object? sender, EventArgs args) => RefreshPresentation();

    private void RefreshPresentation()
    {
        SceneGroups.Clear();
        SceneGroups.Add(new SceneGroup("Habitaciones", "▱", Document.Sectors));
        SceneGroups.Add(new SceneGroup("Objetos y personajes", "◇", Document.Markers));
        SceneGroups.Add(new SceneGroup("Puertas y ventanas", "□", Document.Barriers));
        SceneGroups.Add(new SceneGroup("Zonas de evento", "⌁", Document.Triggers));
        SceneGroups.Add(new SceneGroup("Iluminación", "✦", Document.Lights));
        SceneGroups.Add(new SceneGroup("Definiciones", "◉", Document.Characters));
        SceneGroups.Add(new SceneGroup("Reglas", "↳", Document.Rules));
        SceneGroups.Add(new SceneGroup("Conversaciones", "☰", Document.Dialogues));

        Resources.Clear();
        string[] known = Directory.Exists(Overview.Root)
            ? Directory.EnumerateFiles(Overview.Root, "*", SearchOption.AllDirectories)
                .Where(path => path.EndsWith(".png", StringComparison.OrdinalIgnoreCase) ||
                               path.EndsWith(".wav", StringComparison.OrdinalIgnoreCase) ||
                               path.EndsWith(".actor", StringComparison.OrdinalIgnoreCase))
                .OrderBy(path => path).ToArray()
            : [];
        foreach (string path in known)
            Resources.Add(new ResourceItem(Path.GetFileNameWithoutExtension(path),
                Path.GetExtension(path).TrimStart('.').ToUpperInvariant(), path));

        Problems.Clear();
        if (Document.Sectors.Count == 0)
            Problems.Add("El nivel no contiene habitaciones transitables.");
        if (!Document.Markers.Any(marker => marker.MarkerKind == "player"))
            Problems.Add("Falta un punto de aparición para el jugador.");
        if (Problems.Count == 0)
            Problems.Add("Sin errores de validación.");

        if (SelectedItem is not null)
            SelectedItem = FindCurrent(SelectedItem);
        BuildInspector();
        Raise(nameof(Overview));
        Raise(nameof(DirtyMark));
        Raise(nameof(SelectionTitle));
        Raise(nameof(SelectionSubtitle));
        UndoCommand.Notify();
        RedoCommand.Notify();
    }

    private IEditorItem? FindCurrent(IEditorItem old) => old.Kind switch
    {
        EditorNative.ObjectKind.Sector => Document.Sectors.ElementAtOrDefault((int)old.Index),
        EditorNative.ObjectKind.Marker => Document.Markers.ElementAtOrDefault((int)old.Index),
        EditorNative.ObjectKind.Barrier => Document.Barriers.ElementAtOrDefault((int)old.Index),
        EditorNative.ObjectKind.Character => Document.Characters.ElementAtOrDefault((int)old.Index),
        EditorNative.ObjectKind.Rule => Document.Rules.ElementAtOrDefault((int)old.Index),
        EditorNative.ObjectKind.Dialogue => Document.Dialogues.ElementAtOrDefault((int)old.Index),
        EditorNative.ObjectKind.Trigger => Document.Triggers.ElementAtOrDefault((int)old.Index),
        EditorNative.ObjectKind.Light => Document.Lights.ElementAtOrDefault((int)old.Index),
        _ => null
    };

    private void BuildInspector()
    {
        InspectorFields.Clear();
        switch (SelectedItem)
        {
            case SectorModel sector:
                Add("Cota del suelo", "floor", sector.Floor, "m", "Altura absoluta de la planta.");
                Add("Altura del techo", "ceiling", sector.Ceiling, "m");
                Add("Luz ambiental", "light", sector.Light, "0–1");
                Add("Material de pared", "wall_material", sector.WallMaterial);
                Add("Material de suelo", "floor_material", sector.FloorMaterial);
                Add("Material de techo", "ceiling_material", sector.CeilingMaterial);
                break;
            case MarkerModel marker:
                Add("Identificador", "id", marker.Id);
                Add("Definición compartida", "definition", marker.Definition);
                Add("Posición X", "x", marker.X, "m");
                Add("Posición Y", "y", marker.Y, "m");
                Add("Posición Z", "z", marker.Z, "m");
                Add("Orientación", "yaw", marker.Yaw, "°");
                break;
            case BarrierModel barrier:
                Add("Identificador", "id", barrier.Id);
                Add("Apertura", "open_fraction", barrier.OpenFraction, "0–1");
                Add("Resistencia", "health", barrier.Health, "HP");
                break;
            case CharacterModel character:
                Add("Identificador", "id", character.Id);
                Add("Hoja de sprites", "sprite", character.Native.Sprite);
                Add("Ancho de celda", "cell_width", character.Native.CellWidth, "px");
                Add("Alto de celda", "cell_height", character.Native.CellHeight, "px");
                Add("Velocidad", "speed", character.Native.Speed, "m/s");
                Add("Distancia de visión", "sight_range", character.Native.SightRange, "m");
                Add("Vida máxima", "max_health", character.Native.MaxHealth);
                Add("Daño", "attack_damage", character.Native.AttackDamage);
                Add("Invulnerable", "invulnerable", character.Native.Invulnerable != 0);
                Add("Captura causa derrota", "capture_game_over", character.Native.CaptureGameOver != 0);
                break;
            case RuleModel rule:
                Add("Identificador", "id", rule.Id);
                Add("Origen del evento", "source", rule.Source);
                Add("Prioridad", "priority", rule.Native.Priority);
                Add("Cooldown", "cooldown", rule.Native.Cooldown, "s");
                Add("Ejecutar una vez", "once", rule.Native.Once != 0);
                break;
            case DialogueModel dialogue:
                Add("Identificador", "id", dialogue.Id);
                Add("Hablante", "speaker", dialogue.Native.Speaker);
                Add("Texto", "text", dialogue.Text);
                Add("Siguiente nodo", "next", dialogue.Next);
                Add("Pausar el mundo", "pauses_world", dialogue.Native.PausesWorld != 0);
                break;
        }
    }

    private void Add(string label, string property, object value, string suffix = "", string help = "")
    {
        string text = value switch
        {
            bool boolean => boolean ? "true" : "false",
            float number => number.ToString("0.###", CultureInfo.InvariantCulture),
            _ => Convert.ToString(value, CultureInfo.InvariantCulture) ?? string.Empty
        };
        InspectorFields.Add(new InspectorField(label, property, text, suffix, value is bool, help));
    }

    private void ApplyField(InspectorEdit? edit)
    {
        if (edit is null || SelectedItem is null || IsPlaying)
            return;
        try
        {
            Document.SetProperty(SelectedItem.Kind, SelectedItem.Index, edit.Field.Property, edit.Value);
            Status = $"{edit.Field.Label} actualizado";
        }
        catch (Exception exception)
        {
            Status = exception.Message;
            MessageBox.Show(exception.Message, "Valor no válido", MessageBoxButton.OK, MessageBoxImage.Warning);
        }
    }

    private void Save()
    {
        try
        {
            Document.Save();
            Status = "Todos los archivos del proyecto fueron guardados";
        }
        catch (Exception exception)
        {
            Status = exception.Message;
            MessageBox.Show(exception.Message, "No se pudo guardar", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void Undo()
    {
        if (Document.Undo())
            Status = "Cambio deshecho";
    }

    private void Redo()
    {
        if (Document.Redo())
            Status = "Cambio rehecho";
    }

    private void Play()
    {
        if (Overview.Dirty)
            Save();
        string player = Path.Combine(AppContext.BaseDirectory, "retro_player.exe");
        if (!File.Exists(player))
        {
            Status = "Compila retro_player para iniciar la prueba";
            return;
        }
        Process.Start(new ProcessStartInfo(player)
        {
            UseShellExecute = false,
            ArgumentList = { "--project", Overview.Manifest }
        });
        IsPlaying = true;
        Status = "Prueba iniciada en Player con este proyecto";
    }

    private void Stop()
    {
        IsPlaying = false;
        Status = "Edición reactivada; la prueba no modificó el documento";
    }

    public void Dispose()
    {
        Document.Changed -= DocumentChanged;
        Document.Dispose();
    }
}

internal sealed record InspectorEdit(InspectorField Field, string Value);
internal sealed record ResourceItem(string Name, string Kind, string Path);
