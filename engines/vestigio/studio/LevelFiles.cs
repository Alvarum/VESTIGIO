using System.Text.Json;

namespace Vestigio.Studio;

internal static class LevelFiles
{
    internal static bool IsExample(string path)
    {
        string target = Path.GetFullPath(path);
        string? directory = AppContext.BaseDirectory;
        for (int depth = 0; directory is not null && depth < 8; depth++)
        {
            foreach (string relative in new[] { "assets/demo/atrium.level.json", "engines/vestigio/assets/demo/atrium.level.json" })
                if (target.Equals(Path.GetFullPath(Path.Combine(directory, relative)), StringComparison.OrdinalIgnoreCase)) return true;
            directory = Directory.GetParent(directory)?.FullName;
        }
        return false;
    }

    // A private backing file lets the native document own edits before Save As.
    // Imported resources remain here until the native atomic save copies them.
    internal static string CreateNew(string name, string? directory = null)
    {
        directory ??= Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "VESTIGIO", "Unsaved", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        string path = Path.Combine(directory, "nuevo.level.json");
        var level = new
        {
            format = "vestigio.level", version = 1, id = Guid.NewGuid().ToString(), name,
            coordinates = "right-handed-z-up-meters",
            required = new[] { "core", "transform", "environment", "component.engine.camera.v1" },
            environment = new { ambient_linear = new[] { .8, .8, .8 },
                clear_linear = new[] { .025, .035, .045 },
                fog = new { mode = "none", color_linear = new[] { .1, .1, .1 }, start = 10, end = 80 } },
            entities = new[] { new { id = Guid.NewGuid().ToString(), parent = (string?)null,
                transform = new { position = new[] { 0, -1, 1.7 }, rotation = new[] { 0, 0, 0, 1 }, scale = new[] { 1, 1, 1 } },
                required_components = new[] { "engine.camera" },
                components = new Dictionary<string, object> { ["engine.camera"] = new {
                    version = 1, fov_y_radians = 1.0471976, near = .05, far = 80 } } } }
        };
        File.WriteAllText(path, JsonSerializer.Serialize(level, new JsonSerializerOptions { WriteIndented = true }));
        return path;
    }
}
