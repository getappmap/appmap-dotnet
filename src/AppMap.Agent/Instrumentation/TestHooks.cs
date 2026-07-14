using System.Reflection;
using AppMap.Config;
using AppMap.Output;
using AppMap.Record;
using AppMap.Util;
using HarmonyLib;

namespace AppMap.Instrumentation;

/// <summary>
/// Records one AppMap per test with NO change to the test code — the analog
/// of appmap-java recording JUnit tests when the agent is attached. When the
/// agent is active (startup hook / runner) and a test framework loads, its
/// per-test execution method is patched to bracket a recording session:
///
///   - xUnit:  Xunit.Sdk.XunitTestRunner.InvokeTestAsync
///   - NUnit:  NUnit.Framework.Internal.Commands.TestMethodCommand.Execute
///
/// Sessions are AsyncLocal, so parallel test execution records coherent
/// per-test maps. Disable with APPMAP_RECORDING_TESTS=false, or per
/// test/class with [AppMap.NoRecord] (matched by full type name, so only the
/// dependency-free AppMap.Attributes package is needed). The
/// [AppMap] attributes in AppMap.Testing.* remain for explicit control and
/// for runs without the agent attached; they stand down when these hooks are
/// active.
/// </summary>
public static class TestHooks
{
    private static readonly Harmony harmony = new("com.appland.appmap.tests");
    private static readonly HashSet<Assembly> seen = new();
    private static readonly object gate = new();
    private static int installed;

    /// <summary>True once a test framework has been hooked in this process.</summary>
    public static bool Active { get; private set; }

    public static void Install()
    {
        if (!Properties.RecordingTests)
            return;
        if (Interlocked.Exchange(ref installed, 1) == 1)
            return;
        AppDomain.CurrentDomain.AssemblyLoad += (_, args) => Scan(args.LoadedAssembly);
        foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            Scan(assembly);
    }

    private static void Scan(Assembly assembly)
    {
        lock (gate)
        {
            if (!seen.Add(assembly))
                return;
        }
        try
        {
            switch (assembly.GetName().Name)
            {
                case "xunit.execution.dotnet":
                case "xunit.execution.desktop":
                    Patch(assembly, "Xunit.Sdk.XunitTestRunner", "InvokeTestAsync",
                        nameof(XunitPrefix), nameof(XunitFinalizer), "xunit");
                    break;
                case "nunit.framework":
                    Patch(assembly, "NUnit.Framework.Internal.Commands.TestMethodCommand",
                        "Execute", nameof(NUnitPrefix), nameof(NUnitFinalizer), "nunit");
                    break;
            }
        }
        catch (Exception e)
        {
            Logger.Debug($"test hooks: cannot patch {assembly.GetName().Name}: {e.Message}");
        }
    }

    private static void Patch(Assembly assembly, string typeName, string methodName,
        string prefix, string finalizer, string framework)
    {
        var method = assembly.GetType(typeName)?.GetMethod(methodName,
            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
        if (method == null)
        {
            Logger.Debug($"test hooks: {typeName}.{methodName} not found");
            return;
        }
        harmony.Patch(method,
            prefix: new HarmonyMethod(typeof(TestHooks), prefix),
            finalizer: new HarmonyMethod(typeof(TestHooks), finalizer));
        Active = true;
        Logger.Debug($"test hooks: recording {framework} tests (one AppMap per test)");
    }

    // --- xUnit -------------------------------------------------------------

    public static void XunitPrefix(object __instance, ref object? __state)
    {
        __state = null;
        try
        {
            var testMethod = Prop(__instance, "TestMethod") as MethodInfo;
            var testClass = Prop(__instance, "TestClass") as Type ?? testMethod?.DeclaringType;
            if (!ShouldRecord(testMethod, testClass))
                return;
            __state = Start("xunit", testClass, testMethod,
                Prop(__instance, "DisplayName") as string);
        }
        catch (Exception e)
        {
            Logger.Error("xunit test hook failed", e);
        }
    }

    public static Exception? XunitFinalizer(object __instance, object? aggregator,
        object? __state, object? __result, Exception? __exception)
    {
        if (__state is not RecordingSession session)
            return __exception;
        try
        {
            var testMethod = Prop(__instance, "TestMethod") as MethodInfo;
            var testClass = Prop(__instance, "TestClass") as Type ?? testMethod?.DeclaringType;
            if (__exception == null && __result is Task task)
            {
                // The test body runs inside the returned task; save when it
                // completes. xUnit reports assertion failures through the
                // ExceptionAggregator argument rather than a thrown exception.
                task.ContinueWith(t => Save(session, testClass, testMethod,
                        failed: t.IsFaulted || HasExceptions(aggregator)),
                    TaskContinuationOptions.ExecuteSynchronously);
            }
            else
            {
                Save(session, testClass, testMethod, failed: true);
            }
        }
        catch (Exception e)
        {
            Logger.Error("xunit test hook failed", e);
        }
        return __exception;
    }

    private static bool HasExceptions(object? aggregator)
    {
        try
        {
            return aggregator != null && Prop(aggregator, "HasExceptions") is true;
        }
        catch
        {
            return false;
        }
    }

    // --- NUnit -------------------------------------------------------------

    public static void NUnitPrefix(object context, ref object? __state)
    {
        __state = null;
        try
        {
            var (testClass, testMethod) = NUnitTestOf(context);
            if (!ShouldRecord(testMethod, testClass))
                return;
            __state = Start("nunit", testClass, testMethod, null);
        }
        catch (Exception e)
        {
            Logger.Error("nunit test hook failed", e);
        }
    }

    public static Exception? NUnitFinalizer(object context, object? __state,
        object? __result, Exception? __exception)
    {
        if (__state is not RecordingSession session)
            return __exception;
        try
        {
            var (testClass, testMethod) = NUnitTestOf(context);
            // TestResult.ResultState.Status: Passed / Failed / Skipped / ...
            var status = __result is { } result
                ? Prop(Prop(result, "ResultState")!, "Status")?.ToString() : null;
            Save(session, testClass, testMethod,
                failed: __exception != null || status == "Failed");
        }
        catch (Exception e)
        {
            Logger.Error("nunit test hook failed", e);
        }
        return __exception;
    }

    private static (Type?, MethodInfo?) NUnitTestOf(object context)
    {
        var test = Prop(context, "CurrentTest");
        if (test == null)
            return (null, null);
        var method = Prop(test, "Method") is { } m ? Prop(m, "MethodInfo") as MethodInfo : null;
        var type = Prop(test, "TypeInfo") is { } t ? Prop(t, "Type") as Type : null;
        return (type ?? method?.DeclaringType, method);
    }

    // --- shared ------------------------------------------------------------

    private static bool ShouldRecord(MethodInfo? method, Type? type)
    {
        if (!Properties.RecordingTests)
            return false;
        // Don't nest inside another local recording (a test spawning a test).
        if (Recorder.Instance.LocalSession != null)
            return false;
        return !HasNoRecord(method) && !HasNoRecord(type);
    }

    /// <summary>[AppMap.NoRecord] opt-out, matched by full type name so only
    /// the dependency-free attributes package is required.</summary>
    private static bool HasNoRecord(MemberInfo? member)
    {
        if (member == null)
            return false;
        try
        {
            return member.GetCustomAttributes(inherit: true)
                .Any(a => a.GetType().FullName == "AppMap.NoRecordAttribute");
        }
        catch
        {
            return false;
        }
    }

    private static RecordingSession Start(string framework, Type? testClass,
        MethodInfo? testMethod, string? displayName)
    {
        var definedClass = testClass != null ? Value.TypeName(testClass) : null;
        var name = displayName
            ?? (definedClass != null && testMethod != null
                ? $"{definedClass}.{testMethod.Name}" : "test");
        var (path, lineno) = testMethod != null
            ? SourceLocator.Locate(testMethod) : (null, null);
        Recorder.Instance.StartLocal(new Metadata
        {
            RecorderName = framework,
            RecorderType = "tests",
            Name = name,
            RecordingDefinedClass = definedClass,
            RecordingMethodId = testMethod?.Name,
            SourceLocation = path != null && lineno.HasValue ? $"{path}:{lineno}" : null,
            Frameworks = { new Framework { Name = framework } },
        });
        return Recorder.Instance.LocalSession!;
    }

    private static void Save(RecordingSession session, Type? testClass,
        MethodInfo? testMethod, bool failed)
    {
        try
        {
            var recording = Recorder.Instance.FinishLocal(session);
            if (recording.EventCount == 0)
            {
                recording.Discard();
                return;
            }
            recording.Metadata.TestStatus = failed ? "failed" : "succeeded";
            var definedClass = testClass != null ? Value.TypeName(testClass) : "test";
            recording.Save($"{definedClass}_{testMethod?.Name ?? "unknown"}");
        }
        catch (Exception e)
        {
            Logger.Error("failed to save test recording", e);
        }
    }

    /// <summary>Reflection property read that also sees non-public members on
    /// base classes (the xUnit runner state lives on a generic base).</summary>
    private static object? Prop(object o, string name)
    {
        for (var t = o.GetType(); t != null; t = t.BaseType)
        {
            var p = t.GetProperty(name, BindingFlags.Public | BindingFlags.NonPublic
                | BindingFlags.Instance | BindingFlags.DeclaredOnly);
            if (p != null)
                return p.GetValue(o);
        }
        return null;
    }
}
