using Microsoft.EntityFrameworkCore;
using PetClinic.Models;

namespace PetClinic.Data;

public class PetClinicContext : DbContext
{
    public PetClinicContext(DbContextOptions<PetClinicContext> options) : base(options) { }

    public DbSet<Owner> Owners => Set<Owner>();
    public DbSet<Pet> Pets => Set<Pet>();
    public DbSet<Vet> Vets => Set<Vet>();
}
