using System.Globalization;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;

namespace RetroForge.Studio;

public partial class Vestigio3DWindow
{
    private sealed class FieldView(GpuInspectorField field, FrameworkElement input,
        TextBlock error)
    {
        internal GpuInspectorField Field { get; } = field;
        internal FrameworkElement Input { get; } = input;
        internal TextBlock Error { get; } = error;
        internal bool Changed { get; set; }
    }

    private readonly List<FieldView> _fieldViews = [];
    private readonly Dictionary<string, (string Group, string Layer, bool Hidden,
        string Parent, string Asset)> _entityEditor = new(StringComparer.OrdinalIgnoreCase);
    private bool _refreshingSchema;
    private bool _refreshingAssets;
    private bool _refreshingOrganization;
    private string _displayedGroup = "";
    private string _displayedLayer = "";
    private string _previewedAssetId = "";

    internal FrameworkElement? InspectorInputForTest(string path) =>
        _fieldViews.FirstOrDefault(view => view.Field.Path == path)?.Input;

    internal string InspectorErrorForTest(string path) =>
        _fieldViews.FirstOrDefault(view => view.Field.Path == path)?.Error.Text ?? "";

    internal bool ImportAssetForTest(string path) => ImportAssetPath(path);

    private void RefreshAssetLibrary()
    {
        string? selectedId = (AssetList.SelectedItem as ListBoxItem)?.Tag as string;
        IReadOnlyList<GpuAssetInfo> assets = Viewport.AssetLibrary();
        _refreshingAssets = true;
        try
        {
            AssetList.Items.Clear();
            foreach (GpuAssetInfo asset in assets)
            {
                var row = new ListBoxItem
                {
                    Content = $"{asset.Name} · {asset.Status}",
                    Tag = asset.Id,
                    ToolTip = $"{asset.Path}\nID {asset.Id}\nFingerprint {asset.Fingerprint}" +
                        (asset.Diagnostic.Length > 0 ? $"\n{asset.Diagnostic}" : "")
                };
                AssetList.Items.Add(row);
                if (asset.Id == selectedId) row.IsSelected = true;
                if (asset.Diagnostic.Length > 0)
                    RecordProblem($"{asset.Name}: {asset.Diagnostic}");
            }
            if (AssetList.SelectedItem is null && AssetList.Items.Count > 0)
                AssetList.SelectedIndex = 0;
        }
        finally { _refreshingAssets = false; }
        ShowSelectedAsset(assets);
    }

    private void ShowSelectedAsset(IReadOnlyList<GpuAssetInfo>? known = null)
    {
        string? id = (AssetList.SelectedItem as ListBoxItem)?.Tag as string;
        GpuAssetInfo? asset = (known ?? Viewport.AssetLibrary())
            .FirstOrDefault(candidate => candidate.Id == id);
        bool selected = asset is not null;
        PlaceAssetButton.IsEnabled = selected && !Viewport.IsPlaying &&
            string.Equals(asset!.Status, "ready", StringComparison.OrdinalIgnoreCase);
        ReimportAssetButton.IsEnabled = selected && !Viewport.IsPlaying;
        RenameAssetButton.IsEnabled = selected && !Viewport.IsPlaying;
        AssetName.Text = asset?.Name ?? "";
        bool preview = false;
        string previewId = asset is not null &&
            string.Equals(asset.Status, "ready", StringComparison.OrdinalIgnoreCase)
            ? id ?? "" : "";
        if (!Viewport.IsPlaying && previewId != _previewedAssetId)
        {
            preview = Viewport.TryPreviewAsset(previewId);
            if (preview) _previewedAssetId = previewId;
            else if (selected) RecordProblem($"Vista previa GPU: {Viewport.LastError}");
        }
        else preview = selected && _previewedAssetId == previewId && previewId.Length > 0;
        AssetDetails.Text = asset is null
            ? "Selecciona un modelo. Miniatura no generada."
            : $"{asset.Status} · {(preview ? "vista previa GPU temporal" : "miniatura no disponible")}\n" +
              $"{Path.GetFileName(asset.Path)}\n" +
              $"Huella: {(asset.Fingerprint.Length > 16 ? asset.Fingerprint[..16] : asset.Fingerprint)}" +
              (asset.Diagnostic.Length > 0 ? $"\n{asset.Diagnostic}" : "");
    }

    private void AssetList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!_refreshingAssets && Viewport is not null && Viewport.IsNativeReady)
            ShowSelectedAsset();
    }

    private void ImportAsset_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Importar modelo 3D",
            Filter = "Modelos glTF (*.glb;*.gltf)|*.glb;*.gltf",
            InitialDirectory = Path.GetDirectoryName(ActiveLevelPath)
        };
        if (dialog.ShowDialog(this) != true) return;
        _ = ImportAssetPath(dialog.FileName);
    }

    private bool ImportAssetPath(string path)
    {
        if (!Viewport.TryImportAsset(path, out string id))
        {
            ShowNativeError("No se pudo importar el modelo");
            return false;
        }
        RefreshDocument();
        foreach (ListBoxItem row in AssetList.Items)
            if ((string?)row.Tag == id) row.IsSelected = true;
        StatusText.Text = "Modelo importado · selecciona Colocar para verlo en la escena";
        return true;
    }

    private void PlaceAsset_Click(object sender, RoutedEventArgs e)
    {
        if ((AssetList.SelectedItem as ListBoxItem)?.Tag is not string id) return;
        if (!Viewport.TryPlaceAsset(id, out string entityId))
        {
            ShowNativeError("No se pudo colocar el modelo");
            return;
        }
        _entityLabels[entityId] = AssetName.Text;
        RefreshDocument();
        StatusText.Text = "Modelo colocado · transforma y guarda el nivel";
    }

    private void ReimportAsset_Click(object sender, RoutedEventArgs e)
    {
        if ((AssetList.SelectedItem as ListBoxItem)?.Tag is not string id) return;
        var dialog = new OpenFileDialog
        {
            Title = "Reimportar modelo conservando su ID",
            Filter = "Modelos glTF (*.glb;*.gltf)|*.glb;*.gltf",
            InitialDirectory = Path.GetDirectoryName(ActiveLevelPath)
        };
        if (dialog.ShowDialog(this) != true) return;
        if (!Viewport.TryReimportAsset(id, dialog.FileName))
        {
            ShowNativeError("No se pudo reimportar el modelo");
            return;
        }
        _previewedAssetId = "";
        RefreshDocument();
        StatusText.Text = "Asset reimportado · ID de referencia conservado";
    }

    private void RenameAsset_Click(object sender, RoutedEventArgs e)
    {
        if ((AssetList.SelectedItem as ListBoxItem)?.Tag is not string id) return;
        string name = AssetName.Text.Trim();
        if (name.Length is 0 or > 128)
        {
            RecordProblem("Nombre del asset: usa entre 1 y 128 caracteres.");
            return;
        }
        if (!Viewport.TryRenameAsset(id, name))
        {
            ShowNativeError("No se pudo renombrar el asset");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Asset renombrado · ID de referencia conservado";
    }

    private void RefreshSchemaInspector()
    {
        _refreshingSchema = true;
        try
        {
            _fieldViews.Clear();
            SchemaFieldsPanel.Children.Clear();
            IReadOnlyList<GpuInspectorField> fields = Viewport.SelectionFields();
            foreach (GpuInspectorField field in fields)
            {
                var row = new StackPanel { Margin = new Thickness(0, 0, 0, 8) };
                string label = field.Path switch
                {
                    "engine.mesh.asset" => "Modelo · asset ID",
                    "engine.mesh.node_index" => "Nodo de malla · índice",
                    "editor.group" => "Grupo del editor",
                    "editor.layer" => "Capa del editor",
                    "editor.hidden" => "Oculto sólo en editor",
                    _ => field.Path
                };
                row.Children.Add(new TextBlock
                {
                    Text = label + (field.Unit.Length > 0 ? $" · {field.Unit}" : ""),
                    Foreground = (Brush)FindResource("MutedBrush")
                });
                FrameworkElement input;
                if (field.Type == "boolean")
                {
                    var check = new CheckBox
                    {
                        IsThreeState = field.Mixed,
                        IsChecked = field.Mixed ? null :
                            field.Value.ValueKind == JsonValueKind.True,
                        IsEnabled = field.Editable,
                        Content = field.Mixed ? "Valores distintos" : "Activado",
                        Margin = new Thickness(0, 3, 0, 0)
                    };
                    input = check;
                }
                else if (field.Type == "reference")
                {
                    var choice = new ComboBox
                    {
                        IsEnabled = field.Editable,
                        Margin = new Thickness(0, 3, 0, 0),
                        ToolTip = field.Mixed ? "Valores distintos" : field.DisplayValue
                    };
                    if (field.Mixed) choice.Items.Add(new ComboBoxItem
                    {
                        Content = "— Valores distintos —", Tag = ""
                    });
                    foreach (GpuAssetInfo asset in Viewport.AssetLibrary())
                        choice.Items.Add(new ComboBoxItem
                        {
                            Content = $"{asset.Name} · {asset.Id[..Math.Min(8, asset.Id.Length)]}",
                            Tag = asset.Id
                        });
                    int match = choice.Items.OfType<ComboBoxItem>().ToList().FindIndex(
                        item => (string?)item.Tag == field.DisplayValue);
                    if (!field.Mixed && match < 0 && field.DisplayValue.Length > 0)
                    {
                        choice.Items.Add(new ComboBoxItem
                        {
                            Content = $"Referencia no disponible · {field.DisplayValue}",
                            Tag = field.DisplayValue
                        });
                        match = choice.Items.Count - 1;
                    }
                    choice.SelectedIndex = match >= 0 ? match : field.Mixed ? 0 : -1;
                    input = choice;
                }
                else
                    input = new TextBox
                    {
                        Text = field.DisplayValue,
                        IsEnabled = field.Editable,
                        Margin = new Thickness(0, 3, 0, 0),
                        ToolTip = field.Mixed ? "Valores distintos: escribe para unificar" :
                            field.Minimum.HasValue || field.Maximum.HasValue
                                ? $"Rango {field.Minimum} a {field.Maximum} {field.Unit}" : field.Path
                    };
                var error = new TextBlock
                {
                    Foreground = (Brush)FindResource("DangerBrush"),
                    TextWrapping = TextWrapping.Wrap,
                    Visibility = Visibility.Collapsed
                };
                var view = new FieldView(field, input, error);
                if (input is TextBox text)
                    text.TextChanged += (_, _) => MarkFieldChanged(view);
                else if (input is CheckBox check)
                    check.Click += (_, _) => MarkFieldChanged(view);
                else if (input is ComboBox choice)
                    choice.SelectionChanged += (_, _) => MarkFieldChanged(view);
                row.Children.Add(input);
                row.Children.Add(error);
                SchemaFieldsPanel.Children.Add(row);
                _fieldViews.Add(view);
            }
            ApplyFieldsButton.IsEnabled = fields.Count > 0 && !Viewport.IsPlaying;
            RefreshOrganizationFromSchema(fields);
        }
        finally { _refreshingSchema = false; }
    }

    private void RefreshEditorMetadata(IReadOnlyList<string> ids)
    {
        _entityEditor.Clear();
        foreach (string id in ids)
        {
            if (!Viewport.TryGetEntityEditor(id, out string json)) continue;
            try
            {
                using JsonDocument document = JsonDocument.Parse(json);
                JsonElement root = document.RootElement;
                static string Read(JsonElement value, string name) =>
                    value.TryGetProperty(name, out JsonElement found) &&
                    found.ValueKind == JsonValueKind.String ? found.GetString() ?? "" : "";
                _entityEditor[id] = (Read(root, "group"), Read(root, "layer"),
                    root.TryGetProperty("hidden", out JsonElement hidden) &&
                    hidden.ValueKind == JsonValueKind.True, Read(root, "parent"),
                    Read(root, "asset"));
            }
            catch (JsonException exception)
            {
                RecordProblem($"Metadatos editor de {id}: {exception.Message}");
            }
        }
    }

    private IEnumerable<string> HierarchyOrder(IReadOnlyList<string> ids)
    {
        var known = ids.ToHashSet(StringComparer.OrdinalIgnoreCase);
        var visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        IEnumerable<string> Children(string parent) => ids.Where(id =>
            _entityEditor.TryGetValue(id, out var editor) &&
            string.Equals(editor.Parent, parent, StringComparison.OrdinalIgnoreCase));
        IEnumerable<string> Visit(string id)
        {
            if (!visited.Add(id)) yield break;
            yield return id;
            foreach (string child in Children(id))
                foreach (string item in Visit(child)) yield return item;
        }
        foreach (string id in ids.Where(candidate =>
                     !_entityEditor.TryGetValue(candidate, out var editor) ||
                     string.IsNullOrEmpty(editor.Parent) || !known.Contains(editor.Parent)))
            foreach (string item in Visit(id)) yield return item;
        foreach (string id in ids)
            foreach (string item in Visit(id)) yield return item;
    }

    private int HierarchyDepth(string id)
    {
        int depth = 0;
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { id };
        while (depth < 10 && _entityEditor.TryGetValue(id, out var editor) &&
               !string.IsNullOrEmpty(editor.Parent) && seen.Add(editor.Parent))
        {
            ++depth;
            id = editor.Parent;
        }
        return depth;
    }

    private void MarkFieldChanged(FieldView view)
    {
        if (_refreshingSchema) return;
        view.Changed = true;
        view.Error.Text = "";
        view.Error.Visibility = Visibility.Collapsed;
    }

    private void ApplyFields_Click(object sender, RoutedEventArgs e)
    {
        var changes = new Dictionary<string, object?>();
        bool invalid = false;
        foreach (FieldView view in _fieldViews.Where(item => item.Changed))
        {
            string raw = view.Input switch
            {
                TextBox text => text.Text.Trim(),
                CheckBox check => (check.IsChecked == true).ToString(),
                ComboBox choice => (choice.SelectedItem as ComboBoxItem)?.Tag as string ?? "",
                _ => ""
            };
            if (!view.Field.TryParseValue(raw, out object? value, out string error))
            {
                view.Error.Text = error;
                view.Error.Visibility = Visibility.Visible;
                RecordProblem($"{view.Field.Path}: {error}");
                invalid = true;
            }
            else changes[view.Field.Path] = value;
        }
        if (invalid || changes.Count == 0) return;
        if (!Viewport.TrySetSelectionFields(changes))
        {
            FieldView? target = _fieldViews.FirstOrDefault(view => view.Changed &&
                Viewport.LastError.Contains(view.Field.Path, StringComparison.OrdinalIgnoreCase))
                ?? _fieldViews.FirstOrDefault(view => view.Changed);
            if (target is not null)
            {
                target.Error.Text = Viewport.LastError;
                target.Error.Visibility = Visibility.Visible;
            }
            ShowNativeError("No se pudo aplicar el inspector");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Campos aplicados en un lote · Deshacer disponible";
    }

    private void RefreshOrganizationFromSchema(IReadOnlyList<GpuInspectorField> fields)
    {
        _refreshingOrganization = true;
        try
        {
            GpuInspectorField? group = fields.FirstOrDefault(item => item.Path == "editor.group");
            GpuInspectorField? layer = fields.FirstOrDefault(item => item.Path == "editor.layer");
            GpuInspectorField? hidden = fields.FirstOrDefault(item => item.Path == "editor.hidden");
            _displayedGroup = group?.DisplayValue ?? "";
            _displayedLayer = layer?.DisplayValue ?? "";
            EditorGroup.Text = _displayedGroup;
            EditorLayer.Text = _displayedLayer;
            OrganizationError.Text = "";
            HideLayer.IsChecked = hidden?.Mixed == true ? null :
                hidden?.Value.ValueKind == JsonValueKind.True;
            HideLayer.IsThreeState = hidden?.Mixed == true;
            ApplyOrganizationButton.IsEnabled = group is not null && !Viewport.IsPlaying;
            HideLayer.IsEnabled = hidden is not null && !Viewport.IsPlaying;
        }
        finally { _refreshingOrganization = false; }
    }

    private void ApplyOrganization_Click(object sender, RoutedEventArgs e)
    {
        if (EditorGroup.Text.Trim().Length > 128 || EditorLayer.Text.Trim().Length > 128)
        {
            OrganizationError.Text = "Grupo y capa admiten hasta 128 caracteres cada uno.";
            RecordProblem(OrganizationError.Text);
            return;
        }
        var changes = new Dictionary<string, object?>();
        if (EditorGroup.Text != _displayedGroup)
            changes["editor.group"] = EditorGroup.Text.Trim();
        if (EditorLayer.Text != _displayedLayer)
            changes["editor.layer"] = EditorLayer.Text.Trim();
        if (changes.Count == 0) return;
        if (!Viewport.TrySetSelectionFields(changes))
        {
            OrganizationError.Text = Viewport.LastError;
            ShowNativeError("No se pudo organizar la selección");
            return;
        }
        RefreshDocument();
        StatusText.Text = "Organización editorial guardada · Deshacer disponible";
    }

    private void HideLayer_Changed(object sender, RoutedEventArgs e)
    {
        if (_refreshingOrganization || Viewport is null || !Viewport.IsNativeReady ||
            Viewport.IsPlaying || HideLayer.IsChecked is null) return;
        bool hide = HideLayer.IsChecked == true;
        string layer = EditorLayer.Text.Trim();
        IReadOnlyList<string> before = Viewport.SelectedUuids();
        List<string> members = layer.Length == 0 ? before.ToList() :
            _entityEditor.Where(pair => pair.Value.Layer == layer)
                .Select(pair => pair.Key).ToList();
        if (members.Count == 0) members = before.ToList();
        if (!Viewport.TrySelectMany(members) ||
            !Viewport.TrySetSelectionFields(new Dictionary<string, object?>
            {
                ["editor.hidden"] = hide
            }))
        {
            _ = Viewport.TrySelectMany(before);
            ShowNativeError("No se pudo cambiar la visibilidad editorial");
            return;
        }
        _ = Viewport.TrySelectMany(before);
        RefreshDocument();
        StatusText.Text = $"Capa {(hide ? "oculta" : "visible")} en editor · juego sin cambios";
    }

    private void RecordProblem(string message)
    {
        if (string.IsNullOrWhiteSpace(message)) return;
        if (!ProblemsList.Items.OfType<string>().Contains(message))
            ProblemsList.Items.Insert(0, message);
        while (ProblemsList.Items.Count > 12)
            ProblemsList.Items.RemoveAt(ProblemsList.Items.Count - 1);
    }
}
