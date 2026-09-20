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
    private CharacterModel? TryCharacter(uint index) =>
        EditorNative.re_editor_character(_handle, index, out EditorNative.Character item) != 0
            ? new CharacterModel(index, item)
            : null;
    private RuleModel? TryRule(uint index) =>
        EditorNative.re_editor_rule(_handle, index, out EditorNative.Rule item) != 0
            ? new RuleModel(index, item)
            : null;
    private DialogueModel? TryDialogue(uint index) =>
        EditorNative.re_editor_dialogue(_handle, index, out EditorNative.Dialogue item) != 0
            ? new DialogueModel(index, item)
            : null;
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
