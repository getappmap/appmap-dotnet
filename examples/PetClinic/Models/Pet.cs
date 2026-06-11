using System.Text.Json.Serialization;

namespace PetClinic.Models;

public class Pet
{
    public int Id { get; set; }
    public string Name { get; set; } = "";
    public string Type { get; set; } = "";
    public int OwnerId { get; set; }

    [JsonIgnore]   // break the Owner <-> Pet cycle when serializing responses
    public Owner? Owner { get; set; }
}
