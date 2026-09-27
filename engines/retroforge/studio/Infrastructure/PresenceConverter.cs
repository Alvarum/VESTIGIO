using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace RetroForge.Studio;

public sealed class PresenceConverter : IValueConverter
{
    public static PresenceConverter Instance { get; } = new();
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture) =>
        value is null ? Visibility.Collapsed : Visibility.Visible;
    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        Binding.DoNothing;
}
