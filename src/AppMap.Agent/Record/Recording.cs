using System.Security.Cryptography;
using System.Text;
using AppMap.Config;
using AppMap.Output;
using AppMap.Util;

namespace AppMap.Record;

/// <summary>
/// A finished recording, ready to be serialized: metadata and class map in
/// memory, events as pre-serialized fragments in a temp file — mirroring
/// com.appland.appmap.record.Recording, which likewise hands off a streamed
/// temp file. Save/Discard remove the temp file.
/// </summary>
public sealed class Recording
{
    private const int FileNameMaxLength = 255;
    public const string AppMapSuffix = ".appmap.json";

    private readonly string? eventsPath;
    private readonly IReadOnlyDictionary<int, Event> eventUpdates;

    public Metadata Metadata { get; }
    public CodeObjectTree ClassMap { get; }
    public int EventCount { get; }

    public Recording(Metadata metadata, CodeObjectTree classMap,
        string? eventsPath, int eventCount, IReadOnlyDictionary<int, Event> eventUpdates)
    {
        Metadata = metadata;
        ClassMap = classMap;
        this.eventsPath = eventsPath;
        EventCount = eventCount;
        this.eventUpdates = eventUpdates;
        Metadata.App ??= AppMapConfig.Current.Name;
    }

    public void WriteTo(Stream stream) =>
        AppMapSerializer.WriteDocument(stream, Metadata, ClassMap, CopyEvents, eventUpdates);

    private void CopyEvents(Stream stream)
    {
        if (eventsPath == null || !File.Exists(eventsPath))
            return;
        using var events = new FileStream(eventsPath, FileMode.Open,
            FileAccess.Read, FileShare.ReadWrite);
        events.CopyTo(stream);
    }

    public string ToJson()
    {
        using var buffer = new MemoryStream();
        WriteTo(buffer);
        return Encoding.UTF8.GetString(buffer.ToArray());
    }

    /// <summary>
    /// Writes to {appmap_dir}/{recorder_name}/{name}.appmap.json, sanitizing
    /// and hashing over-long names like the Java agent. Returns the path.
    /// </summary>
    public string Save(string? baseName = null)
    {
        var dir = Path.Combine(AppMapConfig.Current.OutputDirectory, Metadata.RecorderName);
        Directory.CreateDirectory(dir);
        var fileName = SanitizeFileName(baseName ?? Metadata.Name ?? $"recording_{DateTime.Now:yyyyMMddHHmmssfff}");
        var path = Path.Combine(dir, fileName + AppMapSuffix);
        using (var stream = File.Create(path))
            WriteTo(stream);
        Discard();
        Logger.Debug($"wrote {EventCount} events to {path}");
        return path;
    }

    /// <summary>Deletes the temp events file (also called by Save).</summary>
    public void Discard()
    {
        try
        {
            if (eventsPath != null && File.Exists(eventsPath))
                File.Delete(eventsPath);
        }
        catch (Exception e)
        {
            Logger.Debug($"could not remove {eventsPath}: {e.Message}");
        }
    }

    private static string SanitizeFileName(string name)
    {
        var sb = new StringBuilder(name.Length);
        foreach (var c in name)
            sb.Append(char.IsLetterOrDigit(c) || c is '-' or '.' ? c : '_');
        var sanitized = sb.ToString();

        var budget = FileNameMaxLength - AppMapSuffix.Length;
        if (sanitized.Length <= budget)
            return sanitized;

        // Keep the name unique after truncation by appending a short hash,
        // as the Java agent does.
        using var sha = SHA256.Create();
        var digest = sha.ComputeHash(Encoding.UTF8.GetBytes(sanitized));
        var hash = BitConverter.ToString(digest, 0, 4).Replace("-", "")
            .Substring(0, 7).ToLowerInvariant();
        return sanitized.Substring(0, budget - hash.Length - 1) + "-" + hash;
    }
}
