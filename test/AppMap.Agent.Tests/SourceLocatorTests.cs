using System.Runtime.InteropServices;
using AppMap.Util;
using Xunit;

namespace AppMap.Agent.Tests;

public class SourceLocatorTests
{
    [Fact]
    public void WindowsPdbFallbackIsGatedToWindows()
    {
        Assert.Equal(
            RuntimeInformation.IsOSPlatform(OSPlatform.Windows),
            WindowsPdbReader.IsSupported);
    }

    [Fact]
    public void WindowsPdbReaderNeverThrowsAndReturnsNullOffWindows()
    {
        // The whole point of the guard: on non-Windows the COM binder is
        // never touched, so Open is a safe no-op rather than a P/Invoke
        // failure. On Windows this still must not throw for our own assembly.
        var reader = WindowsPdbReader.Open(typeof(SourceLocatorTests).Assembly);
        if (!WindowsPdbReader.IsSupported)
            Assert.Null(reader);
    }

    [Fact]
    public void PortablePdbStillResolvesThisAssembly()
    {
        // The test assembly is built with a portable PDB (SDK default), so
        // the primary path keeps working alongside the fallback.
        var method = typeof(SourceLocatorTests).GetMethod(nameof(PortablePdbStillResolvesThisAssembly))!;
        var (path, line) = SourceLocator.Locate(method);
        Assert.NotNull(path);
        Assert.EndsWith("SourceLocatorTests.cs", path);
        Assert.True(line > 0);
    }
}
