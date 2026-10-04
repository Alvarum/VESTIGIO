using System.Windows;

namespace Vestigio.Studio;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        StudioLog.Write("Inicio de VESTIGIO Studio");
        try
        {
            MainWindow = e.Args.Contains("--level", StringComparer.OrdinalIgnoreCase) ||
                         e.Args.Contains("--example", StringComparer.OrdinalIgnoreCase)
                ? CreateLevelWindow(e.Args) : new StudioStartWindow(e.Args);
            MainWindow.Show();
        }
        catch (Exception exception)
        {
            StudioLog.Write($"Error de inicio: {exception}");
            MessageBox.Show($"VESTIGIO Studio no pudo abrir el nivel.\n\n{exception.Message}",
                "No se pudo iniciar Studio", MessageBoxButton.OK, MessageBoxImage.Error);
            Shutdown(1);
        }
    }

    internal static Vestigio3DWindow CreateLevelWindow(IReadOnlyList<string> arguments)
    {
        string? level = null;
        string? settings = null;
        bool noAudio = false;
        for (int index = 0; index < arguments.Count; ++index)
        {
            if (string.Equals(arguments[index], "--settings", StringComparison.OrdinalIgnoreCase))
            {
                if (index + 1 >= arguments.Count || arguments[index + 1].StartsWith("--", StringComparison.Ordinal))
                    throw new ArgumentException("Indica la ruta del archivo tras --settings.");
                settings = Path.GetFullPath(arguments[++index]);
                continue;
            }
            if (string.Equals(arguments[index], "--no-audio", StringComparison.OrdinalIgnoreCase))
            {
                noAudio = true;
                continue;
            }
            if (!string.Equals(arguments[index], "--level", StringComparison.OrdinalIgnoreCase))
                continue;
            if (index + 1 >= arguments.Count || arguments[index + 1].StartsWith("--", StringComparison.Ordinal))
                throw new ArgumentException("Indica la ruta del archivo tras --level.");
            level = Path.GetFullPath(arguments[++index]);
        }
        level ??= GpuViewportHost.ResolveDemoAsset("atrium.level.json");
        string model = GpuViewportHost.ResolveDemoAsset("atrium.gltf");
        return new Vestigio3DWindow(level, model, settings, !noAudio);
    }
}
