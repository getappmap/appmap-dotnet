using Microsoft.EntityFrameworkCore;

namespace ZeroTouchWeb;

public class Widget
{
    public int Id { get; set; }
    public string Name { get; set; } = "";
    public int Quantity { get; set; }
}

public class WidgetContext : DbContext
{
    public WidgetContext(DbContextOptions<WidgetContext> options) : base(options) { }

    public DbSet<Widget> Widgets => Set<Widget>();
}

/// <summary>
/// A small service layer so recordings show method calls wrapping the SQL —
/// the shape the agent is meant to capture end to end (HTTP → method → SQL).
/// </summary>
public class WidgetService
{
    private readonly WidgetContext db;

    public WidgetService(WidgetContext db) => this.db = db;

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
    public static void Initialize(WidgetContext db)
    {
        db.Database.EnsureDeleted();
        db.Database.EnsureCreated();
        db.Widgets.AddRange(
            new Widget { Name = "Sprocket", Quantity = 12 },
            new Widget { Name = "Cog", Quantity = 7 },
            new Widget { Name = "Flange", Quantity = 3 });
        db.SaveChanges();
    }
}
