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
    private string _mapTool = "Seleccionar";
    private float _floorElevation, _roomHeight = 3;
    public string[] MapTools { get; } = ["Seleccionar", "Habitación", "Puerta", "Ventana"];
    public string MapTool { get => _mapTool; set { Set(ref _mapTool, value); Raise(nameof(MapHelp)); } }
    public string MapHelp => MapTool switch
    {
        "Habitación" => "Arrastra un rectángulo · mínimo 1 × 1 m · Esc: cancelar",
        "Puerta" or "Ventana" => "Haz clic en la pared compartida entre dos habitaciones",
        _ => "Arrastra personajes desde la biblioteca · clic: seleccionar · arrastrar: mover"
    };
    public float FloorElevation { get => _floorElevation; set => Set(ref _floorElevation, value); }
    public float RoomHeight { get => _roomHeight; set => Set(ref _roomHeight, value); }

    public void CreateRoom(Point a, Point b)
    {
        if (IsPlaying) return;
        try {
            SelectedItem = Document.CreateRoom((float)Math.Min(a.X,b.X), (float)Math.Min(a.Y,b.Y),
                (float)Math.Max(a.X,b.X), (float)Math.Max(a.Y,b.Y), FloorElevation, RoomHeight);
            Status = "Habitación creada; las paredes compartidas compatibles se conectaron";
        } catch (Exception error) { Status = error.Message; }
    }

    public void AddBarrier(SectorModel room, int edge)
    {
        if (IsPlaying) return;
        try {
            SelectedItem = Document.AddBarrier(room.Index, (uint)edge, MapTool == "Ventana", 1.5f);
            Status = "Barrera creada; configura su apertura en el inspector";
            MapTool = "Seleccionar";
        } catch (Exception error) { Status = error.Message; }
    }
    private bool _isPlaying;
    private nint _session;
    private bool _simulationPaused, _stepRequested;
    public nint Session => _session;
    public bool SimulationPaused { get => _simulationPaused; set => Set(ref _simulationPaused, value); }
    internal bool ConsumeStep() { bool value = _stepRequested; _stepRequested = false; return value; }
    public RelayCommand PauseSessionCommand { get; }
    public RelayCommand StepSessionCommand { get; }
    private DialogueModel? _quickDialogue;
    private BarrierModel? _quickBarrier;
    private string _quickMessage = "Hola. Hay algo que debes saber.";

    public StudioViewModel(EditorDocument document)
    {
        Document = document;
        Document.Changed += DocumentChanged;
        SaveCommand = new RelayCommand(() => Save());
        UndoCommand = new RelayCommand(Undo, () => Document.Overview.CanUndo && !IsPlaying);
        RedoCommand = new RelayCommand(Redo, () => Document.Overview.CanRedo && !IsPlaying);
        PlayCommand = new RelayCommand(Play, () => !IsPlaying);
        StopCommand = new RelayCommand(Stop, () => IsPlaying);
        PauseSessionCommand = new RelayCommand(() => SimulationPaused = !SimulationPaused, () => IsPlaying);
        StepSessionCommand = new RelayCommand(() => { SimulationPaused = true; _stepRequested = true; }, () => IsPlaying);
        SelectCommand = new RelayCommand<IEditorItem>(item => SelectedItem = item);
        WorkspaceCommand = new RelayCommand<string>(name => Workspace = name ?? "Construir");
        ApplyFieldCommand = new RelayCommand<InspectorEdit>(ApplyField);
        DuplicateCommand = new RelayCommand(DuplicateSelected,
            () => SelectedItem is MarkerModel && !IsPlaying);
        DeleteCommand = new RelayCommand(DeleteSelected,
            () => SelectedItem is MarkerModel && !IsPlaying);
        AddDropCommand = new RelayCommand<string>(AddDrop,
            item => SelectedItem is MarkerModel { MarkerKind: "actor" } &&
                    Document.Items.Any(definition => definition.Id == item) && !IsPlaying);
        StartDialogueCommand = new RelayCommand(StartDialogue,
            () => SelectedItem is MarkerModel && QuickDialogue is not null && !IsPlaying);
        OpenBarrierCommand = new RelayCommand(OpenBarrier,
            () => SelectedItem is MarkerModel && QuickBarrier is not null && !IsPlaying);
        ShowMessageCommand = new RelayCommand(ShowMessage,
            () => SelectedItem is MarkerModel && !string.IsNullOrWhiteSpace(QuickMessage) && !IsPlaying);
        GiveItemCommand = new RelayCommand<string>(GiveItem,
            item => SelectedItem is MarkerModel && Document.Items.Any(definition => definition.Id == item) && !IsPlaying);
        RefreshPresentation();
    }

    public EditorDocument Document { get; }
    public ProjectOverview Overview => Document.Overview;
    public ObservableCollection<SceneGroup> SceneGroups { get; } = [];
    public ObservableCollection<InspectorField> InspectorFields { get; } = [];
    public ObservableCollection<string> Problems { get; } = [];
    public ObservableCollection<ResourceItem> Resources { get; } = [];
    public ObservableCollection<ConnectionItem> Connections { get; } = [];
    public ObservableCollection<PlacementTool> PlacementTools { get; } = [];

    public ICommand SaveCommand { get; }
    public RelayCommand UndoCommand { get; }
    public RelayCommand RedoCommand { get; }
    public RelayCommand PlayCommand { get; }
    public RelayCommand StopCommand { get; }
    public ICommand SelectCommand { get; }
    public ICommand WorkspaceCommand { get; }
    public ICommand ApplyFieldCommand { get; }
    public RelayCommand DuplicateCommand { get; }
    public RelayCommand DeleteCommand { get; }
    public RelayCommand<string> AddDropCommand { get; }
    public RelayCommand StartDialogueCommand { get; }
    public RelayCommand OpenBarrierCommand { get; }
    public RelayCommand ShowMessageCommand { get; }
    public RelayCommand<string> GiveItemCommand { get; }

    public IEditorItem? SelectedItem
    {
        get => _selectedItem;
        set
        {
            if (!Set(ref _selectedItem, value))
                return;
            BuildInspector();
            BuildConnections();
            Raise(nameof(SelectionTitle));
            Raise(nameof(SelectionSubtitle));
            Raise(nameof(SelectedCharacter));
            Raise(nameof(SelectedRule));
            Raise(nameof(SelectedDialogue));
            Raise(nameof(SelectedMarker));
            Raise(nameof(ConnectionsMessage));
            DuplicateCommand.Notify();
            DeleteCommand.Notify();
            AddDropCommand.Notify();
            StartDialogueCommand.Notify();
            OpenBarrierCommand.Notify();
            ShowMessageCommand.Notify();
            GiveItemCommand.Notify();
        }
    }

    public string SelectionTitle => SelectedItem?.DisplayName ?? "Nada seleccionado";
    public string SelectionSubtitle => SelectedItem?.Subtitle ?? "Selecciona un elemento en la escena o en el mapa.";
    public CharacterModel? SelectedCharacter => SelectedItem as CharacterModel;
    public RuleModel? SelectedRule => SelectedItem as RuleModel;
    public DialogueModel? SelectedDialogue => SelectedItem as DialogueModel;
    public MarkerModel? SelectedMarker => SelectedItem as MarkerModel;
    public string ConnectionsMessage => Connections.Count == 0
        ? "Este elemento todavía no tiene conexiones directas."
        : $"{Connections.Count} conexión{(Connections.Count == 1 ? string.Empty : "es")} encontrada{(Connections.Count == 1 ? string.Empty : "s")}.";

    public DialogueModel? QuickDialogue
    {
        get => _quickDialogue;
        set
        {
            if (Set(ref _quickDialogue, value))
                StartDialogueCommand.Notify();
        }
    }

    public BarrierModel? QuickBarrier
    {
        get => _quickBarrier;
        set
        {
            if (Set(ref _quickBarrier, value))
                OpenBarrierCommand.Notify();
        }
    }

    public string QuickMessage
    {
        get => _quickMessage;
        set
        {
            if (Set(ref _quickMessage, value))
                ShowMessageCommand.Notify();
        }
    }

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
        set
        {
            if (Set(ref _searchText, value))
                BuildSceneGroups();
        }
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
            PauseSessionCommand.Notify();
            StepSessionCommand.Notify();
            UndoCommand.Notify();
            RedoCommand.Notify();
            DuplicateCommand.Notify();
            DeleteCommand.Notify();
            AddDropCommand.Notify();
            StartDialogueCommand.Notify();
            OpenBarrierCommand.Notify();
            ShowMessageCommand.Notify();
            GiveItemCommand.Notify();
        }
    }

    public bool EditEnabled => !IsPlaying;
    public string DirtyMark => Overview.Dirty ? "●" : string.Empty;

    private void DocumentChanged(object? sender, EventArgs args) => RefreshPresentation();

    private void RefreshPresentation()
    {
        BuildSceneGroups();

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

        PlacementTools.Clear();
        foreach (CharacterModel character in Document.Characters)
            PlacementTools.Add(new PlacementTool(character.DisplayName, character.Subtitle,
                "actor", character.Id, "◉", character.Thumbnail));
        foreach (EditorNative.ItemDefinition item in Document.Items)
            PlacementTools.Add(new PlacementTool(item.Name.Replace('_', ' '), "Objeto de inventario",
                "pickup", item.Id, "◆"));
        PlacementTools.Add(new PlacementTool("Interruptor", "Objeto interactuable", "interact", "switch", "⌁"));
        if (QuickDialogue is null || !Document.Dialogues.Any(item => item.Id == QuickDialogue.Id))
            QuickDialogue = Document.Dialogues.FirstOrDefault();
        if (QuickBarrier is null || !Document.Barriers.Any(item => item.Id == QuickBarrier.Id))
            QuickBarrier = Document.Barriers.FirstOrDefault();

        Problems.Clear();
        foreach (CharacterModel character in Document.Characters.Where(item => item.ThumbnailError.Length > 0))
            Problems.Add($"{character.DisplayName}: {character.ThumbnailError}");
        if (Document.Sectors.Count == 0)
            Problems.Add("El nivel no contiene habitaciones transitables.");
        if (!Document.Markers.Any(marker => marker.MarkerKind == "player"))
            Problems.Add("Falta un punto de aparición para el jugador.");
        if (Problems.Count == 0)
            Problems.Add("Sin errores de validación.");

        if (SelectedItem is not null)
            SelectedItem = FindCurrent(SelectedItem);
        BuildInspector();
        BuildConnections();
        Raise(nameof(Overview));
        Raise(nameof(DirtyMark));
        Raise(nameof(SelectionTitle));
        Raise(nameof(SelectionSubtitle));
        Raise(nameof(SelectedCharacter));
        Raise(nameof(SelectedRule));
        Raise(nameof(SelectedDialogue));
        Raise(nameof(SelectedMarker));
        Raise(nameof(ConnectionsMessage));
        UndoCommand.Notify();
        RedoCommand.Notify();
        DuplicateCommand.Notify();
        DeleteCommand.Notify();
        AddDropCommand.Notify();
        StartDialogueCommand.Notify();
        OpenBarrierCommand.Notify();
        ShowMessageCommand.Notify();
        GiveItemCommand.Notify();
    }

    private void BuildSceneGroups()
    {
        SceneGroups.Clear();
        AddGroup("Habitaciones", "▱", Document.Sectors);
        AddGroup("Objetos y personajes", "◇", Document.Markers);
        AddGroup("Puertas y ventanas", "□", Document.Barriers);
        AddGroup("Zonas de evento", "⌁", Document.Triggers);
        AddGroup("Iluminación", "✦", Document.Lights);
        AddGroup("Definiciones", "◉", Document.Characters);
        AddGroup("Reglas", "↳", Document.Rules);
        AddGroup("Conversaciones", "☰", Document.Dialogues);
    }

    private void AddGroup<T>(string name, string symbol, IEnumerable<T> source) where T : IEditorItem
    {
        string query = SearchText.Trim();
        IEditorItem[] matches = source.Where(item => string.IsNullOrEmpty(query) ||
            item.DisplayName.Contains(query, StringComparison.CurrentCultureIgnoreCase) ||
            item.Subtitle.Contains(query, StringComparison.CurrentCultureIgnoreCase) ||
            item.Id.Contains(query, StringComparison.OrdinalIgnoreCase)).Cast<IEditorItem>().ToArray();
        if (matches.Length > 0 || string.IsNullOrEmpty(query))
            SceneGroups.Add(new SceneGroup(name, symbol, matches));
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
            case TriggerModel trigger:
                Add("Identificador", "id", trigger.Id);
                Add("Posición X", "x", trigger.Native.X, "m");
                Add("Posición Y", "y", trigger.Native.Y, "m");
                Add("Posición Z", "z", trigger.Native.Z, "m");
                Add("Ancho / semieje X", "size_x", trigger.Native.SizeX, "m");
                Add("Fondo / semieje Y", "size_y", trigger.Native.SizeY, "m");
                Add("Altura / semieje Z", "size_z", trigger.Native.SizeZ, "m");
                Add("Radio", "radius", trigger.Native.Radius, "m");
                Add("Ejecutar una vez", "once", trigger.Native.Once != 0);
                break;
            case LightModel light:
                Add("Identificador", "id", light.Id);
                Add("Activa", "enabled", light.Native.Enabled != 0);
                Add("Posición X", "x", light.Native.X, "m");
                Add("Posición Y", "y", light.Native.Y, "m");
                Add("Posición Z", "z", light.Native.Z, "m");
                Add("Color rojo", "red", light.Native.Red, "0–1");
                Add("Color verde", "green", light.Native.Green, "0–1");
                Add("Color azul", "blue", light.Native.Blue, "0–1");
                Add("Radio", "radius", light.Native.Radius, "m");
                Add("Intensidad", "intensity", light.Native.Intensity);
                Add("Parpadeo", "flicker", light.Native.Flicker, "0–1");
                break;
        }
    }

    /// <summary>
    /// Construye enlaces navegables desde los identificadores usados por las
    /// reglas. El autor ve así la relación entre una instancia, su definición
    /// y la lógica que la activa sin tener que buscar cadenas manualmente.
    /// </summary>
    private void BuildConnections()
    {
        Connections.Clear();
        if (SelectedItem is null)
            return;

        var found = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        void Add(string label, IEditorItem target)
        {
            string key = $"{target.Kind}:{target.Index}:{label}";
            if (found.Add(key))
                Connections.Add(new ConnectionItem(label, target));
        }

        if (SelectedItem is CharacterModel character)
            foreach (MarkerModel marker in Document.Markers.Where(marker =>
                         string.Equals(marker.Definition, character.Id, StringComparison.OrdinalIgnoreCase)))
                Add($"Instancia colocada: {marker.DisplayName}", marker);

        if (SelectedItem is RuleModel selectedRule)
        {
            IEditorItem? source = FindById(selectedRule.Source);
            if (source is not null)
                Add($"Origen: {source.DisplayName}", source);
            foreach (RuleActionModel action in selectedRule.Actions)
            {
                IEditorItem? target = FindById(action.Target);
                if (target is not null)
                    Add($"{action.Kind}: {target.DisplayName}", target);
            }
        }
        else
        {
            foreach (RuleModel rule in Document.Rules)
            {
                if (string.Equals(rule.Source, SelectedItem.Id, StringComparison.OrdinalIgnoreCase))
                    Add($"Activa la regla: {rule.DisplayName}", rule);
                if (rule.Actions.Any(action =>
                        string.Equals(action.Target, SelectedItem.Id, StringComparison.OrdinalIgnoreCase)))
                    Add($"Recibe acciones de: {rule.DisplayName}", rule);
            }
        }
    }

    private IEditorItem? FindById(string id)
    {
        if (string.IsNullOrWhiteSpace(id) || id == "-")
            return null;
        return Document.Sectors.Cast<IEditorItem>()
            .Concat(Document.Markers)
            .Concat(Document.Barriers)
            .Concat(Document.Characters)
            .Concat(Document.Rules)
            .Concat(Document.Dialogues)
            .Concat(Document.Triggers)
            .Concat(Document.Lights)
            .FirstOrDefault(item => string.Equals(item.Id, id, StringComparison.OrdinalIgnoreCase));
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

    public void Place(PlacementTool tool, double x, double y)
    {
        if (IsPlaying)
            return;
        ExecuteEditorAction(() =>
        {
            SelectedItem = Document.CreateMarker(tool.MarkerKind, tool.Definition,
                (float)x, (float)y, FloorElevation);
            Status = $"{tool.Name} colocado en el nivel";
        });
    }

    public void MoveMarker(MarkerModel marker, double x, double y)
    {
        if (IsPlaying)
            return;
        ExecuteEditorAction(() =>
        {
            SelectedItem = Document.MoveMarker(marker.Index, (float)x, (float)y);
            Status = $"{marker.DisplayName} movido";
        });
    }

    private void DuplicateSelected()
    {
        if (SelectedItem is not MarkerModel marker)
            return;
        ExecuteEditorAction(() =>
        {
            SelectedItem = Document.DuplicateMarker(marker.Index);
            Status = $"Copia de {marker.DisplayName} creada";
        });
    }

    private void DeleteSelected()
    {
        if (SelectedItem is not MarkerModel marker)
            return;
        ExecuteEditorAction(() =>
        {
            Document.DeleteMarker(marker.Index);
            SelectedItem = null;
            Status = $"{marker.DisplayName} eliminado";
        });
    }

    private void AddDrop(string? item)
    {
        if (SelectedItem is not MarkerModel marker || string.IsNullOrWhiteSpace(item))
            return;
        ExecuteEditorAction(() =>
        {
            SelectedItem = Document.AddDropRule(marker.Index, item);
            Status = $"Regla creada: {marker.DisplayName} soltará {item} una sola vez";
        });
    }

    private void StartDialogue()
    {
        if (QuickDialogue is not null)
            AddInteraction(12, QuickDialogue.Id, string.Empty,
                $"Al interactuar se iniciará {QuickDialogue.DisplayName}");
    }

    private void OpenBarrier()
    {
        if (QuickBarrier is not null)
            AddInteraction(6, QuickBarrier.Id, string.Empty,
                $"Al interactuar se abrirá {QuickBarrier.DisplayName}");
    }

    private void ShowMessage() => AddInteraction(5, "-", QuickMessage,
        "Mensaje de interacción creado");

    private void GiveItem(string? item)
    {
        if (!string.IsNullOrWhiteSpace(item))
            AddInteraction(2, item, string.Empty, $"Al interactuar se entregará {item}");
    }

    private void AddInteraction(int action, string target, string value, string status)
    {
        if (SelectedItem is not MarkerModel marker)
            return;
        ExecuteEditorAction(() =>
        {
            SelectedItem = Document.AddInteractionRule(marker.Index, action, target, value);
            Status = status;
        });
    }

    private void ExecuteEditorAction(Action action)
    {
        try
        {
            action();
        }
        catch (Exception exception)
        {
            Status = exception.Message;
            MessageBox.Show(exception.Message, "No se pudo aplicar la acción",
                MessageBoxButton.OK, MessageBoxImage.Warning);
        }
    }

    private bool Save()
    {
        try
        {
            Document.Save();
            Status = "Todos los archivos del proyecto fueron guardados";
            return true;
        }
        catch (Exception exception)
        {
            Status = exception.Message;
            MessageBox.Show(exception.Message, "No se pudo guardar", MessageBoxButton.OK, MessageBoxImage.Error);
            return false;
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
        try {
            _session = Document.StartSession();
            SimulationPaused = false;
            IsPlaying = true;
            Raise(nameof(Session));
            Status = "Prueba aislada · clic para controlar · WASD · botón derecho para mirar";
        } catch (Exception exception) { Status = exception.Message; }
    }

    private void Stop()
    {
        SessionNative.re_session_destroy(_session);
        _session = 0;
        _stepRequested = false;
        IsPlaying = false;
        Raise(nameof(Session));
        Status = "Prueba terminada; el documento conserva todos sus cambios sin guardar";
    }

    public void Dispose()
    {
        Stop();
        Document.Changed -= DocumentChanged;
        Document.Dispose();
    }
}

internal sealed record InspectorEdit(InspectorField Field, string Value);
internal sealed record ResourceItem(string Name, string Kind, string Path);
internal sealed record ConnectionItem(string Label, IEditorItem Target);
internal sealed record PlacementTool(string Name, string Description, string MarkerKind,
    string Definition, string Symbol, System.Windows.Media.ImageSource? Thumbnail = null);
