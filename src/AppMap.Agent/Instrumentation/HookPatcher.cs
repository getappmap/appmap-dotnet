using System.Reflection;
using AppMap.Util;
using HarmonyLib;

namespace AppMap.Instrumentation;

/// <summary>
/// Applies the MethodHooks prefix/finalizer pair to a method and registers
/// its event template — shared by the config-driven Instrumentor and the
/// built-in framework hooks.
/// </summary>
internal static class HookPatcher
{
    /// <summary>
    /// Patches the method unless it was already patched by another hook
    /// source. Returns true when this call performed the patch.
    /// </summary>
    public static bool TryPatch(Harmony harmony, MethodBase method,
        IReadOnlyList<string>? labels)
    {
        if (EventTemplateRegistry.IsRegistered(method))
            return false;
        try
        {
            var isVoid = method is MethodInfo { ReturnType.FullName: "System.Void" }
                || method.IsConstructor;
            var finalizer = isVoid ? nameof(MethodHooks.FinalizerVoid) : nameof(MethodHooks.Finalizer);
            EventTemplateRegistry.Register(method, labels);
            harmony.Patch(method,
                prefix: new HarmonyMethod(typeof(MethodHooks), nameof(MethodHooks.Prefix)),
                finalizer: new HarmonyMethod(typeof(MethodHooks), finalizer));
            return true;
        }
        catch (Exception e)
        {
            // Some methods (JIT intrinsics, [RequiresDynamicCode] BCL
            // helpers) cannot be rewritten and make Harmony throw; the
            // method is simply left uninstrumented. Phrased as a skip so the
            // CLR's "invalid program" wording does not read like the agent
            // broke something.
            Logger.Debug($"skipping {method.DeclaringType?.Name}.{method.Name} "
                + $"(not instrumentable: {e.GetType().Name})");
            return false;
        }
    }
}
