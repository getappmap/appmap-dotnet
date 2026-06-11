using AppMap.Instrumentation;
using Xunit;

namespace AppMap.Agent.Tests;

public class AsyncResultTests
{
    [Fact]
    public void RecognizesTaskAndValueTaskAsAwaitable()
    {
        Assert.NotNull(AsyncResult.AsTask(Task.CompletedTask));
        Assert.NotNull(AsyncResult.AsTask(Task.FromResult(42)));
        Assert.NotNull(AsyncResult.AsTask(new ValueTask(Task.CompletedTask)));
        Assert.NotNull(AsyncResult.AsTask(new ValueTask<int>(7)));
    }

    [Fact]
    public void NonAwaitablesAreNotTasks()
    {
        Assert.Null(AsyncResult.AsTask(null));
        Assert.Null(AsyncResult.AsTask(42));
        Assert.Null(AsyncResult.AsTask("hello"));
        Assert.Null(AsyncResult.AsTask(new object()));
    }

    [Fact]
    public void UnwrapsGenericResult()
    {
        var (value, type, ex) = AsyncResult.Unwrap(Task.FromResult(123));
        Assert.Equal(123, value);
        Assert.Equal(typeof(int), type);
        Assert.Null(ex);
    }

    [Fact]
    public void NonGenericTaskHasNoValue()
    {
        var (value, type, ex) = AsyncResult.Unwrap(Task.CompletedTask);
        Assert.Null(value);
        Assert.Null(type);
        Assert.Null(ex);
    }

    [Fact]
    public void UnwrapsFaultToFirstInnerException()
    {
        var task = Task.FromException<int>(new InvalidOperationException("boom"));
        var (value, _, ex) = AsyncResult.Unwrap(task);
        Assert.Null(value);
        Assert.IsType<InvalidOperationException>(ex);
        Assert.Equal("boom", ex!.Message);
    }

    [Fact]
    public void UnwrapsCancellation()
    {
        var source = new TaskCompletionSource<int>();
        source.SetCanceled();
        var (_, _, ex) = AsyncResult.Unwrap(source.Task);
        Assert.IsType<TaskCanceledException>(ex);
    }

    [Fact]
    public async Task UnwrapsValueTaskOfTViaAsTask()
    {
        async ValueTask<string> Make() { await Task.Yield(); return "done"; }
        var task = AsyncResult.AsTask(Make());
        Assert.NotNull(task);
        await task!;
        var (value, type, ex) = AsyncResult.Unwrap(task);
        Assert.Equal("done", value);
        Assert.Equal(typeof(string), type);
        Assert.Null(ex);
    }

    [Fact]
    public void VoidTaskResultIsTreatedAsValueless()
    {
        // `await Task.Run(() => {})` boxes Task<VoidTaskResult> internally.
        var task = (Task)Task.Run(() => { });
        task.Wait();
        var (value, type, _) = AsyncResult.Unwrap(task);
        Assert.Null(value);
        Assert.Null(type);
    }
}
