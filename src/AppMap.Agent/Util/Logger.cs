using AppMap.Config;

namespace AppMap.Util;

/// <summary>
/// Minimal stderr logger. The agent must never write to stdout (the host
/// application owns it) and must never throw from a logging call.
/// </summary>
public static class Logger
{
    public static void Debug(string message)
    {
        if (Properties.Debug)
            Write("debug", message);
    }

    public static void Warn(string message) => Write("warn", message);

    public static void Error(string message, Exception? e = null) =>
        Write("error", e == null ? message : $"{message}: {e}");

    private static void Write(string level, string message)
    {
        try
        {
            Console.Error.WriteLine($"[appmap {level}] {message}");
        }
        catch
        {
            // Nothing sensible to do.
        }
    }
}
