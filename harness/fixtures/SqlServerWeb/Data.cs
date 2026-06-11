using Microsoft.EntityFrameworkCore;

namespace SqlServerWeb;

public class Widget
{
    public int Id { get; set; }
    public string Name { get; set; } = "";
    public int Quantity { get; set; }
}

public class ShopContext : DbContext
{
    public ShopContext(DbContextOptions<ShopContext> options) : base(options) { }

    public DbSet<Widget> Widgets => Set<Widget>();
}

public class WidgetService
{
    private readonly ShopContext db;

    public WidgetService(ShopContext db) => this.db = db;

    public List<Widget> All() => db.Widgets.OrderBy(w => w.Id).ToList();

    public Widget? Find(int id) => db.Widgets.FirstOrDefault(w => w.Id == id);

    public Widget Add(Widget widget)
    {
        db.Widgets.Add(widget);
        db.SaveChanges();
        return widget;
    }
}

public static class SeedData
{
    public static void Initialize(ShopContext db)
    {
        db.Database.EnsureCreated();
        if (db.Widgets.Any())
            return;
        db.Widgets.AddRange(
            new Widget { Name = "Sprocket", Quantity = 12 },
            new Widget { Name = "Cog", Quantity = 7 },
            new Widget { Name = "Flange", Quantity = 3 });
        db.SaveChanges();
    }
}
