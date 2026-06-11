namespace PetClinic.Models;

public class Owner
{
    public int Id { get; set; }
    public string FirstName { get; set; } = "";
    public string LastName { get; set; } = "";
    public string City { get; set; } = "";
    public List<Pet> Pets { get; set; } = new();
}
