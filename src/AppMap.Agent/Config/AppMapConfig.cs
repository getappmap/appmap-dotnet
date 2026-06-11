using AppMap.Util;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace AppMap.Config;

/// <summary>
/// In-memory representation of appmap.yml, mirroring
/// com.appland.appmap.config.AppMapConfig. The file is searched for in the
/// current directory and its ancestors unless APPMAP_CONFIG_FILE is set.
/// </summary>
public sealed class AppMapConfig
{
    [YamlMember(Alias = "name")]
    public string? Name { get; set; }

    [YamlMember(Alias = "appmap_dir")]
    public string AppMapDir { get; set; } = "tmp/appmap";

    [YamlMember(Alias = "packages")]
    public List<AppMapPackage> Packages { get; set; } = new();

    /// <summary>Directory containing appmap.yml; relative paths resolve against it.</summary>
    [YamlIgnore]
    public string BaseDirectory { get; set; } = Directory.GetCurrentDirectory();

    [YamlIgnore]
    public string OutputDirectory =>
        Properties.OutputDirectory ?? System.IO.Path.Combine(BaseDirectory, AppMapDir);

    private static AppMapConfig? current;

    public static AppMapConfig Current => current ??= Load();

    /// <summary>
    /// Finds the package entry covering a fully qualified name
    /// (namespace.Type.Member), or null if the name is not instrumented.
    /// First matching package wins, as in the Java agent.
    /// </summary>
    public AppMapPackage? FindPackage(string fullyQualifiedName)
    {
        foreach (var pkg in Packages)
        {
            if (pkg.Matches(fullyQualifiedName))
                return pkg;
        }
        return null;
    }

    public static AppMapConfig Load()
    {
        var path = Properties.ConfigFile ?? FindConfigFile();
        if (path == null || !File.Exists(path))
        {
            Logger.Warn("appmap.yml not found; only HTTP and SQL events will be recorded. "
                + "Create appmap.yml with a 'packages:' list to record application code.");
            return new AppMapConfig { Name = AppNameFallback() };
        }

        try
        {
            var deserializer = new DeserializerBuilder()
                .WithNamingConvention(UnderscoredNamingConvention.Instance)
                .IgnoreUnmatchedProperties()
                .Build();
            var config = deserializer.Deserialize<AppMapConfig>(File.ReadAllText(path))
                ?? new AppMapConfig();
            config.Name ??= AppNameFallback();
            config.BaseDirectory = System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(path))!;
            Logger.Debug($"loaded config from {path}: name={config.Name}, "
                + $"{config.Packages.Count} package(s)");
            return config;
        }
        catch (Exception e)
        {
            Logger.Warn($"failed to parse {path}: {e.Message}; using empty config");
            return new AppMapConfig { Name = AppNameFallback() };
        }
    }

    private static string? FindConfigFile()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            var candidate = System.IO.Path.Combine(dir.FullName, "appmap.yml");
            if (File.Exists(candidate))
                return candidate;
            dir = dir.Parent;
        }
        return null;
    }

    private static string AppNameFallback() =>
        System.IO.Path.GetFileName(Directory.GetCurrentDirectory());
}

/// <summary>
/// One entry of the packages: list. <c>path</c> is a namespace (or namespace
/// prefix) such as <c>MyApp.Services</c>; <c>exclude</c> entries are
/// fully-qualified-name prefixes carved back out of the package.
/// </summary>
public sealed class AppMapPackage
{
    [YamlMember(Alias = "path")]
    public string? Path { get; set; }

    [YamlMember(Alias = "exclude")]
    public List<string> Exclude { get; set; } = new();

    [YamlMember(Alias = "shallow")]
    public bool Shallow { get; set; }

    [YamlMember(Alias = "methods")]
    public List<MethodConfig>? Methods { get; set; }

    public bool Matches(string fullyQualifiedName)
    {
        if (Path == null || !IsPrefix(Path, fullyQualifiedName))
            return false;

        if (Methods != null)
            return Methods.Any(m => m.Matches(fullyQualifiedName));

        return !Exclude.Any(e => IsPrefix(e, fullyQualifiedName));
    }

    /// <summary>Labels contributed by a matching methods: entry, if any.</summary>
    public IReadOnlyList<string>? LabelsFor(string fullyQualifiedName) =>
        Methods?.FirstOrDefault(m => m.Matches(fullyQualifiedName))?.Labels;

    private static bool IsPrefix(string prefix, string name) =>
        name == prefix || name.StartsWith(prefix + ".", StringComparison.Ordinal);
}

/// <summary>A methods: entry — regex match on class and method name.</summary>
public sealed class MethodConfig
{
    [YamlMember(Alias = "class")]
    public string? Class { get; set; }

    [YamlMember(Alias = "name")]
    public string? Name { get; set; }

    [YamlMember(Alias = "labels")]
    public List<string> Labels { get; set; } = new();

    public bool Matches(string fullyQualifiedName)
    {
        var lastDot = fullyQualifiedName.LastIndexOf('.');
        if (lastDot < 0)
            return false;
        var className = fullyQualifiedName.Substring(0, lastDot);
        var methodName = fullyQualifiedName.Substring(lastDot + 1);

        if (Class != null && !System.Text.RegularExpressions.Regex.IsMatch(className, Class))
            return false;
        return Name == null || System.Text.RegularExpressions.Regex.IsMatch(methodName, Name);
    }
}
