using System.Security.Cryptography;
using AppMap.Instrumentation;
using Xunit;

namespace AppMap.Agent.Tests;

[Labels("class-label")]
internal class LabeledService
{
    [Labels("method-label", "class-label")] // duplicate of the class label
    public void Save() { }

    public void Plain() { }
}

internal class UnlabeledService
{
    public void Plain() { }
}

public class AttributeLabelsTests
{
    [Fact]
    public void MergesClassAndMethodLabelsWithoutDuplicates()
    {
        var labels = AttributeLabels.Of(typeof(LabeledService).GetMethod("Save")!);
        Assert.Equal(new[] { "class-label", "method-label" }, labels);
    }

    [Fact]
    public void ClassLabelAppliesToUnattributedMethod()
    {
        var labels = AttributeLabels.Of(typeof(LabeledService).GetMethod("Plain")!);
        Assert.Equal(new[] { "class-label" }, labels);
    }

    [Fact]
    public void ReturnsNullWhenNoAttribute()
    {
        Assert.Null(AttributeLabels.Of(typeof(UnlabeledService).GetMethod("Plain")!));
    }

    [Fact]
    public void MergeCombinesConfigAndAttributeLabels()
    {
        Assert.Equal(new[] { "crud", "audit" },
            AttributeLabels.Merge(new[] { "crud" }, new[] { "audit", "crud" }));
        Assert.Equal(new[] { "crud" }, AttributeLabels.Merge(new[] { "crud" }, null));
        Assert.Equal(new[] { "crud" }, AttributeLabels.Merge(null, new[] { "crud" }));
        Assert.Null(AttributeLabels.Merge(null, null));
    }
}

public class BuiltinHooksTests
{
    private sealed class FakeSession : Microsoft.AspNetCore.Http.ISession
    {
        public bool IsAvailable => true;
        public string Id => "";
        public IEnumerable<string> Keys => Array.Empty<string>();
        public void Clear() { }
        public Task CommitAsync(CancellationToken token = default) => Task.CompletedTask;
        public Task LoadAsync(CancellationToken token = default) => Task.CompletedTask;
        public void Remove(string key) { }
        public void Set(string key, byte[] value) { }
        public bool TryGetValue(string key, out byte[]? value)
        {
            value = null;
            return false;
        }
    }

    private static IEnumerable<HookRule> RulesMatching(Type type) =>
        BuiltinHooks.Rules.Where(r => r.Matches(type));

    [Fact]
    public void HashAlgorithmSubclassesMatchTheDigestRule()
    {
        // SHA256.Create() returns an internal implementation type; the rule
        // must match through the abstract HashAlgorithm base.
        using var sha = SHA256.Create();
        Assert.Contains(RulesMatching(sha.GetType()),
            r => r.Labels.Contains("crypto.digest"));
        // ComputeHash is declared concrete on the abstract base itself.
        Assert.Contains(RulesMatching(typeof(HashAlgorithm)),
            r => r.Labels.Contains("crypto.digest"));
    }

    [Fact]
    public void SymmetricAlgorithmMatchesEncryptAndDecryptRules()
    {
        using var aes = System.Security.Cryptography.Aes.Create();
        var labels = RulesMatching(aes.GetType()).SelectMany(r => r.Labels).ToList();
        Assert.Contains("crypto.encrypt", labels);
        Assert.Contains("crypto.decrypt", labels);
    }

    [Fact]
    public void SessionImplementationsMatchThroughTheInterface()
    {
        var labels = RulesMatching(typeof(FakeSession)).SelectMany(r => r.Labels).ToList();
        Assert.Contains("http.session.read", labels);
        Assert.Contains("http.session.write", labels);
    }

    [Fact]
    public void HttpClientMatchesTheOutboundRule()
    {
        var labels = RulesMatching(typeof(System.Net.Http.HttpClient))
            .SelectMany(r => r.Labels).ToList();
        Assert.Contains("http.client.request", labels);
    }

    [Fact]
    public void XmlSerializerMatchesTheDeserializeRule()
    {
        var labels = RulesMatching(typeof(System.Xml.Serialization.XmlSerializer))
            .SelectMany(r => r.Labels).ToList();
        Assert.Contains("deserialize", labels);
    }

    [Fact]
    public void EveryRuleHasTypeMethodsAndLabels()
    {
        foreach (var rule in BuiltinHooks.Rules)
        {
            Assert.False(string.IsNullOrEmpty(rule.Type));
            Assert.NotEmpty(rule.Methods);
            Assert.NotEmpty(rule.Labels);
        }
    }

    [Fact]
    public void UnrelatedTypesMatchNoRule()
    {
        Assert.Empty(RulesMatching(typeof(UnlabeledService)));
    }
}
