using System.Collections.Concurrent;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using AppMap.Config;

namespace AppMap.Util;

/// <summary>
/// Resolves a method's source file and line from its PDB, the .NET analog of
/// the Java agent reading the LineNumberTable from bytecode. Portable PDBs
/// are read directly; classic Windows PDBs fall back to the native
/// diasymreader binder (Windows only). Best-effort: returns nulls when no
/// usable PDB sits next to the assembly.
///
/// Paths are emitted relative to the project root (the appmap.yml directory,
/// then the git root) with forward slashes — like appmap-java — so a map
/// recorded on Windows (<c>C:\src\repo\...</c>) resolves against the same
/// repo checked out on Linux. PDBs embed the absolute build-machine path, so
/// without this, cross-platform queries (record on Windows, analyze on Linux)
/// break.
/// </summary>
public static class SourceLocator
{
    private static readonly Lazy<string[]> Roots = new(ResolveRoots);

    private static string[] ResolveRoots()
    {
        var roots = new List<string>();
        // Repo root (git) first — "relative to the repo root" is what the CLI
        // and IDE resolve against. The appmap.yml directory is a fallback for
        // apps run outside a git checkout (e.g. a published deployment).
        var gitRoot = GitMetadata.RepositoryRoot;
        if (!string.IsNullOrEmpty(gitRoot))
            roots.Add(gitRoot!);
        var baseDir = AppMapConfig.Current.BaseDirectory;
        if (!string.IsNullOrEmpty(baseDir) && !roots.Contains(baseDir))
            roots.Add(baseDir);
        return roots.ToArray();
    }

    /// <summary>
    /// Makes an absolute PDB document path relative to the project/git root
    /// and normalizes separators to '/'. Out-of-tree paths (e.g. third-party
    /// sources) keep their location but still get forward slashes. Pure;
    /// exposed for tests.
    /// </summary>
    internal static string RelativizeAgainst(string path, IEnumerable<string> roots)
    {
        var normalized = path.Replace('\\', '/');
        foreach (var root in roots)
        {
            if (string.IsNullOrEmpty(root))
                continue;
            var r = root.Replace('\\', '/').TrimEnd('/');
            if (r.Length > 0 &&
                normalized.StartsWith(r + "/", StringComparison.OrdinalIgnoreCase))
                return normalized.Substring(r.Length + 1);
        }
        return normalized;
    }

    private abstract class PdbSource
    {
        public abstract (string? Path, int? LineNo) Locate(MethodBase method);
    }

    private static readonly ConcurrentDictionary<Assembly, PdbSource?> sources = new();

    public static (string? Path, int? LineNo) Locate(MethodBase method)
    {
        try
        {
            var source = sources.GetOrAdd(method.Module.Assembly, Open);
            var (path, lineNo) = source?.Locate(method) ?? (null, null);
            return (path == null ? null : RelativizeAgainst(path, Roots.Value), lineNo);
        }
        catch (Exception e)
        {
            Logger.Debug($"no source info for {method.Name}: {e.Message}");
            return (null, null);
        }
    }

    private static PdbSource? Open(Assembly assembly)
    {
        var location = assembly.Location;
        if (string.IsNullOrEmpty(location))
            return null;
        var pdbPath = Path.ChangeExtension(location, ".pdb");
        if (!File.Exists(pdbPath))
            return null;

        try
        {
            // The provider must outlive the reader; it is intentionally kept
            // alive for the process lifetime alongside the cached reader.
            var provider = MetadataReaderProvider.FromPortablePdbStream(
                File.OpenRead(pdbPath));
            return new PortableSource(provider.GetMetadataReader());
        }
        catch (BadImageFormatException)
        {
            // Not a portable PDB; try the classic Windows reader.
            if (WindowsPdbReader.IsSupported
                && WindowsPdbReader.Open(assembly) is { } reader)
            {
                Logger.Debug($"using Windows PDB for {assembly.GetName().Name}");
                return new WindowsSource(reader);
            }
            Logger.Debug($"{pdbPath} is not a portable PDB; "
                + "build with <DebugType>portable</DebugType> for source locations");
            return null;
        }
        catch (Exception e)
        {
            Logger.Debug($"failed to open PDB for {assembly.GetName().Name}: {e.Message}");
            return null;
        }
    }

    private sealed class PortableSource : PdbSource
    {
        private readonly MetadataReader reader;

        public PortableSource(MetadataReader reader) => this.reader = reader;

        public override (string? Path, int? LineNo) Locate(MethodBase method)
        {
            var handle = MetadataTokens.MethodDebugInformationHandle(method.MetadataToken);
            var debugInfo = reader.GetMethodDebugInformation(handle);
            if (debugInfo.SequencePointsBlob.IsNil)
                return (null, null);

            foreach (var sp in debugInfo.GetSequencePoints())
            {
                if (sp.IsHidden)
                    continue;
                var doc = reader.GetDocument(sp.Document);
                return (reader.GetString(doc.Name), sp.StartLine);
            }
            return (null, null);
        }
    }

    private sealed class WindowsSource : PdbSource
    {
        private readonly WindowsPdbReader.ISymUnmanagedReader reader;
        private readonly object gate = new();

        public WindowsSource(WindowsPdbReader.ISymUnmanagedReader reader) =>
            this.reader = reader;

        public override (string? Path, int? LineNo) Locate(MethodBase method)
        {
            // diasymreader readers are not thread-safe.
            lock (gate)
                return WindowsPdbReader.Locate(reader, method);
        }
    }
}
