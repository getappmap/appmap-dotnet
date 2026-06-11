using AppMap.Output;
using Xunit;

namespace AppMap.Agent.Tests;

public class ValueTests
{
    [Fact]
    public void CapturesNull()
    {
        var v = Value.Capture(null, name: "arg", declaredType: typeof(string));
        Assert.Equal("null", v.StringValue);
        Assert.Equal("System.String", v.Class);
        Assert.Null(v.ObjectId);
    }

    [Fact]
    public void ValueTypesHaveNoObjectId()
    {
        Assert.Null(Value.Capture(7).ObjectId);
        Assert.NotNull(Value.Capture(new object()).ObjectId);
    }

    [Fact]
    public void TruncatesLongValues()
    {
        var v = Value.Capture(new string('x', 5000));
        Assert.NotNull(v.StringValue);
        Assert.Equal(1024, v.StringValue!.Length);
        Assert.EndsWith("...", v.StringValue);
    }

    private sealed class Hostile
    {
        public override string ToString() => throw new InvalidOperationException("nope");
    }

    [Fact]
    public void SurvivesThrowingToString()
    {
        Assert.Equal("< invalid >", Value.Capture(new Hostile()).StringValue);
    }

    [Fact]
    public void NestedTypeNamesUseDots()
    {
        Assert.Equal("AppMap.Agent.Tests.ValueTests.Hostile",
            Value.TypeName(typeof(Hostile)));
    }
}
