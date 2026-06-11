using Microsoft.EntityFrameworkCore;
using PetClinic.Data;
using PetClinic.Models;

namespace PetClinic.Services;

public class VetService
{
    private readonly PetClinicContext context;

    public VetService(PetClinicContext context) => this.context = context;

    public List<Vet> FindAll() => context.Vets.ToList();

    // Async path: the agent records the return when the awaited work
    // completes, so elapsed covers the real query time and the value is the
    // unwrapped List<Vet>, not a Task.
    public async Task<List<Vet>> FindAllAsync()
    {
        await Task.Delay(5);
        return await context.Vets.ToListAsync();
    }
}
