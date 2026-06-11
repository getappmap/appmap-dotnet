using System.Collections.Concurrent;
using System.Reflection;
using AppMap.Output;
using AppMap.Util;

namespace AppMap.Instrumentation;

/// <summary>
/// Per-method immutable facts gathered once at patch time — the analog of
/// appmap-java's EventTemplateRegistry, which caches event templates built
/// from bytecode so the hot path only clones them.
/// </summary>
public sealed class MethodTemplate
{
    public required string DefinedClass { get; init; }
    public required string MethodId { get; init; }
    public required bool IsStatic { get; init; }
    public string? Path { get; init; }
    public int? LineNo { get; init; }
    public required string NamespaceName { get; init; }
    public required IReadOnlyList<string> ClassChain { get; init; }
    public required IReadOnlyList<ParameterInfo> Parameters { get; init; }
    public IReadOnlyList<string>? Labels { get; init; }

    public Event BuildCallEvent(object? instance, object?[]? args)
    {
        var e = new Event
        {
            EventType = "call",
            DefinedClass = DefinedClass,
            MethodId = MethodId,
            Static = IsStatic,
            Path = Path,
            LineNo = LineNo,
        };
        if (!IsStatic && instance != null)
            e.Receiver = Value.Capture(instance, kind: "req");
        if (args != null)
        {
            e.Parameters = new List<Value>(args.Length);
            for (var i = 0; i < args.Length; i++)
            {
                var p = i < Parameters.Count ? Parameters[i] : null;
                e.Parameters.Add(Value.Capture(args[i],
                    name: p?.Name ?? $"arg{i}",
                    declaredType: p?.ParameterType,
                    kind: "req"));
            }
        }
        return e;
    }

    public Event BuildReturnEvent(int parentId, double elapsedSeconds,
        object? result, Type? returnType, Exception? exception)
    {
        var e = new Event
        {
            EventType = "return",
            ParentId = parentId,
            Elapsed = elapsedSeconds,
        };
        if (exception != null)
            e.Exceptions = ExceptionValue.ChainOf(exception);
        else if (returnType != null && returnType != typeof(void))
            e.ReturnValue = Value.Capture(result, declaredType: returnType);
        return e;
    }

    public void RegisterCodeObject(CodeObjectTree tree) =>
        tree.RegisterFunction(NamespaceName, ClassChain, MethodId, IsStatic,
            Path != null && LineNo.HasValue ? $"{Path}:{LineNo}" : Path, Labels);
}

public static class EventTemplateRegistry
{
    private static readonly ConcurrentDictionary<MethodBase, MethodTemplate> templates = new();

    public static MethodTemplate? Get(MethodBase method) =>
        templates.TryGetValue(method, out var t) ? t : null;

    /// <summary>True once a method has been registered (and so patched);
    /// used to keep the config-driven instrumentor and the built-in hooks
    /// from double-patching the same method.</summary>
    public static bool IsRegistered(MethodBase method) => templates.ContainsKey(method);

    public static MethodTemplate Register(MethodBase method, IReadOnlyList<string>? labels)
    {
        return templates.GetOrAdd(method, m =>
        {
            var type = m.DeclaringType!;
            var (path, lineno) = SourceLocator.Locate(m);

            // Nested types come back as Outer+Inner; the class_map wants the
            // chain, defined_class wants dots.
            var classChain = new List<string>();
            for (var t = type; t != null; t = t.DeclaringType)
                classChain.Insert(0, t.Name);

            return new MethodTemplate
            {
                DefinedClass = Value.TypeName(type),
                MethodId = m.IsConstructor ? (m.IsStatic ? ".cctor" : ".ctor") : m.Name,
                IsStatic = m.IsStatic,
                Path = path,
                LineNo = lineno,
                NamespaceName = type.Namespace ?? string.Empty,
                ClassChain = classChain,
                Parameters = m.GetParameters(),
                Labels = labels,
            };
        });
    }
}
