using Microsoft.AspNetCore.Mvc;
using PetClinic.Models;
using PetClinic.Services;

namespace PetClinic.Controllers;

[ApiController]
[Route("owners")]
public class OwnersController : ControllerBase
{
    private readonly OwnerService owners;

    public OwnersController(OwnerService owners) => this.owners = owners;

    [HttpGet]
    public IEnumerable<Owner> List() => owners.FindAll();

    [HttpGet("{lastName}")]
    public ActionResult<Owner> Find(string lastName)
    {
        var owner = owners.FindByLastName(lastName);
        return owner == null ? NotFound() : owner;
    }

    [HttpPost]
    public Owner Create(Owner owner) => owners.Add(owner);
}
