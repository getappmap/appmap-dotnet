namespace AppMap.Output;

/// <summary>
/// One node of the class_map: a package (namespace segment), class, or
/// function. Mirrors com.appland.appmap.output.v1.CodeObject.
/// </summary>
public sealed class CodeObject
{
    public required string Name { get; init; }

    /// <summary>"package", "class", or "function".</summary>
    public required string Type { get; init; }

    public bool? Static { get; init; }

    /// <summary>"path:lineno" for functions, when source info is available.</summary>
    public string? Location { get; init; }

    public List<string>? Labels { get; init; }

    public List<CodeObject> Children { get; } = new();
}

/// <summary>
/// Accumulates the class_map for one recording: only code objects whose
/// functions actually produced events are included, matching the Java
/// agent's behavior. Thread-safe.
/// </summary>
public sealed class CodeObjectTree
{
    private readonly object gate = new();
    private readonly List<CodeObject> roots = new();

    /// <summary>
    /// Registers a function under namespaceName (dotted, possibly empty) and
    /// a chain of class names (outer-to-inner, for nested types).
    /// Idempotent per function.
    /// </summary>
    public void RegisterFunction(string namespaceName, IReadOnlyList<string> classChain,
        string functionName, bool isStatic, string? location, IReadOnlyList<string>? labels)
    {
        lock (gate)
        {
            var children = roots;
            if (namespaceName.Length > 0)
            {
                foreach (var part in namespaceName.Split('.'))
                    children = ChildOf(children, part, "package").Children;
            }
            foreach (var className in classChain)
                children = ChildOf(children, className, "class").Children;

            if (children.Any(c => c.Type == "function" && c.Name == functionName
                    && c.Static == isStatic))
                return;
            children.Add(new CodeObject
            {
                Name = functionName,
                Type = "function",
                Static = isStatic,
                Location = location,
                Labels = labels is { Count: > 0 } ? labels.ToList() : null,
            });
        }
    }

    public IReadOnlyList<CodeObject> Roots
    {
        get { lock (gate) { return roots.ToList(); } }
    }

    private static CodeObject ChildOf(List<CodeObject> children, string name, string type)
    {
        var existing = children.FirstOrDefault(c => c.Name == name && c.Type == type);
        if (existing != null)
            return existing;
        var node = new CodeObject { Name = name, Type = type };
        children.Add(node);
        return node;
    }
}
