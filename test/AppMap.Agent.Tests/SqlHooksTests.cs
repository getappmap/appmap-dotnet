using System;
using System.Reflection;
using AppMap.Instrumentation;
using Xunit;

namespace AppMap.Agent.Tests;

public class SqlHooksTests
{
    [Fact]
    public void PartialTypeLoadKeepsTheLoadableTypes()
    {
        // The Microsoft.Data.SqlClient-on-Linux shape: most types load, one
        // does not. Before the fix, GetTypes() throwing made SqlHooks discard
        // the whole assembly — so SqlCommand was never patched and zero SQL
        // was recorded against SQL Server. The loadable types must survive.
        var ex = new ReflectionTypeLoadException(
            new Type?[] { typeof(string), null, typeof(int) },
            new Exception?[] { null, new TypeLoadException("unloadable"), null });

        var types = SqlHooks.LoadableTypes(ex);

        Assert.Equal(new[] { typeof(string), typeof(int) }, types);
    }
}
