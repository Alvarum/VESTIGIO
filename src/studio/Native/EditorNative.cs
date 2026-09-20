using System.Runtime.InteropServices;

namespace RetroForge.Studio;

/// <summary>
/// Declaraciones exactas de la ABI C. Sólo esta clase conoce tamaños y nombres
/// nativos; el resto del editor trabaja con modelos administrados normales.
/// </summary>
public static class EditorNative
{
    private const string Library = "retro_editor";

    public enum ObjectKind
    {
        Project,
        Sector,
        Marker,
        Barrier,
        Character,
        Rule,
        Dialogue,
        Trigger,
        Light
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Error
    {
        public nuint Line;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string Message;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Overview
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Name;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)] public string Manifest;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)] public string Root;
        public ulong Revision;
        public uint SectorCount, MarkerCount, BarrierCount, CharacterCount;
        public uint RuleCount, DialogueCount, TriggerCount, LightCount;
        public int Dirty, CanUndo, CanRedo;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Sector
    {
        public float Floor, Ceiling, Light;
        public int WallMaterial, FloorMaterial, CeilingMaterial;
        public uint VertexCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public float[] Vertices;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Marker
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 24)] public string Kind;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Definition;
        public int Sector;
        public float X, Y, Z, Yaw;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Barrier
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Id;
        public int Kind, Sector, Edge, Material;
        public uint Blocks;
        public float OpenFraction, Health;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Character
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 160)] public string Sprite;
        public int CellWidth, CellHeight, MaxHealth, AttackDamage, Tracking;
        public float Radius, Height, Speed, SightRange, FieldOfView, CaptureRange, AttackRange;
        public uint AnimationCount, PhaseCount;
        public int Invulnerable, CanBeStunned, CanOpenDoors, CaptureGameOver;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Rule
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Source;
        public int Event, Priority, Once;
        public float Cooldown;
        public uint ConditionCount, ActionCount;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Dialogue
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 48)] public string Speaker;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 192)] public string Text;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Next;
        public int PausesWorld;
        public uint ChoiceCount;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Trigger
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Id;
        public int Shape, Sector, Once;
        public float X, Y, Z, SizeX, SizeY, SizeZ, Radius;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Light
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Id;
        public int Kind, Enabled;
        public float X, Y, Z, Red, Green, Blue, Radius, Intensity, Yaw, Cone, Flicker;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct Animation
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 24)] public string Name;
        public uint Directions, FrameCount;
        public int Loop;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AnimationFrame
    {
        public uint Cell;
        public float Duration;
        public int Event;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct BossPhase
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 24)] public string Name;
        public float HealthThreshold, SpeedMultiplier, Cooldown;
        public int Tracking, Action;
        public uint SummonLimit;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct RuleCondition
    {
        public int Kind, Comparison;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Key;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 96)] public string Value;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct RuleAction
    {
        public int Kind;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Target;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 96)] public string Value;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct DialogueChoice
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 24)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 96)] public string Text;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Next;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string ConditionVariable;
        public int ConditionValue;
    }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int re_editor_open(string manifest, out nint document, out Error error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void re_editor_close(nint document);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_overview(nint document, out Overview overview);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_sector(nint document, uint index, out Sector sector);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_marker(nint document, uint index, out Marker marker);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_barrier(nint document, uint index, out Barrier barrier);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_character(nint document, uint index, out Character character);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_rule(nint document, uint index, out Rule rule);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_dialogue(nint document, uint index, out Dialogue dialogue);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_trigger(nint document, uint index, out Trigger trigger);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_light(nint document, uint index, out Light light);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_animation(nint document, uint character, uint animation,
        out Animation value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_animation_frame(nint document, uint character, uint animation,
        uint frame, out AnimationFrame value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_boss_phase(nint document, uint character, uint phase,
        out BossPhase value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_rule_condition(nint document, uint rule, uint condition,
        out RuleCondition value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_rule_action(nint document, uint rule, uint action,
        out RuleAction value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_dialogue_choice(nint document, uint dialogue, uint choice,
        out DialogueChoice value);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int re_editor_create_marker(nint document, string kind, string definition,
        float x, float y, out uint index, out Error error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_move_marker(nint document, uint index, float x, float y,
        out Error error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_duplicate_marker(nint document, uint index, out uint duplicate,
        out Error error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_delete_marker(nint document, uint index, out Error error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int re_editor_add_drop_rule(nint document, uint marker, string item,
        out uint rule, out Error error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern int re_editor_set_property(nint document, ObjectKind kind, uint index,
        string property, string value, out Error error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_undo(nint document);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_redo(nint document);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int re_editor_save(nint document, out Error error);
}
