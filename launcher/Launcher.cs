using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace AppLand.AppMap
{
    class Launcher
    {
        static bool PlatformSupported()
        {
            return RuntimeInformation.IsOSPlatform(OSPlatform.Linux)
                && RuntimeInformation.ProcessArchitecture == Architecture.X64;
        }

        static string? SetOrKeep(IDictionary<string, string?> dict, string key, string value)
        {
            if (dict.ContainsKey(key))
                return dict[key];

            return dict[key] = value;
        }

        public static readonly string[] RequiredFiles = {
            "libappmap-instrumentation.so",
            "libInstrumentationEngine.so",
            "ProductionBreakpoints_x64.config"
        };

        public const string RuntimePathEnvar = @"APPMAP_RUNTIME_DIR";

        static string? FindRuntimeDirectory() {
            var paths = new List<string>();


            var envPath = Environment.GetEnvironmentVariable(RuntimePathEnvar);
            if (envPath != null) {
                paths.Add(envPath);
            }

            var thisPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            if (thisPath != null) {
                paths.Add(thisPath);
                // in a package this assembly will be at /tools/net5.0/any/AppLand.AppMap.dll
                // and native libraries will be at /runtimes/linux-x64/native/
                paths.Add(Path.GetFullPath(Path.Combine(thisPath, "../../../runtimes/linux-x64/native")));
            }

            foreach (string path in paths) {
                if (CheckRuntimeDirectory(path))
                    return Path.GetFullPath(path);
            }

            Console.WriteLine($"Could not find AppMap instrumentation runtime files ({String.Join(", ", RequiredFiles)}).");
            Console.WriteLine($"(Searched in: {String.Join(", ", paths)}.)");
            Console.WriteLine($"You can set {RuntimePathEnvar} environment variable to explicitly set the location.");

            return null;
        }

        static bool CheckRuntimeDirectory(string path) {
            foreach (string file in RequiredFiles) {
                if (!File.Exists(Path.Combine(path, file))) {
                    return false;
                }
            }

            return true;
        }

        // GUID of Microsoft's CLR Instrumentation Engine
        public const string CLRIE_GUID = @"{324F817A-7420-4E6D-B3C1-143FBED6D855}";

        static void PrepareEnvironment(IDictionary<string, string?> env, string runtimeDir)
        {
            SetOrKeep(env, "CORECLR_ENABLE_PROFILING", "1");
            SetOrKeep(env, "CORECLR_PROFILER", CLRIE_GUID);
            SetOrKeep(env, "CORECLR_PROFILER_PATH_64", Path.Join(runtimeDir, "libInstrumentationEngine.so"));
            SetOrKeep(env, "MicrosoftInstrumentationEngine_DisableCodeSignatureValidation", "1");
            SetOrKeep(env, "MicrosoftInstrumentationEngine_ConfigPath64_AppMap", Path.Join(runtimeDir, "ProductionBreakpoints_x64.config"));
        }

        static int Main(string[] args)
        {
            if (!PlatformSupported()) {
                Console.WriteLine($"AppMap for .net is currently only supported on linux-x64.");
                Console.WriteLine($"Platform {RuntimeInformation.RuntimeIdentifier} not supported yet.");
                Console.WriteLine($"Please go to https://github.com/applandinc/appmap-dotnet to tell us about platform support you're interested in.");
                return 127;
            }

            var runtimeDir = FindRuntimeDirectory();

            if (runtimeDir == null)
                return 128;

            var exec = new ProcessStartInfo("dotnet");
            PrepareEnvironment(exec.Environment, runtimeDir);

            var arguments = exec.ArgumentList;
            foreach (var arg in args) {
                arguments.Add(arg);
            }

            try {
                var proc = Process.Start(exec);
                if (proc == null) {
                    Console.WriteLine($"Error running dotnet");
                    return 129;
                }

                proc.WaitForExit();
                return proc.ExitCode;
            } catch (Exception e) {
                Console.WriteLine($"Error running dotnet: {e.Message}");
                return 129;
            }
        }
    }
}
