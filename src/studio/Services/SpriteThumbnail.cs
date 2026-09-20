using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace RetroForge.Studio;

/// <summary>
/// Recorta una celda real, nunca sustituye un recorte inválido por el atlas.
/// OnLoad libera el archivo inmediatamente para permitir reimportarlo.
/// La caché pertenece al documento; una fecha nueva invalida la imagen anterior.
/// </summary>
internal sealed class SpriteThumbnail
{
    private readonly Dictionary<string, ImageSource> _cache = new();

    public ImageSource? Load(string root, string relative, int width, int height, uint cell,
        out string error)
    {
        error = string.Empty;
        try
        {
            string directory = Path.GetFullPath(root) + Path.DirectorySeparatorChar;
            string path = Path.GetFullPath(Path.Combine(directory, relative));
            if (!path.StartsWith(directory, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("La imagen debe estar dentro del proyecto.");
            if (width <= 0 || height <= 0)
                throw new InvalidDataException("El tamaño del fotograma debe ser positivo.");
            string key = $"{path}|{File.GetLastWriteTimeUtc(path).Ticks}|{width}|{height}|{cell}";
            if (_cache.TryGetValue(key, out ImageSource? cached)) return cached;
            using var stream = File.OpenRead(path);
            var image = BitmapDecoder.Create(stream, BitmapCreateOptions.PreservePixelFormat,
                BitmapCacheOption.OnLoad).Frames[0];
            int columns = image.PixelWidth / width, rows = image.PixelHeight / height;
            if (columns == 0 || rows == 0 || (ulong)cell >= (ulong)columns * (ulong)rows)
                throw new InvalidDataException("El fotograma queda fuera de la hoja de sprites.");
            var crop = new CroppedBitmap(image, new Int32Rect(
                (int)(cell % (uint)columns) * width, (int)(cell / (uint)columns) * height, width, height));
            crop.Freeze();
            if (_cache.Count >= 256) _cache.Clear();
            _cache[key] = crop;
            return crop;
        }
        catch (Exception exception) when (exception is IOException or InvalidDataException or ArgumentException or
                                           NotSupportedException or UnauthorizedAccessException or
                                           System.Runtime.InteropServices.COMException)
        {
            error = exception.Message;
            return null;
        }
    }
}
