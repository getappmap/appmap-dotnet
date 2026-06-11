using System.Text.Json;
using AppMap.Instrumentation;
using AppMap.Output;
using AppMap.Record;
using Xunit;

namespace AppMap.Agent.Tests;

public class RecordingSessionTests
{
    private static RecordingSession NewSession() => new(new Metadata
    {
        RecorderName = "tests",
        RecorderType = "tests",
    });

    private static JsonDocument Render(Recording recording)
    {
        using var buffer = new MemoryStream();
        recording.WriteTo(buffer);
        recording.Discard();
        return JsonDocument.Parse(buffer.ToArray());
    }

    [Fact]
    public void StreamsEventsToTempFileAndRendersValidDocument()
    {
        var session = NewSession();
        session.Add(new Event { EventType = "call", DefinedClass = "A", MethodId = "M" }, null);
        session.Add(new Event { EventType = "return", ParentId = 1 }, null);

        var recording = session.Finish();
        Assert.Equal(2, recording.EventCount);

        using var doc = Render(recording);
        var events = doc.RootElement.GetProperty("events");
        Assert.Equal(2, events.GetArrayLength());
        Assert.Equal("call", events[0].GetProperty("event").GetString());
        Assert.Equal("return", events[1].GetProperty("event").GetString());
        Assert.False(doc.RootElement.TryGetProperty("eventUpdates", out _));
    }

    [Fact]
    public void UpdatedEventsAppearInEventUpdates()
    {
        var session = NewSession();
        var call = new Event
        {
            EventType = "call",
            HttpServerRequest = new HttpServerRequest
            {
                RequestMethod = "GET",
                PathInfo = "/owners/Davis",
            },
        };
        session.Add(call, null);

        // Route template discovered after the event was streamed.
        call.HttpServerRequest.NormalizedPathInfo = "/owners/{lastName}";
        session.Update(call);

        using var doc = Render(session.Finish());

        // The streamed copy lacks the route; the update carries it.
        var streamed = doc.RootElement.GetProperty("events")[0];
        Assert.False(streamed.GetProperty("http_server_request")
            .TryGetProperty("normalized_path_info", out _));

        var updated = doc.RootElement.GetProperty("eventUpdates")
            .GetProperty(call.Id.ToString());
        Assert.Equal("/owners/{lastName}", updated.GetProperty("http_server_request")
            .GetProperty("normalized_path_info").GetString());
    }

    [Fact]
    public void EmptySessionRendersEmptyEventsArray()
    {
        using var doc = Render(NewSession().Finish());
        Assert.Equal(0, doc.RootElement.GetProperty("events").GetArrayLength());
    }

    [Fact]
    public void SnapshotLeavesSessionRecording()
    {
        var session = NewSession();
        session.Add(new Event { EventType = "call" }, null);

        var checkpoint = session.Snapshot();
        session.Add(new Event { EventType = "return", ParentId = 1 }, null);
        var final = session.Finish();

        using var checkpointDoc = Render(checkpoint);
        using var finalDoc = Render(final);
        Assert.Equal(1, checkpointDoc.RootElement.GetProperty("events").GetArrayLength());
        Assert.Equal(2, finalDoc.RootElement.GetProperty("events").GetArrayLength());
    }
}

public class DefaultExcludesTests
{
    private sealed class Sample
    {
        public override string ToString() => "sample";

        public override int GetHashCode() => 1;

        public void DoWork() { }

        [Labels("important")]
        public override bool Equals(object? obj) => false;
    }

    [Fact]
    public void TrivialOverridesAreExcluded()
    {
        Assert.True(Instrumentor.IsDefaultExcluded(typeof(Sample).GetMethod("ToString")!));
        Assert.True(Instrumentor.IsDefaultExcluded(typeof(Sample).GetMethod("GetHashCode")!));
        Assert.False(Instrumentor.IsDefaultExcluded(typeof(Sample).GetMethod("DoWork")!));
    }

    [Fact]
    public void LabeledMethodsWinOverDefaultExcludes()
    {
        Assert.False(Instrumentor.IsDefaultExcluded(typeof(Sample).GetMethod("Equals")!));
    }
}
