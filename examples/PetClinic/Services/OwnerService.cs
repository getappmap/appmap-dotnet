using AppMap;
using Microsoft.EntityFrameworkCore;
using PetClinic.Data;
using PetClinic.Models;

namespace PetClinic.Services;

public class OwnerService
{
    private readonly PetClinicContext context;
    private readonly ILogger<OwnerService> logger;

    public OwnerService(PetClinicContext context, ILogger<OwnerService> logger)
    {
        this.context = context;
        this.logger = logger;
    }

    public List<Owner> FindAll()
    {
        // LogInformation is hooked by the agent's built-in rules and shows
        // up in the AppMap labeled "log".
        logger.LogInformation("Listing all owners");
        return context.Owners.Include(o => o.Pets).ToList();
    }

    public Owner? FindByLastName(string lastName) =>
        context.Owners.Include(o => o.Pets)
            .FirstOrDefault(o => o.LastName == lastName);

    [Labels("crud")] // appears on this function's classMap entry
    public Owner Add(Owner owner)
    {
        logger.LogInformation("Adding owner {LastName}", owner.LastName);
        context.Owners.Add(owner);
        context.SaveChanges();
        return owner;
    }
}
