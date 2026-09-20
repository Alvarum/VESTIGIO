namespace RetroForge.Studio;

/// <summary>Localiza Haunted desde un binario o desde el árbol de fuentes.</summary>
internal static class ProjectLocator
{
    public static string Resolve(string? argument)
    {
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
