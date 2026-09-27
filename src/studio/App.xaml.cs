using System.Windows;

namespace RetroForge.Studio;

/// <summary>
/// Punto de entrada administrado. La aplicación resuelve el proyecto antes de
/// crear la ventana para que cualquier error aparezca como una decisión clara.
/// </summary>
public partial class App : Application
{
    private ProjectLock? _projectLock;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        StudioLog.Write("Inicio de RetroForge Studio");
        try
        {
            if (e.Args.Contains("--atrium", StringComparer.OrdinalIgnoreCase))
            {
                MainWindow = CreateAtriumWindow(e.Args);
                StudioLog.Write($"Documento 3D: {((Vestigio3DWindow)MainWindow).GpuViewport.LevelPath}");
                MainWindow.Show();
                return;
            }
            string manifest = ProjectLocator.Resolve(e.Args);
            _projectLock = ProjectLock.Acquire(manifest);
            var document = new EditorDocument(manifest);
            StudioLog.Write($"Documento abierto: {manifest}");
            MainWindow = new MainWindow(new StudioViewModel(document));
            StudioLog.Write("Ventana construida");
            MainWindow.Show();
            StudioLog.Write("Ventana visible");
        }
        catch (Exception exception)
        {
            StudioLog.Write($"Error de inicio: {exception}");
            MessageBox.Show(
                $"RetroForge Studio no pudo abrir el proyecto.\n\n{exception.Message}",
                "No se pudo iniciar Studio",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
            Shutdown(1);
        }
    }

    internal static Vestigio3DWindow CreateAtriumWindow(IReadOnlyList<string> arguments)
    {
        string? level = null;
        string? settings = null;
        bool noAudio = false;
        for (int index = 0; index < arguments.Count; ++index)
        {
            if (string.Equals(arguments[index], "--settings", StringComparison.OrdinalIgnoreCase))
            {
                if (index + 1 >= arguments.Count || arguments[index + 1].StartsWith("--",
                        StringComparison.Ordinal))
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
            if (index + 1 >= arguments.Count || arguments[index + 1].StartsWith("--",
                    StringComparison.Ordinal))
                throw new ArgumentException("Indica la ruta del archivo tras --level.");
            level = Path.GetFullPath(arguments[++index]);
        }
        level ??= GpuViewportHost.ResolveDemoAsset("atrium.level.json");
        string model = GpuViewportHost.ResolveDemoAsset("atrium.gltf");
        return new Vestigio3DWindow(level, model, settings, !noAudio);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _projectLock?.Dispose();
        base.OnExit(e);
    }
}
