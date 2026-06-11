using AppMap.Config;

namespace AppMap.Util;

/// <summary>
/// Collects git repository/branch/commit for the metadata section by reading
/// .git directly (the Java agent uses JGit; we avoid the dependency).
/// </summary>
public sealed class GitMetadata
{
    public string? Repository { get; private init; }
    public string? Branch { get; private init; }
    public string? Commit { get; private init; }

    private static GitMetadata? cached;
    private static bool resolved;

    public static GitMetadata? Collect()
    {
        if (resolved)
            return cached;
        resolved = true;
        if (Properties.DisableGit)
            return null;
        try
        {
            cached = Read();
        }
        catch (Exception e)
        {
            Logger.Debug($"git metadata unavailable: {e.Message}");
        }
        return cached;
    }

    private static GitMetadata? Read()
    {
        var gitDir = FindGitDir();
        if (gitDir == null)
            return null;

        string? branch = null, commit = null;
        var head = File.ReadAllText(Path.Combine(gitDir, "HEAD")).Trim();
        if (head.StartsWith("ref: ", StringComparison.Ordinal))
        {
            var refName = head.Substring(5);
            branch = refName.StartsWith("refs/heads/", StringComparison.Ordinal)
                ? refName.Substring(11) : refName;
            commit = ResolveRef(gitDir, refName);
        }
        else
        {
            commit = head; // detached HEAD
        }

        return new GitMetadata
        {
            Repository = ReadOriginUrl(gitDir),
            Branch = branch,
            Commit = commit,
        };
    }

    /// <summary>
    /// The working-tree root (the directory containing <c>.git</c>), or null
    /// when not in a git checkout. Used to relativize source paths so maps
    /// recorded on one machine/OS resolve on another.
    /// </summary>
    public static string? RepositoryRoot
    {
        get
        {
            var gitDir = FindGitDir();
            return gitDir == null ? null : Path.GetDirectoryName(gitDir);
        }
    }

    private static string? FindGitDir()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            var candidate = Path.Combine(dir.FullName, ".git");
            if (Directory.Exists(candidate))
                return candidate;
            dir = dir.Parent;
        }
        return null;
    }

    private static string? ResolveRef(string gitDir, string refName)
    {
        var refFile = Path.Combine(gitDir, refName);
        if (File.Exists(refFile))
            return File.ReadAllText(refFile).Trim();

        var packedRefs = Path.Combine(gitDir, "packed-refs");
        if (File.Exists(packedRefs))
        {
            foreach (var line in File.ReadLines(packedRefs))
            {
                if (line.EndsWith(" " + refName, StringComparison.Ordinal))
                    return line.Split(' ')[0];
            }
        }
        return null;
    }

    private static string? ReadOriginUrl(string gitDir)
    {
        var configFile = Path.Combine(gitDir, "config");
        if (!File.Exists(configFile))
            return null;
        var inOrigin = false;
        foreach (var raw in File.ReadLines(configFile))
        {
            var line = raw.Trim();
            if (line.StartsWith("[", StringComparison.Ordinal))
                inOrigin = line.Replace(" ", "") == "[remote\"origin\"]";
            else if (inOrigin && line.StartsWith("url", StringComparison.Ordinal))
            {
                var eq = line.IndexOf('=');
                if (eq > 0)
                    return line.Substring(eq + 1).Trim();
            }
        }
        return null;
    }
}
