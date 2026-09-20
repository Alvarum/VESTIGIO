namespace RetroForge.Studio;

/// <summary>Bitácora mínima para errores que ocurren antes de mostrar la UI.</summary>
internal static class StudioLog
{
    private static readonly string DirectoryPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "RetroForge", "Studio");
    private static readonly string FilePath = Path.Combine(DirectoryPath, "studio.log");

    public static void Write(string message)
    {
        try
        {
            Directory.CreateDirectory(DirectoryPath);
            File.AppendAllText(FilePath, $"{DateTimeOffset.Now:O}  {message}{Environment.NewLine}");
        }
        catch (IOException)
        {
            // El diagnóstico nunca debe impedir abrir o cerrar el editor.
        }
    }
}
