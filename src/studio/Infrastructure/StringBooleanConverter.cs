using System.Globalization;
using System.Windows.Data;

namespace RetroForge.Studio;

public sealed class StringBooleanConverter : IValueConverter
{
    public static StringBooleanConverter Instance { get; } = new();
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture) =>
        string.Equals(value as string, "true", StringComparison.OrdinalIgnoreCase);
    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        value is true ? "true" : "false";
}
