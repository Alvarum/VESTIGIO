using System.Globalization;
using System.Text.Json;

namespace Vestigio.Studio;

internal sealed record GpuAssetInfo(string Id, string Name, string Path,
    string Fingerprint, string Status, string Diagnostic)
{
    internal static IReadOnlyList<GpuAssetInfo> Parse(string json)
    {
        using JsonDocument document = JsonDocument.Parse(json);
        if (document.RootElement.ValueKind != JsonValueKind.Array)
            throw new JsonException("La biblioteca de assets debe ser una lista.");
        return document.RootElement.EnumerateArray().Select(item => new GpuAssetInfo(
            Read(item, "id"), Read(item, "name"), Read(item, "path"),
            Read(item, "fingerprint"), Read(item, "status"),
            Read(item, "diagnostic"))).ToArray();
    }

    private static string Read(JsonElement item, string name) =>
        item.TryGetProperty(name, out JsonElement value) &&
        value.ValueKind == JsonValueKind.String ? value.GetString() ?? "" : "";
}

internal sealed record GpuInspectorField(string Path, string Type, string Unit,
    double? Minimum, double? Maximum, JsonElement Value, bool Mixed, bool Editable)
{
    internal string DisplayValue => Mixed ? "" : Value.ValueKind switch
    {
        JsonValueKind.String => Value.GetString() ?? "",
        JsonValueKind.True => "true",
        JsonValueKind.False => "false",
        JsonValueKind.Number => Value.GetDouble().ToString("0.######",
            CultureInfo.CurrentCulture),
        _ => ""
    };

    internal static IReadOnlyList<GpuInspectorField> Parse(string json)
    {
        using JsonDocument document = JsonDocument.Parse(json);
        JsonElement fields = document.RootElement.GetProperty("fields");
        return fields.EnumerateArray().Select(item => new GpuInspectorField(
            item.GetProperty("path").GetString() ?? "",
            item.GetProperty("type").GetString() ?? "",
            item.TryGetProperty("unit", out JsonElement unit) && unit.ValueKind == JsonValueKind.String
                ? unit.GetString() ?? "" : "",
            Number(item, "min"), Number(item, "max"),
            item.TryGetProperty("value", out JsonElement value) ? value.Clone() : default,
            item.TryGetProperty("mixed", out JsonElement mixed) && mixed.ValueKind == JsonValueKind.True,
            !item.TryGetProperty("editable", out JsonElement editable) ||
            editable.ValueKind != JsonValueKind.False)).ToArray();
    }

    private static double? Number(JsonElement item, string key) =>
        item.TryGetProperty(key, out JsonElement value) &&
        value.ValueKind == JsonValueKind.Number ? value.GetDouble() : null;

    internal bool TryParseValue(string raw, out object? value, out string error)
    {
        value = null;
        error = "";
        if (!Editable)
        {
            error = "Este campo es de sólo lectura.";
            return false;
        }
        if (Type is "number" or "integer")
        {
            if ((!double.TryParse(raw, NumberStyles.Float,
                     CultureInfo.CurrentCulture, out double parsed) &&
                 !double.TryParse(raw, NumberStyles.Float,
                     CultureInfo.InvariantCulture, out parsed)) || !double.IsFinite(parsed))
            {
                error = "Introduce un número válido.";
                return false;
            }
            if (Type == "integer" && parsed != Math.Truncate(parsed))
            {
                error = "Introduce un número entero.";
                return false;
            }
            if (Type == "integer" && (parsed < int.MinValue || parsed > int.MaxValue))
            {
                error = "El entero supera el rango admitido.";
                return false;
            }
            if ((Minimum.HasValue && parsed < Minimum) ||
                (Maximum.HasValue && parsed > Maximum))
            {
                error = $"Rango permitido: {Minimum?.ToString(CultureInfo.CurrentCulture) ?? "−∞"} a " +
                    $"{Maximum?.ToString(CultureInfo.CurrentCulture) ?? "∞"}{(Unit.Length > 0 ? " " + Unit : "")}.";
                return false;
            }
            value = Type == "integer" ? checked((int)parsed) : parsed;
            return true;
        }
        if (Type == "boolean")
        {
            if (!bool.TryParse(raw, out bool parsed))
            {
                error = "Selecciona verdadero o falso.";
                return false;
            }
            value = parsed;
            return true;
        }
        if (Type == "reference" && !Guid.TryParse(raw, out _))
        {
            error = "Selecciona una referencia de asset válida.";
            return false;
        }
        if (raw.Length > 256)
        {
            error = "Máximo 256 caracteres.";
            return false;
        }
        value = raw;
        return true;
    }
}
