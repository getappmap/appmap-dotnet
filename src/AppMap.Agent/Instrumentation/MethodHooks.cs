using System.Diagnostics;
using System.Reflection;
using AppMap.Record;

namespace AppMap.Instrumentation;

/// <summary>
/// The Harmony prefix/finalizer pair applied to every instrumented method —
/// the runtime half of what appmap-java injects with Javassist. The prefix
/// emits the "call" event; the finalizer (which runs on both normal and
/// exceptional exit) emits the matching "return" event.
/// </summary>
public static class MethodHooks
{
    private sealed class CallContext
    {
        public required MethodTemplate Template { get; init; }
        public required int CallEventId { get; init; }
        public required long StartTimestamp { get; init; }
    }

    // Guards against the recorder's own code (ToString calls, file IO inside
    // patched assemblies, ...) re-entering the hooks. Same role as the Java
    // agent's ThreadLock.
    [ThreadStatic]
    private static bool inHook;

    public static void Prefix(MethodBase __originalMethod, object? __instance,
        object?[]? __args, ref object? __state)
    {
        __state = null;
        if (inHook || !Recorder.Instance.HasActiveSession)
            return;
        inHook = true;
        try
        {
            var template = EventTemplateRegistry.Get(__originalMethod);
            if (template == null)
                return;
            var callEvent = template.BuildCallEvent(__instance, __args);
            Recorder.Instance.Add(callEvent, template.RegisterCodeObject);
            __state = new CallContext
            {
                Template = template,
                CallEventId = callEvent.Id,
                StartTimestamp = Stopwatch.GetTimestamp(),
            };
        }
        catch (Exception e)
        {
            Util.Logger.Error("call hook failed", e);
        }
        finally
        {
            inHook = false;
        }
    }

    /// <summary>Finalizer for methods with a return value.</summary>
    public static Exception? Finalizer(MethodBase __originalMethod, object? __result,
        Exception? __exception, object? __state)
    {
        Record(__originalMethod, __result, __exception, __state);
        return __exception;
    }

    /// <summary>Finalizer for void methods (Harmony forbids __result there).</summary>
    public static Exception? FinalizerVoid(MethodBase __originalMethod,
        Exception? __exception, object? __state)
    {
        Record(__originalMethod, null, __exception, __state);
        return __exception;
    }

    private static void Record(MethodBase method, object? result,
        Exception? exception, object? state)
    {
        if (state is not CallContext ctx || inHook)
            return;

        // For an awaited method that returned a Task, defer the return event
        // until the Task completes so elapsed and the value are real. A
        // method that threw synchronously is recorded immediately.
        if (exception == null && Config.Properties.RecordAsync
            && AsyncResult.AsTask(result) is { } task)
        {
            RecordWhenComplete(ctx, task);
            return;
        }

        inHook = true;
        try
        {
            var elapsed = Elapsed(ctx);
            var returnType = (method as MethodInfo)?.ReturnType;
            var returnEvent = ctx.Template.BuildReturnEvent(
                ctx.CallEventId, elapsed, result, returnType, exception);
            Recorder.Instance.Add(returnEvent);
        }
        catch (Exception e)
        {
            Util.Logger.Error("return hook failed", e);
        }
        finally
        {
            inHook = false;
        }
    }

    private static void RecordWhenComplete(CallContext ctx, Task task)
    {
        // ContinueWith flows ExecutionContext, so the AsyncLocal request
        // session is still visible when the continuation runs.
        task.ContinueWith(t =>
        {
            if (inHook)
                return;
            inHook = true;
            try
            {
                var elapsed = Elapsed(ctx);
                var (value, valueType, exception) = AsyncResult.Unwrap(t);
                var returnEvent = ctx.Template.BuildReturnEvent(
                    ctx.CallEventId, elapsed, value, valueType, exception);
                Recorder.Instance.Add(returnEvent);
            }
            catch (Exception e)
            {
                Util.Logger.Error("async return hook failed", e);
            }
            finally
            {
                inHook = false;
            }
        }, CancellationToken.None, TaskContinuationOptions.ExecuteSynchronously, TaskScheduler.Default);
    }

    private static double Elapsed(CallContext ctx) =>
        (Stopwatch.GetTimestamp() - ctx.StartTimestamp) / (double)Stopwatch.Frequency;
}
