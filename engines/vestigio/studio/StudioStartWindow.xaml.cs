using System.Windows;
using Microsoft.Win32;

namespace Vestigio.Studio;

public partial class StudioStartWindow : Window
{
    private readonly IReadOnlyList<string> _arguments;
    internal StudioStartWindow(IReadOnlyList<string> arguments)
    {
        _arguments = arguments;
        InitializeComponent();
    }

    private void New_Click(object sender, RoutedEventArgs e)
    {
        string? name = NewLevelDialog.GetName(this);
        if (name is not null) OpenEditor(LevelFiles.CreateNew(name), true);
    }

    private void Open_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog { Title = "Abrir nivel VESTIGIO", Filter = "Nivel VESTIGIO (*.level.json;*.json)|*.level.json;*.json" };
        if (dialog.ShowDialog(this) == true) OpenEditor(dialog.FileName, false);
    }

    private void Example_Click(object sender, RoutedEventArgs e) =>
        OpenEditor(GpuViewportHost.ResolveDemoAsset("atrium.level.json"), false);

    private void OpenEditor(string path, bool unsaved)
    {
        try
        {
            var window = App.CreateLevelWindow(_arguments.Concat(new[] { "--level", path }).ToArray());
            window.MarkUnsaved(unsaved);
            Application.Current.MainWindow = window;
            window.Show();
            Close();
        }
        catch (Exception exception) when (exception is IOException or ArgumentException)
        {
            ErrorText.Text = $"No se pudo abrir: {exception.Message}";
        }
    }
}
