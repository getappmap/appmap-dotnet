using AppMap.AspNetCore;
using Microsoft.EntityFrameworkCore;
using PetClinic.Data;
using PetClinic.Services;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddControllers();
builder.Services.AddDbContext<PetClinicContext>(options =>
    options.UseSqlite("Data Source=petclinic.db"));
builder.Services.AddScoped<OwnerService>();
builder.Services.AddScoped<VetService>();

var app = builder.Build();

app.UseAppMap();   // first in the pipeline: HTTP events + /_appmap/record

using (var scope = app.Services.CreateScope())
{
    var context = scope.ServiceProvider.GetRequiredService<PetClinicContext>();
    context.Database.EnsureCreated();
    SeedData.Initialize(context);
}

app.MapControllers();

app.Run();
