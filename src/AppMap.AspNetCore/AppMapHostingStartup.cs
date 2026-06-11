using AppMap.AspNetCore;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.DependencyInjection;

// Registered automatically by ASP.NET Core when this assembly is named in
// ASPNETCORE_HOSTINGSTARTUPASSEMBLIES — that is the whole zero-touch hook.
[assembly: HostingStartup(typeof(AppMapHostingStartup))]

namespace AppMap.AspNetCore;

/// <summary>
/// Zero-touch attach. Setting
/// <c>ASPNETCORE_HOSTINGSTARTUPASSEMBLIES=AppMap.AspNetCore</c> makes ASP.NET
/// Core load this assembly and run <see cref="Configure"/> before the
/// application's own startup — with no change to the app's source. It is the
/// .NET analog of a Java <c>-javaagent</c> auto-registering its servlet
/// filter. We register an <see cref="IStartupFilter"/> that prepends
/// <c>UseAppMap()</c> to the pipeline, so HTTP (and, through the agent's SQL
/// hooks, sql_query) events are recorded for an unmodified app.
///
/// Pair it with <c>DOTNET_STARTUP_HOOKS</c> so method/SQL instrumentation is
/// installed too; the hook's assembly-resolve handler also makes this
/// assembly loadable from the agent directory.
/// </summary>
public sealed class AppMapHostingStartup : IHostingStartup
{
    public void Configure(IWebHostBuilder builder)
    {
        builder.ConfigureServices(services =>
            services.AddTransient<IStartupFilter, AppMapStartupFilter>());
    }
}

/// <summary>
/// Prepends <c>UseAppMap()</c> to the request pipeline. Running before the
/// app's own configuration brackets the whole request, just like adding
/// <c>app.UseAppMap()</c> as the first middleware by hand.
/// </summary>
internal sealed class AppMapStartupFilter : IStartupFilter
{
    public Action<IApplicationBuilder> Configure(Action<IApplicationBuilder> next)
        => app =>
        {
            app.UseAppMap();
            next(app);
        };
}
