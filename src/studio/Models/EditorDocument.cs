using System.Collections.ObjectModel;

namespace RetroForge.Studio;

/// <summary>
/// Dueño administrado del handle nativo. Toda mutación pasa por este servicio,
/// lo que garantiza un único historial aunque existan paneles flotantes.
/// </summary>
public sealed class EditorDocument : IDisposable
{
    private nint _handle;

    public EditorDocument(string manifest)
    {
        if (EditorNative.re_editor_open(manifest, out _handle, out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
    }

    public ProjectOverview Overview { get; private set; } = new();
    public ObservableCollection<SectorModel> Sectors { get; } = [];
    public ObservableCollection<MarkerModel> Markers { get; } = [];
    public ObservableCollection<BarrierModel> Barriers { get; } = [];
    public ObservableCollection<CharacterModel> Characters { get; } = [];
    public ObservableCollection<RuleModel> Rules { get; } = [];
    public ObservableCollection<DialogueModel> Dialogues { get; } = [];
    public ObservableCollection<TriggerModel> Triggers { get; } = [];
    public ObservableCollection<LightModel> Lights { get; } = [];

    public event EventHandler? Changed;

    public void Refresh()
    {
        EnsureOpen();
        if (EditorNative.re_editor_overview(_handle, out EditorNative.Overview native) == 0)
            throw new InvalidOperationException("El motor no pudo describir el proyecto.");
        Overview = new ProjectOverview(native);
        Replace(Sectors, native.SectorCount, TrySector);
        Replace(Markers, native.MarkerCount, TryMarker);
        Replace(Barriers, native.BarrierCount, TryBarrier);
        Replace(Characters, native.CharacterCount, TryCharacter);
        Replace(Rules, native.RuleCount, TryRule);
        Replace(Dialogues, native.DialogueCount, TryDialogue);
        Replace(Triggers, native.TriggerCount, TryTrigger);
        Replace(Lights, native.LightCount, TryLight);
        Changed?.Invoke(this, EventArgs.Empty);
    }

    public void SetProperty(EditorNative.ObjectKind kind, uint index, string property, string value)
    {
        EnsureOpen();
        if (EditorNative.re_editor_set_property(_handle, kind, index, property, value,
                out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
    }

    public MarkerModel CreateMarker(string kind, string definition, float x, float y)
    {
        EnsureOpen();
        if (EditorNative.re_editor_create_marker(_handle, kind, definition, x, y,
                out uint index, out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
        return Markers[(int)index];
    }

    public MarkerModel MoveMarker(uint index, float x, float y)
    {
        EnsureOpen();
        if (EditorNative.re_editor_move_marker(_handle, index, x, y,
                out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
        return Markers[(int)index];
    }

    public MarkerModel DuplicateMarker(uint index)
    {
        EnsureOpen();
        if (EditorNative.re_editor_duplicate_marker(_handle, index, out uint duplicate,
                out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
        return Markers[(int)duplicate];
    }

    public void DeleteMarker(uint index)
    {
        EnsureOpen();
        if (EditorNative.re_editor_delete_marker(_handle, index, out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
    }

    public RuleModel AddDropRule(uint marker, string item)
    {
        EnsureOpen();
        if (EditorNative.re_editor_add_drop_rule(_handle, marker, item, out uint rule,
                out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
        return Rules[(int)rule];
    }

    public bool Undo()
    {
        EnsureOpen();
        if (EditorNative.re_editor_undo(_handle) == 0)
            return false;
        Refresh();
        return true;
    }

    public bool Redo()
    {
        EnsureOpen();
        if (EditorNative.re_editor_redo(_handle) == 0)
            return false;
        Refresh();
        return true;
    }

    public void Save()
    {
        EnsureOpen();
        if (EditorNative.re_editor_save(_handle, out EditorNative.Error error) == 0)
            throw NativeFailure(error);
        Refresh();
    }

    private SectorModel? TrySector(uint index) =>
        EditorNative.re_editor_sector(_handle, index, out EditorNative.Sector item) != 0
            ? new SectorModel(index, item)
            : null;
    private MarkerModel? TryMarker(uint index) =>
        EditorNative.re_editor_marker(_handle, index, out EditorNative.Marker item) != 0
            ? new MarkerModel(index, item)
            : null;
    private BarrierModel? TryBarrier(uint index) =>
        EditorNative.re_editor_barrier(_handle, index, out EditorNative.Barrier item) != 0
            ? new BarrierModel(index, item)
            : null;
    private CharacterModel? TryCharacter(uint index)
    {
        if (EditorNative.re_editor_character(_handle, index, out EditorNative.Character item) == 0)
            return null;
        var animations = new List<AnimationModel>();
        for (uint animation = 0; animation < item.AnimationCount; animation++)
        {
            if (EditorNative.re_editor_animation(_handle, index, animation, out EditorNative.Animation clip) == 0)
                continue;
            var frames = new List<AnimationFrameModel>();
            for (uint frame = 0; frame < clip.FrameCount; frame++)
                if (EditorNative.re_editor_animation_frame(_handle, index, animation, frame,
                        out EditorNative.AnimationFrame value) != 0)
                    frames.Add(new AnimationFrameModel(value.Cell, value.Duration,
                        value.Event switch { 1 => "sonido", 2 => "ataque", _ => "—" }));
            animations.Add(new AnimationModel(clip.Name, clip.Directions, clip.Loop != 0, frames));
        }
        var phases = new List<BossPhaseModel>();
        for (uint phase = 0; phase < item.PhaseCount; phase++)
            if (EditorNative.re_editor_boss_phase(_handle, index, phase, out EditorNative.BossPhase value) != 0)
                phases.Add(new BossPhaseModel(value.Name, value.HealthThreshold,
                    value.Tracking == 0 ? "percepción" : "seguimiento constante",
                    BossActionName(value.Action), value.SpeedMultiplier, value.Cooldown, value.SummonLimit));
        return new CharacterModel(index, item) { Animations = animations, Phases = phases };
    }

    private RuleModel? TryRule(uint index)
    {
        if (EditorNative.re_editor_rule(_handle, index, out EditorNative.Rule item) == 0)
            return null;
        string[] comparisons = ["=", "≠", "<", "≤", ">", "≥"];
        string[] conditionKinds = ["Variable", "Inventario", "Objetivo", "Salud", "Vidas"];
        var conditions = new List<RuleConditionModel>();
        for (uint condition = 0; condition < item.ConditionCount; condition++)
            if (EditorNative.re_editor_rule_condition(_handle, index, condition,
                    out EditorNative.RuleCondition value) != 0)
                conditions.Add(new RuleConditionModel(
                    NameAt(conditionKinds, value.Kind, "Condición"), value.Key,
                    NameAt(comparisons, value.Comparison, "?"), value.Value));
        var actions = new List<RuleActionModel>();
        for (uint action = 0; action < item.ActionCount; action++)
            if (EditorNative.re_editor_rule_action(_handle, index, action,
                    out EditorNative.RuleAction value) != 0)
                actions.Add(new RuleActionModel(ActionName(value.Kind), value.Target, value.Value));
        return new RuleModel(index, item) { Conditions = conditions, Actions = actions };
    }

    private DialogueModel? TryDialogue(uint index)
    {
        if (EditorNative.re_editor_dialogue(_handle, index, out EditorNative.Dialogue item) == 0)
            return null;
        var choices = new List<DialogueChoiceModel>();
        for (uint choice = 0; choice < item.ChoiceCount; choice++)
            if (EditorNative.re_editor_dialogue_choice(_handle, index, choice,
                    out EditorNative.DialogueChoice value) != 0)
                choices.Add(new DialogueChoiceModel(value.Id, value.Text, value.Next,
                    string.IsNullOrWhiteSpace(value.ConditionVariable) || value.ConditionVariable == "-"
                        ? string.Empty : $"{value.ConditionVariable} = {(value.ConditionValue != 0 ? "true" : "false")}"));
        return new DialogueModel(index, item) { Choices = choices };
    }
    private TriggerModel? TryTrigger(uint index) =>
        EditorNative.re_editor_trigger(_handle, index, out EditorNative.Trigger item) != 0
            ? new TriggerModel(index, item)
            : null;
    private LightModel? TryLight(uint index) =>
        EditorNative.re_editor_light(_handle, index, out EditorNative.Light item) != 0
            ? new LightModel(index, item)
            : null;

    private static void Replace<T>(ObservableCollection<T> target, uint count,
        Func<uint, T?> read) where T : class
    {
        target.Clear();
        for (uint index = 0; index < count; index++)
        {
            T? item = read(index);
            if (item is not null)
                target.Add(item);
        }
    }

    private static Exception NativeFailure(EditorNative.Error error)
    {
        string line = error.Line == 0 ? string.Empty : $" (línea {error.Line})";
        return new InvalidOperationException($"{error.Message}{line}");
    }

    private static string NameAt(string[] names, int index, string fallback) =>
        index >= 0 && index < names.Length ? names[index] : fallback;

    private static string ActionName(int action) => NameAt(
        ["Asignar variable", "Sumar variable", "Dar objeto", "Quitar objeto", "Cambiar objetivo",
         "Mostrar mensaje", "Abrir barrera", "Cerrar barrera", "Dañar jugador", "Curar jugador",
         "Cambiar vidas", "Crear checkpoint", "Iniciar diálogo", "Cambiar luz", "Crear pickup",
         "Emitir evento", "Victoria", "Game over"], action, "Acción");

    private static string BossActionName(int action) => NameAt(
        ["Esperar", "Ataque cuerpo a cuerpo", "Proyectil", "Capturar", "Invocar", "Activar objeto"],
        action, "Acción");

    private void EnsureOpen()
    {
        ObjectDisposedException.ThrowIf(_handle == 0, this);
    }

    public void Dispose()
    {
        if (_handle == 0)
            return;
        EditorNative.re_editor_close(_handle);
        _handle = 0;
        GC.SuppressFinalize(this);
    }
}
