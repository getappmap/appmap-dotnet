using Microsoft.AspNetCore.Mvc;
using PetClinic.Models;
using PetClinic.Services;

namespace PetClinic.Controllers;

[ApiController]
[Route("vets")]
public class VetsController : ControllerBase
{
    private readonly VetService vets;

    public VetsController(VetService vets) => this.vets = vets;

    [HttpGet]
    public IEnumerable<Vet> List() => vets.FindAll();

    [HttpGet("async")]
    public async Task<IEnumerable<Vet>> ListAsync() => await vets.FindAllAsync();
}
