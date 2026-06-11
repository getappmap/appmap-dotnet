using AppMap.Config;
using Xunit;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace AppMap.Agent.Tests;

public class AppMapConfigTests
{
    private static AppMapConfig Parse(string yaml) =>
        new DeserializerBuilder()
            .WithNamingConvention(UnderscoredNamingConvention.Instance)
            .IgnoreUnmatchedProperties()
            .Build()
            .Deserialize<AppMapConfig>(yaml);

    [Fact]
    public void ParsesPackagesWithExcludes()
    {
        var config = Parse("""
            name: my-app
            packages:
            - path: MyApp
              exclude:
              - MyApp.Generated
            - path: OtherLib.Core
              shallow: true
            """);

        Assert.Equal("my-app", config.Name);
        Assert.Equal(2, config.Packages.Count);
        Assert.NotNull(config.FindPackage("MyApp.Services.UserService.Find"));
        Assert.Null(config.FindPackage("MyApp.Generated.Dto.Build"));
        Assert.Null(config.FindPackage("MyAppOther.Thing.Do"));
        Assert.True(config.FindPackage("OtherLib.Core.Engine.Run")!.Shallow);
    }

    [Fact]
    public void MethodsListRestrictsAndLabels()
    {
        var config = Parse("""
            name: my-app
            packages:
            - path: MyApp
              methods:
              - class: .*Service
                name: Find.*
                labels: [crud]
            """);

        var pkg = config.FindPackage("MyApp.UserService.FindUser");
        Assert.NotNull(pkg);
        Assert.Equal(new[] { "crud" }, pkg!.LabelsFor("MyApp.UserService.FindUser"));
        Assert.Null(config.FindPackage("MyApp.UserService.DeleteUser"));
    }

    [Fact]
    public void DefaultAppMapDir()
    {
        var config = Parse("name: x");
        Assert.Equal("tmp/appmap", config.AppMapDir);
    }
}
