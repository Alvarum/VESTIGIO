/* Pruebas de integración administrada. Un directorio único evita tocar ejemplos
 * o preferencias. Las imágenes son composiciones fuera de pantalla, no una
 * certificación de interacción humana ni de monitores físicos. */
using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Interop;
using System.Windows.Threading;
using RetroForge.Studio;

internal static class Program
{
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            string output = Path.GetFullPath(args[0]);
            Directory.CreateDirectory(output);
            string scratch = Path.Combine(output, "project-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(scratch);
            string manifest = Path.Combine(scratch, "Mi juego.retro");
            EditorDocument.CreateProject(manifest);
            using (var vm = new StudioViewModel(new EditorDocument(manifest)))
            {
                Check(vm.Document.Sectors.Count == 1, "La plantilla debe tener una habitación jugable.");
                Check(!vm.PlacementTools.Any(tool => tool.MarkerKind == "pickup"), "No ofrecer objetos sin definición.");
                vm.CreateRoom(new Point(6, 1), new Point(10, 5));
                Check(vm.Document.Sectors.Count == 2, "La herramienta debe crear una habitación.");
                vm.UndoCommand.Execute(null);
                vm.CreateRoom(new Point(1, 1), new Point(5, 5));
                Check(vm.Document.Overview.CanRedo, "Un intento inválido debe conservar rehacer.");
                vm.RedoCommand.Execute(null);
                Check(vm.Document.Sectors.Count == 2, "Rehacer debe recuperar la habitación.");
                vm.MapTool = "Puerta";
                vm.AddBarrier(vm.Document.Sectors[0], 1);
                Check(vm.Document.Barriers.Count == 1, "La herramienta debe colocar una puerta.");
                vm.Document.Save();
                using var reopened = new EditorDocument(manifest);
                Check(reopened.Barriers.Count == 1 && reopened.Sectors.Count == 2, "Reabrir debe conservar geometría y puertas.");
                vm.Document.SetProperty(EditorNative.ObjectKind.Sector, 0, "light", "0.4");
                ulong revision = vm.Document.Overview.Revision;
                vm.PlayCommand.Execute(null);
                Check(vm.IsPlaying && !vm.EditEnabled && vm.Session != 0, "Probar debe crear una sesión aislada.");
                Check(vm.Document.Overview.Dirty && vm.Document.Overview.Revision == revision, "Probar no guarda ni edita.");
                Check(!vm.UndoCommand.CanExecute(null), "No se puede editar durante la prueba.");
                vm.StopCommand.Execute(null);
                Check(vm.EditEnabled && vm.Session == 0, "Detener devuelve la autoría.");
            }

            // Hoja sintética: dos celdas; pedir la tercera debe producir un error.
            string atlas = Path.Combine(scratch, "atlas.png");
            var bitmap = BitmapSource.Create(8, 4, 96, 96, PixelFormats.Bgra32, null, new byte[8 * 4 * 4], 32);
            var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
            using (var file = File.Create(atlas)) encoder.Save(file);
            var thumbnails = new SpriteThumbnail();
            var valid = thumbnails.Load(scratch, "atlas.png", 4, 4, 1, out string error) as BitmapSource;
            Check(valid?.PixelWidth == 4 && valid.PixelHeight == 4 && error.Length == 0, "La miniatura debe ser una celda.");
            Check(thumbnails.Load(scratch, "atlas.png", 4, 4, 2, out error) == null && error.Length > 0,
                "Un recorte inválido nunca puede mostrar el atlas.");
            Check(thumbnails.Load(scratch, "../atlas.png", 4, 4, 0, out error) == null, "No se permiten recursos fuera del proyecto.");
            Console.WriteLine("PASS managed authoring, playtest isolation, sprite crops");

            // Componer XAML real permite descubrir errores de recursos y bindings.
            // No se muestra una ventana ni se carga/guarda el layout personal.
            var app = new Application();
            app.Resources.MergedDictionaries.Add(new ResourceDictionary
                { Source = new Uri("pack://application:,,,/retro_studio;component/Themes/Graphite.xaml") });
            using var showcase = new StudioViewModel(new EditorDocument(Path.GetFullPath(args[1])));
            var window = new MainWindow(showcase);
            var content = (FrameworkElement)window.Content;
            window.Content = null;
            content.DataContext = showcase;
            System.Windows.Documents.TextElement.SetFontSize(content, window.FontSize);
            System.Windows.Documents.TextElement.SetFontFamily(content, window.FontFamily);
            content.Resources.MergedDictionaries.Add(window.Resources);
            // AvalonDock compone sus paneles al conectarse a una fuente WPF.
            // Un HWND oculto ejecuta ese ciclo sin mostrar ventanas al usuario.
            foreach (var size in new[] { new Size(1280, 800), new Size(1920, 1080) })
            {
                using var source = new HwndSource(new HwndSourceParameters("Studio layout test")
                    { Width = (int)size.Width, Height = (int)size.Height, WindowStyle = unchecked((int)0x80000000) });
                source.RootVisual = content;
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                content.Measure(size); content.Arrange(new Rect(size)); content.UpdateLayout();
                window.MapView.FrameScene();
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                var render = new RenderTargetBitmap((int)size.Width, (int)size.Height, 96, 96, PixelFormats.Pbgra32);
                render.Render(content);
                var png = new PngBitmapEncoder(); png.Frames.Add(BitmapFrame.Create(render));
                using var file = File.Create(Path.Combine(output, $"studio-{size.Width:0}x{size.Height:0}.png"));
                png.Save(file);
                source.RootVisual = null;
            }
            Console.WriteLine("PASS WPF composition 1280x800, 1920x1080 (offscreen)");
            return 0;
        }
        catch (Exception exception) { Console.Error.WriteLine(exception); return 1; }
    }
}
