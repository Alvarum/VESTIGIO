using System.IO;
using System.Text.Json;
using System.Security.Cryptography;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using Vestigio.Studio;

internal static partial class Program
{
    private static void VerifyUX01(string output, string model)
    {
        Directory.CreateDirectory(output);
        string run = Path.Combine(output, Guid.NewGuid().ToString("N"));
        string fresh = LevelFiles.CreateNew("Mi primer juego", Path.Combine(run, "nuevo"));
        string saved = Path.Combine(run, "nivel propio", "mi-juego.level.json");
        Directory.CreateDirectory(Path.GetDirectoryName(saved)!);
        var app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        app.Resources.MergedDictionaries.Add(new ResourceDictionary {
            Source = new Uri("pack://application:,,,/vestigio_studio;component/Themes/Graphite.xaml") });
        static void Pump() => Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
        static void Click(Vestigio3DWindow window, string control) => ((Button)window.FindName(control)!).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
        static void Set(Vestigio3DWindow window, string control, string value) => ((TextBox)window.FindName(control)!).Text = value;
        Vestigio3DWindow Open(string path, bool unsaved = false)
        {
            var window = App.CreateLevelWindow(["--level", path, "--no-audio", "--settings", Path.Combine(run, "visual.settings")]);
            window.MarkUnsaved(unsaved); window.Show(); Pump(); window.RefreshForTest();
            Check(window.GpuViewport.IsNativeReady, $"UX01 no abrió GPU: {window.GpuViewport.LastError}");
            return window;
        }
        string roomId, objectId;
        var window = Open(fresh, true);
        try
        {
            var viewport = window.GpuViewport;
            Check(viewport.EntityUuids().Count == 1 && viewport.AssetLibrary().Count == 0,
                "Nuevo debe contener sólo el inicio del jugador, sin objetos/recursos Atrium.");
            Check(viewport.TrySetPlaying(true) && GpuHostNative.vg_gpu_host_animation_count(viewport.NativeHandleForTest) == 0 &&
                viewport.AudioDeviceState == 0 && viewport.TrySetPlaying(false), "Nivel vacío debe jugar sin efectos demo.");
            string spawn = viewport.EntityUuids().Single();
            Check(viewport.TrySelect(spawn), "Inicio del jugador no seleccionable."); window.RefreshForTest();
            Set(window, "RotationZ", "45"); Click(window, "ApplyButton");
            Check(viewport.TrySetPlaying(true) && GpuHostNative.vg_gpu_host_frame(viewport.NativeHandleForTest, 1.0 / 60, 0, 1, 0, 0, 0, 1) != 0 &&
                  GpuHostNative.vg_gpu_host_camera_position(viewport.NativeHandleForTest, out float spawnX, out _, out _) != 0 && spawnX < -.001f &&
                  viewport.TrySetPlaying(false), "Probar no respetó orientación del inicio del jugador.");
            window.RefreshForTest(); Set(window, "RotationZ", "0"); Click(window, "ApplyButton");
            Check(!viewport.TryDeleteSelection(), "No se debe borrar el inicio requerido para jugar.");
            Click(window, "OpenRoomEditorButton"); Set(window, "RoomRectWidth", "6"); Set(window, "RoomRectDepth", "6");
            Click(window, "RoomRectButton"); Click(window, "AddOpeningButton");
            ulong revision = GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest);
            Click(window, "PreviewRoomButton"); Click(window, "CancelRoomPreviewButton");
            Check(GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest) == revision, "Preview/Cancelar mutó documento.");
            Click(window, "CommitRoomButton"); roomId = viewport.SelectedUuid;
            Check(viewport.TryGetRoomRecipe(roomId, out string recipe) && recipe.Contains("\"door\""), $"Habitación/abertura no creada: {viewport.LastError}");
            Click(window, "OpenRoomEditorButton");
            CaptureUX01(window, output, "constructor", 1366, 768, 1);
            Check(window.ImportAssetForTest(model), $"Importar falló: {viewport.LastError}");
            Check(!((TextBlock)window.FindName("AssetDetails")!).Text.Contains("GPU temporal"), "Seleccionar recurso no debe reemplazar la escena por preview.");
            Click(window, "PreviewAssetButton");
            Check(((TextBlock)window.FindName("AssetDetails")!).Text.Contains("GPU temporal"), "Preview explícito no se activó.");
            Click(window, "PreviewAssetButton"); Click(window, "PlaceAssetButton"); objectId = viewport.SelectedUuid;
            Set(window, "PositionX", "1.25"); Set(window, "PositionY", "0.5"); Set(window, "PositionZ", "1");
            Click(window, "ApplyButton");
            Check(viewport.TryGetSelectedTransform(out float[] position, out _, out _) && Math.Abs(position[0] - 1.25f) < .001f,
                "Transformar el modelo no aplicó posición.");
            window.LeaveChoiceForTest = () => MessageBoxResult.Cancel;
            window.Close(); Check(window.IsVisible && viewport.IsNativeReady, "Cancelar cerrar debe conservar la ventana y GPU.");
            string active = window.ActiveLevelPath;
            Check(!window.SwitchLevel(GpuViewportHost.ResolveDemoAsset("atrium.level.json")) && window.ActiveLevelPath == active && viewport.IsDocumentDirty,
                "Cancelar abrir debe conservar documento y cambios.");
            window.LeaveChoiceForTest = () => MessageBoxResult.Yes; window.SavePathForTest = () => null;
            Check(!window.SwitchLevel(fresh) && viewport.IsDocumentDirty, "Cancelar Guardar como debe cancelar el cambio de documento.");
            window.SavePathForTest = () => saved; Click(window, "SaveButton");
            Check(File.Exists(saved) && !viewport.IsDocumentDirty && window.ActiveLevelPath == saved, $"Guardar falló: {viewport.LastError}");
            string bad = Path.Combine(run, "invalid.level.json"); File.WriteAllText(bad, "{}");
            ulong savedRevision = GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest);
            Check(!window.SwitchLevel(bad) && window.ActiveLevelPath == saved && viewport.EntityUuids().Contains(objectId) &&
                  GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest) == savedRevision,
                "Abrir inválido debe conservar el nivel activo.");
            Check(!window.TrySaveToPath(GpuViewportHost.ResolveDemoAsset("atrium.level.json")), "Atrium fuente no debe sobrescribirse.");
        }
        finally { window.LeaveChoiceForTest = () => MessageBoxResult.No; window.Close(); Pump(); }

        window = Open(saved);
        try
        {
            var viewport = window.GpuViewport;
            Check(viewport.EntityUuids().Contains(objectId) && viewport.TryGetRoomRecipe(roomId, out _) && viewport.AssetLibrary().Count == 1,
                "Cerrar/Abrir perdió habitación, modelo o biblioteca.");
            Check(viewport.TrySelect(objectId) && viewport.TryGetSelectedTransform(out float[] position, out _, out _) &&
                  Math.Abs(position[0] - 1.25f) < .001f, "Abrir perdió transformación.");
            window.RefreshForTest();
            ((TabControl)window.FindName("InspectorTabs")!).SelectedIndex = 0;
            byte[] hash = SHA256.HashData(File.ReadAllBytes(saved));
            ulong revision = GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest);
            Click(window, "PlayButton");
            Check(viewport.IsPlaying && GpuHostNative.vg_gpu_host_animation_count(viewport.NativeHandleForTest) == 0,
                "Play nivel propio falló o añadió animación demo.");
            for (int i = 0; i < 20; i++) Check(GpuHostNative.vg_gpu_host_frame(viewport.NativeHandleForTest, 1.0 / 60, 0, .5f, 0, 0, 0, 1) != 0, "Play movimiento falló.");
            Click(window, "StopButton");
            Check(!viewport.IsPlaying && revision == GpuHostNative.vg_gpu_host_document_revision(viewport.NativeHandleForTest) &&
                  SHA256.HashData(File.ReadAllBytes(saved)).AsSpan().SequenceEqual(hash), "Play/Stop mutó el documento o archivo.");
            Set(window, "PositionX", "2"); Click(window, "ApplyButton"); Click(window, "UndoButton");
            Check(viewport.TryGetSelectedTransform(out position, out _, out _) && Math.Abs(position[0] - 1.25f) < .001f, "Undo tras Stop perdió edición.");
            Click(window, "RedoButton"); Check(viewport.TryGetSelectedTransform(out position, out _, out _) && Math.Abs(position[0] - 2) < .001f, "Redo tras Stop falló.");
            Click(window, "UndoButton");
            foreach (var size in new[] { (1366, 768, 1.0), (1920, 1080, 1.0), (1366, 768, 1.5), (1920, 1080, 1.5) })
                CaptureUX01(window, output, "editor", size.Item1, size.Item2, size.Item3);
            // Discard changes when switching; preserve the example source on Save.
            window.LeaveChoiceForTest = () => MessageBoxResult.No;
            Check(window.SwitchLevel(fresh) && viewport.EntityUuids().Count == 1 && viewport.SelectedUuid == "Ninguno", "Nuevo cambio de archivo conservó objetos/selección anterior.");
            Check(window.SwitchLevel(saved), "No se pudo volver al archivo propio.");
        }
        finally { window.LeaveChoiceForTest = () => MessageBoxResult.No; window.Close(); Pump(); }
        File.WriteAllText(Path.Combine(output, "ux01-result.json"), JsonSerializer.Serialize(new { passed = true, saved, roomId, objectId,
            layout = "1366x768 + 1920x1080, escala WPF 100/150; DPI fisico no certificado", user_review = "pending" }, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"PASS UX01 new/room/opening/import/preview/place/transform/save/close/open/play/stop/undo/redo/cancel/invalid + layouts; nivel={saved}");
    }

    private static void CaptureUX01(Vestigio3DWindow window, string output, string name, int width, int height, double scale)
    {
        var content = (FrameworkElement)window.Content;
        // Physical DPI changes are user-owned; reproduce available layout space without changing OS settings.
        content.LayoutTransform = new ScaleTransform(scale, scale);
        window.Width = width; window.Height = height;
        window.UpdateLayout();
        content.Measure(new Size(width - 16, height - 40)); content.Arrange(new Rect(0, 0, width - 16, height - 40)); content.UpdateLayout();
        Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);
        var viewport = window.GpuViewport;
        Check(viewport.ActualWidth > 180 && viewport.ActualHeight > 100,
            $"Viewport no tiene espacio útil a {width}x{height}/{scale}: {viewport.ActualWidth}x{viewport.ActualHeight}, root {content.RenderSize}.");
        foreach (string control in new[] { "PlayButton", "StopButton", "OpenRoomEditorButton", "SaveButton", "EntityList", "InspectorTabs", "ResourcesTabs" })
        {
            var element = (FrameworkElement)window.FindName(control)!;
            Rect bounds = element.TransformToAncestor(content).TransformBounds(new Rect(0, 0, element.ActualWidth, element.ActualHeight));
            Check(bounds.Left >= -1 && bounds.Top >= -1 && bounds.Right <= content.ActualWidth + 1 && bounds.Bottom <= content.ActualHeight + 1,
                $"{control} sale del layout a {width}x{height}/{scale}: {bounds} / {content.RenderSize}");
        }
        string gpuPath = Path.Combine(output, $"ux01-{name}-{width}-{height}-{scale:0.0}-gpu.png");
        Check(viewport.RenderForTest() && viewport.CaptureForTest(gpuPath), $"No se capturó GPU: {viewport.LastError}");
        var shell = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32); shell.Render(content);
        var gpu = new BitmapImage(new Uri(gpuPath));
        Rect viewportBounds = viewport.TransformToAncestor(content).TransformBounds(new Rect(0, 0, viewport.ActualWidth, viewport.ActualHeight));
        viewportBounds.Scale(scale, scale);
        var composed = new DrawingVisual(); using (DrawingContext dc = composed.RenderOpen())
        { dc.DrawImage(shell, new Rect(0, 0, width, height)); dc.DrawImage(gpu, viewportBounds); }
        var image = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32); image.Render(composed);
        var png = new PngBitmapEncoder(); png.Frames.Add(BitmapFrame.Create(image));
        using var stream = File.Create(Path.Combine(output, $"ux01-{name}-{width}-{height}-{scale:0.0}.png")); png.Save(stream);
        content.LayoutTransform = Transform.Identity;
    }
}
