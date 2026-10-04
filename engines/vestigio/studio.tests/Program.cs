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
using Vestigio.Studio;

internal static partial class Program
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
            float actorA0 = 0, actorB0 = 0;
            Check(GpuHostNative.vg_gpu_host_animation_count(host) == 2 &&
                  GpuHostNative.vg_gpu_host_animation_height(host, 0, out actorA0) != 0 &&
                  GpuHostNative.vg_gpu_host_animation_height(host, 1, out actorB0) != 0 &&
                  Math.Abs(actorA0 - actorB0) > 0.4f,
                "Las dos instancias de animación no tienen fases independientes.");
            string animationBefore = Path.Combine(output, "w08-before.png");
            string animationAfter = Path.Combine(output, "w08-after.png");
            Check(viewport.RenderForTest() && viewport.CaptureForTest(animationBefore),
                "No se capturó la primera pose de animación GPU.");
            for (int frame = 0; frame < 30; ++frame)
                Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 0, 0, 0, 0, 1) != 0,
                    $"Falló el frame de animación {frame}.");
            Check(viewport.RenderForTest() && viewport.CaptureForTest(animationAfter) &&
                  !SHA256.HashData(File.ReadAllBytes(animationBefore)).AsSpan().SequenceEqual(
                      SHA256.HashData(File.ReadAllBytes(animationAfter))),
                "Las poses capturadas antes/después son idénticas.");
            Check(GpuHostNative.vg_gpu_host_set_camera_mode(host, 2) == 0,
                "No se debe editar la cámara mientras se prueba.");
            Check(GpuHostNative.vg_gpu_host_camera_position(host, out float playX,
                    out float playY, out float playZ) != 0,
                "La cámara de prueba no está disponible.");
            for (int frame = 0; frame < 30; ++frame)
                Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 1, 0, 0, 0, 1) != 0,
                    $"Falló el frame de prueba {frame}.");
            Check(GpuHostNative.vg_gpu_host_animation_height(host, 0, out float actorA1) != 0 &&
                  GpuHostNative.vg_gpu_host_animation_height(host, 1, out float actorB1) != 0 &&
                  actorA1 > actorA0 + 0.2f && actorB1 > actorB0 + 0.1f,
                "El clip rígido no avanzó durante Probar.");
            Check(GpuHostNative.vg_gpu_host_camera_position(host, out float movedX,
                    out float movedY, out float movedZ) != 0 &&
                  Math.Abs(movedX - playX) + Math.Abs(movedY - playY) +
                  Math.Abs(movedZ - playZ) > 0.02f,
                "La prueba no produjo movimiento con colisión.");
            string playCapture = Path.Combine(output, "e01-play.png");
            Check(viewport.CaptureForTest(playCapture), "No se capturó la prueba.");
            Check(viewport.TrySetPlaying(false), "Detener no pudo activar Editar.");
            Check(GpuHostNative.vg_gpu_host_animation_count(host) == 0,
                "Detener no liberó las instancias de animación.");
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
            Check(GpuHostNative.vg_gpu_host_readbacks(host) == 4,
                "Solo las cuatro capturas explícitas deben leer el framebuffer.");
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
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        var window = App.CreateLevelWindow(["--level", level]);
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
            window.RefreshForTest();
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
            Check(((ListBox)window.FindName("EntityList")!).Items.Count > 0,
                "La jerarquía 3D quedó vacía tras Detener.");
            content.UpdateLayout();
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

    private static void VerifyWave4Editing(string output, string level)
    {
        Directory.CreateDirectory(output);
        string sourceCopy = Path.Combine(output, "e04-source.level.json");
        string saved = Path.Combine(output, "e04-edited.level.json");
        File.Copy(level, sourceCopy, true);
        byte[] fixtureHash = SHA256.HashData(File.ReadAllBytes(level));
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(sourceCopy));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        var window = App.CreateLevelWindow(["--level", sourceCopy]);
        var content = (FrameworkElement)window.Content;
        window.Content = null;
        using (var source = new HwndSource(new HwndSourceParameters("VESTIGIO E04 editor")
        {
            Width = 1320, Height = 820, WindowStyle = unchecked((int)0x80000000)
        }))
        {
            try
            {
                source.RootVisual = content;
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                content.Measure(new Size(1320, 820));
                content.Arrange(new Rect(0, 0, 1320, 820));
                content.UpdateLayout();
                var viewport = window.GpuViewport;
                window.RefreshForTest();
                Check(viewport.IsNativeReady && viewport.RenderForTest(),
                    $"E04 no abrió viewport GPU: {viewport.LastError}");
                string initialGpu = Path.Combine(output, "e04-before.png");
                Check(viewport.CaptureForTest(initialGpu), "No se capturó Atrium antes de editar.");
                int originalCount = viewport.EntityUuids().Count;
                var add = (Button)window.FindName("AddButton")!;
                add.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.EntityUuids().Count == originalCount + 1 &&
                      viewport.IsDocumentDirty &&
                      ((TextBlock)window.FindName("DirtyMark")!).Visibility == Visibility.Visible,
                    "Añadir pilar no actualizó documento, jerarquía y dirty.");
                string addedId = viewport.SelectedUuid;
                Check(Guid.TryParse(addedId, out _), "Añadir no seleccionó UUID nuevo.");
                var hierarchy = (ListBox)window.FindName("EntityList")!;
                Check(hierarchy.Items.OfType<ListBoxItem>().Any(item =>
                      Equals(item.Tag, addedId) &&
                      item.Content?.ToString()?.Contains("Pilar nuevo") == true),
                    "El pilar nuevo no se distingue en jerarquía.");
                void Set(string name, string value) =>
                    ((TextBox)window.FindName(name)!).Text = value;
                Set("PositionX", "1.25"); Set("PositionY", "-3.25");
                Set("PositionZ", "0.95");
                Set("RotationX", "0"); Set("RotationY", "0"); Set("RotationZ", "45");
                Set("ScaleX", "0.8"); Set("ScaleY", "1.1"); Set("ScaleZ", "2");
                ulong beforeApply = GpuHostNative.vg_gpu_host_document_revision(
                    viewport.NativeHandleForTest);
                var apply = (Button)window.FindName("ApplyButton")!;
                apply.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(GpuHostNative.vg_gpu_host_document_revision(
                          viewport.NativeHandleForTest) > beforeApply &&
                      viewport.TryGetSelectedTransform(out float[] pos,
                          out float[] rot, out float[] scale) &&
                      Math.Abs(pos[0] - 1.25f) < 0.001f &&
                      Math.Abs(pos[1] + 3.25f) < 0.001f &&
                      Math.Abs(scale[2] - 2f) < 0.001f &&
                      Math.Abs(rot[2] - 0.3826834f) < 0.001f &&
                      Math.Abs(rot[3] - 0.9238795f) < 0.001f,
                    "Inspector XYZ/grados no aplicó posición, rotación y escala nativas.");
                ulong afterApply = GpuHostNative.vg_gpu_host_document_revision(
                    viewport.NativeHandleForTest);
                Set("PositionX", "NaN");
                apply.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(GpuHostNative.vg_gpu_host_document_revision(
                          viewport.NativeHandleForTest) == afterApply &&
                      !string.IsNullOrWhiteSpace(
                          ((TextBlock)window.FindName("FieldError")!).Text),
                    "Un campo inválido no debe mutar el documento y debe mostrar error.");
                Set("PositionX", "1.25");
                ((Button)window.FindName("UndoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.TryGetSelectedTransform(out pos, out _, out _) &&
                      Math.Abs(pos[0] - 1.25f) > 0.05f,
                    "Deshacer no restauró transformación anterior.");
                ((Button)window.FindName("RedoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.TryGetSelectedTransform(out pos, out rot, out scale) &&
                      Math.Abs(pos[0] - 1.25f) < 0.001f &&
                      Math.Abs(rot[2] - 0.3826834f) < 0.001f,
                    "Rehacer no restauró transformación y giro.");
                Check(window.TrySaveToPath(saved) && File.Exists(saved) &&
                      !viewport.IsDocumentDirty && window.ActiveLevelPath == saved,
                    $"Guardar copia falló: {viewport.LastError}");
                using (var savedDocument = System.Text.Json.JsonDocument.Parse(
                           File.ReadAllText(saved)))
                {
                    var entity = savedDocument.RootElement.GetProperty("entities")
                        .EnumerateArray().FirstOrDefault(item =>
                            item.GetProperty("id").GetString() == addedId);
                    Check(entity.ValueKind == System.Text.Json.JsonValueKind.Object &&
                          entity.GetProperty("components").GetProperty("engine.mesh")
                              .GetProperty("node_index").GetInt32() == 2,
                        "Archivo guardado perdió identidad o modelo del pilar.");
                }
                Check(SHA256.HashData(File.ReadAllBytes(sourceCopy)).AsSpan()
                          .SequenceEqual(sourceHash) &&
                      SHA256.HashData(File.ReadAllBytes(level)).AsSpan()
                          .SequenceEqual(fixtureHash),
                    "Guardar modificó el Atrium original o la copia fuente.");
                Set("PositionX", "1.5");
                apply.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.IsDocumentDirty, "Editar tras guardar debe marcar dirty.");
                ((Button)window.FindName("UndoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(!viewport.IsDocumentDirty &&
                      viewport.TryGetSelectedTransform(out pos, out _, out _) &&
                      Math.Abs(pos[0] - 1.25f) < 0.001f,
                    "Deshacer hasta el estado guardado debe limpiar dirty.");
                ((Button)window.FindName("RedoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.IsDocumentDirty,
                    "Rehacer desde el estado guardado debe volver a marcar dirty.");
                ((Button)window.FindName("UndoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(!viewport.IsDocumentDirty,
                    "El segundo deshacer debe restaurar el estado guardado.");
                Set("PositionY", "1.2345678");
                apply.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Set("PositionX", "1.5");
                apply.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.TryGetSelectedTransform(out pos, out _, out _) &&
                      Math.Abs(pos[1] - 1.2345678f) < 0.0000001f,
                    "Reaplicar otro campo redondeó un valor preciso del inspector.");
                ((Button)window.FindName("UndoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                ((Button)window.FindName("UndoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(!viewport.IsDocumentDirty,
                    "Deshacer ambos campos precisos debe volver al archivo guardado.");
                byte[] savedBytes = File.ReadAllBytes(saved);
                File.WriteAllText(saved, "{ invalid");
                Check(!window.TryReopen() && viewport.EntityUuids().Count ==
                      originalCount + 1 && viewport.TryGetSelectedTransform(
                          out pos, out _, out _) &&
                      Math.Abs(pos[0] - 1.25f) < 0.001f,
                    "Reabrir archivo inválido destruyó la escena activa.");
                File.WriteAllBytes(saved, savedBytes);
                Check(window.TryReopen() && viewport.EntityUuids().Count ==
                      originalCount + 1 && viewport.TrySelect(addedId) &&
                      viewport.TryGetSelectedTransform(out pos, out rot, out scale) &&
                      Math.Abs(pos[0] - 1.25f) < 0.001f &&
                      Math.Abs(rot[2] - 0.3826834f) < 0.001f &&
                      Math.Abs(scale[2] - 2f) < 0.001f,
                    $"Round-trip guardar/reabrir no conservó UUID y transform: {viewport.LastError}");
                string editedGpu = Path.Combine(output, "e04-edited.png");
                Check(viewport.RenderForTest() && viewport.CaptureForTest(editedGpu) &&
                      !SHA256.HashData(File.ReadAllBytes(initialGpu)).AsSpan()
                          .SequenceEqual(SHA256.HashData(File.ReadAllBytes(editedGpu))),
                    "No se capturó el nivel editado en GPU.");
                byte[] beforePlay = SHA256.HashData(File.ReadAllBytes(saved));
                ((Button)window.FindName("PlayButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                string playGpu = Path.Combine(output, "e04-play.png");
                Check(viewport.IsPlaying && !add.IsEnabled &&
                      !((Button)window.FindName("SaveButton")!).IsEnabled &&
                      viewport.RenderForTest() && viewport.CaptureForTest(playGpu) &&
                      !SHA256.HashData(File.ReadAllBytes(initialGpu)).AsSpan()
                          .SequenceEqual(SHA256.HashData(File.ReadAllBytes(playGpu))),
                    "Probar no mostró el objeto añadido ni bloqueó comandos de edición.");
                ((Button)window.FindName("StopButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(!viewport.IsPlaying && !viewport.IsDocumentDirty &&
                      SHA256.HashData(File.ReadAllBytes(saved)).AsSpan()
                          .SequenceEqual(beforePlay),
                    "Probar/Detener ensució o escribió el archivo guardado.");
                content.UpdateLayout();
                var shell = new RenderTargetBitmap(1320, 820, 96, 96,
                    PixelFormats.Pbgra32);
                shell.Render(content);
                var png = new PngBitmapEncoder();
                png.Frames.Add(BitmapFrame.Create(shell));
                using var image = File.Create(Path.Combine(output, "e04-studio-shell.png"));
                png.Save(image);
            }
            finally
            {
                source.RootVisual = null;
                window.GpuViewport.Dispose();
            }
        }
        Console.WriteLine("PASS E04 Studio add/transform/undo/redo/save/reopen/play GPU");
    }

    private static void VerifyE02Editing(string output, string level)
    {
        Directory.CreateDirectory(output);
        string sourceCopy = Path.Combine(output, "e02-source.level.json");
        string saved = Path.Combine(output, "e02-edited.level.json");
        File.Copy(level, sourceCopy, true);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(sourceCopy));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        var window = App.CreateLevelWindow(["--level", sourceCopy]);
        var content = (FrameworkElement)window.Content;
        window.Content = null;
        using (var source = new HwndSource(new HwndSourceParameters("VESTIGIO E02 editor")
        {
            Width = 1320, Height = 820, WindowStyle = unchecked((int)0x80000000)
        }))
        {
            try
            {
                source.RootVisual = content;
                Dispatcher.CurrentDispatcher.Invoke(() => { },
                    DispatcherPriority.ApplicationIdle);
                content.Measure(new Size(1320, 820));
                content.Arrange(new Rect(0, 0, 1320, 820));
                content.UpdateLayout();
                var viewport = window.GpuViewport;
                Check(viewport.IsNativeReady && viewport.RenderForTest(),
                    $"E02 no abrió GPU: {viewport.LastError}");
                bool FindGizmoHit(out float u, out float v, out int axis)
                {
                    for (int y = 0; y < 180; y += 2)
                        for (int x = 0; x < 320; x += 2)
                        {
                            float testU = (x + 0.5f) / 320f;
                            float testV = (y + 0.5f) / 180f;
                            int testAxis = GpuHostNative.vg_gpu_host_gizmo_hit(
                                    viewport.NativeHandleForTest,
                                    testU, testV);
                            if (testAxis > 0)
                            {
                                u = testU; v = testV; axis = testAxis - 1;
                                return true;
                            }
                        }
                    u = v = 0f; axis = -1;
                    return false;
                }
                bool HasGizmoHit() => FindGizmoHit(out _, out _, out _);
                var add = (Button)window.FindName("AddButton")!;
                add.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                string first = viewport.SelectedUuid;
                add.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                string second = viewport.SelectedUuid;
                Check(first != second && Guid.TryParse(first, out _) &&
                      Guid.TryParse(second, out _),
                    "E02 no creó dos entidades independientes.");
                var list = (ListBox)window.FindName("EntityList")!;
                list.SelectedItems.Clear();
                list.SelectedItems.Add(list.Items.OfType<ListBoxItem>()
                    .Single(item => Equals(item.Tag, first)));
                list.SelectedItems.Add(list.Items.OfType<ListBoxItem>()
                    .Single(item => Equals(item.Tag, second)));
                Check(list.SelectedItems.Count == 2 &&
                      viewport.SelectedUuids().Count == 2 &&
                      ((TextBlock)window.FindName("SelectedLabel")!).Text.Contains("2 objetos"),
                    "Multiselección WPF no llegó al Tool API nativo.");
                float XOf(string uuid)
                {
                    Check(viewport.TrySelect(uuid),
                        $"No se seleccionó {uuid}.");
                    Check(viewport.TryGetSelectedTransform(out float[] position,
                              out _, out _),
                        $"No se leyó posición de {uuid}.");
                    return position[0];
                }
                float firstX = XOf(first), secondX = XOf(second);
                Check(viewport.TrySelectMany([first, second]),
                    "No se restauró la multiselección para el gesto.");
                var tool = (ComboBox)window.FindName("GizmoTool")!;
                tool.SelectedIndex = 1;
                ((ComboBox)window.FindName("GizmoSpace")!).SelectedIndex = 1;
                ((ComboBox)window.FindName("GizmoPivot")!).SelectedIndex = 2;
                ((TextBox)window.FindName("GizmoSnap")!).Text = "0.25";
                ((ComboBox)window.FindName("GizmoAxis")!).SelectedIndex = 0;
                Check(viewport.TrySetGizmoOptions(1, 0, 1, 2, 0.25f),
                    "Controles de espacio local, pivote y ajuste no configuran gizmo.");
                ulong before = GpuHostNative.vg_gpu_host_document_revision(
                    viewport.NativeHandleForTest);
                Check(viewport.TryBeginGesture(0) && viewport.TryUpdateGesture(0.34f),
                    $"Preview de gesto E02 falló: {viewport.LastError}");
                Check(GpuHostNative.vg_gpu_host_document_revision(
                          viewport.NativeHandleForTest) == before,
                    "Preview efímero cambió revisión de documento.");
                Check(viewport.TryEndGesture(false) &&
                      GpuHostNative.vg_gpu_host_document_revision(
                          viewport.NativeHandleForTest) == before,
                    "Escape/cancelación de gesto alteró revisión.");
                Check(viewport.TryBeginGesture(0) && viewport.TryUpdateGesture(0.34f) &&
                      viewport.TryEndGesture(true) &&
                      GpuHostNative.vg_gpu_host_document_revision(
                          viewport.NativeHandleForTest) == before + 1,
                    $"Un drag no fue exactamente un comando: {viewport.LastError}");
                Check(Math.Abs(XOf(first) - firstX - 0.25f) < 0.002f &&
                      Math.Abs(XOf(second) - secondX - 0.25f) < 0.002f,
                    "Mover en lote con snap local no aplicó +0,25 m a ambos UUID.");
                Check(viewport.TrySelectMany([first, second]),
                    "No se restauró selección tras leer transforms.");
                Check(viewport.TryUndo() &&
                      viewport.TrySelectMany([first, second]) &&
                      viewport.TryBeginGesture(0) &&
                      viewport.TryUpdateGesture(0.12f) &&
                      viewport.TryEndGesture(false) &&
                      viewport.TryRedo(),
                    "Cancelación de gesto destruyó redo o undo/redo del lote falló.");
                Check(Math.Abs(XOf(first) - firstX - 0.25f) < 0.002f &&
                      Math.Abs(XOf(second) - secondX - 0.25f) < 0.002f,
                    "Redo no restauró ambos valores transformados.");
                Check(viewport.TrySelectMany([first, second]),
                    "No se restauró selección para rotación.");
                tool.SelectedIndex = 2;
                Check(viewport.TrySetGizmoOptions(2, 2, 0, 0, 15f) &&
                      viewport.TryBeginGesture(2) &&
                      viewport.TryUpdateGesture(0.34f) &&
                      viewport.TryEndGesture(true),
                    $"Rotación múltiple con snap falló: {viewport.LastError}");
                foreach (string id in new[] { first, second })
                {
                    Check(viewport.TrySelect(id) &&
                          viewport.TryGetSelectedTransform(out _, out float[] q,
                              out _) &&
                          Math.Abs(Math.Abs(q[2]) - 0.1305262f) < 0.01f,
                        "Rotación múltiple no aplicó 15° alrededor de Z.");
                }
                Check(viewport.TrySelectMany([first, second]),
                    "No se restauró selección para capturar rotación.");
                Check(viewport.RenderForTest() && viewport.CaptureForTest(
                          Path.Combine(output, "e02-rotate-gpu.png")) && HasGizmoHit(),
                    "Gizmo de rotación no se capturó o no responde a hit-test GPU.");
                tool.SelectedIndex = 3;
                Check(viewport.TrySetGizmoOptions(3, 2, 1, 2, 0.1f) &&
                      viewport.TryBeginGesture(2) &&
                      viewport.TryUpdateGesture(0.18f) &&
                      viewport.TryEndGesture(true),
                    $"Escalado múltiple con snap falló: {viewport.LastError}");
                foreach (string id in new[] { first, second })
                {
                    Check(viewport.TrySelect(id) &&
                          viewport.TryGetSelectedTransform(out _, out _,
                              out float[] scale) &&
                          Math.Abs(scale[2] - 2.28f) < 0.01f,
                        "Escalado local múltiple no aplicó factor 1,2.");
                }
                Check(viewport.TrySelectMany([first, second]) &&
                      viewport.RenderForTest() && viewport.CaptureForTest(
                          Path.Combine(output, "e02-scale-gpu.png")) && HasGizmoHit(),
                    "Gizmo de escala no se capturó o no responde a hit-test GPU.");
                tool.SelectedIndex = 1;
                Check(viewport.TrySetGizmoOptions(1, 0, 0, 0, 0.25f) &&
                      viewport.RenderForTest() &&
                      viewport.CaptureForTest(Path.Combine(output, "e02-gizmo-gpu.png")) &&
                      HasGizmoHit(),
                    "Gizmo de movimiento no se capturó o no responde a hit-test GPU.");
                int count = viewport.EntityUuids().Count;
                ((Button)window.FindName("DuplicateButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.EntityUuids().Count == count + 2 &&
                      viewport.SelectedUuids().Count == 2,
                    $"Duplicar lote no remapeó dos entidades: {viewport.LastError}");
                string[] duplicates = viewport.SelectedUuids().ToArray();
                Check(duplicates.All(id => id != first && id != second),
                    "Duplicar lote no asignó UUID nuevos.");
                var parents = (ComboBox)window.FindName("ParentTarget")!;
                parents.SelectedItem = parents.Items.OfType<ComboBoxItem>()
                    .Single(item => Equals(item.Tag, first));
                ((Button)window.FindName("ReparentButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(string.IsNullOrWhiteSpace(
                          ((TextBlock)window.FindName("FieldError")!).Text),
                    "Reparentar lote válido informó error.");
                ((Button)window.FindName("DeleteButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.EntityUuids().Count == count,
                    "Borrar selección no quitó lote completo.");
                Check(viewport.TryUndo() && viewport.EntityUuids().Count == count + 2,
                    "Undo de borrado no restauró lote.");
                Check(window.TrySaveToPath(saved) && window.TryReopen() &&
                      viewport.EntityUuids().Count == count + 2 &&
                      SHA256.HashData(File.ReadAllBytes(sourceCopy)).AsSpan()
                          .SequenceEqual(sourceHash),
                    $"Round-trip de lote cambió fuente o perdió objetos: {viewport.LastError}");
                using (var savedDocument = System.Text.Json.JsonDocument.Parse(
                           File.ReadAllText(saved)))
                {
                    var entities = savedDocument.RootElement.GetProperty("entities")
                        .EnumerateArray().ToDictionary(entity =>
                            entity.GetProperty("id").GetString()!);
                    foreach (string id in new[] { first, second })
                    {
                        var transform = entities[id].GetProperty("transform");
                        Check(Math.Abs(transform.GetProperty("position")[0]
                                           .GetSingle() - 0.25f) < 0.002f &&
                              Math.Abs(Math.Abs(transform.GetProperty("rotation")[2]
                                                .GetSingle()) - 0.1305262f) < 0.01f &&
                              Math.Abs(transform.GetProperty("scale")[2]
                                           .GetSingle() - 2.28f) < 0.01f,
                            "Round-trip perdió move/rotate/scale en entidad original.");
                    }
                    foreach (string id in duplicates)
                        Check(entities[id].GetProperty("parent").GetString() == first,
                            "Round-trip perdió remap/parent de duplicado.");
                }
                Check(viewport.RenderForTest() &&
                      viewport.CaptureForTest(Path.Combine(output, "e02-edited-gpu.png")),
                    "No se capturó escena editada en GPU.");
                Check(viewport.TrySelect(first) &&
                      viewport.TrySetGizmoOptions(1, 0, 0, 0, 0.25f) &&
                      viewport.SetCameraMode(1),
                    "No se preparó órbita para el arrastre proyectado.");
                float hitU = 0f, hitV = 0f;
                int hitAxis = -1;
                Check(GpuHostNative.vg_gpu_host_frame(viewport.NativeHandleForTest,
                          0.016, 0, 0, 170, 0, 0, 1) != 0 &&
                      viewport.RenderForTest() &&
                      FindGizmoHit(out hitU, out hitV, out hitAxis),
                    "No se encontró gizmo tras orbitar cámara.");
                float dx = 0f, dy = 0f;
                Check(GpuHostNative.vg_gpu_host_gizmo_drag_direction(
                          viewport.NativeHandleForTest, hitU, hitV, hitAxis,
                          out dx, out dy) != 0 &&
                      Math.Abs(dx * dx + dy * dy - 1f) < 0.02f,
                    "Dirección proyectada del gizmo orbitado no es unitaria.");
                float[] beforeDrag = [];
                uint width = 0, height = 0;
                Check(viewport.TryGetSelectedTransform(out beforeDrag,
                          out _, out _) &&
                      GpuHostNative.vg_gpu_host_size(viewport.NativeHandleForTest,
                          out width, out height) != 0,
                    "No se obtuvo transform o tamaño antes de arrastrar.");
                int startX = (int)(hitU * width), startY = (int)(hitV * height);
                float[] afterDrag = [];
                Check(viewport.TryBeginPointerGesture(hitU, hitV, startX, startY) &&
                      viewport.TryUpdatePointerGesture(startX + (int)(dx * 80),
                          startY + (int)(dy * 80)) &&
                      viewport.TryEndGesture(true) &&
                      viewport.TryGetSelectedTransform(out afterDrag,
                          out _, out _) &&
                      afterDrag[hitAxis] - beforeDrag[hitAxis] > 1.2f,
                    "Arrastre orbitado no siguió el eje proyectado del gizmo.");
                Check(viewport.RenderForTest() && viewport.CaptureForTest(
                          Path.Combine(output, "e02-orbit-drag-gpu.png")),
                    "No se capturó resultado del arrastre con cámara orbitada.");
            }
            finally
            {
                source.RootVisual = null;
                window.GpuViewport.Dispose();
            }
        }
        Console.WriteLine("PASS E02 WPF multi, gizmo, gesture cancel/commit, batch, save/reopen GPU");
    }

    private static void VerifyE03Studio(string output, string level, string model)
    {
        Directory.CreateDirectory(output);
        string sourceCopy = Path.Combine(output, "e03-source.level.json");
        string savedDirectory = Path.Combine(output, "saved");
        Directory.CreateDirectory(savedDirectory);
        string saved = Path.Combine(savedDirectory, "e03-edited.level.json");
        File.Copy(level, sourceCopy, true);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(sourceCopy));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        var window = App.CreateLevelWindow(["--level", sourceCopy]);
        var content = (FrameworkElement)window.Content;
        window.Content = null;
        using var source = new HwndSource(new HwndSourceParameters("VESTIGIO E03 editor")
        {
            Width = 1400, Height = 900, WindowStyle = unchecked((int)0x80000000)
        });
        try
        {
            source.RootVisual = content;
            Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
            content.Measure(new Size(1400, 900));
            content.Arrange(new Rect(0, 0, 1400, 900));
            content.UpdateLayout();
            var viewport = window.GpuViewport;
            Check(viewport.IsNativeReady && viewport.RenderForTest(),
                $"E03 no abrió GPU: {viewport.LastError}");
            string[] originalAssets = viewport.AssetLibrary()
                .Select(asset => asset.Id).ToArray();
            ulong originalRevision = GpuHostNative.vg_gpu_host_document_revision(
                viewport.NativeHandleForTest);
            Check(window.ImportAssetForTest(model),
                $"No se importó GLB en Studio: {viewport.LastError}");
            var assets = viewport.AssetLibrary();
            Check(assets.Count == originalAssets.Length + 1,
                "La biblioteca no añadió exactamente un asset importado.");
            var imported = assets.Single(asset => !originalAssets.Contains(asset.Id));
            Check(Guid.TryParse(imported.Id, out _) && imported.Fingerprint.Length > 0,
                "Importación no expuso identidad y fingerprint.");
            ulong importedRevision = GpuHostNative.vg_gpu_host_document_revision(
                viewport.NativeHandleForTest);
            Check(importedRevision >= originalRevision,
                "La importación retrocedió revisión del documento.");
            var assetList = (ListBox)window.FindName("AssetList")!;
            assetList.SelectedItem = assetList.Items.OfType<ListBoxItem>()
                .Single(row => (string?)row.Tag == imported.Id);
            ((Button)window.FindName("PreviewAssetButton")!).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
            string previewDetails = ((TextBlock)window.FindName("AssetDetails")!).Text;
            bool previewReady = previewDetails.Contains("GPU temporal");
            Check(GpuHostNative.vg_gpu_host_document_revision(
                      viewport.NativeHandleForTest) == importedRevision,
                "Vista previa cambió revisión del documento.");
            if (previewReady)
                Check(viewport.RenderForTest() && viewport.CaptureForTest(
                          Path.Combine(output, "e03-preview-gpu.png")),
                    "No se pudo capturar preview GPU.");
            var place = (Button)window.FindName("PlaceAssetButton")!;
            Check(place.IsEnabled, "Colocar no está habilitado para un asset listo.");
            place.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
            string first = viewport.SelectedUuid;
            Check(Guid.TryParse(first, out _), "Colocar no creó entidad estable.");
            place.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
            string second = viewport.SelectedUuid;
            Check(first != second && Guid.TryParse(second, out _),
                "Dos colocaciones no crearon entidades independientes.");
            Check(viewport.TryPreviewAsset("") && viewport.RenderForTest() &&
                  viewport.CaptureForTest(Path.Combine(output,
                      "e03-placed-visible-gpu.png")),
                "No se capturó modelo colocado antes de ocultarlo.");
            Check(viewport.TrySelectMany([first, second]), "No se logró multiselección.");
            window.RefreshForTest();
            Check(window.InspectorInputForTest("transform.position.x") is TextBox xField,
                "Inspector no expuso posición tipada.");
            var positionInput = (TextBox)window.InspectorInputForTest("transform.position.x")!;
            positionInput.Text = "2.25";
            ulong beforeBatch = GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest);
            ((Button)window.FindName("ApplyFieldsButton")!).RaiseEvent(
                new RoutedEventArgs(ButtonBase.ClickEvent));
            Check(GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest) ==
                  beforeBatch + 1, "Multi-edición no creó un solo comando undo.");
            foreach (string id in new[] { first, second })
            {
                Check(viewport.TrySelect(id) && viewport.TryGetSelectedTransform(
                          out float[] position, out _, out _) &&
                      Math.Abs(position[0] - 2.25f) < 0.001f,
                    "Multi-edición no aplicó X a ambos objetos.");
            }
            Check(viewport.TryUndo(), "No se pudo deshacer multi-edición.");
            foreach (string id in new[] { first, second })
            {
                Check(viewport.TrySelect(id) && viewport.TryGetSelectedTransform(
                          out float[] position, out _, out _) &&
                      Math.Abs(position[0] - 2.25f) > 0.1f,
                    "Undo no revirtió todo el lote.");
            }
            Check(viewport.TryRedo() && viewport.TrySelectMany([first, second]),
                "Redo de lote falló.");
            window.RefreshForTest();
            ((TextBox)window.InspectorInputForTest("transform.scale.x")!).Text = "0";
            ((Button)window.FindName("ApplyFieldsButton")!).RaiseEvent(
                new RoutedEventArgs(ButtonBase.ClickEvent));
            Check(window.InspectorErrorForTest("transform.scale.x").Length > 0 &&
                  ((ListBox)window.FindName("ProblemsList")!).Items.Count > 0,
                "Valor fuera de rango no apareció junto al campo y en Problemas.");
            window.RefreshForTest();
            ((TextBox)window.FindName("EditorLayer")!).Text = "Arquitectura";
            ((TextBox)window.FindName("EditorGroup")!).Text = "Hall";
            ((Button)window.FindName("ApplyOrganizationButton")!).RaiseEvent(
                new RoutedEventArgs(ButtonBase.ClickEvent));
            Check(viewport.TryGetEntityEditor(first, out string metadata) &&
                  metadata.Contains("Arquitectura") && metadata.Contains("Hall"),
                "Grupo/capa editor no llegó al documento nativo.");
            var hide = (CheckBox)window.FindName("HideLayer")!;
            hide.IsChecked = true;
            Check(viewport.TryGetEntityEditor(first, out metadata) &&
                  metadata.Contains("\"hidden\":true"),
                $"Ocultar capa no guardó estado editorial: {metadata}; " +
                $"UI={hide.IsChecked}, error={viewport.LastError}, " +
                $"status={((TextBlock)window.FindName("StatusText")!).Text}");
            Check(viewport.RenderForTest() && viewport.CaptureForTest(
                      Path.Combine(output, "e03-hidden-editor-gpu.png")) &&
                  viewport.TrySetPlaying(true) && viewport.RenderForTest() &&
                  viewport.CaptureForTest(Path.Combine(output, "e03-play-visible-gpu.png")) &&
                  viewport.TrySetPlaying(false) &&
                  viewport.TryGetEntityEditor(first, out metadata) &&
                  metadata.Contains("\"hidden\":true"),
                "Ocultar capa editorial afectó el modo Probar o perdió metadatos.");
            Check(viewport.TrySelect(first) && viewport.TrySetSelectedTransform(
                      [1.5f, 0f, 1f], [0f, 0f, 0f, 1f], [1f, 1f, 1f]),
                "No se pudo transformar modelo colocado.");
            Check(window.TrySaveToPath(saved) && window.TryReopen(),
                $"Save As/Reopen falló: {viewport.LastError}");
            Check(SHA256.HashData(File.ReadAllBytes(sourceCopy)).AsSpan()
                .SequenceEqual(sourceHash), "Se alteró el archivo fuente.");
            using (var json = System.Text.Json.JsonDocument.Parse(File.ReadAllText(saved)))
            {
                var root = json.RootElement;
                var manifest = root.GetProperty("assets").EnumerateArray()
                    .Single(asset => asset.GetProperty("id").GetString() == imported.Id);
                Check(manifest.GetProperty("fingerprint").GetString()?.Length > 0,
                    "Round-trip perdió fingerprint del asset.");
                string assetSource = manifest.GetProperty("source").GetString() ?? "";
                string copiedModel = Path.GetFullPath(Path.Combine(
                    Path.GetDirectoryName(saved)!, assetSource));
                Check(File.Exists(copiedModel) &&
                      SHA256.HashData(File.ReadAllBytes(copiedModel)).AsSpan()
                          .SequenceEqual(SHA256.HashData(File.ReadAllBytes(model))),
                    "Round-trip perdió bytes de malla/material del GLB importado.");
                var entities = root.GetProperty("entities").EnumerateArray().ToDictionary(
                    item => item.GetProperty("id").GetString()!);
                foreach (string id in new[] { first, second })
                    Check(entities[id].GetProperty("components").GetProperty("engine.mesh")
                              .GetProperty("asset").GetString() == imported.Id,
                        "Round-trip perdió referencia de modelo/material por entidad.");
            }
            Check(viewport.AssetLibrary().Any(asset => asset.Id == imported.Id),
                "Reabrir perdió identidad del asset.");
            Check(viewport.TryRenameAsset(imported.Id, "Escultura") &&
                  viewport.TryReimportAsset(imported.Id, model) &&
                  viewport.AssetLibrary().Any(asset => asset.Id == imported.Id &&
                      asset.Name == "Escultura"),
                $"Rename/Reimport perdió identidad: {viewport.LastError}");
            Check(viewport.RenderForTest() && viewport.CaptureForTest(
                      Path.Combine(output, "e03-reopened-hidden-gpu.png")),
                "No se capturó modelo editado en GPU.");
            content.UpdateLayout();
            var studioImage = new RenderTargetBitmap(1400, 900, 96, 96,
                PixelFormats.Pbgra32);
            studioImage.Render(content);
            var studioPng = new PngBitmapEncoder();
            studioPng.Frames.Add(BitmapFrame.Create(studioImage));
            using (var stream = File.Create(Path.Combine(output, "e03-studio.png")))
                studioPng.Save(stream);
            Check(previewReady,
                $"No se activó preview GPU: {previewDetails}; {viewport.LastError}");
        }
        finally
        {
            source.RootVisual = null;
            window.GpuViewport.Dispose();
        }
        Console.WriteLine("PASS E03 WPF import/preview/place/inspector/layer/save/reopen/rename GPU");
    }

    private static void VerifyWave9Room(string output, string level)
    {
        Directory.CreateDirectory(output);
        string sourceCopy = Path.Combine(output, "w09-source.level.json");
        string saved = Path.Combine(output, "w09-room.level.json");
        File.Copy(level, sourceCopy, true);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(sourceCopy));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        var window = App.CreateLevelWindow(["--level", sourceCopy]);
        var content = (FrameworkElement)window.Content;
        window.Content = null;
        using (var source = new HwndSource(new HwndSourceParameters("VESTIGIO W09 room")
        {
            Width = 1320, Height = 820, WindowStyle = unchecked((int)0x80000000)
        }))
        {
            try
            {
                source.RootVisual = content;
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                content.Measure(new Size(1320, 820));
                content.Arrange(new Rect(0, 0, 1320, 820));
                content.UpdateLayout();
                var viewport = window.GpuViewport;
                window.RefreshForTest();
                Check(viewport.IsNativeReady && viewport.RenderForTest(),
                    $"W09 no abrió viewport GPU: {viewport.LastError}");
                string beforeGpu = Path.Combine(output, "w09-before.png");
                Check(viewport.CaptureForTest(beforeGpu), "No se capturó Atrium antes de añadir sala.");
                int originalCount = viewport.EntityUuids().Count;
                var addRoom = (Button)window.FindName("AddRoomButton")!;
                addRoom.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                string selected = viewport.SelectedUuid;
                var hierarchy = (ListBox)window.FindName("EntityList")!;
                var transform = (StackPanel)window.FindName("TransformPanel")!;
                var duplicate = (Button)window.FindName("DuplicateButton")!;
                Check(viewport.EntityUuids().Count == originalCount + 6 &&
                      viewport.IsDocumentDirty && Guid.TryParse(selected, out _) &&
                      ((TextBlock)window.FindName("DirtyMark")!).Visibility == Visibility.Visible &&
                      hierarchy.Items.OfType<ListBoxItem>().Count(item =>
                          item.Content?.ToString()?.Contains("Sala:") == true) == 6 &&
                      hierarchy.SelectedItem is ListBoxItem { Tag: string id } && id == selected &&
                      !transform.IsEnabled && !duplicate.IsEnabled &&
                      ((TextBlock)window.FindName("SelectedLabel")!).Text.Contains("plantilla fija"),
                    "Crear habitación no actualizó seis piezas, etiquetas, selección y dirty.");
                string roomGpu = Path.Combine(output, "w09-room.png");
                Check(viewport.RenderForTest() && viewport.CaptureForTest(roomGpu) &&
                      !SHA256.HashData(File.ReadAllBytes(beforeGpu)).AsSpan().SequenceEqual(
                          SHA256.HashData(File.ReadAllBytes(roomGpu))),
                    "La sala añadida no produjo una captura GPU diferente.");
                ulong roomRevision = GpuHostNative.vg_gpu_host_document_revision(
                    viewport.NativeHandleForTest);
                addRoom.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.EntityUuids().Count == originalCount + 6 &&
                      GpuHostNative.vg_gpu_host_document_revision(
                          viewport.NativeHandleForTest) == roomRevision &&
                      !string.IsNullOrWhiteSpace(
                          ((TextBlock)window.FindName("FieldError")!).Text),
                    "Segunda habitación debe rechazarse sin mutar el documento y mostrar error.");
                ((Button)window.FindName("UndoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.EntityUuids().Count == originalCount && !viewport.IsDocumentDirty,
                    "Deshacer habitación no restauró el documento original.");
                ((Button)window.FindName("RedoButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.EntityUuids().Count == originalCount + 6 &&
                      viewport.IsDocumentDirty,
                    "Rehacer habitación no restauró las seis piezas.");
                Check(window.TrySaveToPath(saved) && !viewport.IsDocumentDirty &&
                      File.Exists(saved), $"Guardar habitación falló: {viewport.LastError}");
                Check(window.TryReopen() && viewport.EntityUuids().Count == originalCount + 6 &&
                      hierarchy.Items.OfType<ListBoxItem>().Count(item =>
                          item.Content?.ToString()?.Contains("Sala:") == true) == 6,
                    $"Reabrir perdió geometría o etiquetas de habitación: {viewport.LastError}");
                byte[] savedHash = SHA256.HashData(File.ReadAllBytes(saved));
                ((Button)window.FindName("PlayButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.IsPlaying && !addRoom.IsEnabled && !viewport.TryAddRoom() &&
                      viewport.RenderForTest(),
                    "Probar no bloqueó edición de habitación o dejó de renderizar.");
                ((Button)window.FindName("StopButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(!viewport.IsPlaying && addRoom.IsEnabled && !viewport.IsDocumentDirty &&
                      SHA256.HashData(File.ReadAllBytes(saved)).AsSpan().SequenceEqual(savedHash) &&
                      SHA256.HashData(File.ReadAllBytes(sourceCopy)).AsSpan().SequenceEqual(sourceHash),
                    "Probar/Detener modificó el archivo guardado o el Atrium fuente.");
                var addPillar = (Button)window.FindName("AddButton")!;
                addPillar.RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.EntityUuids().Count == originalCount + 7 &&
                      transform.IsEnabled && duplicate.IsEnabled &&
                      !((TextBlock)window.FindName("SelectedLabel")!).Text.Contains("plantilla fija"),
                    "El pilar normal debe conservar inspector y duplicación editables.");
                Check(viewport.TrySelect(selected) && !transform.IsEnabled &&
                      !duplicate.IsEnabled,
                    "Volver a seleccionar la pieza de sala debe bloquear edición de plantilla.");
            }
            finally
            {
                source.RootVisual = null;
                window.GpuViewport.Dispose();
            }
        }
        Console.WriteLine("PASS W09 Studio room six pieces/labels/undo/redo/save/reopen/play GPU");
    }

    private static void VerifyE04RoomRecipe(string output, string level)
    {
        Directory.CreateDirectory(output);
        string sourceCopy = Path.Combine(output, "e04-room-source.level.json");
        string saved = Path.Combine(output, "e04-room-saved.level.json");
        File.Copy(level, sourceCopy, true);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(sourceCopy));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        var window = App.CreateLevelWindow(["--level", sourceCopy]);
        var content = (FrameworkElement)window.Content;
        window.Content = null;
        using var source = new HwndSource(new HwndSourceParameters("VESTIGIO E04 recipe")
        {
            Width = 1320, Height = 820, WindowStyle = unchecked((int)0x80000000)
        });
        try
        {
            source.RootVisual = content;
            Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
            content.Measure(new Size(1320, 820));
            content.Arrange(new Rect(0, 0, 1320, 820));
            content.UpdateLayout();
            var viewport = window.GpuViewport;
            window.RefreshForTest();
            Check(viewport.IsNativeReady && viewport.RenderForTest(),
                $"E04 recipe no abrió GPU: {viewport.LastError}");
            int initialCount = viewport.EntityUuids().Count;
            ulong initialRevision = GpuHostNative.vg_gpu_host_document_revision(
                viewport.NativeHandleForTest);
            string initialGpu = Path.Combine(output, "e04-room-before.png");
            Check(viewport.CaptureForTest(initialGpu),
                "No se capturó escena antes de preview.");
            static void Click(Vestigio3DWindow window, string name) =>
                ((Button)window.FindName(name)!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
            ((TextBox)window.FindName("RoomVertices")!).Text =
                "-2,-3\n2,-3\n3,1\n0,3\n-2,1";
            ((TextBox)window.FindName("OpeningEdge")!).Text = "0";
            ((TextBox)window.FindName("OpeningOffset")!).Text = "1.5";
            Click(window, "AddOpeningButton");
            Click(window, "PreviewRoomButton");
            Check(viewport.EntityUuids().Count == initialCount &&
                  GpuHostNative.vg_gpu_host_document_revision(
                      viewport.NativeHandleForTest) == initialRevision &&
                  !viewport.IsDocumentDirty && viewport.RenderForTest() &&
                  viewport.CaptureForTest(Path.Combine(output, "e04-room-preview.png")),
                "Preview creó entidades, ensució el documento o no se dibujó.");
            Click(window, "CancelRoomPreviewButton");
            string afterCancelGpu = Path.Combine(output, "e04-room-cancelled.png");
            Check(viewport.EntityUuids().Count == initialCount &&
                  GpuHostNative.vg_gpu_host_document_revision(
                      viewport.NativeHandleForTest) == initialRevision &&
                  viewport.RenderForTest() && viewport.CaptureForTest(afterCancelGpu) &&
                  SHA256.HashData(File.ReadAllBytes(initialGpu)).AsSpan().SequenceEqual(
                      SHA256.HashData(File.ReadAllBytes(afterCancelGpu))),
                "Cancelar preview dejó objetos huérfanos o píxeles residuales.");
            Click(window, "CommitRoomButton");
            string roomId = viewport.SelectedUuid;
            Check(Guid.TryParse(roomId, out _) &&
                  viewport.EntityUuids().Count > initialCount &&
                  viewport.IsDocumentDirty &&
                  viewport.TryGetRoomRecipe(roomId, out string recipe) &&
                  recipe.Contains("\"door\"", StringComparison.Ordinal) &&
                  recipe.Contains("\"vertices\"", StringComparison.Ordinal),
                $"Crear habitación no conservó receta/selección: {viewport.LastError}");
            string createdGpu = Path.Combine(output, "e04-room-created.png");
            Check(viewport.RenderForTest() && viewport.CaptureForTest(createdGpu) &&
                  !SHA256.HashData(File.ReadAllBytes(createdGpu)).AsSpan().SequenceEqual(
                      SHA256.HashData(File.ReadAllBytes(initialGpu))),
                "Crear y encuadrar no hizo visible la habitación en GPU.");
            Click(window, "OpenRoomEditorButton");
            Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.Render);
            var shell = new RenderTargetBitmap(1320, 820, 96, 96,
                PixelFormats.Pbgra32);
            shell.Render(content);
            var shellPng = new PngBitmapEncoder();
            shellPng.Frames.Add(BitmapFrame.Create(shell));
            using (var image = File.Create(Path.Combine(output, "e04-room-studio-shell.png")))
                shellPng.Save(image);
            var openings = (ListBox)window.FindName("RoomOpenings")!;
            openings.SelectedIndex = -1;
            ((TextBox)window.FindName("OpeningEdge")!).Text = "2";
            ((ComboBox)window.FindName("OpeningKind")!).SelectedIndex = 1;
            ((TextBox)window.FindName("OpeningOffset")!).Text = "1";
            ((TextBox)window.FindName("OpeningHeight")!).Text = "1";
            ((TextBox)window.FindName("OpeningSill")!).Text = "1";
            Click(window, "AddOpeningButton");
            openings.SelectedIndex = -1;
            ((TextBox)window.FindName("OpeningEdge")!).Text = "4";
            ((ComboBox)window.FindName("OpeningKind")!).SelectedIndex = 2;
            ((TextBox)window.FindName("OpeningSill")!).Text = "0";
            Click(window, "AddOpeningButton");
            ((TextBox)window.FindName("RoomHeight")!).Text = "4";
            Click(window, "CommitRoomButton");
            Check(viewport.TryGetRoomRecipe(roomId, out recipe) &&
                  recipe.Contains("\"window\"", StringComparison.Ordinal) &&
                  recipe.Contains("\"gap\"", StringComparison.Ordinal),
                "Actualizar habitación perdió ventana/hueco.");
            Click(window, "UndoButton");
            Check(viewport.TryGetRoomRecipe(roomId, out recipe) &&
                  !recipe.Contains("\"window\"", StringComparison.Ordinal) &&
                  openings.Items.Count == 1 &&
                  ((TextBox)window.FindName("RoomHeight")!).Text == "3",
                "Deshacer no restauró receta y campos visibles anteriores.");
            Click(window, "RedoButton");
            Check(viewport.TryGetRoomRecipe(roomId, out recipe) &&
                  recipe.Contains("\"window\"", StringComparison.Ordinal) &&
                  openings.Items.Count == 3 &&
                  ((TextBox)window.FindName("RoomHeight")!).Text == "4",
                "Rehacer no restauró abertura y campos visibles.");
            Check(viewport.TrySelectMany([]), "No se pudo limpiar selección para otra planta.");
            window.RefreshForTest();
            ((TextBox)window.FindName("RoomFloor")!).Text = "3";
            ((TextBox)window.FindName("RoomVertices")!).Text =
                "0,-1\n4,-1\n4,3\n0,3";
            openings.Items.Clear();
            Click(window, "CommitRoomButton");
            string upperRoomId = viewport.SelectedUuid;
            bool hasUpperRecipe = viewport.TryGetRoomRecipe(upperRoomId,
                out string upperRecipe);
            using var parsedUpper = hasUpperRecipe
                ? System.Text.Json.JsonDocument.Parse(upperRecipe) : null;
            Check(upperRoomId != roomId &&
                  hasUpperRecipe &&
                  parsedUpper?.RootElement.GetProperty("floor_z").GetSingle() == 3,
                $"La segunda planta no se creó: primera={roomId}, activa={upperRoomId}, " +
                $"error={((TextBlock)window.FindName("RoomEditorError")!).Text}, " +
                $"botón={((Button)window.FindName("CommitRoomButton")!).Content}, " +
                $"nativa={viewport.LastError}, receta={upperRecipe}");
            var plan = (RoomPlanView)window.FindName("RoomPlan")!;
            var floorView = (ComboBox)window.FindName("RoomFloorView")!;
            floorView.Text = "3";
            window.RefreshForTest();
            byte[] PlanPixels()
            {
                plan.UpdateLayout();
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.Render);
                var image = new RenderTargetBitmap(
                    Math.Max(1, (int)Math.Ceiling(plan.ActualWidth)),
                    Math.Max(1, (int)Math.Ceiling(plan.ActualHeight)),
                    96, 96, PixelFormats.Pbgra32);
                image.Render(plan);
                byte[] pixels = new byte[image.PixelWidth * image.PixelHeight * 4];
                image.CopyPixels(pixels, image.PixelWidth * 4, 0);
                return pixels;
            }
            byte[] noGhost = SHA256.HashData(PlanPixels());
            ((CheckBox)window.FindName("RoomGhost")!).IsChecked = true;
            Check(!SHA256.HashData(PlanPixels()).AsSpan().SequenceEqual(noGhost),
                $"Ghosting no cambió el plano de dos plantas: " +
                $"rooms={plan.Rooms.Count}, floor={plan.ActiveFloor}, " +
                $"ghost={plan.ShowGhost}, size={plan.ActualWidth}x{plan.ActualHeight}.");
            var planImage = new RenderTargetBitmap(
                Math.Max(1, (int)Math.Ceiling(plan.ActualWidth)),
                Math.Max(1, (int)Math.Ceiling(plan.ActualHeight)),
                96, 96, PixelFormats.Pbgra32);
            planImage.Render(plan);
            var planPng = new PngBitmapEncoder();
            planPng.Frames.Add(BitmapFrame.Create(planImage));
            using (var image = File.Create(Path.Combine(output, "e04-room-plan-ghost.png")))
                planPng.Save(image);
            byte[] grid = SHA256.HashData(PlanPixels());
            ((CheckBox)window.FindName("RoomGrid")!).IsChecked = false;
            Check(!SHA256.HashData(PlanPixels()).AsSpan().SequenceEqual(grid),
                "Cuadrícula no cambió el plano editorial.");
            Check(window.TrySaveToPath(saved) && window.TryReopen() &&
                  viewport.TryGetRoomRecipe(roomId, out recipe) &&
                  recipe.Contains("\"gap\"", StringComparison.Ordinal) &&
                  viewport.TryGetRoomRecipe(upperRoomId, out _) &&
                  viewport.RenderForTest() &&
                  viewport.CaptureForTest(Path.Combine(output, "e04-room-reopened.png")),
                $"Round-trip habitación falló: {viewport.LastError}");
            Click(window, "PlayButton");
            Check(viewport.IsPlaying && viewport.RenderForTest() &&
                  viewport.CaptureForTest(Path.Combine(output, "e04-room-play.png")),
                "La sala reabierta no se pudo jugar.");
            Click(window, "StopButton");
            Check(!viewport.IsPlaying && !viewport.IsDocumentDirty &&
                  SHA256.HashData(File.ReadAllBytes(sourceCopy)).AsSpan()
                      .SequenceEqual(sourceHash),
                "Probar cambió documento o archivo fuente.");
        }
        finally
        {
            source.RootVisual = null;
            window.GpuViewport.Dispose();
        }
        Console.WriteLine("PASS E04 Studio room recipe preview/cancel/edit/save/play GPU");
    }

    private static void VerifyStudioDoor(string output, string level, string model)
    {
        Directory.CreateDirectory(output);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(level));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        using var source = new HwndSource(new HwndSourceParameters("VESTIGIO E05 door")
        {
            Width = 640, Height = 360, WindowStyle = unchecked((int)0x80000000)
        });
        var viewport = new GpuViewportHost
        {
            Width = 640, Height = 360, OpenAtrium = true,
            LevelPath = level, ModelPath = model
        };
        try
        {
            source.RootVisual = viewport;
            Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
            viewport.Measure(new Size(640, 360));
            viewport.Arrange(new Rect(0, 0, 640, 360));
            viewport.UpdateLayout();
            Check(viewport.IsNativeReady && viewport.RenderForTest() &&
                  !viewport.InteractForTest(), "E solo debe estar disponible en Probar.");
            nint host = viewport.NativeHandleForTest;
            ulong revision = GpuHostNative.vg_gpu_host_document_revision(host);
            Check(viewport.TrySetPlaying(true) &&
                  GpuHostNative.vg_gpu_host_door_count(host) == (nuint)1 &&
                  GpuHostNative.vg_gpu_host_door_angle(host, 0, out float closedAngle) != 0 &&
                  Math.Abs(closedAngle) < 0.001f,
                "Studio Play no instanció la puerta cerrada del documento.");
            Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 1, 280, 0, 0, 1) != 0,
                "No se pudo orientar la cámara de Play hacia la puerta.");
            for (int frame = 0; frame < 35; ++frame)
                Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 1, 0, 0, 0, 1) != 0,
                    $"Acercamiento a puerta falló en frame {frame}.");
            Check(viewport.InteractForTest(), "Studio no encoló E en Play.");
            for (int frame = 0; frame < 80; ++frame)
                Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 0, 0, 0, 0, 1) != 0,
                    $"Animación de puerta falló en frame {frame}.");
            Check(GpuHostNative.vg_gpu_host_door_angle(host, 0, out float openAngle) != 0 &&
                  openAngle > 1.4f, $"E no abrió la puerta en Studio: {openAngle:0.###}");
            Check(viewport.CaptureForTest(Path.Combine(output, "e05-studio-door-open.png")),
                "No se capturó Play con puerta abierta.");
            Check(viewport.TrySetPlaying(false) &&
                  GpuHostNative.vg_gpu_host_door_count(host) == (nuint)0 &&
                  GpuHostNative.vg_gpu_host_document_revision(host) == revision &&
                  SHA256.HashData(File.ReadAllBytes(level)).AsSpan().SequenceEqual(sourceHash),
                "Detener no descartó puerta de Play o modificó documento.");
            Check(viewport.TrySetPlaying(true) &&
                  GpuHostNative.vg_gpu_host_door_angle(host, 0, out closedAngle) != 0 &&
                  Math.Abs(closedAngle) < 0.001f &&
                  viewport.TrySetPlaying(false),
                "Segundo Play no restauró la puerta cerrada del documento.");
        }
        finally
        {
            source.RootVisual = null;
            viewport.Dispose();
        }
        var editor = App.CreateLevelWindow(["--level", level]);
        var editorContent = (FrameworkElement)editor.Content;
        editor.Content = null;
        using (var editorSource = new HwndSource(new HwndSourceParameters(
                   "VESTIGIO E05 protected door")
               {
                   Width = 1320, Height = 820,
                   WindowStyle = unchecked((int)0x80000000)
               }))
        {
            try
            {
                editorSource.RootVisual = editorContent;
                Dispatcher.CurrentDispatcher.Invoke(() => { },
                    DispatcherPriority.ApplicationIdle);
                editorContent.Measure(new Size(1320, 820));
                editorContent.Arrange(new Rect(0, 0, 1320, 820));
                editorContent.UpdateLayout();
                editor.RefreshForTest();
                nint editHost = editor.GpuViewport.NativeHandleForTest;
                int beforeCount = editor.GpuViewport.EntityUuids().Count;
                ulong beforeRevision = GpuHostNative.vg_gpu_host_document_revision(editHost);
                Check(editor.GpuViewport.TrySelect(
                          "60000000-0000-0000-0000-000000000010"),
                    "No se seleccionó panel de puerta del documento.");
                ((Button)editor.FindName("DuplicateButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(editor.GpuViewport.EntityUuids().Count == beforeCount &&
                      GpuHostNative.vg_gpu_host_document_revision(editHost) ==
                          beforeRevision &&
                      ((TextBlock)editor.FindName("FieldError")!).Text.Contains(
                          "puerta", StringComparison.OrdinalIgnoreCase),
                    "Duplicar panel de puerta debe preservar jerarquía y mostrar error.");
            }
            finally
            {
                editorSource.RootVisual = null;
                editor.GpuViewport.Dispose();
            }
        }
        Console.WriteLine("PASS E05 Studio Play E door, isolated Stop/replay");
    }

    private static void VerifyWave6Visual(string output, string level)
    {
        Directory.CreateDirectory(output);
        string settings = Path.Combine(output, "preferencias-área", "e06-visual.settings");
        if (File.Exists(settings)) File.Delete(settings);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(level));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        void OpenAndCheck(bool shouldPersist)
        {
            var window = App.CreateLevelWindow(["--level", level,
                "--settings", settings]);
            var content = (FrameworkElement)window.Content;
            window.Content = null;
            using var source = new HwndSource(new HwndSourceParameters("VESTIGIO W06 visual")
            {
                Width = 1320, Height = 820, WindowStyle = unchecked((int)0x80000000)
            });
            try
            {
                source.RootVisual = content;
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                content.Measure(new Size(1320, 820));
                content.Arrange(new Rect(0, 0, 1320, 820));
                content.UpdateLayout();
                window.RefreshForTest();
                var viewport = window.GpuViewport;
                nint host = viewport.NativeHandleForTest;
                Check(viewport.IsNativeReady && viewport.RenderForTest(),
                    $"W06 Studio GPU no abrió: {viewport.LastError}");
                Check(window.VisualSummaryForTest.Contains("2 luces") &&
                      window.VisualSummaryForTest.Contains("niebla lineal"),
                    "Studio no resume luces y niebla del nivel.");
                Check(viewport.VisualMode == (shouldPersist ? 1 : 0) &&
                      window.VisualProfileForTest == (shouldPersist ? 1 : 0),
                    "Studio no cargó preferencia visual persistida.");
                if (shouldPersist) return;
                ulong revision = GpuHostNative.vg_gpu_host_document_revision(host);
                Check(GpuHostNative.vg_gpu_host_readbacks(host) == 0,
                    "Frame normal no debe leer GPU hacia CPU.");
                string clean = Path.Combine(output, "e06-studio-clean.png");
                string retro = Path.Combine(output, "e06-studio-retro.png");
                Check(viewport.CaptureForTest(clean), "No se capturó perfil limpio.");
                window.SelectVisualProfileForTest(1);
                Check(viewport.VisualMode == 1 && File.Exists(settings) &&
                      File.ReadAllText(settings).Contains("visual_profile 1"),
                    $"Selector retro no persistió: {viewport.LastError}");
                Check(viewport.RenderForTest() && viewport.CaptureForTest(retro) &&
                      !SHA256.HashData(File.ReadAllBytes(clean)).AsSpan().SequenceEqual(
                          SHA256.HashData(File.ReadAllBytes(retro))),
                    "Limpio y retro produjeron la misma imagen GPU.");
                Check(GpuHostNative.vg_gpu_host_resize(host, 840, 460) != 0 &&
                      viewport.RenderForTest() &&
                      GpuHostNative.vg_gpu_host_readbacks(host) == 2 &&
                      GpuHostNative.vg_gpu_host_document_revision(host) == revision &&
                      !viewport.IsDocumentDirty,
                    "Resize/perfil alteró documento o leyó GPU sin captura.");
            }
            finally
            {
                source.RootVisual = null;
                window.GpuViewport.Dispose();
            }
        }
        OpenAndCheck(false);
        OpenAndCheck(true);
        Check(SHA256.HashData(File.ReadAllBytes(level)).AsSpan().SequenceEqual(sourceHash),
            "Cambiar perfil visual modificó el nivel fuente.");
        Console.WriteLine("PASS W06 Studio clean/retro GPU, fog/lights, persisted reopen, resize/readbacks");
    }

    private static void VerifyWave7Audio(string output, string level)
    {
        Directory.CreateDirectory(output);
        string settings = Path.Combine(output, "audio-área", "e07.settings");
        if (File.Exists(settings)) File.Delete(settings);
        byte[] sourceHash = SHA256.HashData(File.ReadAllBytes(level));
        var app = new Application();
        app.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml")
        });
        void RunWindow(bool noAudio)
        {
            var args = noAudio
                ? new[] { "--level", level, "--settings", settings, "--no-audio" }
                : new[] { "--level", level, "--settings", settings };
            var window = App.CreateLevelWindow(args);
            var content = (FrameworkElement)window.Content;
            window.Content = null;
            using var source = new HwndSource(new HwndSourceParameters("VESTIGIO W07 audio")
            {
                Width = 1320, Height = 820, WindowStyle = unchecked((int)0x80000000)
            });
            try
            {
                source.RootVisual = content;
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                content.Measure(new Size(1320, 820));
                content.Arrange(new Rect(0, 0, 1320, 820));
                content.UpdateLayout();
                window.RefreshForTest();
                var viewport = window.GpuViewport;
                nint host = viewport.NativeHandleForTest;
                Check(viewport.IsNativeReady && viewport.RenderForTest() &&
                      GpuHostNative.vg_gpu_host_audio_device_state(host) == 0 &&
                      GpuHostNative.vg_gpu_host_audio_voice_starts(host) == 0,
                    "Editar debe permanecer sin dispositivo/voz de audio.");
                if (!noAudio)
                {
                    window.SelectAudioGainForTest(0, 55);
                    window.SelectAudioGainForTest(1, 40);
                    window.SelectAudioGainForTest(2, 70);
                    window.SelectAudioGainForTest(3, 20);
                    Check(File.Exists(settings) &&
                          File.ReadAllText(settings).Contains("audio_master_gain 0.55") &&
                          File.ReadAllText(settings).Contains("audio_ambience_gain 0.2") &&
                          Math.Abs(viewport.AudioGain(2) - 0.7f) < 0.001f,
                        $"Sliders no persistieron ganancias: {viewport.LastError}");
                }
                else
                {
                    Check(Math.Abs(viewport.AudioGain(0) - 0.55f) < 0.001f &&
                          Math.Abs(viewport.AudioGain(3) - 0.2f) < 0.001f,
                        "Studio no restauró volumen persistido desde ruta Unicode.");
                }
                ((Button)window.FindName("PlayButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(viewport.IsPlaying, "Play falló con audio habilitado/deshabilitado.");
                if (!noAudio && viewport.AudioDeviceState == 1)
                {
                    Check(GpuHostNative.vg_gpu_host_audio_music_streams(host) == 1 &&
                          GpuHostNative.vg_gpu_host_audio_focus_paused(host) != 0,
                        "Ambiente streaming no inició pausado hasta enfocar Play.");
                    Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 1, 280, 0, 0, 1) != 0,
                        "Audio Play no orientó cámara.");
                    for (int frame = 0; frame < 35; ++frame)
                        Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 1, 0, 0, 0, 1) != 0,
                            "Audio Play no acercó cámara a puerta.");
                    Check(GpuHostNative.vg_gpu_host_audio_focus_paused(host) == 0 &&
                          GpuHostNative.vg_gpu_host_audio_stream_updates(host) > 0 &&
                          GpuHostNative.vg_gpu_host_audio_voice_starts(host) == 0,
                        "Ambiente no siguió cámara o puerta sonó sin E.");
                    Check(viewport.InteractForTest() && viewport.InteractForTest(),
                        "E no se encoló en Play.");
                    for (int frame = 0; frame < 80; ++frame)
                        Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 0, 0, 0, 0, 1) != 0,
                            "Play audio falló al animar puerta.");
                    Check(GpuHostNative.vg_gpu_host_audio_voice_starts(host) == 1 &&
                          GpuHostNative.vg_gpu_host_door_angle(host, 0, out float angle) != 0 &&
                          angle > 1.4f,
                        "Un E efectivo debe producir un solo SFX espacial.");
                    Check(GpuHostNative.vg_gpu_host_frame(host, 1.0 / 60, 0, 0, 0, 0, 0, 0) != 0 &&
                          GpuHostNative.vg_gpu_host_audio_focus_paused(host) != 0,
                        "Perder foco no pausó audio de Play.");
                    Console.WriteLine("READY W07 Studio hardware: ambience stream + one door SFX + focus pause");
                }
                else if (!noAudio)
                    Console.WriteLine("SKIP W07 Studio hardware: dispositivo no disponible; Play continuó");
                else
                    Console.WriteLine("PASS W07 Studio --no-audio: Play continuó sin dispositivo");
                ((Button)window.FindName("StopButton")!).RaiseEvent(
                    new RoutedEventArgs(ButtonBase.ClickEvent));
                Check(!viewport.IsPlaying &&
                      GpuHostNative.vg_gpu_host_audio_device_state(host) == 0 &&
                      GpuHostNative.vg_gpu_host_audio_music_streams(host) == 0,
                    "Stop no limpió audio ni volvió a Editar silencioso.");
            }
            finally
            {
                source.RootVisual = null;
                window.GpuViewport.Dispose();
            }
        }
        RunWindow(false);
        RunWindow(true);
        string emptyAssetDir = Path.Combine(output, "missing-wav");
        Directory.CreateDirectory(emptyAssetDir);
        string bareModel = Path.Combine(emptyAssetDir, "atrium.gltf");
        File.Copy(GpuViewportHost.ResolveDemoAsset("atrium.gltf"), bareModel, true);
        using (var source = new HwndSource(new HwndSourceParameters("W07 missing WAV")
        {
            Width = 640, Height = 360, WindowStyle = unchecked((int)0x80000000)
        }))
        {
            var viewport = new GpuViewportHost
            {
                Width = 640, Height = 360, OpenAtrium = true,
                LevelPath = level, ModelPath = bareModel, VisualSettingsPath = settings
            };
            try
            {
                source.RootVisual = viewport;
                Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
                viewport.Measure(new Size(640, 360));
                viewport.Arrange(new Rect(0, 0, 640, 360));
                viewport.UpdateLayout();
                Check(viewport.IsNativeReady && viewport.TrySetPlaying(true) &&
                      viewport.AudioDeviceState == 2 && viewport.RenderForTest() &&
                      viewport.TrySetPlaying(false),
                    "WAV faltante debe informar audio no disponible y permitir Play/Stop.");
            }
            finally
            {
                source.RootVisual = null;
                viewport.Dispose();
            }
        }
        Console.WriteLine("PASS W07 missing WAV: audio unavailable, Play/Stop usable");
        Check(SHA256.HashData(File.ReadAllBytes(level)).AsSpan().SequenceEqual(sourceHash),
            "Audio o volumen modificaron el nivel fuente.");
        Console.WriteLine("PASS W07 Studio volumes Unicode, Play/Stop audio, no-audio fallback");
    }

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && args[0] == "--ux01")
            {
                VerifyUX01(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            if (args.Length == 4 && args[0] == "--e01")
            {
                VerifyAtrium3D(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]),
                    Path.GetFullPath(args[3]));
                VerifyAtriumWindow(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            if (args.Length == 3 && args[0] == "--e04")
            {
                VerifyWave4Editing(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            if (args.Length == 3 && args[0] == "--w09")
            {
                VerifyWave9Room(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            if (args.Length == 3 && args[0] == "--e04-room-recipe")
            {
                VerifyE04RoomRecipe(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            if (args.Length == 3 && args[0] == "--e02")
            {
                VerifyE02Editing(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            if (args.Length == 4 && args[0] == "--e03")
            {
                VerifyE03Studio(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]),
                    Path.GetFullPath(args[3]));
                return 0;
            }
            if (args.Length == 4 && args[0] == "--e05")
            {
                VerifyStudioDoor(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]),
                    Path.GetFullPath(args[3]));
                return 0;
            }
            if (args.Length == 3 && args[0] == "--e06")
            {
                VerifyWave6Visual(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            if (args.Length == 3 && args[0] == "--e07")
            {
                VerifyWave7Audio(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
                return 0;
            }
            throw new ArgumentException("Indica una prueba 3D (--e01, --e02, --e03, --e04, --e04-room-recipe, --w09, --e05, --e06 o --e07).");
        }
        catch (Exception exception) { Console.Error.WriteLine(exception); return 1; }
    }
}
