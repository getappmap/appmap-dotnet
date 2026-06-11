using System.Data.Common;
using System.Diagnostics;
using System.Reflection;
using AppMap.Output;
using AppMap.Record;
using AppMap.Util;
using HarmonyLib;

namespace AppMap.Instrumentation;

/// <summary>
/// Records sql_query events by patching the Execute* overrides of every
/// concrete DbCommand implementation found in the process — the .NET analog
/// of appmap-java's JDBC Statement hooks. Provider assemblies loaded later
/// are caught by the AssemblyLoad handler.
/// </summary>
public static class SqlHooks
{
    private static readonly Harmony harmony = new("com.appland.appmap.sql");
    private static readonly HashSet<Assembly> seen = new();
    private static readonly object gate = new();

    private static readonly string[] ExecuteMethods =
    {
        "ExecuteNonQuery", "ExecuteScalar", "ExecuteDbDataReader",
        "ExecuteNonQueryAsync", "ExecuteScalarAsync", "ExecuteDbDataReaderAsync",
    };

    public static void Install()
    {
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
        if (assembly.IsDynamic)
            return;

        // Cheap pre-filter: only providers reference System.Data.Common.
        if (!assembly.GetReferencedAssemblies().Any(a =>
                a.Name is "System.Data.Common" or "System.Data" or "netstandard"))
            return;

        Type[] types;
        try
        {
            types = assembly.GetTypes();
        }
        catch (ReflectionTypeLoadException e)
        {
            // Providers are routinely only partially loadable (optional
            // dependencies the app doesn't ship); patch the types that did
            // load instead of bailing. Microsoft.Data.SqlClient on Linux
            // hits this, and SqlCommand itself loads fine.
            types = e.Types.Where(t => t is not null).Cast<Type>().ToArray();
            Logger.Debug($"partial type load in {assembly.GetName().Name}: "
                + $"{e.LoaderExceptions.Length} loader error(s), "
                + $"{types.Length} usable type(s)");
        }
        catch
        {
            return;
        }

        foreach (var type in types)
        {
            if (type.IsAbstract || !typeof(DbCommand).IsAssignableFrom(type))
                continue;
            foreach (var name in ExecuteMethods)
            {
                var method = type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic
                        | BindingFlags.Instance | BindingFlags.DeclaredOnly)
                    .FirstOrDefault(m => m.Name == name && !m.ContainsGenericParameters);
                if (method == null || method.GetMethodBody() == null)
                    continue;
                try
                {
                    harmony.Patch(method,
                        prefix: new HarmonyMethod(typeof(SqlHooks), nameof(Prefix)),
                        finalizer: new HarmonyMethod(typeof(SqlHooks), nameof(Finalizer)));
                    Logger.Debug($"hooked {type.Name}.{name}");
                }
                catch (Exception e)
                {
                    Logger.Debug($"cannot hook {type.Name}.{name}: {e.Message}");
                }
            }
        }
    }

    private sealed class SqlCallContext
    {
        public required int CallEventId { get; init; }
        public required long StartTimestamp { get; init; }
    }

    [ThreadStatic]
    private static bool inHook;

    public static void Prefix(MethodBase __originalMethod, object __instance, ref object? __state)
    {
        __state = null;
        if (inHook || !Recorder.Instance.HasActiveSession || __instance is not DbCommand command)
            return;
        inHook = true;
        try
        {
            var e = new Event
            {
                EventType = "call",
                DefinedClass = Value.TypeName(__instance.GetType()),
                MethodId = __originalMethod.Name,
                Static = false,
                SqlQuery = new SqlQuery
                {
                    Sql = command.CommandText,
                    DatabaseType = DatabaseTypeOf(__instance.GetType()),
                },
            };
            Recorder.Instance.Add(e);
            __state = new SqlCallContext
            {
                CallEventId = e.Id,
                StartTimestamp = Stopwatch.GetTimestamp(),
            };
        }
        catch (Exception ex)
        {
            Logger.Error("sql call hook failed", ex);
        }
        finally
        {
            inHook = false;
        }
    }

    public static Exception? Finalizer(Exception? __exception, object? __state)
    {
        if (__state is SqlCallContext ctx && !inHook)
        {
            inHook = true;
            try
            {
                var e = new Event
                {
                    EventType = "return",
                    ParentId = ctx.CallEventId,
                    Elapsed = (Stopwatch.GetTimestamp() - ctx.StartTimestamp)
                        / (double)Stopwatch.Frequency,
                };
                if (__exception != null)
                    e.Exceptions = ExceptionValue.ChainOf(__exception);
                Recorder.Instance.Add(e);
            }
            catch (Exception ex)
            {
                Logger.Error("sql return hook failed", ex);
            }
            finally
            {
                inHook = false;
            }
        }
        return __exception;
    }

    /// <summary>
    /// JDBC exposes DatabaseProductName; ADO.NET has no portable equivalent,
    /// so infer from the provider's type name.
    /// </summary>
    private static string DatabaseTypeOf(Type commandType)
    {
        var name = commandType.FullName ?? commandType.Name;
        if (name.IndexOf("Npgsql", StringComparison.OrdinalIgnoreCase) >= 0)
            return "postgres";
        if (name.IndexOf("Sqlite", StringComparison.OrdinalIgnoreCase) >= 0)
            return "sqlite";
        if (name.IndexOf("MySql", StringComparison.OrdinalIgnoreCase) >= 0)
            return "mysql";
        if (name.IndexOf("Oracle", StringComparison.OrdinalIgnoreCase) >= 0)
            return "oracle";
        if (name.IndexOf("SqlClient", StringComparison.OrdinalIgnoreCase) >= 0)
            return "mssql";
        return commandType.Name.ToLowerInvariant();
    }
}
