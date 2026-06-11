namespace AppMap.Output;

/// <summary>
/// The metadata section of an AppMap document. Static fields (language,
/// client, git) are filled in by AppMapSerializer; this class carries the
/// per-recording fields. Mirrors com.appland.appmap.record.Metadata.
/// </summary>
public sealed class Metadata
{
    /// <summary>Scenario name, e.g. "GET /users (200) - 2026-06-10T12:00:00".</summary>
    public string? Name { get; set; }

    /// <summary>Application name, from appmap.yml.</summary>
    public string? App { get; set; }

    /// <summary>e.g. "xunit", "remote_recording", "request_recording", "process_recording".</summary>
    public required string RecorderName { get; init; }

    /// <summary>e.g. "tests", "remote", "requests", "process".</summary>
    public required string RecorderType { get; init; }

    /// <summary>recording.defined_class — the test class, when recording a test.</summary>
    public string? RecordingDefinedClass { get; set; }

    /// <summary>recording.method_id — the test method, when recording a test.</summary>
    public string? RecordingMethodId { get; set; }

    /// <summary>"file:lineno" of the recorded method, when known.</summary>
    public string? SourceLocation { get; set; }

    public List<Framework> Frameworks { get; } = new();

    /// <summary>"succeeded" or "failed", for test recordings.</summary>
    public string? TestStatus { get; set; }

    public TestFailure? TestFailure { get; set; }
}

public sealed class Framework
{
    public required string Name { get; init; }
    public string? Version { get; init; }
}

public sealed class TestFailure
{
    public required string Message { get; init; }
    public string? Location { get; init; }
}
