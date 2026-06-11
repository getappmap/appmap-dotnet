using Microsoft.EntityFrameworkCore;
using SqlServerWeb;

// No AppMap reference: the agent attaches via env vars only (zero-touch).
var builder = WebApplication.CreateBuilder(args);

// Connection string from ConnectionStrings__Default (CI sets it to the SQL
// Server service container); a localhost default keeps it runnable by hand.
var connectionString = builder.Configuration.GetConnectionString("Default")
    ?? "Server=localhost,1433;Database=AppMapHarness;User Id=sa;"
       + "Password=Your_password123;TrustServerCertificate=True;Encrypt=False";

builder.Services.AddDbContext<ShopContext>(o =>
    o.UseSqlServer(connectionString, sql => sql.EnableRetryOnFailure()));
builder.Services.AddScoped<WidgetService>();

var app = builder.Build();

// Seed with retries: a SQL Server container can still be coming up. Never
// crash the host on failure — /health must answer so the harness can drive it.
using (var scope = app.Services.CreateScope())
{
    var db = scope.ServiceProvider.GetRequiredService<ShopContext>();
    for (var attempt = 1; attempt <= 20; attempt++)
    {
        try { SeedData.Initialize(db); break; }
        catch (Exception e)
        {
            app.Logger.LogWarning("seed attempt {Attempt} failed: {Message}", attempt, e.Message);
            Thread.Sleep(3000);
        }
    }
}

app.MapGet("/health", () => "ok");
app.MapGet("/widgets", (WidgetService svc) => svc.All());
app.MapGet("/widgets/{id:int}", (int id, WidgetService svc) =>
    svc.Find(id) is { } w ? Results.Ok(w) : Results.NotFound());
app.MapPost("/widgets", (Widget widget, WidgetService svc) => Results.Ok(svc.Add(widget)));

app.Run();
