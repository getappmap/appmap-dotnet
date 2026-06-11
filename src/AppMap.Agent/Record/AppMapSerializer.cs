using System.Text;
using System.Text.Json;
using AppMap.Output;
using AppMap.Util;

namespace AppMap.Record;

/// <summary>
/// Writes AppMap JSON (format version 1.2, matching appmap-java's
/// AppMapSerializer). Events are serialized one at a time as standalone
/// fragments — the recorder streams them to a temp file as they happen, as
/// the Java agent does — and the final document is assembled around the raw
/// fragment bytes. Events mutated after being streamed (e.g. the HTTP route
/// template, known only after routing) are emitted in the spec's
/// "eventUpdates" section.
/// </summary>
public static class AppMapSerializer
{
    public const string FormatVersion = "1.2";
    public const string ClientName = "appmap-dotnet";
    public const string ClientUrl = "https://github.com/getappmap/appmap-dotnet";

    /// <summary>Serializes one event as a standalone JSON object.</summary>
    public static void WriteEventFragment(Stream stream, Event e)
    {
        using var json = new Utf8JsonWriter(stream);
        WriteEvent(json, e);
    }

    /// <summary>
    /// Assembles a complete document. writeEvents must emit zero or more
    /// comma-separated event fragments (raw bytes) — the body of the events
    /// array.
    /// </summary>
    public static void WriteDocument(Stream stream, Metadata metadata,
        CodeObjectTree classMap, Action<Stream> writeEvents,
        IReadOnlyDictionary<int, Event>? eventUpdates = null)
    {
        WriteRaw(stream, $"{{\"version\":\"{FormatVersion}\",\"metadata\":");
        using (var json = new Utf8JsonWriter(stream))
            WriteMetadata(json, metadata);

        // The top-level key is camelCase in the AppMap spec ("classMap"),
        // unlike the snake_case event fields; the AppMap CLI / VS Code
        // extension key off this exact name to build the code-object tree.
        WriteRaw(stream, ",\"classMap\":");
        using (var json = new Utf8JsonWriter(stream))
        {
            json.WriteStartArray();
            foreach (var root in classMap.Roots)
                WriteCodeObject(json, root);
            json.WriteEndArray();
        }

        WriteRaw(stream, ",\"events\":[");
        writeEvents(stream);
        WriteRaw(stream, "]");

        if (eventUpdates is { Count: > 0 })
        {
            WriteRaw(stream, ",\"eventUpdates\":{");
            var first = true;
            foreach (var update in eventUpdates)
            {
                WriteRaw(stream, first ? $"\"{update.Key}\":" : $",\"{update.Key}\":");
                WriteEventFragment(stream, update.Value);
                first = false;
            }
            WriteRaw(stream, "}");
        }

        WriteRaw(stream, "}");
    }

    /// <summary>Convenience for buffered event lists (tests, simple callers).</summary>
    public static void Write(Stream stream, Metadata metadata,
        IReadOnlyList<Event> events, CodeObjectTree classMap)
    {
        WriteDocument(stream, metadata, classMap, s =>
        {
            for (var i = 0; i < events.Count; i++)
            {
                if (i > 0)
                    WriteRaw(s, ",");
                WriteEventFragment(s, events[i]);
            }
        });
    }

    private static void WriteRaw(Stream stream, string text)
    {
        var bytes = Encoding.UTF8.GetBytes(text);
        stream.Write(bytes, 0, bytes.Length);
    }

    private static void WriteMetadata(Utf8JsonWriter json, Metadata md)
    {
        json.WriteStartObject();
        if (md.Name != null)
            json.WriteString("name", md.Name);
        if (md.App != null)
            json.WriteString("app", md.App);

        json.WriteStartObject("language");
        json.WriteString("name", "csharp");
        json.WriteString("version", Environment.Version.ToString());
        json.WriteString("engine", System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription);
        json.WriteEndObject();

        json.WriteStartObject("client");
        json.WriteString("name", ClientName);
        json.WriteString("url", ClientUrl);
        json.WriteEndObject();

        json.WriteStartObject("recorder");
        json.WriteString("name", md.RecorderName);
        json.WriteString("type", md.RecorderType);
        json.WriteEndObject();

        if (md.RecordingDefinedClass != null || md.RecordingMethodId != null)
        {
            json.WriteStartObject("recording");
            if (md.RecordingDefinedClass != null)
                json.WriteString("defined_class", md.RecordingDefinedClass);
            if (md.RecordingMethodId != null)
                json.WriteString("method_id", md.RecordingMethodId);
            json.WriteEndObject();
        }

        if (md.SourceLocation != null)
            json.WriteString("source_location", md.SourceLocation);

        if (md.Frameworks.Count > 0)
        {
            json.WriteStartArray("frameworks");
            foreach (var fw in md.Frameworks)
            {
                json.WriteStartObject();
                json.WriteString("name", fw.Name);
                if (fw.Version != null)
                    json.WriteString("version", fw.Version);
                json.WriteEndObject();
            }
            json.WriteEndArray();
        }

        if (md.TestStatus != null)
            json.WriteString("test_status", md.TestStatus);
        if (md.TestFailure != null)
        {
            json.WriteStartObject("test_failure");
            json.WriteString("message", md.TestFailure.Message);
            if (md.TestFailure.Location != null)
                json.WriteString("location", md.TestFailure.Location);
            json.WriteEndObject();
        }

        var git = GitMetadata.Collect();
        if (git != null)
        {
            json.WriteStartObject("git");
            if (git.Repository != null)
                json.WriteString("repository", git.Repository);
            if (git.Branch != null)
                json.WriteString("branch", git.Branch);
            if (git.Commit != null)
                json.WriteString("commit", git.Commit);
            json.WriteEndObject();
        }

        json.WriteEndObject();
    }

    private static void WriteCodeObject(Utf8JsonWriter json, CodeObject co)
    {
        json.WriteStartObject();
        json.WriteString("name", co.Name);
        json.WriteString("type", co.Type);
        if (co.Static.HasValue)
            json.WriteBoolean("static", co.Static.Value);
        if (co.Location != null)
            json.WriteString("location", co.Location);
        if (co.Labels is { Count: > 0 })
        {
            json.WriteStartArray("labels");
            foreach (var label in co.Labels)
                json.WriteStringValue(label);
            json.WriteEndArray();
        }
        if (co.Children.Count > 0)
        {
            json.WriteStartArray("children");
            foreach (var child in co.Children)
                WriteCodeObject(json, child);
            json.WriteEndArray();
        }
        json.WriteEndObject();
    }

    private static void WriteEvent(Utf8JsonWriter json, Event e)
    {
        json.WriteStartObject();
        json.WriteNumber("id", e.Id);
        json.WriteString("event", e.EventType);
        json.WriteNumber("thread_id", e.ThreadId);

        if (e.DefinedClass != null)
            json.WriteString("defined_class", e.DefinedClass);
        if (e.MethodId != null)
            json.WriteString("method_id", e.MethodId);
        if (e.Path != null)
            json.WriteString("path", e.Path);
        if (e.LineNo.HasValue)
            json.WriteNumber("lineno", e.LineNo.Value);
        if (e.Static.HasValue)
            json.WriteBoolean("static", e.Static.Value);

        if (e.Receiver != null)
        {
            json.WritePropertyName("receiver");
            WriteValue(json, e.Receiver);
        }
        if (e.Parameters != null)
        {
            json.WriteStartArray("parameters");
            foreach (var p in e.Parameters)
                WriteValue(json, p);
            json.WriteEndArray();
        }
        if (e.Message != null)
        {
            json.WriteStartArray("message");
            foreach (var p in e.Message)
                WriteValue(json, p);
            json.WriteEndArray();
        }

        if (e.ParentId.HasValue)
            json.WriteNumber("parent_id", e.ParentId.Value);
        if (e.Elapsed.HasValue)
            json.WriteNumber("elapsed", e.Elapsed.Value);
        if (e.ReturnValue != null)
        {
            json.WritePropertyName("return_value");
            WriteValue(json, e.ReturnValue);
        }
        if (e.Exceptions is { Count: > 0 })
        {
            json.WriteStartArray("exceptions");
            foreach (var ex in e.Exceptions)
            {
                json.WriteStartObject();
                json.WriteString("class", ex.Class);
                if (ex.Message != null)
                    json.WriteString("message", ex.Message);
                if (ex.Path != null)
                    json.WriteString("path", ex.Path);
                if (ex.LineNo.HasValue)
                    json.WriteNumber("lineno", ex.LineNo.Value);
                json.WriteNumber("object_id", ex.ObjectId);
                json.WriteEndObject();
            }
            json.WriteEndArray();
        }

        if (e.HttpServerRequest is { } req)
        {
            json.WriteStartObject("http_server_request");
            json.WriteString("request_method", req.RequestMethod);
            json.WriteString("path_info", req.PathInfo);
            if (req.NormalizedPathInfo != null)
                json.WriteString("normalized_path_info", req.NormalizedPathInfo);
            if (req.Protocol != null)
                json.WriteString("protocol", req.Protocol);
            WriteHeaders(json, req.Headers);
            json.WriteEndObject();
        }
        if (e.HttpServerResponse is { } res)
        {
            json.WriteStartObject("http_server_response");
            json.WriteNumber("status", res.Status);
            WriteHeaders(json, res.Headers);
            json.WriteEndObject();
        }
        if (e.SqlQuery is { } sql)
        {
            json.WriteStartObject("sql_query");
            json.WriteString("sql", sql.Sql);
            if (sql.DatabaseType != null)
                json.WriteString("database_type", sql.DatabaseType);
            json.WriteEndObject();
        }

        json.WriteEndObject();
    }

    private static void WriteHeaders(Utf8JsonWriter json, Dictionary<string, string>? headers)
    {
        if (headers is not { Count: > 0 })
            return;
        json.WriteStartObject("headers");
        foreach (var header in headers)
            json.WriteString(header.Key, header.Value);
        json.WriteEndObject();
    }

    private static void WriteValue(Utf8JsonWriter json, Value v)
    {
        json.WriteStartObject();
        if (v.Name != null)
            json.WriteString("name", v.Name);
        if (v.Kind != null)
            json.WriteString("kind", v.Kind);
        if (v.Class != null)
            json.WriteString("class", v.Class);
        json.WriteString("value", v.StringValue ?? "null");
        if (v.ObjectId.HasValue)
            json.WriteNumber("object_id", v.ObjectId.Value);
        json.WriteEndObject();
    }
}
