using System.Diagnostics;
using AppMap.Config;
using AppMap.Output;
using AppMap.Record;
using AppMap.Util;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Routing;

namespace AppMap.AspNetCore;

/// <summary>
/// Records http_server_request / http_server_response events around the
/// pipeline, and (when APPMAP_RECORDING_REQUESTS is on and no remote
/// recording is active) writes one AppMap per request — the analog of
/// appmap-java's servlet hooks plus RequestRecording.
/// </summary>
public sealed class AppMapMiddleware
{
    private readonly RequestDelegate next;

    public AppMapMiddleware(RequestDelegate next) => this.next = next;

    public async Task InvokeAsync(HttpContext context)
    {
        if (context.Request.Path.StartsWithSegments(RemoteRecordingMiddleware.RecordRoute))
        {
            await next(context);
            return;
        }

        var requestRecording = Properties.RecordingRequests;
        if (requestRecording)
        {
            Recorder.Instance.StartLocal(new Metadata
            {
                RecorderName = "request_recording",
                RecorderType = "requests",
            });
        }

        var callEvent = BuildRequestEvent(context);
        Recorder.Instance.Add(callEvent);
        var start = Stopwatch.GetTimestamp();

        try
        {
            await next(context);
        }
        finally
        {
            // The route template is only known after routing has run; the
            // call event was already streamed out, so re-record it through
            // the document's eventUpdates section.
            var routePattern = (context.GetEndpoint() as RouteEndpoint)?.RoutePattern.RawText;
            if (routePattern != null && callEvent.HttpServerRequest != null)
            {
                callEvent.HttpServerRequest.NormalizedPathInfo = NormalizePattern(routePattern);
                Recorder.Instance.Update(callEvent);
            }

            Recorder.Instance.Add(new Event
            {
                EventType = "return",
                ParentId = callEvent.Id,
                Elapsed = (Stopwatch.GetTimestamp() - start) / (double)Stopwatch.Frequency,
                HttpServerResponse = new HttpServerResponse
                {
                    Status = context.Response.StatusCode,
                    Headers = context.Response.Headers.ToDictionary(
                        h => h.Key, h => h.Value.ToString()),
                },
            });

            if (requestRecording)
                SaveRequestRecording(context);
        }
    }

    private static Event BuildRequestEvent(HttpContext context)
    {
        var request = context.Request;
        var e = new Event
        {
            EventType = "call",
            HttpServerRequest = new HttpServerRequest
            {
                RequestMethod = request.Method,
                PathInfo = request.Path.Value ?? "/",
                Protocol = request.Protocol,
                Headers = request.Headers.ToDictionary(h => h.Key, h => h.Value.ToString()),
            },
        };

        var message = new List<Value>();
        foreach (var (key, values) in request.Query)
            message.Add(Value.Capture(values.ToString(), name: key, kind: "req"));
        if (request.HasFormContentType)
        {
            foreach (var (key, values) in request.Form)
                message.Add(Value.Capture(values.ToString(), name: key, kind: "req"));
        }
        if (message.Count > 0)
            e.Message = message;
        return e;
    }

    private static void SaveRequestRecording(HttpContext context)
    {
        try
        {
            var recording = Recorder.Instance.StopLocal();
            if (recording == null)
                return;
            if (recording.EventCount == 0)
            {
                recording.Discard();
                return;
            }
            var timestamp = DateTime.Now;
            var request = context.Request;
            recording.Metadata.Name =
                $"{request.Method} {request.Path} ({context.Response.StatusCode}) - "
                + timestamp.ToString("yyyy-MM-ddTHH:mm:ss.fff");
            recording.Save($"{timestamp:yyyyMMddHHmmssfff}_{request.Path}");
        }
        catch (Exception e)
        {
            Logger.Error("failed to save request recording", e);
        }
    }

    /// <summary>The AppMap spec keeps the framework's native template form, so
    /// preserve the route text and just strip constraints ("{id:int}" → "{id}").</summary>
    private static string NormalizePattern(string pattern)
    {
        var normalized = System.Text.RegularExpressions.Regex.Replace(
            pattern, @"\{([^}:?]+)[^}]*\}", "{$1}");
        return normalized.StartsWith('/') ? normalized : "/" + normalized;
    }
}
