using AppMap.AspNetCore;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.DependencyInjection;
using Xunit;

namespace AppMap.Agent.Tests;

/// <summary>
/// Unit coverage for the zero-touch HostingStartup wiring. The end-to-end
/// behavior (an unmodified app actually recording HTTP + SQL via env vars
/// only) is exercised by the web target in the harness, which runs in CI.
/// These tests avoid triggering the real agent pipeline — calling UseAppMap
/// would instrument the test process — so they verify the DI registration
/// the HostingStartup performs.
/// </summary>
public class HostingStartupTests
{
    [Fact]
    public void HostingStartupRegistersTheStartupFilter()
    {
        var builder = WebApplication.CreateBuilder();
        new AppMapHostingStartup().Configure(builder.WebHost);

        var app = builder.Build();
        var filters = app.Services.GetServices<IStartupFilter>();

        // The filter is internal; identify it by name without widening the API.
        Assert.Contains(filters, f => f.GetType().Name == "AppMapStartupFilter");
    }

    [Fact]
    public void StartupFilterIsRegisteredExactlyOnce()
    {
        var builder = WebApplication.CreateBuilder();
        new AppMapHostingStartup().Configure(builder.WebHost);

        var app = builder.Build();
        var ours = app.Services.GetServices<IStartupFilter>()
            .Count(f => f.GetType().Name == "AppMapStartupFilter");

        Assert.Equal(1, ours);
    }
}
