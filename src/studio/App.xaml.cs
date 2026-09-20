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
        string manifest = ProjectLocator.Resolve(e.Args);
        try
        {
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

    protected override void OnExit(ExitEventArgs e)
    {
        _projectLock?.Dispose();
        base.OnExit(e);
    }
}
