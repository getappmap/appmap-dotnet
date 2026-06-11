using AppMap.Output;
using AppMap.Record;
using Microsoft.AspNetCore.Http;

namespace AppMap.AspNetCore;

/// <summary>
/// Serves the AppMap remote-recording protocol, byte-compatible with
/// appmap-java's RemoteRecordingManager:
///   GET    /_appmap/record            → {"enabled": bool}
///   POST   /_appmap/record            → start (409 if already recording)
///   DELETE /_appmap/record            → stop, body is the AppMap JSON (404 if none)
///   GET    /_appmap/record/checkpoint → snapshot without stopping (404 if none)
/// </summary>
public sealed class RemoteRecordingMiddleware
{
    public const string RecordRoute = "/_appmap/record";
    private const string CheckpointRoute = "/_appmap/record/checkpoint";

    private readonly RequestDelegate next;

    public RemoteRecordingMiddleware(RequestDelegate next) => this.next = next;

    public async Task InvokeAsync(HttpContext context)
    {
        var path = context.Request.Path;
        if (path == CheckpointRoute && HttpMethods.IsGet(context.Request.Method))
        {
            await Respond(context, Recorder.Instance.Checkpoint());
            return;
        }
        if (path != RecordRoute)
        {
            await next(context);
            return;
        }

        switch (context.Request.Method)
        {
            case "GET":
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(
                    $"{{\"enabled\":{(Recorder.Instance.HasGlobalSession ? "true" : "false")}}}");
                break;

            case "POST":
                if (Recorder.Instance.HasGlobalSession)
                {
                    context.Response.StatusCode = StatusCodes.Status409Conflict;
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
                await Respond(context, Recorder.Instance.Stop());
                break;

            default:
                context.Response.StatusCode = StatusCodes.Status405MethodNotAllowed;
                break;
        }
    }

    private static async Task Respond(HttpContext context, Recording? recording)
    {
        if (recording == null)
        {
            context.Response.StatusCode = StatusCodes.Status404NotFound;
            return;
        }
        try
        {
            context.Response.ContentType = "application/json";
            await context.Response.WriteAsync(recording.ToJson());
        }
        finally
        {
            recording.Discard();
        }
    }
}
