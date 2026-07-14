namespace AppMap.Config;

/// <summary>
/// Environment-variable knobs, mirroring com.appland.appmap.config.Properties
/// in appmap-java. All values are read lazily so tests can mutate the
/// environment.
/// </summary>
public static class Properties
{
    private static string? Env(string name) => Environment.GetEnvironmentVariable(name);

    private static bool Flag(string name, bool defaultValue)
    {
        var v = Env(name);
        if (string.IsNullOrEmpty(v))
            return defaultValue;
        return v is "true" or "1" or "yes" or "on";
    }

    /// <summary>APPMAP_CONFIG_FILE: explicit path to appmap.yml.</summary>
    public static string? ConfigFile => Env("APPMAP_CONFIG_FILE");

    /// <summary>APPMAP_OUTPUT_DIRECTORY: overrides appmap_dir from appmap.yml.</summary>
    public static string? OutputDirectory => Env("APPMAP_OUTPUT_DIRECTORY");

    /// <summary>APPMAP_RECORDING_REMOTE: serve /_appmap/record endpoints (default true).</summary>
    public static bool RecordingRemote => Flag("APPMAP_RECORDING_REMOTE", true);

    /// <summary>APPMAP_RECORDING_REQUESTS: record one AppMap per HTTP request (default true).</summary>
    public static bool RecordingRequests => Flag("APPMAP_RECORDING_REQUESTS", true);

    /// <summary>APPMAP_RECORD_PROCESS: record the whole process, written at exit (default false).</summary>
    public static bool RecordingProcess => Flag("APPMAP_RECORD_PROCESS", false);

    /// <summary>APPMAP_RECORDING_TESTS: record one AppMap per xUnit/NUnit test
    /// when the agent is attached, with no test-code changes (default true).</summary>
    public static bool RecordingTests => Flag("APPMAP_RECORDING_TESTS", true);

    /// <summary>APPMAP_RECORD_PRIVATE: instrument private methods too (default false).</summary>
    public static bool RecordPrivate => Flag("APPMAP_RECORD_PRIVATE", false);

    /// <summary>APPMAP_EVENT_DISABLEVALUE: never stringify parameter/return values.</summary>
    public static bool DisableValue => Flag("APPMAP_EVENT_DISABLEVALUE", false);

    /// <summary>APPMAP_EVENT_VALUESIZE: max length of a captured value string (default 1024, -1 unlimited).</summary>
    public static int MaxValueSize =>
        int.TryParse(Env("APPMAP_EVENT_VALUESIZE"), out var n) ? n : 1024;

    /// <summary>APPMAP_DEFAULT_EXCLUDES: skip noise methods (Equals, GetHashCode,
    /// ToString, ..., EF migrations) by default (default true).</summary>
    public static bool DefaultExcludes => Flag("APPMAP_DEFAULT_EXCLUDES", true);

    /// <summary>APPMAP_RECORD_ASYNC: emit an async method's return event when
    /// its Task completes, not when the Task is returned (default true).</summary>
    public static bool RecordAsync => Flag("APPMAP_RECORD_ASYNC", true);

    /// <summary>APPMAP_DEBUG: log agent diagnostics to stderr.</summary>
    public static bool Debug => Flag("APPMAP_DEBUG", false);

    /// <summary>APPMAP_DEBUG_DISABLEGIT: skip git metadata collection.</summary>
    public static bool DisableGit => Flag("APPMAP_DEBUG_DISABLEGIT", false);
}
