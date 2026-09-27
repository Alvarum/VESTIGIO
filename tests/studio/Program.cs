/* Pruebas de integración administrada. Un directorio único evita tocar ejemplos
 * o preferencias. Las imágenes son composiciones fuera de pantalla, no una
 * certificación de interacción humana ni de monitores físicos. */
using System.IO;
using System.Security.Cryptography;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
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

    private static void VerifyGpuHostLifecycle()
    {
        using var parent = new HwndSource(new HwndSourceParameters("GPU host lifecycle")
        {
            Width = 640,
            Height = 360,
            WindowStyle = unchecked((int)0x80000000)
        });
        for (int cycle = 0; cycle < 50; cycle++)
        {
            byte[] error = new byte[512];
            nint host = 0;
            try
            {
                host = GpuHostNative.vg_gpu_host_create(parent.Handle, 320, 180,
                    error, (nuint)error.Length);
                Check(host != 0,
                    $"No se pudo crear el host GPU en ciclo {cycle}: {GpuHostNative.Error(error)}");
                Check(GpuHostNative.vg_gpu_host_window(host) != 0,
                    $"El host GPU no expuso HWND en ciclo {cycle}.");
                if (cycle == 0)
                {
                    byte[] duplicateError = new byte[512];
                    nint duplicate = GpuHostNative.vg_gpu_host_create(parent.Handle, 320, 180,
                        duplicateError, (nuint)duplicateError.Length);
                    Check(duplicate == 0 && GpuHostNative.Error(duplicateError).Contains("existe"),
                        "Un segundo contexto GPU simult\u00e1neo debe rechazarse con diagn\u00f3stico.");
                }
                Check(GpuHostNative.vg_gpu_host_render(host) != 0,
                    $"El host GPU no renderiz\u00f3 en ciclo {cycle}.");
                if (cycle == 0)
                {
                    Check(Task.Run(() => GpuHostNative.vg_gpu_host_render(host)).Result == 0,
                        "Render GPU desde un hilo ajeno debe rechazarse.");
                    Check(Task.Run(() => GpuHostNative.vg_gpu_host_destroy(host)).Result == 0,
                        "Destroy GPU desde un hilo ajeno debe rechazarse.");
                }
                Check(GpuHostNative.vg_gpu_host_resize(host, 640, 360) != 0,
                    $"El host GPU no cambi\u00f3 de tama\u00f1o en ciclo {cycle}.");
                Check(GpuHostNative.vg_gpu_host_size(host, out uint clientWidth,
                    out uint clientHeight) != 0 && clientWidth == 640 && clientHeight == 360,
                    $"El HWND GPU no midi\u00f3 640x360 en ciclo {cycle}.");
                Check(GpuHostNative.vg_gpu_host_render(host) != 0,
                    $"El host GPU no renderiz\u00f3 tras resize en ciclo {cycle}.");
            }
            finally
            {
                if (host != 0)
                    _ = GpuHostNative.vg_gpu_host_destroy(host);
            }
        }
        Console.WriteLine("PASS GPU native host create/render/resize/destroy x50");
    }

    private static void VerifyGpuHwndHost(string output)
    {
        using var source = new HwndSource(new HwndSourceParameters("GPU HwndHost integration")
        {
            Width = 640,
            Height = 360,
            WindowStyle = unchecked((int)0x80000000)
        });
        var viewport = new GpuViewportHost { Width = 640, Height = 360 };
        source.RootVisual = viewport;
        Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
        viewport.Measure(new Size(640, 360));
        viewport.Arrange(new Rect(0, 0, 640, 360));
        viewport.UpdateLayout();
        Check(viewport.IsNativeReady, "HwndHost no cre\u00f3 la superficie GPU nativa.");
        Check(viewport.FocusNativeForTest() && viewport.NativeHasFocus,
            "HwndHost no transfiri\u00f3 el foco al HWND GPU.");
        Check(viewport.RenderForTest(), "HwndHost no present\u00f3 un frame GPU.");
        using (var duplicateSource = new HwndSource(new HwndSourceParameters("GPU fallback")
            { Width = 320, Height = 180, WindowStyle = unchecked((int)0x80000000) }))
        {
            var duplicateViewport = new GpuViewportHost { Width = 320, Height = 180 };
            duplicateSource.RootVisual = duplicateViewport;
            Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
            duplicateViewport.Measure(new Size(320, 180));
            duplicateViewport.Arrange(new Rect(0, 0, 320, 180));
            duplicateViewport.UpdateLayout();
            Check(!duplicateViewport.IsNativeReady && duplicateViewport.IsFallback &&
                duplicateViewport.LastError.Contains("existe"),
                "El segundo viewport debe degradar a diagn\u00f3stico sin cerrar Studio.");
            duplicateSource.RootVisual = null;
            duplicateViewport.Dispose();
        }
        viewport.Width = 426;
        viewport.Height = 240;
        viewport.Measure(new Size(426, 240));
        viewport.Arrange(new Rect(0, 0, 426, 240));
        viewport.UpdateLayout();
        Check(viewport.RenderForTest() && viewport.RenderCount >= 2,
            "HwndHost no sobrevivi\u00f3 al resize y segundo frame.");
        string capture = Path.Combine(output, "gpu-hwndhost.png");
        Check(viewport.CaptureForTest(capture) && File.Exists(capture),
            "HwndHost no produjo la captura GPU solicitada.");
        using (var stream = File.OpenRead(capture))
        {
            var frame = BitmapFrame.Create(stream, BitmapCreateOptions.DelayCreation,
                BitmapCacheOption.OnLoad);
            Check(frame.PixelWidth == 320 && frame.PixelHeight == 180,
                "La captura GPU embebida no conserva la resoluci\u00f3n interna 320x180.");
        }
        source.RootVisual = null;
        viewport.Dispose();
        Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
        Console.WriteLine("PASS WPF HwndHost GPU render and resize");
    }

    private static void VerifyAtrium3D(string output, string level, string model)
    {
        Directory.CreateDirectory(output);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(level));
        var app = new Application();
        using var source = new HwndSource(new HwndSourceParameters("Atrium 3D WPF")
        {
            Width = 960,
            Height = 540,
            WindowStyle = unchecked((int)0x80000000)
        });
        var viewport = new GpuViewportHost
        {
            Width = 960,
            Height = 540,
            OpenAtrium = true,
            LevelPath = level,
            ModelPath = model
        };
        try
        {
            source.RootVisual = viewport;
            Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
            viewport.Measure(new Size(960, 540));
            viewport.Arrange(new Rect(0, 0, 960, 540));
            viewport.UpdateLayout();
            Check(viewport.IsNativeReady && viewport.NativeModeForTest == 0,
                $"Atrium no abrió el documento en HwndHost: {viewport.LastError}");
            nint host = viewport.NativeHandleForTest;
            ulong revision = GpuHostNative.vg_gpu_host_document_revision(host);
            Check(revision != 0, "El host no expuso revisión del documento cargado.");
            Check(viewport.RenderForTest() && GpuHostNative.vg_gpu_host_readbacks(host) == 0,
                "El frame GPU normal no debe copiar la imagen a CPU.");
            Check(GpuHostNative.vg_gpu_host_camera_position(host, out float editX,
                    out float editY, out float editZ) != 0,
                "La cámara de edición no está disponible.");
            Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 1, 0, 0, 0, 0, 0) != 0 &&
                  GpuHostNative.vg_gpu_host_camera_position(host, out float idleX,
                      out float idleY, out float idleZ) != 0 &&
                  editX == idleX && editY == idleY && editZ == idleZ,
                "El movimiento no debe avanzar sin foco.");
            Check(viewport.FocusNativeForTest() && viewport.NativeHasFocus,
                "El HWND GPU no recuperó el foco.");
            float raisedZ = 0;
            Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 0, 0, 0, 1, 1) != 0 &&
                  GpuHostNative.vg_gpu_host_camera_position(host, out float raisedX,
                      out float raisedY, out raisedZ) != 0 && raisedZ > editZ,
                "En cámara libre, Espacio debe elevar la cámara de edición.");
            Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 0, 0, 0, -1, 1) != 0 &&
                  GpuHostNative.vg_gpu_host_camera_position(host, out float loweredX,
                      out float loweredY, out float loweredZ) != 0 && loweredZ < raisedZ,
                "En cámara libre, Ctrl izquierdo debe bajar la cámara de edición.");
            Check(GpuHostNative.vg_gpu_host_set_camera_mode(host, 1) != 0 &&
                  GpuHostNative.vg_gpu_host_set_camera_mode(host, 2) != 0 &&
                  GpuHostNative.vg_gpu_host_set_camera_mode(host, 0) != 0,
                "Faltan modos de cámara libre, órbita u ortográfica.");
            byte[] selection = new byte[80];
            bool picked = false;
            float pickU = 0, pickV = 0;
            foreach (float u in new[] { 0.5f, 0.45f, 0.55f, 0.35f, 0.65f })
            {
                foreach (float v in new[] { 0.5f, 0.45f, 0.55f })
                {
                    if (GpuHostNative.vg_gpu_host_pick(host, u, v, selection,
                            (nuint)selection.Length) == 0)
                        continue;
                    string uuid = GpuHostNative.Error(selection);
                    Check(uuid.Length == 36 && File.ReadAllText(level).Contains(uuid,
                        StringComparison.OrdinalIgnoreCase),
                        "Picking devolvió un UUID ajeno al documento.");
                    picked = true;
                    pickU = u;
                    pickV = v;
                    break;
                }
                if (picked) break;
            }
            Check(picked, "No se pudo seleccionar una entidad visible.");
            Check(GpuHostNative.vg_gpu_host_set_pick_mask(host, 0) != 0 &&
                  GpuHostNative.vg_gpu_host_pick(host, pickU, pickV, selection,
                      (nuint)selection.Length) == 0 && !viewport.FrameSelection(),
                "Máscara 0 debe invalidar selección e impedir encuadre.");
            Check(GpuHostNative.vg_gpu_host_set_pick_mask(host, 15) != 0 &&
                  GpuHostNative.vg_gpu_host_pick(host, pickU, pickV, selection,
                      (nuint)selection.Length) != 0 && viewport.FrameSelection(),
                "Restaurar máscara editorial debe permitir seleccionar y encuadrar.");
            Check(GpuHostNative.vg_gpu_host_document_revision(host) == revision,
                "Cámara o selección marcó dirty el documento.");
            string editCapture = Path.Combine(output, "e01-edit.png");
            Check(viewport.RenderForTest() && viewport.CaptureForTest(editCapture) &&
                File.Exists(editCapture), "No se capturó la escena en edición.");

            Check(GpuHostNative.vg_gpu_host_camera_position(host, out editX,
                out editY, out editZ) != 0, "Cámara edit no disponible tras encuadre.");
            Check(viewport.TrySetPlaying(true), "Probar no pudo activar el modo nativo.");
            Check(viewport.NativeModeForTest == 1, "Probar no creó una instancia aislada.");
            Check(GpuHostNative.vg_gpu_host_set_camera_mode(host, 2) == 0,
                "No se debe editar la cámara mientras se prueba.");
            Check(GpuHostNative.vg_gpu_host_camera_position(host, out float playX,
                    out float playY, out float playZ) != 0,
                "La cámara de prueba no está disponible.");
            for (int frame = 0; frame < 30; ++frame)
                Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 1, 0, 0, 0, 1) != 0,
                    $"Falló el frame de prueba {frame}.");
            Check(GpuHostNative.vg_gpu_host_camera_position(host, out float movedX,
                    out float movedY, out float movedZ) != 0 &&
                  Math.Abs(movedX - playX) + Math.Abs(movedY - playY) +
                  Math.Abs(movedZ - playZ) > 0.02f,
                "La prueba no produjo movimiento con colisión.");
            string playCapture = Path.Combine(output, "e01-play.png");
            Check(viewport.CaptureForTest(playCapture), "No se capturó la prueba.");
            Check(viewport.TrySetPlaying(false), "Detener no pudo activar Editar.");
            Check(viewport.NativeModeForTest == 0 && viewport.RenderForTest(),
                "Detener no restauró la edición GPU.");
            Check(GpuHostNative.vg_gpu_host_camera_position(host, out float restoredX,
                    out float restoredY, out float restoredZ) != 0 &&
                  Math.Abs(restoredX - editX) < 0.001f &&
                  Math.Abs(restoredY - editY) < 0.001f &&
                  Math.Abs(restoredZ - editZ) < 0.001f,
                "Detener no restauró la cámara de edición.");
            Check(GpuHostNative.vg_gpu_host_document_revision(host) == revision &&
                  SHA256.HashData(File.ReadAllBytes(level)).AsSpan().SequenceEqual(sourceHash),
                "Play/Stop modificó el documento o archivo de nivel.");
            Check(GpuHostNative.vg_gpu_host_readbacks(host) == 2,
                "Solo las dos capturas explícitas deben leer el framebuffer.");
            viewport.Width = 640;
            viewport.Height = 360;
            viewport.Measure(new Size(640, 360));
            viewport.Arrange(new Rect(0, 0, 640, 360));
            viewport.UpdateLayout();
            int measured = GpuHostNative.vg_gpu_host_size(host, out uint width, out uint height);
            bool frameAfterResize = viewport.RenderForTest();
            Check(measured != 0 && width == 640 && height == 360,
                $"El HWND 3D mide {width}x{height} tras resize, esperado 640x360.");
            Check(frameAfterResize, $"El frame 3D falló tras resize: {viewport.LastError}");
            Console.WriteLine($"PASS E01 HwndHost 3D GPU edit/pick/camera/play/stop/resize revision={revision}");
        }
        finally
        {
            source.RootVisual = null;
            viewport.Dispose();
        }
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            using var cycleSource = new HwndSource(new HwndSourceParameters("Atrium recreate")
            {
                Width = 640,
                Height = 360,
                WindowStyle = unchecked((int)0x80000000)
            });
            var cycleViewport = new GpuViewportHost
            {
                Width = 640, Height = 360, OpenAtrium = true,
                LevelPath = level, ModelPath = model
            };
            try
            {
                cycleSource.RootVisual = cycleViewport;
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                cycleViewport.Measure(new Size(640, 360));
                cycleViewport.Arrange(new Rect(0, 0, 640, 360));
                cycleViewport.UpdateLayout();
                Check(cycleViewport.IsNativeReady && cycleViewport.RenderForTest() &&
                      cycleViewport.TrySetPlaying(true) &&
                      cycleViewport.TrySetPlaying(false),
                    $"La recreación GPU/Play falló en ciclo {cycle}: {cycleViewport.LastError}");
            }
            finally
            {
                cycleSource.RootVisual = null;
                cycleViewport.Dispose();
            }
        }
    }

    private static void VerifyAtriumWindow(string output, string level)
    {
        Application.Current.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/retro_studio;component/Themes/Graphite.xaml")
        });
        var window = App.CreateAtriumWindow(["--atrium", "--level", level]);
        Check(window.Title.Contains("VESTIGIO") &&
              window.GpuViewport.LevelPath == level &&
              window.GpuViewport.ModelPath is not null,
            "El startup 3D debe abrir vestigio.level sin proyecto .retro.");
        var content = (FrameworkElement)window.Content;
        window.Content = null;
        using var source = new HwndSource(new HwndSourceParameters("VESTIGIO Studio UI")
        {
            Width = 1280,
            Height = 800,
            WindowStyle = unchecked((int)0x80000000)
        });
        try
        {
            source.RootVisual = content;
            Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
            content.Measure(new Size(1280, 800));
            content.Arrange(new Rect(0, 0, 1280, 800));
            content.UpdateLayout();
            Check(window.GpuViewport.IsNativeReady && window.GpuViewport.RenderForTest(),
                $"La ventana 3D no compuso su viewport GPU: {window.GpuViewport.LastError}");
            var play = window.FindName("PlayButton") as Button;
            var stop = window.FindName("StopButton") as Button;
            Check(play is not null && stop is not null && play.IsEnabled && !stop.IsEnabled,
                "La ventana 3D no inició en Editar.");
            play!.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
            Check(window.GpuViewport.NativeModeForTest == 1 && !play.IsEnabled && stop!.IsEnabled,
                "El botón Probar no sincronizó su estado con native.");
            stop!.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
            Check(window.GpuViewport.NativeModeForTest == 0 && play.IsEnabled && !stop.IsEnabled,
                "El botón Detener no restauró Editar.");
            var shell = new RenderTargetBitmap(1280, 800, 96, 96, PixelFormats.Pbgra32);
            shell.Render(content);
            var png = new PngBitmapEncoder();
            png.Frames.Add(BitmapFrame.Create(shell));
            using var image = File.Create(Path.Combine(output, "e01-studio-shell.png"));
            png.Save(image);
        }
        finally
        {
            source.RootVisual = null;
            window.GpuViewport.Dispose();
        }
        Console.WriteLine("PASS E01 startup/UI VESTIGIO 3D sin EditorDocument .retro");
    }

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            if (args.Length == 4 && args[0] == "--e01")
            {
                VerifyAtrium3D(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]),
                    Path.GetFullPath(args[3]));
                VerifyAtriumWindow(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
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

            var app = new Application();
            VerifyGpuHostLifecycle();
            VerifyGpuHwndHost(output);

            // Componer XAML real permite descubrir errores de recursos y bindings.
            // No se muestra una ventana ni se carga/guarda el layout personal.
            app.Resources.MergedDictionaries.Add(new ResourceDictionary
                { Source = new Uri("pack://application:,,,/retro_studio;component/Themes/Graphite.xaml") });
            using var showcase = new StudioViewModel(new EditorDocument(Path.GetFullPath(args[1])));
            var window = new MainWindow(showcase);
            window.TestDocument.IsSelected = true;
            window.TestDocument.IsActive = true;
            var content = (FrameworkElement)window.Content;
            window.Content = null;
            content.DataContext = showcase;
            System.Windows.Documents.TextElement.SetFontSize(content, window.FontSize);
            System.Windows.Documents.TextElement.SetFontFamily(content, window.FontFamily);
            content.Resources.MergedDictionaries.Add(window.Resources);
            // AvalonDock compone sus paneles al conectarse a una fuente WPF.
            // Un HWND oculto ejecuta ese ciclo sin mostrar ventanas al usuario.
            using (var source = new HwndSource(new HwndSourceParameters("Studio layout test")
                { Width = 1920, Height = 1080, WindowStyle = unchecked((int)0x80000000) }))
            {
                source.RootVisual = content;
                foreach (var size in new[] { new Size(1280, 800), new Size(1920, 1080) })
                {
                    Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                    content.Measure(size); content.Arrange(new Rect(size)); content.UpdateLayout();
                    Check(window.GpuViewport.IsNativeReady,
                        $"El documento Probar no aloj\u00f3 GPU al componer {size.Width:0}x{size.Height:0}.");
                    Check(window.GpuViewport.RenderForTest(),
                        $"La superficie GPU acoplada no renderiz\u00f3 a {size.Width:0}x{size.Height:0}.");
                    window.MapView.FrameScene();
                    Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                    var render = new RenderTargetBitmap((int)size.Width, (int)size.Height, 96, 96, PixelFormats.Pbgra32);
                    render.Render(content);
                    var png = new PngBitmapEncoder(); png.Frames.Add(BitmapFrame.Create(render));
                    using var file = File.Create(Path.Combine(output, $"studio-{size.Width:0}x{size.Height:0}.png"));
                    png.Save(file);
                }
                source.RootVisual = null;
                window.GpuViewport.Dispose();
            }
            Console.WriteLine("PASS WPF composition 1280x800, 1920x1080 (offscreen)");
            return 0;
        }
        catch (Exception exception) { Console.Error.WriteLine(exception); return 1; }
    }
}
