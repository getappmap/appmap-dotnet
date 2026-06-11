using System.Reflection;

namespace AppMap.Instrumentation;

/// <summary>
/// Reads [AppMap.Labels(...)] from a method and its declaring type — the
/// analog of appmap-java's @Labels handling. The attribute is matched by
/// full type name rather than assembly identity, so applications may
/// reference any version of AppMap.Attributes (or define a compatible
/// attribute themselves).
/// </summary>
public static class AttributeLabels
{
    public const string AttributeFullName = "AppMap.LabelsAttribute";

    /// <summary>
    /// Labels declared on the method plus those on its declaring type, in
    /// declaration order with duplicates removed; null when neither carries
    /// the attribute.
    /// </summary>
    public static IReadOnlyList<string>? Of(MethodBase method)
    {
        var labels = Read(method.DeclaringType, null);
        labels = Read(method, labels);
        return labels;
    }

    private static List<string>? Read(MemberInfo? member, List<string>? labels)
    {
        if (member == null)
            return labels;

        IList<CustomAttributeData> attributes;
        try
        {
            // GetCustomAttributesData avoids instantiating the attribute, so
            // a name match never requires loading AppMap.Attributes itself.
            attributes = member.GetCustomAttributesData();
        }
        catch
        {
            return labels;
        }

        foreach (var data in attributes)
        {
            if (data.AttributeType.FullName != AttributeFullName)
                continue;
            if (data.ConstructorArguments.Count != 1
                || data.ConstructorArguments[0].Value
                    is not IReadOnlyCollection<CustomAttributeTypedArgument> items)
                continue;
            foreach (var item in items)
            {
                if (item.Value is string label && label.Length > 0)
                {
                    labels ??= new List<string>();
                    if (!labels.Contains(label))
                        labels.Add(label);
                }
            }
        }
        return labels;
    }

    /// <summary>Merges config-supplied labels with attribute labels.</summary>
    public static IReadOnlyList<string>? Merge(
        IReadOnlyList<string>? first, IReadOnlyList<string>? second)
    {
        if (first is not { Count: > 0 })
            return second;
        if (second is not { Count: > 0 })
            return first;
        var merged = new List<string>(first);
        foreach (var label in second)
        {
            if (!merged.Contains(label))
                merged.Add(label);
        }
        return merged;
    }
}
