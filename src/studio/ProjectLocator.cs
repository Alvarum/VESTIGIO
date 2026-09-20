namespace RetroForge.Studio;

/// <summary>Localiza Haunted desde un binario o desde el árbol de fuentes.</summary>
internal static class ProjectLocator
{
    public static string Resolve(IReadOnlyList<string> arguments)
    {
        string? argument = null;
        for (int index = 0; index < arguments.Count; index++)
        {
            if (string.Equals(arguments[index], "--project", StringComparison.OrdinalIgnoreCase) &&
                index + 1 < arguments.Count)
            {
                argument = arguments[index + 1];
                break;
            }
            if (!arguments[index].StartsWith("-", StringComparison.Ordinal))
            {
                argument = arguments[index];
                break;
            }
        }
        if (!string.IsNullOrWhiteSpace(argument))
            return Path.GetFullPath(argument.Trim('"'));

        string directory = AppContext.BaseDirectory;
        for (int parent = 0; parent < 7; parent++)
        {
            string packaged = Path.Combine(directory, "assets", "studio", "haunted.retro");
            if (File.Exists(packaged))
                return packaged;
            DirectoryInfo? info = Directory.GetParent(directory);
            if (info is null)
                break;
            directory = info.FullName;
        }
        throw new FileNotFoundException(
            "No se encontró assets/studio/haunted.retro. También puedes arrastrar un manifiesto sobre retro_studio.exe.");
    }
}
