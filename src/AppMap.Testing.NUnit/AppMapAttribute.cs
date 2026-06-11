using AppMap.Output;
using AppMap.Record;
using AppMap.Util;
using NUnit.Framework;
using NUnit.Framework.Interfaces;

namespace AppMap.Testing.NUnit;

/// <summary>
/// Records one AppMap per test method — the analog of appmap-java's TestNG
/// hooks. Unlike the xUnit integration, NUnit's ITestAction sees the test
/// outcome, so test_status and test_failure are populated.
/// <code>[AppMap] public class MyTests { ... }</code>
/// </summary>
[AttributeUsage(AttributeTargets.Method | AttributeTargets.Class | AttributeTargets.Assembly)]
public sealed class AppMapAttribute : Attribute, ITestAction
{
    public ActionTargets Targets => ActionTargets.Test;

    private bool startedHere;

    public void BeforeTest(ITest test)
    {
        startedHere = false;
        AgentBootstrap.Init();
        // Don't displace a remote or process recording already in progress.
        if (Recorder.Instance.HasGlobalSession || test.Method == null)
            return;
        startedHere = true;
        var definedClass = Value.TypeName(test.Method.MethodInfo.DeclaringType!);
        var (path, lineno) = SourceLocator.Locate(test.Method.MethodInfo);
        Recorder.Instance.Start(new Metadata
        {
            RecorderName = "nunit",
            RecorderType = "tests",
            Name = test.FullName,
            RecordingDefinedClass = definedClass,
            RecordingMethodId = test.Method.Name,
            SourceLocation = path != null && lineno.HasValue ? $"{path}:{lineno}" : null,
            Frameworks = { new Framework { Name = "NUnit" } },
        });
    }

    public void AfterTest(ITest test)
    {
        if (!startedHere)
            return;
        try
        {
            var recording = Recorder.Instance.Stop();
            if (recording == null || test.Method == null)
                return;

            var result = TestContext.CurrentContext.Result;
            recording.Metadata.TestStatus =
                result.Outcome.Status == TestStatus.Failed ? "failed" : "succeeded";
            if (result.Outcome.Status == TestStatus.Failed)
            {
                recording.Metadata.TestFailure = new TestFailure
                {
                    Message = result.Message ?? "test failed",
                    Location = recording.Metadata.SourceLocation,
                };
            }

            var definedClass = Value.TypeName(test.Method.MethodInfo.DeclaringType!);
            recording.Save($"{definedClass}_{test.Method.Name}");
        }
        catch (Exception e)
        {
            Logger.Error("failed to save test recording", e);
        }
    }
}
