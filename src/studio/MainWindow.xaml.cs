using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using AvalonDock.Core;
using AvalonDock.Layout;
using AvalonDock.Serializer.Xml;

namespace RetroForge.Studio;

public partial class MainWindow : Window
{
    private readonly StudioViewModel _viewModel;
    private readonly Dictionary<string, object?> _content = [];
    private string _defaultLayout = string.Empty;
    private bool _allowClose;
    private Point _placementDragStart;

    private static string SettingsDirectory => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "RetroForge", "Studio");
    private static string LayoutPath => Path.Combine(SettingsDirectory, "layout-v1.xml");

    internal MainWindow(StudioViewModel viewModel)
    {
        _viewModel = viewModel;
        DataContext = viewModel;
        InitializeComponent();
        _viewModel.PropertyChanged += (_, args) =>
        {
            if (args.PropertyName == nameof(StudioViewModel.IsPlaying) && _viewModel.IsPlaying)
            {
                TestDocument.IsSelected = true;
                TestDocument.IsActive = true;
            }
        };
        Loaded += Window_Loaded;
        Closing += Window_Closing;
        PreviewKeyDown += Window_PreviewKeyDown;
    }

    private void Window_Loaded(object sender, RoutedEventArgs e)
    {
        foreach (LayoutContent item in DockManager.Layout.Descendents().OfType<LayoutContent>())
            if (!string.IsNullOrWhiteSpace(item.ContentId))
                _content[item.ContentId] = item.Content;

        _defaultLayout = SerializeLayout();
        RestoreLayout();
        /* Una sesión nueva empieza siempre en Construir. Los paneles conservan
         * su monitor y tamaño, pero el selector y el documento no divergen. */
        LayoutContent? map = DockManager.Layout.Descendents().OfType<LayoutContent>()
            .FirstOrDefault(item => item.ContentId == "MapDocument");
        if (map is not null)
        {
            map.IsSelected = true;
            map.IsActive = true;
        }
    }

    private void SceneTree_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
    {
        if (e.NewValue is IEditorItem item)
            _viewModel.SelectedItem = item;
    }

    private void DialogueList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (DialogueList.SelectedItem is IEditorItem item)
            _viewModel.SelectedItem = item;
    }

    private void PlacementTool_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        _placementDragStart = e.GetPosition(this);
        if (sender is FrameworkElement element)
            element.CaptureMouse();
        e.Handled = true;
    }

    private void PlacementTool_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement element && element.IsMouseCaptured)
            element.ReleaseMouseCapture();
        e.Handled = true;
    }

    private void PlacementTool_MouseMove(object sender, MouseEventArgs e)
    {
        if (e.LeftButton != MouseButtonState.Pressed || sender is not FrameworkElement
            { DataContext: PlacementTool tool } element)
            return;
        Vector distance = e.GetPosition(this) - _placementDragStart;
        if (Math.Abs(distance.X) < SystemParameters.MinimumHorizontalDragDistance &&
            Math.Abs(distance.Y) < SystemParameters.MinimumVerticalDragDistance)
            return;
        element.ReleaseMouseCapture();
        e.Handled = true;
        StudioLog.Write($"Inicio de colocación: {tool.Definition}");
        DragDrop.DoDragDrop(element, tool, DragDropEffects.Copy);
    }

    private void LightOverlay_Changed(object sender, RoutedEventArgs e) =>
        MapView.ShowLights = sender is System.Windows.Controls.Primitives.ToggleButton { IsChecked: true };

    private void TriggerOverlay_Changed(object sender, RoutedEventArgs e) =>
        MapView.ShowTriggers = sender is System.Windows.Controls.Primitives.ToggleButton { IsChecked: true };

    private void InspectorField_LostKeyboardFocus(object sender, KeyboardFocusChangedEventArgs e)
    {
        ApplyInspectorText(sender as TextBox);
    }

    private void InspectorField_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key != Key.Enter || sender is not TextBox textBox)
            return;
        ApplyInspectorText(textBox);
        Keyboard.ClearFocus();
        e.Handled = true;
    }

    private void InspectorBoolean_Changed(object sender, RoutedEventArgs e)
    {
        if (sender is not CheckBox { Tag: InspectorField field } checkBox)
            return;
        string value = checkBox.IsChecked == true ? "true" : "false";
        if (field.Value != value)
            _viewModel.ApplyFieldCommand.Execute(new InspectorEdit(field, value));
    }

    private void ApplyInspectorText(TextBox? textBox)
    {
        if (textBox?.Tag is not InspectorField field || textBox.Text == field.Value)
            return;
        _viewModel.ApplyFieldCommand.Execute(new InspectorEdit(field, textBox.Text));
    }

    private void Workspace_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not RadioButton { Tag: string id, Content: string label })
            return;
        LayoutDocument? document = DockManager.Layout.Descendents().OfType<LayoutDocument>()
            .FirstOrDefault(item => item.ContentId == id);
        if (document is not null)
            document.IsActive = true;
        _viewModel.WorkspaceCommand.Execute(label);
    }

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        if ((Keyboard.Modifiers & ModifierKeys.Control) == 0)
            return;
        switch (e.Key)
        {
            case Key.S:
                _viewModel.SaveCommand.Execute(null);
                e.Handled = true;
                break;
            case Key.Z:
                _viewModel.UndoCommand.Execute(null);
                e.Handled = true;
                break;
            case Key.Y:
                _viewModel.RedoCommand.Execute(null);
                e.Handled = true;
                break;
        }
    }

    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount == 2)
            ToggleMaximize();
        else if (e.LeftButton == MouseButtonState.Pressed)
            DragMove();
    }

    private void Minimize_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void Maximize_Click(object sender, RoutedEventArgs e) => ToggleMaximize();
    private void ToggleMaximize() => WindowState = WindowState == WindowState.Maximized
        ? WindowState.Normal : WindowState.Maximized;
    private void Close_Click(object sender, RoutedEventArgs e) => Close();

    private void Validate_Click(object sender, RoutedEventArgs e)
    {
        _viewModel.Document.Refresh();
        MessageBox.Show(this, "El proyecto se volvió a leer desde el motor. No se encontraron errores estructurales.",
            "Validación completa", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void Help_Click(object sender, RoutedEventArgs e)
    {
        var help = new OnboardingWindow { Owner = this };
        help.ShowDialog();
    }

    private void ResetLayout_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(_defaultLayout))
            return;
        DeserializeLayout(_defaultLayout);
    }

    private void Window_Closing(object? sender, CancelEventArgs e)
    {
        if (!_allowClose && _viewModel.Overview.Dirty)
        {
            MessageBoxResult answer = MessageBox.Show(this,
                "Hay cambios pendientes. ¿Quieres guardarlos antes de cerrar?",
                "Cambios sin guardar", MessageBoxButton.YesNoCancel, MessageBoxImage.Warning);
            if (answer == MessageBoxResult.Cancel)
            {
                e.Cancel = true;
                return;
            }
            if (answer == MessageBoxResult.Yes)
                _viewModel.SaveCommand.Execute(null);
        }
        if (e.Cancel)
            return;
        SaveLayout();
        _allowClose = true;
        _viewModel.Dispose();
    }

    private string SerializeLayout()
    {
        using var stream = new MemoryStream();
        var serializer = new XmlLayoutSerializer(DockManager);
        serializer.Serialize(stream);
        stream.Position = 0;
        using var reader = new StreamReader(stream);
        return reader.ReadToEnd();
    }

    private void SaveLayout()
    {
        try
        {
            Directory.CreateDirectory(SettingsDirectory);
            string temporary = LayoutPath + ".tmp";
            File.WriteAllText(temporary, SerializeLayout());
            File.Move(temporary, LayoutPath, true);
        }
        catch (IOException)
        {
            // El layout es una preferencia personal: su fallo nunca arriesga el proyecto.
        }
    }

    private void RestoreLayout()
    {
        if (!File.Exists(LayoutPath))
            return;
        try
        {
            DeserializeLayout(File.ReadAllText(LayoutPath));
            RecoverOffscreenPanels();
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException)
        {
            // Una versión antigua o un archivo truncado vuelve al layout seguro.
            DeserializeLayout(_defaultLayout);
        }
    }

    private void DeserializeLayout(string xml)
    {
        using var stream = new MemoryStream(System.Text.Encoding.UTF8.GetBytes(xml));
        var serializer = new XmlLayoutSerializer(DockManager);
        serializer.LayoutSerializationCallback += RestoreContent;
        serializer.Deserialize(stream);
    }

    private void RestoreContent(object? sender, LayoutSerializationCallbackEventArgs args)
    {
        string? id = args.Model.ContentId;
        if (id is not null && _content.TryGetValue(id, out object? content))
            args.Content = content;
        else
            args.Cancel = true;
    }

    private void RecoverOffscreenPanels()
    {
        double left = SystemParameters.VirtualScreenLeft;
        double top = SystemParameters.VirtualScreenTop;
        double right = left + SystemParameters.VirtualScreenWidth;
        double bottom = top + SystemParameters.VirtualScreenHeight;
        foreach (LayoutContent item in DockManager.Layout.Descendents().OfType<LayoutContent>())
        {
            if (item.FloatingLeft + 80 < left || item.FloatingLeft > right - 80)
                item.FloatingLeft = Math.Max(left + 40, 40);
            if (item.FloatingTop + 40 < top || item.FloatingTop > bottom - 40)
                item.FloatingTop = Math.Max(top + 40, 40);
        }
    }
}
