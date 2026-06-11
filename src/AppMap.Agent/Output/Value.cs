using System.Runtime.CompilerServices;
using AppMap.Config;

namespace AppMap.Output;

/// <summary>
/// A captured parameter, receiver, return value, or message entry —
/// the "value object" of the AppMap format (name, class, value, object_id,
/// kind). Mirrors com.appland.appmap.output.v1.Value.
/// </summary>
public sealed class Value
{
    public string? Name { get; set; }
    public string? Kind { get; set; }
    public string? Class { get; set; }
    public string? StringValue { get; set; }
    public long? ObjectId { get; set; }

    public static Value Capture(object? obj, string? name = null,
        Type? declaredType = null, string? kind = null)
    {
        var type = obj?.GetType() ?? declaredType;
        return new Value
        {
            Name = name,
            Kind = kind,
            Class = type != null ? TypeName(type) : "object",
            StringValue = Format(obj),
            ObjectId = obj == null || obj.GetType().IsValueType
                ? null
                : RuntimeHelpers.GetHashCode(obj),
        };
    }

    public static string TypeName(Type type) =>
        (type.FullName ?? type.Name).Replace('+', '.');

    private static string Format(object? obj)
    {
        if (obj == null)
            return "null";
        if (Properties.DisableValue)
            return "< disabled >";

        string text;
        try
        {
            text = obj.ToString() ?? "null";
        }
        catch
        {
            // A throwing ToString() must never take the recording down.
            return "< invalid >";
        }

        var max = Properties.MaxValueSize;
        if (max > 0 && text.Length > max)
            text = text.Substring(0, max - 3) + "...";
        return text;
    }
}
