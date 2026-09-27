using System.Globalization;
using System.Windows;

namespace RetroForge.Studio;

public sealed record ProjectOverview
{
    public ProjectOverview() { }
    internal ProjectOverview(EditorNative.Overview value)
    {
        Name = value.Name.Replace('_', ' ');
        Id = value.Id;
        Manifest = value.Manifest;
        Root = value.Root;
        Revision = value.Revision;
        Dirty = value.Dirty != 0;
        CanUndo = value.CanUndo != 0;
        CanRedo = value.CanRedo != 0;
    }

    public string Name { get; init; } = "RetroForge";
    public string Id { get; init; } = string.Empty;
    public string Manifest { get; init; } = string.Empty;
    public string Root { get; init; } = string.Empty;
    public ulong Revision { get; init; }
    public bool Dirty { get; init; }
    public bool CanUndo { get; init; }
    public bool CanRedo { get; init; }
}

/// <summary>Referencia estable dentro de una revisión del documento.</summary>
public interface IEditorItem
{
    EditorNative.ObjectKind Kind { get; }
    uint Index { get; }
    string Id { get; }
    string DisplayName { get; }
    string Subtitle { get; }
}

public sealed record SectorModel(uint Index, EditorNative.Sector Native) : IEditorItem
{
    public IReadOnlyList<(float Start, float End)> Openings { get; init; } = [];
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Sector;
    public string Id => $"sector-{Index}";
    public string DisplayName => $"Habitación {Index + 1}";
    public string Subtitle => $"Cota {Native.Floor:0.##} m · altura {Native.Ceiling - Native.Floor:0.##} m";
    public float Floor => Native.Floor;
    public float Ceiling => Native.Ceiling;
    public float Light => Native.Light;
    public int WallMaterial => Native.WallMaterial;
    public int FloorMaterial => Native.FloorMaterial;
    public int CeilingMaterial => Native.CeilingMaterial;
    public IReadOnlyList<Point> Vertices => Enumerable.Range(0, (int)Native.VertexCount)
        .Select(i => new Point(Native.Vertices[i * 2], Native.Vertices[i * 2 + 1])).ToArray();
}

public sealed record MarkerModel(uint Index, EditorNative.Marker Native) : IEditorItem
{
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Marker;
    public string Id => Native.Id;
    public string DisplayName => Native.Id;
    public string Subtitle => $"{Native.Kind} · {Native.Definition}";
    public string Definition => Native.Definition;
    public string MarkerKind => Native.Kind;
    public int Sector => Native.Sector;
    public float X => Native.X;
    public float Y => Native.Y;
    public float Z => Native.Z;
    public float Yaw => Native.Yaw;
}

public sealed record BarrierModel(uint Index, EditorNative.Barrier Native) : IEditorItem
{
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Barrier;
    public string Id => Native.Id;
    public string DisplayName => Native.Id;
    public string Subtitle => Native.Kind == 0 ? "Puerta" : "Ventana rompible";
    public int Sector => Native.Sector;
    public int Edge => Native.Edge;
    public float OpenFraction => Native.OpenFraction;
    public float Health => Native.Health;
}

public sealed record CharacterModel(uint Index, EditorNative.Character Native) : IEditorItem
{
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Character;
    public string Id => Native.Id;
    public string DisplayName => Native.Id;
    public string Subtitle => Native.PhaseCount > 0 ? $"Jefe · {Native.PhaseCount} fases" :
        Native.CaptureGameOver != 0 ? "Perseguidor · captura" : "Personaje";
    public string Sprite => Native.Sprite;
    public System.Windows.Media.ImageSource? Thumbnail { get; init; }
    public string ThumbnailError { get; init; } = string.Empty;
    public IReadOnlyList<AnimationModel> Animations { get; init; } = [];
    public IReadOnlyList<BossPhaseModel> Phases { get; init; } = [];
    public string? SpritePath(string root) => string.IsNullOrWhiteSpace(Sprite)
        ? null : Path.GetFullPath(Path.Combine(root, Sprite.Replace('/', Path.DirectorySeparatorChar)));
}

public sealed record RuleModel(uint Index, EditorNative.Rule Native) : IEditorItem
{
    private static readonly string[] Events =
    [
        "Inicio de nivel", "Entrar en zona", "Permanecer en zona", "Salir de zona",
        "Interactuar", "Recibir daño", "Morir", "Captura", "Recoger objeto", "Usar objeto",
        "Opción de diálogo", "Final de diálogo", "Cambiar objetivo", "Temporizador",
        "Evento de animación", "Cambiar barrera", "Muerte del jugador", "Cargar partida", "Personalizado"
    ];
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Rule;
    public string Id => Native.Id;
    public string DisplayName => Native.Id.Replace('_', ' ');
    public string Subtitle => $"CUANDO {EventName} · {Native.ActionCount} acciones";
    public string EventName => Native.Event >= 0 && Native.Event < Events.Length ? Events[Native.Event] : "Evento";
    public string Source => Native.Source;
    public IReadOnlyList<RuleConditionModel> Conditions { get; init; } = [];
    public IReadOnlyList<RuleActionModel> Actions { get; init; } = [];
}

public sealed record DialogueModel(uint Index, EditorNative.Dialogue Native) : IEditorItem
{
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Dialogue;
    public string Id => Native.Id;
    public string DisplayName => Native.Speaker;
    public string Subtitle => Native.Text;
    public string Text => Native.Text;
    public string Next => Native.Next;
    public IReadOnlyList<DialogueChoiceModel> Choices { get; init; } = [];
}

public sealed record TriggerModel(uint Index, EditorNative.Trigger Native) : IEditorItem
{
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Trigger;
    public string Id => Native.Id;
    public string DisplayName => Native.Id.Replace('_', ' ');
    public string Subtitle => Native.Shape switch { 0 => "Zona rectangular", 1 => "Zona cilíndrica", _ => "Sector completo" };
}

public sealed record LightModel(uint Index, EditorNative.Light Native) : IEditorItem
{
    public EditorNative.ObjectKind Kind => EditorNative.ObjectKind.Light;
    public string Id => Native.Id;
    public string DisplayName => Native.Id.Replace('_', ' ');
    public string Subtitle => $"Luz {(Native.Kind == 0 ? "puntual" : "direccional")} · {Native.Intensity:0.0}";
}

public sealed class SceneGroup(string name, string symbol, IEnumerable<IEditorItem> items)
{
    public string Name { get; } = name;
    public string Symbol { get; } = symbol;
    public IReadOnlyList<IEditorItem> Items { get; } = items.ToArray();
    public string Count => Items.Count.ToString(CultureInfo.InvariantCulture);
}

public sealed record InspectorField(string Label, string Property, string Value,
    string Suffix = "", bool IsBoolean = false, string Help = "");

public sealed record AnimationFrameModel(uint Cell, float Duration, string EventName);
public sealed record AnimationModel(string Name, uint Directions, bool Loop,
    IReadOnlyList<AnimationFrameModel> Frames)
{
    public string Summary => $"{Frames.Count} fotogramas · {Directions} dirección{(Directions == 1 ? "" : "es")} · {(Loop ? "bucle" : "una vez")}";
}

public sealed record BossPhaseModel(string Name, float HealthThreshold, string Tracking,
    string Action, float SpeedMultiplier, float Cooldown, uint SummonLimit)
{
    public string Summary => $"≤ {HealthThreshold:P0} de vida · {Tracking} · {Action}";
}

public sealed record RuleConditionModel(string Kind, string Key, string Comparison, string Value)
{
    public string Summary => $"{Kind}: {Key} {Comparison} {Value}";
}

public sealed record RuleActionModel(string Kind, string Target, string Value)
{
    public string Summary => string.IsNullOrWhiteSpace(Target) || Target == "-"
        ? $"{Kind} {Value}" : $"{Kind} → {Target}  {Value}";
}

public sealed record DialogueChoiceModel(string Id, string Text, string Next, string Condition)
{
    public string Summary => string.IsNullOrWhiteSpace(Condition) ? $"→ {Next}" : $"SI {Condition}  → {Next}";
}
