using System.Reflection;
using AppMap.Output;
using AppMap.Record;
using AppMap.Util;
using Xunit.Sdk;

namespace AppMap.Testing.Xunit;

/// <summary>
/// Records one AppMap per test method — the analog of appmap-java's JUnit
/// hooks. Apply to a method, class, or assembly:
/// <code>[AppMap] public class MyTests { ... }</code>
/// Caveat: xUnit's BeforeAfterTestAttribute does not expose the test
/// outcome, so test_status is "succeeded" unless the framework reports
/// otherwise downstream. Disable parallelization for coherent maps.
/// </summary>
[AttributeUsage(AttributeTargets.Method | AttributeTargets.Class | AttributeTargets.Assembly)]
public sealed class AppMapAttribute : BeforeAfterTestAttribute
{
    private bool startedHere;

    public override void Before(MethodInfo methodUnderTest)
    {
        startedHere = false;
        AgentBootstrap.Init();
        // When the agent is attached, tests are auto-recorded with no code
        // changes (TestHooks); this attribute then stands down and remains
        // the explicit path for runs without the agent.
        if (Instrumentation.TestHooks.Active)
            return;
        // Don't displace a remote or process recording already in progress.
        if (Recorder.Instance.HasGlobalSession)
            return;
        startedHere = true;
        var definedClass = Value.TypeName(methodUnderTest.DeclaringType!);
        var (path, lineno) = SourceLocator.Locate(methodUnderTest);
        Recorder.Instance.Start(new Metadata
        {
            RecorderName = "xunit",
            RecorderType = "tests",
            Name = $"{definedClass}.{methodUnderTest.Name}",
            RecordingDefinedClass = definedClass,
            RecordingMethodId = methodUnderTest.Name,
            SourceLocation = path != null && lineno.HasValue ? $"{path}:{lineno}" : null,
            Frameworks = { new Framework { Name = "xunit" } },
        });
    }

    public override void After(MethodInfo methodUnderTest)
    {
        if (!startedHere)
            return;
        try
        {
            var recording = Recorder.Instance.Stop();
            if (recording == null)
                return;
            recording.Metadata.TestStatus = "succeeded";
            var definedClass = Value.TypeName(methodUnderTest.DeclaringType!);
            recording.Save($"{definedClass}_{methodUnderTest.Name}");
        }
        catch (Exception e)
        {
            Logger.Error("failed to save test recording", e);
        }
    }
}
