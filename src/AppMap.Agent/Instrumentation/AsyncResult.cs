using System.Reflection;

namespace AppMap.Instrumentation;

/// <summary>
/// Helpers for recording the completion of an awaitable return value. An
/// async method patched by Harmony returns its Task at the first incomplete
/// await, not when the work finishes — so recording the return eagerly gives
/// the wrong elapsed time and a Task instead of the real value. Instead the
/// finalizer hands the Task here and the return event is emitted when it
/// completes (full appmap-java parity on per-await splitting is still future
/// work; this fixes timing and the unwrapped value/exception).
/// </summary>
public static class AsyncResult
{
    /// <summary>
    /// If <paramref name="result"/> is an awaitable (Task, Task&lt;T&gt;,
    /// ValueTask, ValueTask&lt;T&gt;), returns the underlying Task; otherwise
    /// null. ValueTasks are converted with AsTask().
    /// </summary>
    public static Task? AsTask(object? result)
    {
        switch (result)
        {
            case null:
                return null;
            case Task task:
                return task;
        }

        var type = result.GetType();
        if (!type.IsGenericType && type != typeof(ValueTask))
            return null;

        var name = type.Namespace == "System.Threading.Tasks" ? type.Name : null;
        if (name != "ValueTask" && name != "ValueTask`1")
            return null;
        try
        {
            // ValueTask / ValueTask<T> both expose AsTask().
            return type.GetMethod("AsTask", BindingFlags.Public | BindingFlags.Instance,
                null, Type.EmptyTypes, null)?.Invoke(result, null) as Task;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// The completed task's result and its declared element type, plus any
    /// fault. For a non-generic Task the value is null. The fault is
    /// unwrapped from AggregateException to the first inner exception, the
    /// same shape a synchronous throw would have produced.
    /// </summary>
    public static (object? Value, Type? ValueType, Exception? Exception) Unwrap(Task task)
    {
        if (task.IsFaulted)
        {
            var ex = task.Exception?.InnerExceptions.Count == 1
                ? task.Exception.InnerExceptions[0]
                : task.Exception;
            return (null, null, ex);
        }
        if (task.IsCanceled)
            return (null, null, new TaskCanceledException(task));

        // The runtime type is often a Task<T> subclass (e.g. the async state
        // machine box), so find Task<T> by walking the base chain.
        for (var t = task.GetType(); t != null && t != typeof(object); t = t.BaseType)
        {
            if (!t.IsGenericType || t.GetGenericTypeDefinition() != typeof(Task<>))
                continue;
            // Task<VoidTaskResult> is the boxed form of a non-generic await;
            // it has no meaningful value.
            var elementType = t.GetGenericArguments()[0];
            if (elementType.Name == "VoidTaskResult")
                return (null, null, null);
            var value = t.GetProperty("Result")?.GetValue(task);
            return (value, value?.GetType() ?? elementType, null);
        }
        return (null, null, null);
    }
}
