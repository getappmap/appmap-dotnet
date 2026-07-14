using Microsoft.EntityFrameworkCore;
using PetClinic.Data;
using PetClinic.Services;

// No AppMap code here: the agent attaches from outside (run with
// `appmap-dotnet dotnet run`, or set DOTNET_STARTUP_HOOKS +
// ASPNETCORE_HOSTINGSTARTUPASSEMBLIES=AppMap.AspNetCore). For pipelines that
// need explicit control over middleware order, reference AppMap.AspNetCore
// and call app.UseAppMap() first in the pipeline instead.
var builder = WebApplication.CreateBuilder(args);

builder.Services.AddControllers();
builder.Services.AddDbContext<PetClinicContext>(options =>
    options.UseSqlite("Data Source=petclinic.db"));
builder.Services.AddScoped<OwnerService>();
builder.Services.AddScoped<VetService>();

var app = builder.Build();

using (var scope = app.Services.CreateScope())
{
    var context = scope.ServiceProvider.GetRequiredService<PetClinicContext>();
    context.Database.EnsureCreated();
    SeedData.Initialize(context);
}

app.MapControllers();

app.Run();
