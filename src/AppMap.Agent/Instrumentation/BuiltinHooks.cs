using System.Reflection;
using AppMap.Util;
using HarmonyLib;

namespace AppMap.Instrumentation;

/// <summary>
/// One built-in hook: a framework type (matched by exact name, base class,
/// or implemented interface) whose named methods are recorded with the given
/// labels regardless of the packages: configuration.
/// </summary>
public sealed class HookRule
{
    /// <summary>Full name of the type, base class, or interface to match.</summary>
    public required string Type { get; init; }

    public required string[] Methods { get; init; }

    public required string[] Labels { get; init; }

    public bool Matches(Type type)
    {
        for (var t = type; t != null; t = t.BaseType)
        {
            if (t.FullName == Type)
                return true;
        }
        foreach (var i in type.GetInterfaces())
        {
            if (i.FullName == Type)
                return true;
        }
        return false;
    }
}

/// <summary>
/// Pre-labeled hooks for framework code — the analog of appmap-java's
/// bundled hook definitions for the JDK, Spring Security, slf4j, Jackson,
/// etc. AppMap runtime analysis rules match on these function labels
/// (security.authentication, crypto.*, deserialize.unsafe, log, ...), so
/// they are instrumented even though they fall outside the application's
/// packages: configuration. Provider assemblies loaded later are caught by
/// the AssemblyLoad handler, as in SqlHooks.
/// </summary>
public static class BuiltinHooks
{
    /// <summary>
    /// Label taxonomy follows appmap-java's hooks so existing AppMap
    /// analysis rules apply unchanged.
    /// </summary>
    public static readonly IReadOnlyList<HookRule> Rules = new HookRule[]
    {
        // Logging — powers e.g. the secret-in-log analysis. App code logs
        // through the static LoggerExtensions methods, which are non-generic
        // and so patchable (ILogger.Log<TState> itself is an open generic).
        new()
        {
            Type = "Microsoft.Extensions.Logging.LoggerExtensions",
            Methods = new[] { "Log", "LogTrace", "LogDebug", "LogInformation",
                "LogWarning", "LogError", "LogCritical" },
            Labels = new[] { "log" },
        },

        // Authentication / authorization (ASP.NET Core). SignInManager<T> is
        // an open generic type, so hook the non-generic services beneath it.
        new()
        {
            Type = "Microsoft.AspNetCore.Authentication.IAuthenticationService",
            Methods = new[] { "AuthenticateAsync", "SignInAsync", "SignOutAsync" },
            Labels = new[] { "security.authentication" },
        },
        new()
        {
            Type = "Microsoft.AspNetCore.Authorization.IAuthorizationService",
            Methods = new[] { "AuthorizeAsync" },
            Labels = new[] { "security.authorization" },
        },

        // Cryptography. ComputeHash lives concrete on the abstract
        // HashAlgorithm base, so the base-class match patches it once.
        new()
        {
            Type = "System.Security.Cryptography.SymmetricAlgorithm",
            Methods = new[] { "CreateEncryptor" },
            Labels = new[] { "crypto.encrypt" },
        },
        new()
        {
            Type = "System.Security.Cryptography.SymmetricAlgorithm",
            Methods = new[] { "CreateDecryptor" },
            Labels = new[] { "crypto.decrypt" },
        },
        new()
        {
            Type = "System.Security.Cryptography.HashAlgorithm",
            Methods = new[] { "ComputeHash", "ComputeHashAsync" },
            Labels = new[] { "crypto.digest" },
        },

        // Serialization — deserialize.unsafe powers the
        // deserialization-of-untrusted-data finding.
        new()
        {
            Type = "System.Runtime.Serialization.Formatters.Binary.BinaryFormatter",
            Methods = new[] { "Deserialize", "UnsafeDeserialize" },
            Labels = new[] { "deserialize.unsafe" },
        },
        // System.Text.Json.JsonSerializer.Deserialize is deliberately not
        // hooked: its overloads are generic or [RequiresDynamicCode]
        // intrinsics that Harmony cannot patch, and JSON is a low-risk
        // deserialization sink anyway. Newtonsoft and XML stay.
        new()
        {
            Type = "Newtonsoft.Json.JsonConvert",
            Methods = new[] { "DeserializeObject" },
            Labels = new[] { "deserialize" },
        },
        // XmlSerializer.Deserialize is the classic XXE / unsafe-XML sink.
        new()
        {
            Type = "System.Xml.Serialization.XmlSerializer",
            Methods = new[] { "Deserialize" },
            Labels = new[] { "deserialize" },
        },
        new()
        {
            Type = "System.Runtime.Serialization.DataContractSerializer",
            Methods = new[] { "ReadObject" },
            Labels = new[] { "deserialize" },
        },

        // NOTE: RandomNumberGenerator (random.secure) and AsymmetricAlgorithm
        // Sign/Verify (crypto.sign/verify) were tried here but their methods
        // on the abstract BCL crypto bases are intrinsic-backed and make
        // Harmony throw InvalidProgramException at patch time (safely caught,
        // but noisy). They need concrete-type targeting first — see BACKLOG.

        // Outbound HTTP — the analog of appmap-java's HTTP client hooks
        // (http_client_request is a future event type; the label lets
        // analysis find external calls today).
        new()
        {
            Type = "System.Net.Http.HttpClient",
            Methods = new[] { "SendAsync", "Send" },
            Labels = new[] { "http.client.request" },
        },

        // HTTP session (ASP.NET Core).
        new()
        {
            Type = "Microsoft.AspNetCore.Http.ISession",
            Methods = new[] { "TryGetValue" },
            Labels = new[] { "http.session.read" },
        },
        new()
        {
            Type = "Microsoft.AspNetCore.Http.ISession",
            Methods = new[] { "Set", "Remove", "Clear" },
            Labels = new[] { "http.session.write" },
        },

        // Background jobs.
        new()
        {
            Type = "Hangfire.IBackgroundJobClient",
            Methods = new[] { "Create" },
            Labels = new[] { "job.create" },
        },
    };

    private static readonly Harmony harmony = new("com.appland.appmap.builtin");
    private static readonly HashSet<Assembly> seen = new();
    private static readonly object gate = new();

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
        if (assembly.IsDynamic || assembly == typeof(BuiltinHooks).Assembly)
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
        catch
        {
            return;
        }

        var patched = 0;
        foreach (var type in types)
        {
            // Open generic types cannot be patched (Harmony limitation).
            if (!type.IsClass || type.IsGenericTypeDefinition)
                continue;
            foreach (var rule in Rules)
            {
                if (!rule.Matches(type))
                    continue;
                foreach (var name in rule.Methods)
                {
                    foreach (var method in type.GetMethods(BindingFlags.Public
                        | BindingFlags.NonPublic | BindingFlags.Instance
                        | BindingFlags.Static | BindingFlags.DeclaredOnly))
                    {
                        if (method.Name != name || method.IsAbstract
                            || method.ContainsGenericParameters
                            || method.GetMethodBody() == null)
                            continue;
                        if (HookPatcher.TryPatch(harmony, method, rule.Labels))
                            patched++;
                    }
                }
            }
        }
        if (patched > 0)
            Logger.Debug($"built-in hooks: {patched} method(s) in {assembly.GetName().Name}");
    }
}
