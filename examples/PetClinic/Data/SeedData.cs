using PetClinic.Models;

namespace PetClinic.Data;

public static class SeedData
{
    public static void Initialize(PetClinicContext context)
    {
        if (context.Owners.Any())
            return;

        var george = new Owner
        {
            FirstName = "George", LastName = "Franklin", City = "Madison",
            Pets = { new Pet { Name = "Leo", Type = "cat" } },
        };
        var betty = new Owner
        {
            FirstName = "Betty", LastName = "Davis", City = "Sun Prairie",
            Pets =
            {
                new Pet { Name = "Basil", Type = "hamster" },
                new Pet { Name = "Rosy", Type = "dog" },
            },
        };
        context.Owners.AddRange(george, betty);

        context.Vets.AddRange(
            new Vet { FirstName = "James", LastName = "Carter", Specialty = "general" },
            new Vet { FirstName = "Helen", LastName = "Leary", Specialty = "radiology" });

        context.SaveChanges();
    }
}
