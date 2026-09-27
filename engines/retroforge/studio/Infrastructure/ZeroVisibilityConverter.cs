using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace RetroForge.Studio;

/// <summary>
/// Hace visible una ayuda solamente cuando una colección está vacía. Mantener
/// esta decisión en un converter evita duplicar estados de presentación en el
/// modelo del documento.
/// </summary>
public sealed class ZeroVisibilityConverter : IValueConverter
{
    public static ZeroVisibilityConverter Instance { get; } = new();

    public object Convert(object value, Type targetType, object parameter, CultureInfo culture) =>
        value is int count && count == 0 ? Visibility.Visible : Visibility.Collapsed;

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        Binding.DoNothing;
}
