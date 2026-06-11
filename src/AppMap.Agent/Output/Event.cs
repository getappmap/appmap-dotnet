namespace AppMap.Output;

/// <summary>
/// One entry of the events array: a "call" or "return" event, possibly
/// decorated with HTTP or SQL details. Field-for-field port of
/// com.appland.appmap.output.v1.Event; serialization to snake_case JSON
/// lives in AppMapSerializer.
/// </summary>
public sealed class Event
{
    private static int nextId;

    public static int IssueId() => Interlocked.Increment(ref nextId);

    public int Id { get; init; } = IssueId();

    /// <summary>"call" or "return".</summary>
    public required string EventType { get; init; }

    public int ThreadId { get; init; } = Environment.CurrentManagedThreadId;

    public string? DefinedClass { get; set; }
    public string? MethodId { get; set; }
    public string? Path { get; set; }
    public int? LineNo { get; set; }
    public bool? Static { get; set; }

    public Value? Receiver { get; set; }
    public List<Value>? Parameters { get; set; }

    /// <summary>On return events: id of the matching call event.</summary>
    public int? ParentId { get; set; }

    public Value? ReturnValue { get; set; }
    public List<ExceptionValue>? Exceptions { get; set; }

    /// <summary>Seconds elapsed between call and return.</summary>
    public double? Elapsed { get; set; }

    public HttpServerRequest? HttpServerRequest { get; set; }
    public HttpServerResponse? HttpServerResponse { get; set; }
    public SqlQuery? SqlQuery { get; set; }

    /// <summary>HTTP request parameters (query/form), on the request call event.</summary>
    public List<Value>? Message { get; set; }
}

/// <summary>An exception attached to a return event, including its cause chain.</summary>
public sealed class ExceptionValue
{
    public required string Class { get; init; }
    public string? Message { get; init; }
    public string? Path { get; init; }
    public int? LineNo { get; init; }
    public long ObjectId { get; init; }

    /// <summary>Flattens an exception and its InnerException chain.</summary>
    public static List<ExceptionValue> ChainOf(Exception exception)
    {
        var chain = new List<ExceptionValue>();
        for (Exception? e = exception; e != null; e = e.InnerException)
        {
            string? path = null;
            int? lineno = null;
            try
            {
                var frame = new System.Diagnostics.StackTrace(e, fNeedFileInfo: true).GetFrame(0);
                path = frame?.GetFileName();
                var line = frame?.GetFileLineNumber() ?? 0;
                if (line > 0)
                    lineno = line;
            }
            catch
            {
                // Source info is best-effort.
            }
            chain.Add(new ExceptionValue
            {
                Class = Value.TypeName(e.GetType()),
                Message = e.Message,
                Path = path,
                LineNo = lineno,
                ObjectId = System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(e),
            });
        }
        return chain;
    }
}

public sealed class HttpServerRequest
{
    public required string RequestMethod { get; init; }
    public required string PathInfo { get; init; }
    public string? NormalizedPathInfo { get; set; }
    public string? Protocol { get; init; }
    public Dictionary<string, string>? Headers { get; init; }
}

public sealed class HttpServerResponse
{
    public required int Status { get; init; }
    public Dictionary<string, string>? Headers { get; init; }
}

public sealed class SqlQuery
{
    public required string Sql { get; init; }
    public string? DatabaseType { get; init; }
}
