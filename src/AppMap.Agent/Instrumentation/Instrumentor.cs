using System.Reflection;
using System.Runtime.CompilerServices;
using AppMap.Config;
using AppMap.Util;
using HarmonyLib;

namespace AppMap.Instrumentation;

/// <summary>
/// Selects and patches application methods according to appmap.yml — the
/// counterpart of appmap-java's ClassFileTransformer + ConfigCondition,
/// using Harmony runtime patching instead of load-time bytecode rewriting.
/// Assemblies loaded after startup are picked up via AssemblyLoad.
/// </summary>
public sealed class Instrumentor
{
    private readonly Harmony harmony = new("com.appland.appmap");
    private readonly AppMapConfig config;
    private readonly HashSet<Assembly> instrumented = new();
    private readonly object gate = new();

    public Instrumentor(AppMapConfig config) => this.config = config;

    public void Start()
    {
        // Even with no packages: configured, assemblies may opt methods in
        // with [AppMap.Labels]; the per-assembly reference check keeps the
        // scan cheap in that case.
        AppDomain.CurrentDomain.AssemblyLoad += (_, args) => InstrumentAssembly(args.LoadedAssembly);
        foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            InstrumentAssembly(assembly);
    }

    private void InstrumentAssembly(Assembly assembly)
    {
        lock (gate)
        {
            if (!instrumented.Add(assembly))
                return;
        }
        if (assembly.IsDynamic || assembly == typeof(Instrumentor).Assembly)
            return;

        Type[] types;
        try
        {
            types = assembly.GetTypes();
        }
        catch (ReflectionTypeLoadException e)
        {
            types = e.Types.Where(t => t != null).ToArray()!;
        }
        catch (Exception e)
        {
            Logger.Debug($"cannot inspect {assembly.GetName().Name}: {e.Message}");
            return;
        }

        // Only assemblies that reference AppMap.Attributes (or define the
        // attribute themselves) can carry [AppMap.Labels]; checking once per
        // assembly keeps attribute probing off the common path.
        var mayHaveLabels = ReferencesLabelsAttribute(assembly);
        if (config.Packages.Count == 0 && !mayHaveLabels)
            return;

        var patched = 0;
        foreach (var type in types)
        {
            if (!IsInstrumentableType(type))
                continue;
            foreach (var method in CandidateMethods(type))
            {
                var fqn = $"{Output.Value.TypeName(type)}.{method.Name}";
                var package = config.FindPackage(fqn);
                // [AppMap.Labels] opts a method in even when its namespace
                // is not listed under packages:, as @Labels does in
                // appmap-java.
                var attributeLabels = mayHaveLabels ? AttributeLabels.Of(method) : null;
                if (package == null && attributeLabels == null)
                    continue;
                var labels = AttributeLabels.Merge(package?.LabelsFor(fqn), attributeLabels);
                if (HookPatcher.TryPatch(harmony, method, labels))
                    patched++;
            }
        }
        if (patched > 0)
            Logger.Debug($"instrumented {patched} method(s) in {assembly.GetName().Name}");
    }

    private static bool ReferencesLabelsAttribute(Assembly assembly)
    {
        if (assembly.GetReferencedAssemblies().Any(a => a.Name == "AppMap.Attributes"))
            return true;
        // Compatible attributes may also be declared in the assembly itself.
        return assembly.GetType(AttributeLabels.AttributeFullName, false) != null;
    }

    private static bool IsInstrumentableType(Type type)
    {
        if (!type.IsClass || type.IsGenericTypeDefinition)
            return false;
        // Skip compiler artifacts: closures, async state machines, etc.
        if (type.Name.Contains('<') || type.IsDefined(typeof(CompilerGeneratedAttribute), false))
            return false;
        return true;
    }

    private IEnumerable<MethodBase> CandidateMethods(Type type)
    {
        var visibility = BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static
            | BindingFlags.DeclaredOnly;
        if (Properties.RecordPrivate)
            visibility |= BindingFlags.NonPublic;

        var methods = type.GetMethods(visibility).Cast<MethodBase>()
            .Concat(type.GetConstructors(visibility & ~BindingFlags.Static));

        foreach (var method in methods)
        {
            if (method.IsAbstract || method.ContainsGenericParameters)
                continue;
            if (method.GetMethodBody() == null)
                continue;
            // Property accessors and other compiler-generated bodies are
            // trivial noise (the Java agent filters these too).
            if (method.IsDefined(typeof(CompilerGeneratedAttribute), false))
                continue;
            if (method.IsSpecialName && (method.Name.StartsWith("get_") || method.Name.StartsWith("set_")
                || method.Name.StartsWith("add_") || method.Name.StartsWith("remove_")
                || method.Name.StartsWith("op_")))
                continue;
            if (Properties.DefaultExcludes && IsDefaultExcluded(method))
                continue;
            yield return method;
        }
    }

    /// <summary>
    /// Methods skipped by default to keep recordings readable — the analog
    /// of appmap-java ignoring equals/hashCode/toString and friends. Opt out
    /// with APPMAP_DEFAULT_EXCLUDES=false. [AppMap.Labels] still wins: a
    /// labeled method is recorded regardless.
    /// </summary>
    public static bool IsDefaultExcluded(MethodBase method)
    {
        if (AttributeLabels.Of(method) != null)
            return false;

        // EF Core migrations and model snapshots are generated scaffolding.
        var ns = method.DeclaringType?.Namespace;
        if (ns != null && (ns == "Migrations" || ns.EndsWith(".Migrations", StringComparison.Ordinal)))
            return true;

        switch (method.Name)
        {
            case "Equals":
            case "GetHashCode":
            case "ToString":
            case "CompareTo":
            case "Deconstruct":
            case "Finalize":
            case "Dispose" when method.GetParameters().Length == 0:
                return true;
            default:
                return false;
        }
    }
}
