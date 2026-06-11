using HelloAppMap;

// Run with the agent attached (see README):
//   APPMAP_RECORD_PROCESS=true \
//   DOTNET_STARTUP_HOOKS=$PWD/../../src/AppMap.StartupHook/bin/Debug/net8.0/AppMap.StartupHook.dll \
//   dotnet run
// The AppMap is written to tmp/appmap/process_recording/ on exit.

var greeter = new Greeter();
Console.WriteLine(greeter.Greet("AppMap"));
Console.WriteLine(Calculator.Fibonacci(10));

namespace HelloAppMap
{
    public class Greeter
    {
        public string Greet(string name) => $"Hello, {Decorate(name)}!";

        public string Decorate(string name) => $"*{name}*";
    }

    public static class Calculator
    {
        public static int Fibonacci(int n) =>
            n < 2 ? n : Fibonacci(n - 1) + Fibonacci(n - 2);
    }
}
