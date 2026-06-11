using System.Collections.Concurrent;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;

namespace AppMap.Util;

/// <summary>
/// Resolves a method's source file and line from its PDB, the .NET analog of
/// the Java agent reading the LineNumberTable from bytecode. Portable PDBs
/// are read directly; classic Windows PDBs fall back to the native
/// diasymreader binder (Windows only). Best-effort: returns nulls when no
/// usable PDB sits next to the assembly.
/// </summary>
public static class SourceLocator
{
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
            return source?.Locate(method) ?? (null, null);
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
