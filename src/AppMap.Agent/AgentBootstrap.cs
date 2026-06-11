using AppMap.Config;
using AppMap.Instrumentation;
using AppMap.Output;
using AppMap.Record;
using AppMap.Util;

namespace AppMap;

/// <summary>
/// Agent entry point — the analog of appmap-java's premain. Invoked from the
/// startup hook (DOTNET_STARTUP_HOOKS), from the ASP.NET Core middleware, or
/// explicitly by the host application. Idempotent.
/// </summary>
public static class AgentBootstrap
{
    private static int initialized;

    public static void Init()
    {
        if (Interlocked.Exchange(ref initialized, 1) == 1)
            return;

        try
        {
            var config = AppMapConfig.Current;
            Logger.Debug($"AppMap .NET agent starting (app: {config.Name})");

            new Instrumentor(config).Start();
            SqlHooks.Install();
            BuiltinHooks.Install();

            if (Properties.RecordingProcess)
                StartProcessRecording();
        }
        catch (Exception e)
        {
            // The agent must never prevent the host application from starting.
            Logger.Error("agent initialization failed", e);
        }
    }

    private static void StartProcessRecording()
    {
        Recorder.Instance.Start(new Metadata
        {
            RecorderName = "process_recording",
            RecorderType = "process",
            Name = $"Process recording {DateTime.Now:yyyy-MM-ddTHH:mm:ss}",
        });
        AppDomain.CurrentDomain.ProcessExit += (_, _) =>
        {
            try
            {
                // dotnet's CLI host inherits the startup hook too; don't
                // litter the output directory with its empty recording.
                var recording = Recorder.Instance.Stop();
                if (recording is { EventCount: > 0 })
                    recording.Save();
                else
                    recording?.Discard();
            }
            catch (Exception e)
            {
                Logger.Error("failed to write process recording", e);
            }
        };
    }
}
