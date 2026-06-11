using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Web;
using System.Web.Routing;
using AppMap.Config;
using AppMap.Output;
using AppMap.Record;
using AppMap.Util;

namespace AppMap.SystemWeb;

/// <summary>
/// Classic ASP.NET integration — the IHttpModule analog of the ASP.NET Core
/// middleware (and of appmap-java's servlet hooks). Records
/// http_server_request/http_server_response events around each request,
/// writes one AppMap per request, and serves the /_appmap/record remote
/// recording protocol.
///
/// Register in Web.config:
/// <code>
/// &lt;system.webServer&gt;
///   &lt;modules&gt;
///     &lt;add name="AppMap" type="AppMap.SystemWeb.AppMapHttpModule, AppMap.SystemWeb" /&gt;
///   &lt;/modules&gt;
/// &lt;/system.webServer&gt;
/// </code>
/// </summary>
public sealed class AppMapHttpModule : IHttpModule
{
    private const string RecordRoute = "/_appmap/record";
    private const string CheckpointRoute = "/_appmap/record/checkpoint";

    private const string CallEventKey = "AppMap.CallEvent";
    private const string StartTimestampKey = "AppMap.StartTimestamp";
    private const string RequestRecordingKey = "AppMap.RequestRecording";

    public void Init(HttpApplication application)
    {
        AgentBootstrap.Init();
        application.BeginRequest += (sender, _) => OnBeginRequest(((HttpApplication)sender!).Context);
        application.EndRequest += (sender, _) => OnEndRequest(((HttpApplication)sender!).Context);
    }

    public void Dispose() { }

    private static void OnBeginRequest(HttpContext context)
    {
        try
        {
            var path = context.Request.Path;
            if (Properties.RecordingRemote
                && path.StartsWith(RecordRoute, StringComparison.OrdinalIgnoreCase))
            {
                HandleRemoteRecording(context);
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

            var callEvent = BuildRequestEvent(context.Request);
            Recorder.Instance.Add(callEvent);
            context.Items[CallEventKey] = callEvent;
            context.Items[StartTimestampKey] = Stopwatch.GetTimestamp();
            context.Items[RequestRecordingKey] = requestRecording;
        }
        catch (Exception e)
        {
            Logger.Error("begin-request hook failed", e);
        }
    }

    private static void OnEndRequest(HttpContext context)
    {
        try
        {
            if (context.Items[CallEventKey] is not Event callEvent)
                return;

            // The route is resolved after BeginRequest; the call event was
            // already streamed, so re-record it via eventUpdates.
            var template = RouteTemplateOf(context);
            if (template != null && callEvent.HttpServerRequest != null)
            {
                callEvent.HttpServerRequest.NormalizedPathInfo = template;
                Recorder.Instance.Update(callEvent);
            }

            var start = (long)context.Items[StartTimestampKey]!;
            Recorder.Instance.Add(new Event
            {
                EventType = "return",
                ParentId = callEvent.Id,
                Elapsed = (Stopwatch.GetTimestamp() - start) / (double)Stopwatch.Frequency,
                HttpServerResponse = new HttpServerResponse
                {
                    Status = context.Response.StatusCode,
                    Headers = HeadersOf(context.Response),
                },
            });

            if (context.Items[RequestRecordingKey] is true)
                SaveRequestRecording(context);
        }
        catch (Exception e)
        {
            Logger.Error("end-request hook failed", e);
        }
    }

    private static Event BuildRequestEvent(HttpRequest request)
    {
        var e = new Event
        {
            EventType = "call",
            HttpServerRequest = new HttpServerRequest
            {
                RequestMethod = request.HttpMethod,
                PathInfo = request.Path,
                Protocol = request.ServerVariables["SERVER_PROTOCOL"],
                Headers = ToDictionary(request.Headers),
            },
        };

        var message = new List<Value>();
        foreach (string? key in request.QueryString)
        {
            if (key != null)
                message.Add(Value.Capture(request.QueryString[key], name: key, kind: "req"));
        }
        if (request.ContentType?.IndexOf("form", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            foreach (string? key in request.Form)
            {
                if (key != null)
                    message.Add(Value.Capture(request.Form[key], name: key, kind: "req"));
            }
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
                $"{request.HttpMethod} {request.Path} ({context.Response.StatusCode}) - "
                + timestamp.ToString("yyyy-MM-ddTHH:mm:ss.fff");
            recording.Save($"{timestamp:yyyyMMddHHmmssfff}_{request.Path}");
        }
        catch (Exception e)
        {
            Logger.Error("failed to save request recording", e);
        }
    }

    private static void HandleRemoteRecording(HttpContext context)
    {
        var response = context.Response;
        var isCheckpoint = context.Request.Path.Equals(
            CheckpointRoute, StringComparison.OrdinalIgnoreCase);

        switch (context.Request.HttpMethod)
        {
            case "GET" when isCheckpoint:
                Respond(response, Recorder.Instance.Checkpoint());
                break;

            case "GET":
                response.ContentType = "application/json";
                response.Write(Recorder.Instance.HasGlobalSession
                    ? "{\"enabled\":true}" : "{\"enabled\":false}");
                break;

            case "POST":
                if (Recorder.Instance.HasGlobalSession)
                {
                    response.StatusCode = 409;
                }
                else
                {
                    Recorder.Instance.Start(new Metadata
                    {
                        RecorderName = "remote_recording",
                        RecorderType = "remote",
                        Name = $"Remote recording {DateTime.Now:yyyy-MM-ddTHH:mm:ss}",
                    });
                }
                break;

            case "DELETE":
                Respond(response, Recorder.Instance.Stop());
                break;

            default:
                response.StatusCode = 405;
                break;
        }

        context.ApplicationInstance.CompleteRequest();
    }

    private static void Respond(HttpResponse response, Recording? recording)
    {
        if (recording == null)
        {
            response.StatusCode = 404;
            return;
        }
        try
        {
            response.ContentType = "application/json";
            response.Write(recording.ToJson());
        }
        finally
        {
            recording.Discard();
        }
    }

    private static string? RouteTemplateOf(HttpContext context)
    {
        try
        {
            var route = context.Request.RequestContext?.RouteData?.Route as Route;
            var url = route?.Url;
            if (string.IsNullOrEmpty(url))
                return null;
            // Route templates have no leading slash; strip constraints as the
            // Core middleware does ("{id:int}" -> "{id}").
            var normalized = System.Text.RegularExpressions.Regex.Replace(
                url, @"\{([^}:?*]+)[^}]*\}", "{$1}");
            return "/" + normalized;
        }
        catch
        {
            return null;
        }
    }

    private static Dictionary<string, string> ToDictionary(
        System.Collections.Specialized.NameValueCollection headers)
    {
        var result = new Dictionary<string, string>(headers.Count);
        foreach (string? key in headers)
        {
            if (key != null)
                result[key] = headers[key] ?? string.Empty;
        }
        return result;
    }

    private static Dictionary<string, string>? HeadersOf(HttpResponse response)
    {
        try
        {
            // Response.Headers requires the IIS integrated pipeline.
            return ToDictionary(response.Headers);
        }
        catch (PlatformNotSupportedException)
        {
            return null;
        }
    }
}
