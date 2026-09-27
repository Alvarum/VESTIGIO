using System.Windows;
using System.Windows.Controls;
using System.Text.Json;

namespace RetroForge.Studio;

/// <summary>Editor 3D autónomo: el documento y las instancias Edit/Play pertenecen
/// al host GPU. No abre proyectos .retro ni crea una sesión de renderer CPU.</summary>
public partial class Vestigio3DWindow : Window
{
    internal Vestigio3DWindow(string levelPath, string modelPath)
    {
        if (!File.Exists(levelPath))
            throw new FileNotFoundException("No se encontró el nivel 3D.", levelPath);
        if (!File.Exists(modelPath))
            throw new FileNotFoundException("No se encontró el modelo 3D.", modelPath);
        InitializeComponent();
        Viewport.LevelPath = levelPath;
        Viewport.ModelPath = modelPath;
        LevelTitle.Text = DocumentName(levelPath);
        Loaded += (_, _) =>
        {
            if (!Viewport.IsNativeReady)
                StatusText.Text = $"No se pudo abrir el viewport: {Viewport.LastError}";
        };
        Closed += (_, _) => Viewport.Dispose();
    }

    internal GpuViewportHost GpuViewport => Viewport;

    private static string DocumentName(string levelPath)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(levelPath));
            if (document.RootElement.TryGetProperty("name", out JsonElement name) &&
                name.ValueKind == JsonValueKind.String &&
                !string.IsNullOrWhiteSpace(name.GetString()))
                return name.GetString()!;
        }
        catch (JsonException)
        {
            // El host nativo mostrará el diagnóstico estructural del documento.
        }
        return Path.GetFileNameWithoutExtension(levelPath);
    }

    private void Play_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TrySetPlaying(true))
        {
            StatusText.Text = "No se pudo iniciar la prueba 3D";
            return;
        }
        PlayButton.IsEnabled = false;
        StopButton.IsEnabled = true;
        CameraMode.IsEnabled = false;
        FrameButton.IsEnabled = false;
        StatusText.Text = "Probar · instancia aislada del documento";
    }

    private void Stop_Click(object sender, RoutedEventArgs e)
    {
        if (!Viewport.TrySetPlaying(false))
        {
            StatusText.Text = "No se pudo detener la prueba 3D";
            return;
        }
        PlayButton.IsEnabled = true;
        StopButton.IsEnabled = false;
        CameraMode.IsEnabled = true;
        FrameButton.IsEnabled = true;
        StatusText.Text = "Editar · documento sin cambios por la prueba";
    }

    private void CameraMode_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (Viewport is null || sender is not ComboBox { SelectedIndex: >= 0 } selector)
            return;
        if (!Viewport.SetCameraMode(selector.SelectedIndex))
            StatusText.Text = "No se pudo cambiar la cámara 3D";
        else
            StatusText.Text = $"Editar · {((ComboBoxItem)selector.SelectedItem).Content}";
    }

    private void FrameSelection_Click(object sender, RoutedEventArgs e)
    {
        StatusText.Text = Viewport.FrameSelection()
            ? "Objeto seleccionado encuadrado"
            : "Selecciona un objeto en el viewport antes de encuadrar";
    }
}
