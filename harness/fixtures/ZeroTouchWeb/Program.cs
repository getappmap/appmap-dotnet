using Microsoft.EntityFrameworkCore;
using ZeroTouchWeb;

// Note: no `using AppMap...` and no `app.UseAppMap()`. The agent attaches via
// environment variables only (see harness/README.md).
var builder = WebApplication.CreateBuilder(args);

builder.Services.AddDbContext<WidgetContext>(o => o.UseSqlite("Data Source=zerotouch.db"));
builder.Services.AddScoped<WidgetService>();

var app = builder.Build();

using (var scope = app.Services.CreateScope())
{
    SeedData.Initialize(scope.ServiceProvider.GetRequiredService<WidgetContext>());
}

app.MapGet("/health", () => "ok");
app.MapGet("/widgets", (WidgetService svc) => svc.All());
app.MapGet("/widgets/{id:int}", (int id, WidgetService svc) =>
    svc.Find(id) is { } w ? Results.Ok(w) : Results.NotFound());
app.MapPost("/widgets", (Widget widget, WidgetService svc) => Results.Ok(svc.Add(widget)));

app.Run();
