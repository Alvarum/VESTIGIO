using System.Windows;
using System.Windows.Controls;

namespace Vestigio.Studio;

internal static class NewLevelDialog
{
    internal static string? GetName(Window owner)
    {
        var name = new TextBox { Text = "Mi primer nivel", MaxLength = 128, Margin = new Thickness(0, 10, 0, 18) };
        var dialog = new Window { Owner = owner, Title = "Nuevo nivel", Width = 420,
            SizeToContent = SizeToContent.Height, ResizeMode = ResizeMode.NoResize, WindowStartupLocation = WindowStartupLocation.CenterOwner };
        var create = new Button { Content = "Crear nivel", IsDefault = true, Style = (Style)owner.FindResource("PrimaryButton") };
        create.Click += (_, _) => { if (!string.IsNullOrWhiteSpace(name.Text)) dialog.DialogResult = true; };
        var cancel = new Button { Content = "Cancelar", IsCancel = true, Margin = new Thickness(0, 0, 8, 0) };
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right };
        buttons.Children.Add(cancel); buttons.Children.Add(create);
        var panel = new StackPanel { Margin = new Thickness(24) };
        panel.Children.Add(new TextBlock { Text = "Nombre del nivel", FontWeight = FontWeights.SemiBold });
        panel.Children.Add(name); panel.Children.Add(buttons); dialog.Content = panel;
        dialog.Loaded += (_, _) => { name.Focus(); name.SelectAll(); };
        return dialog.ShowDialog() == true ? name.Text.Trim() : null;
    }
}
