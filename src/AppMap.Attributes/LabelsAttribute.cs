using System;

namespace AppMap;

/// <summary>
/// Attaches AppMap labels to a method (or to every recorded method of a
/// class) — the analog of appmap-java's @Labels annotation. Labels appear on
/// the function's classMap entry and are what AppMap runtime analysis rules
/// match on (e.g. "security.authentication", "crypto.digest", "log", "crud").
///
/// A labeled method is instrumented even when its namespace is not listed
/// under packages: in appmap.yml, matching the Java agent's behavior. The
/// agent matches this attribute by full type name ("AppMap.LabelsAttribute"),
/// so any assembly identity works.
/// </summary>
[AttributeUsage(AttributeTargets.Method | AttributeTargets.Constructor | AttributeTargets.Class,
    Inherited = false)]
public sealed class LabelsAttribute : Attribute
{
    public LabelsAttribute(params string[] labels) => Labels = labels;

    public string[] Labels { get; }
}
