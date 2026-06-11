using AppMap.Config;
using Microsoft.AspNetCore.Builder;

namespace AppMap.AspNetCore;

public static class AppMapApplicationBuilderExtensions
{
    /// <summary>
    /// Enables AppMap recording: initializes the agent (instrumentation per
    /// appmap.yml), serves the /_appmap/record remote-recording endpoints,
    /// and records HTTP server events. Add it first in the pipeline so the
    /// recording brackets the whole request:
    /// <code>app.UseAppMap();</code>
    /// </summary>
    public static IApplicationBuilder UseAppMap(this IApplicationBuilder app)
    {
        // Idempotent: the zero-touch HostingStartup may prepend this while the
        // app also calls it by hand — register the middleware only once.
        const string registeredKey = "__AppMap.Registered";
        if (app.Properties.ContainsKey(registeredKey))
            return app;
        app.Properties[registeredKey] = true;

        AgentBootstrap.Init();
        if (Properties.RecordingRemote)
            app.UseMiddleware<RemoteRecordingMiddleware>();
        app.UseMiddleware<AppMapMiddleware>();
        return app;
    }
}
