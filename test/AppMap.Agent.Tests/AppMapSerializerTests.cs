using System.Text.Json;
using AppMap.Output;
using AppMap.Record;
using Xunit;

namespace AppMap.Agent.Tests;

public class AppMapSerializerTests
{
    private static JsonDocument Serialize(Metadata md, List<Event> events, CodeObjectTree tree)
    {
        using var buffer = new MemoryStream();
        AppMapSerializer.Write(buffer, md, events, tree);
        return JsonDocument.Parse(buffer.ToArray());
    }

    [Fact]
    public void WritesFormatVersionAndMetadata()
    {
        var doc = Serialize(
            new Metadata { RecorderName = "xunit", RecorderType = "tests", App = "demo" },
            new List<Event>(), new CodeObjectTree());
        var root = doc.RootElement;

        Assert.Equal("1.2", root.GetProperty("version").GetString());
        var metadata = root.GetProperty("metadata");
        Assert.Equal("demo", metadata.GetProperty("app").GetString());
        Assert.Equal("csharp", metadata.GetProperty("language").GetProperty("name").GetString());
        Assert.Equal("appmap-dotnet",
            metadata.GetProperty("client").GetProperty("name").GetString());
        Assert.Equal("xunit", metadata.GetProperty("recorder").GetProperty("name").GetString());
        Assert.Equal("tests", metadata.GetProperty("recorder").GetProperty("type").GetString());
    }

    [Fact]
    public void WritesCallAndReturnEventsInSnakeCase()
    {
        var call = new Event
        {
            EventType = "call",
            DefinedClass = "Demo.Service",
            MethodId = "DoWork",
            Static = false,
            Parameters = new List<Value>
            {
                Value.Capture(42, name: "count", declaredType: typeof(int), kind: "req"),
            },
        };
        var ret = new Event
        {
            EventType = "return",
            ParentId = call.Id,
            Elapsed = 0.25,
            ReturnValue = Value.Capture("ok", declaredType: typeof(string)),
        };

        var doc = Serialize(
            new Metadata { RecorderName = "tests", RecorderType = "tests" },
            new List<Event> { call, ret }, new CodeObjectTree());
        var events = doc.RootElement.GetProperty("events");

        Assert.Equal(2, events.GetArrayLength());
        var callJson = events[0];
        Assert.Equal("call", callJson.GetProperty("event").GetString());
        Assert.Equal("Demo.Service", callJson.GetProperty("defined_class").GetString());
        Assert.Equal("DoWork", callJson.GetProperty("method_id").GetString());
        var param = callJson.GetProperty("parameters")[0];
        Assert.Equal("count", param.GetProperty("name").GetString());
        Assert.Equal("42", param.GetProperty("value").GetString());
        Assert.Equal("System.Int32", param.GetProperty("class").GetString());

        var retJson = events[1];
        Assert.Equal("return", retJson.GetProperty("event").GetString());
        Assert.Equal(call.Id, retJson.GetProperty("parent_id").GetInt32());
        Assert.Equal(0.25, retJson.GetProperty("elapsed").GetDouble());
        Assert.Equal("ok", retJson.GetProperty("return_value").GetProperty("value").GetString());
    }

    [Fact]
    public void WritesHttpAndSqlEvents()
    {
        var call = new Event
        {
            EventType = "call",
            HttpServerRequest = new HttpServerRequest
            {
                RequestMethod = "GET",
                PathInfo = "/users/3",
                NormalizedPathInfo = "/users/{id}",
                Protocol = "HTTP/1.1",
            },
        };
        var sql = new Event
        {
            EventType = "call",
            SqlQuery = new SqlQuery { Sql = "SELECT 1", DatabaseType = "sqlite" },
        };

        var doc = Serialize(
            new Metadata { RecorderName = "request_recording", RecorderType = "requests" },
            new List<Event> { call, sql }, new CodeObjectTree());
        var events = doc.RootElement.GetProperty("events");

        var req = events[0].GetProperty("http_server_request");
        Assert.Equal("GET", req.GetProperty("request_method").GetString());
        Assert.Equal("/users/{id}", req.GetProperty("normalized_path_info").GetString());

        var query = events[1].GetProperty("sql_query");
        Assert.Equal("SELECT 1", query.GetProperty("sql").GetString());
        Assert.Equal("sqlite", query.GetProperty("database_type").GetString());
    }

    [Fact]
    public void BuildsNestedClassMap()
    {
        var tree = new CodeObjectTree();
        tree.RegisterFunction("Demo.Services", new[] { "UserService" },
            "FindUser", false, "Services/UserService.cs:12", new[] { "crud" });
        tree.RegisterFunction("Demo.Services", new[] { "UserService" },
            "FindUser", false, "Services/UserService.cs:12", null); // duplicate

        var doc = Serialize(
            new Metadata { RecorderName = "tests", RecorderType = "tests" },
            new List<Event>(), tree);
        var classMap = doc.RootElement.GetProperty("classMap");

        var demo = classMap[0];
        Assert.Equal("package", demo.GetProperty("type").GetString());
        Assert.Equal("Demo", demo.GetProperty("name").GetString());
        var services = demo.GetProperty("children")[0];
        Assert.Equal("Services", services.GetProperty("name").GetString());
        var cls = services.GetProperty("children")[0];
        Assert.Equal("class", cls.GetProperty("type").GetString());
        var fn = cls.GetProperty("children");
        Assert.Equal(1, fn.GetArrayLength());
        Assert.Equal("function", fn[0].GetProperty("type").GetString());
        Assert.Equal("Services/UserService.cs:12", fn[0].GetProperty("location").GetString());
        Assert.Equal("crud", fn[0].GetProperty("labels")[0].GetString());
    }
}
