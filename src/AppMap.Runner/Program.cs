using System.Diagnostics;

// appmap-dotnet — run any .NET command with the AppMap agent attached, with
// no change to the target application:
//
//   appmap-dotnet -- dotnet run
//   appmap-dotnet -- dotnet test
//   appmap-dotnet -- dotnet bin/Release/net8.0/MyApp.dll
//
// It sets the environment (DOTNET_STARTUP_HOOKS for instrumentation,
// ASPNETCORE_HOSTINGSTARTUPASSEMBLIES for the web middleware) and execs the
// command. Everything after `--` (or the first non-option argument) is the
// command. APPMAP_* variables already present are passed through untouched,
// so configuration works exactly as documented for the env-var attach; the
// runner is a convenience over it, not a different mechanism. For pipelines
// where a wrapper is awkward, set the same two variables directly.

var args0 = Environment.GetCommandLineArgs().Skip(1).ToArray();
var command = args0.SkipWhile(a => a == "--").ToArray();
if (command.Length > 0 && command[0] == "--")
    command = command.Skip(1).ToArray();

if (command.Length == 0 || command[0] is "-h" or "--help")
{
    Console.Error.WriteLine("usage: appmap-dotnet [--] <command> [args...]");
    Console.Error.WriteLine("  runs <command> with the AppMap agent attached.");
    Console.Error.WriteLine("  configure with appmap.yml and APPMAP_* environment variables.");
    return command.Length == 0 ? 2 : 0;
}

// The agent ships alongside the runner binary (both in a tool install and in
// a build-output run).
var agentDir = AppContext.BaseDirectory;
var hook = Path.Combine(agentDir, "AppMap.StartupHook.dll");
if (!File.Exists(hook))
{
    Console.Error.WriteLine($"appmap-dotnet: agent not found at {hook}");
    return 2;
}

static string Append(string? existing, string entry, char separator) =>
    string.IsNullOrEmpty(existing) ? entry
    : existing.Split(separator).Contains(entry) ? existing
    : existing + separator + entry;

var psi = new ProcessStartInfo(command[0]) { UseShellExecute = false };
foreach (var arg in command.Skip(1))
    psi.ArgumentList.Add(arg);
psi.Environment["DOTNET_STARTUP_HOOKS"] = Append(
    Environment.GetEnvironmentVariable("DOTNET_STARTUP_HOOKS"), hook, Path.PathSeparator);
psi.Environment["ASPNETCORE_HOSTINGSTARTUPASSEMBLIES"] = Append(
    Environment.GetEnvironmentVariable("ASPNETCORE_HOSTINGSTARTUPASSEMBLIES"),
    "AppMap.AspNetCore", ';');

using var process = Process.Start(psi);
if (process == null)
{
    Console.Error.WriteLine($"appmap-dotnet: failed to start: {command[0]}");
    return 2;
}
// Let the child own Ctrl+C: ignore it here and wait for the child to exit.
Console.CancelKeyPress += (_, e) => e.Cancel = true;
process.WaitForExit();
return process.ExitCode;
