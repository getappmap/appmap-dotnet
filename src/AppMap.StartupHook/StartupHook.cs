using System.Reflection;
using System.Runtime.CompilerServices;

/// <summary>
/// .NET startup hook — the direct analog of the Java agent's premain.
/// Activate with:
///   DOTNET_STARTUP_HOOKS=/path/to/AppMap.StartupHook.dll dotnet run
/// The class must be named StartupHook, outside any namespace, with a static
/// Initialize(): that is the contract the runtime requires.
/// </summary>
internal class StartupHook
{
    public static void Initialize()
    {
        // The hook assembly is loaded outside the app's dependency context,
        // so AppMap.Agent.dll and its dependencies won't resolve on their
        // own. Resolve them from this assembly's directory.
        var dir = Path.GetDirectoryName(typeof(StartupHook).Assembly.Location)!;
        AppDomain.CurrentDomain.AssemblyResolve += (_, args) =>
        {
            var name = new AssemblyName(args.Name).Name;
            if (name == null)
                return null;
            var candidate = Path.Combine(dir, name + ".dll");
            return File.Exists(candidate) ? Assembly.LoadFrom(candidate) : null;
        };
        Boot();
    }

    // Kept out of Initialize() so the JIT doesn't resolve AppMap.Agent
    // before the AssemblyResolve handler is in place.
    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Boot() => AppMap.AgentBootstrap.Init();
}
