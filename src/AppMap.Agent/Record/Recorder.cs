using AppMap.Output;
using AppMap.Util;

namespace AppMap.Record;

/// <summary>
/// The process-wide recorder, mirroring com.appland.appmap.record.Recorder.
/// There is one optional global session (remote/process/test recording) plus
/// an async-local session for per-request recording — AsyncLocal rather than
/// the Java agent's ThreadLocal so a recording follows its request across
/// awaits.
/// </summary>
public sealed class Recorder
{
    public static Recorder Instance { get; } = new();

    private readonly object gate = new();
    private volatile RecordingSession? globalSession;
    private readonly AsyncLocal<RecordingSession?> localSession = new();

    private Recorder() { }

    public bool HasActiveSession => globalSession != null || localSession.Value != null;

    public bool HasGlobalSession => globalSession != null;

    /// <summary>Starts the global session. Throws if one is already active.</summary>
    public void Start(Metadata metadata)
    {
        lock (gate)
        {
            if (globalSession != null)
                throw new InvalidOperationException("a recording session is already in progress");
            globalSession = new RecordingSession(metadata);
            Logger.Debug($"started global recording ({metadata.RecorderName})");
        }
    }

    /// <summary>Stops the global session and returns the recording, or null.</summary>
    public Recording? Stop()
    {
        lock (gate)
        {
            var session = globalSession;
            globalSession = null;
            if (session == null)
                return null;
            Logger.Debug("stopped global recording");
            return session.Finish();
        }
    }

    /// <summary>Snapshots the global session without stopping it (remote checkpoint).</summary>
    public Recording? Checkpoint() => globalSession?.Snapshot();

    /// <summary>Starts a session bound to the current async flow (request recording).</summary>
    public void StartLocal(Metadata metadata) =>
        localSession.Value = new RecordingSession(metadata);

    public Recording? StopLocal()
    {
        var session = localSession.Value;
        localSession.Value = null;
        return session?.Finish();
    }

    /// <summary>The session bound to the current async flow, if any.</summary>
    internal RecordingSession? LocalSession => localSession.Value;

    /// <summary>
    /// Finishes a specific local session. Used by hooks that must finish a
    /// session from a continuation (an async test completing) where the
    /// ambient AsyncLocal may or may not still point at it.
    /// </summary>
    internal Recording FinishLocal(RecordingSession session)
    {
        if (ReferenceEquals(localSession.Value, session))
            localSession.Value = null;
        return session.Finish();
    }

    /// <summary>Routes an event to the active session(s), registering its code object.</summary>
    public void Add(Event e, Action<CodeObjectTree>? registerCodeObject = null)
    {
        var local = localSession.Value;
        local?.Add(e, registerCodeObject);
        // Both can be active at once (e.g. remote recording while request
        // recording is on); the Java agent does the same.
        globalSession?.Add(e, registerCodeObject);
    }

    /// <summary>
    /// Re-records an already-added event that was mutated afterwards (e.g.
    /// normalized_path_info, known only after routing). Streamed sessions
    /// emit it in the document's eventUpdates section.
    /// </summary>
    public void Update(Event e)
    {
        localSession.Value?.Update(e);
        globalSession?.Update(e);
    }
}

/// <summary>
/// An in-progress recording. Events are serialized to a temp file as they
/// arrive — like appmap-java's streaming RecordingSession — so memory use
/// does not grow with recording length; the class map and any post-hoc
/// event updates stay in memory (both are small).
/// </summary>
public sealed class RecordingSession
{
    private readonly object gate = new();
    private readonly CodeObjectTree classMap = new();
    private readonly Dictionary<int, Event> updates = new();
    private string? eventsPath;
    private FileStream? eventsStream;
    private int eventCount;
    private bool finished;

    public Metadata Metadata { get; }

    public RecordingSession(Metadata metadata) => Metadata = metadata;

    public void Add(Event e, Action<CodeObjectTree>? registerCodeObject)
    {
        lock (gate)
        {
            // A deferred async return may arrive after the session was
            // finished (fire-and-forget Task completing post-request); drop
            // it rather than reopening the closed event stream.
            if (finished)
                return;
            if (eventsStream == null)
            {
                eventsPath = Path.Combine(Path.GetTempPath(),
                    $"appmap-{Guid.NewGuid():N}.events.json");
                eventsStream = new FileStream(eventsPath, FileMode.CreateNew,
                    FileAccess.Write, FileShare.Read);
            }
            if (eventCount > 0)
                eventsStream.WriteByte((byte)',');
            AppMapSerializer.WriteEventFragment(eventsStream, e);
            eventCount++;
        }
        registerCodeObject?.Invoke(classMap);
    }

    public void Update(Event e)
    {
        lock (gate)
        {
            if (!finished && eventCount > 0)
                updates[e.Id] = e;
        }
    }

    /// <summary>Closes the event stream and hands the temp file to the Recording.</summary>
    public Recording Finish()
    {
        lock (gate)
        {
            finished = true;
            eventsStream?.Dispose();
            eventsStream = null;
            return new Recording(Metadata, classMap, eventsPath, eventCount,
                new Dictionary<int, Event>(updates));
        }
    }

    /// <summary>Copies the events so far into a new Recording, leaving the
    /// session running (remote checkpoint).</summary>
    public Recording Snapshot()
    {
        lock (gate)
        {
            string? snapshotPath = null;
            if (eventsPath != null)
            {
                eventsStream?.Flush();
                snapshotPath = Path.Combine(Path.GetTempPath(),
                    $"appmap-{Guid.NewGuid():N}.events.json");
                File.Copy(eventsPath, snapshotPath);
            }
            return new Recording(Metadata, classMap, snapshotPath, eventCount,
                new Dictionary<int, Event>(updates));
        }
    }
}
