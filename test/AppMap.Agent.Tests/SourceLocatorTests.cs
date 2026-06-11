using System.Runtime.InteropServices;
using AppMap.Util;
using Xunit;

namespace AppMap.Agent.Tests;

public class SourceLocatorTests
{
    [Fact]
    public void RelativizesLinuxPathAgainstProjectRoot()
    {
        Assert.Equal("src/Web/Index.cs", SourceLocator.RelativizeAgainst(
            "/home/user/eShopOnWeb/src/Web/Index.cs",
            new[] { "/home/user/eShopOnWeb" }));
    }

    [Fact]
    public void RelativizesWindowsPathWithForwardSlashes()
    {
        // A map recorded on Windows must query identically on Linux.
        Assert.Equal("src/Web/Index.cs", SourceLocator.RelativizeAgainst(
            @"C:\agent\work\eShopOnWeb\src\Web\Index.cs",
            new[] { @"C:\agent\work\eShopOnWeb" }));
    }

    [Fact]
    public void PrefersTheFirstMatchingRoot()
    {
        // First root wins; ResolveRoots lists the git/repo root first.
        Assert.Equal("service/Index.cs", SourceLocator.RelativizeAgainst(
            "/repo/service/Index.cs",
            new[] { "/repo", "/repo/service" }));
    }

    [Fact]
    public void LeavesOutOfTreePathsButNormalizesSlashes()
    {
        Assert.Equal("D:/nuget/lib/Foo.cs", SourceLocator.RelativizeAgainst(
            @"D:\nuget\lib\Foo.cs", new[] { @"C:\app" }));
    }

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
