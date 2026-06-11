# AppMap agent for .NET

Records the execution of .NET code as [AppMap](https://appmap.io) JSON
files (format version 1.2), the same format produced by
[appmap-java](https://github.com/getappmap/appmap-java), whose
architecture this agent ports to C#.

This is a fully managed agent. The earlier
[appmap-dotnet](https://github.com/getappmap/appmap-dotnet) prototype
instrumented IL from a C++ CLR-profiler plugin (via the CLR
Instrumentation Engine), which made it Linux-only and hard to evolve.
This implementation instead mirrors what appmap-java does on the JVM —
a managed agent that rewrites methods at runtime — using
[Harmony](https://github.com/pardeike/Harmony) where the Java agent uses
Javassist.

## Layout

| Project | Role | appmap-java counterpart |
|---|---|---|
| `src/AppMap.Agent` | Config, recorder, event model, serializer, Harmony instrumentation, SQL + built-in hooks | `agent` (config / record / output / transform) |
| `src/AppMap.Attributes` | `[Labels]` attribute for application code (dependency-free) | annotation artifact (`@Labels`) |
| `src/AppMap.StartupHook` | `DOTNET_STARTUP_HOOKS` entry point | `premain` |
| `src/AppMap.AspNetCore` | HTTP server events, request recording, remote recording endpoints | servlet hooks, `RemoteRecordingManager` |
| `src/AppMap.SystemWeb` | The same for classic ASP.NET (`IHttpModule`, .NET Framework) | servlet hooks |
| `src/AppMap.Testing.Xunit` | One AppMap per xUnit test | JUnit hooks |
| `src/AppMap.Testing.NUnit` | One AppMap per NUnit test (with test_status) | TestNG hooks |
| `test/AppMap.Agent.Tests` | Serializer / config / value-capture unit tests | — |
| `examples/HelloAppMap` | Smallest possible recorded app | — |
| `examples/PetClinic` | ASP.NET Core + EF Core/SQLite web app (HTTP + SQL) | spring-petclinic |
| `harness/` | Records an unmodified real app (eShopOnWeb) and CLI-validates the maps | agent integration tests |

## Quick start

Build everything:

```sh
dotnet build AppMap.sln
```

Create `appmap.yml` in your project root, listing the namespaces to record:

```yaml
name: my-app
packages:
- path: MyApp
  exclude:
  - MyApp.Generated
```

### Console / worker process

```sh
APPMAP_RECORD_PROCESS=true \
DOTNET_STARTUP_HOOKS=/path/to/AppMap.StartupHook.dll \
dotnet run
```

One AppMap covering the whole process is written to
`tmp/appmap/process_recording/` at exit.

### ASP.NET Core

```csharp
app.UseAppMap();   // first in the pipeline
```

Or attach with **zero source changes** — no package reference, no
`UseAppMap()` call — by naming the integration assembly as a HostingStartup:

```sh
DOTNET_STARTUP_HOOKS=/path/to/AppMap.StartupHook.dll \
ASPNETCORE_HOSTINGSTARTUPASSEMBLIES=AppMap.AspNetCore \
dotnet YourApp.dll
```

`AppMap.AspNetCore` ships an `[assembly: HostingStartup]` that registers an
`IStartupFilter` prepending `UseAppMap()` for you — the .NET analog of a Java
`-javaagent` auto-registering its servlet filter. (See
`harness/fixtures/ZeroTouchWeb` for a real app recorded this way.)

Either way this records `http_server_request`/`http_server_response` events,
writes one AppMap per request to `tmp/appmap/request_recording/` (disable
with `APPMAP_RECORDING_REQUESTS=false`), and serves the remote-recording
protocol used by AppMap clients:

- `GET /_appmap/record` → `{"enabled": <bool>}`
- `POST /_appmap/record` → start (409 if already recording)
- `DELETE /_appmap/record` → stop; response body is the AppMap JSON
- `GET /_appmap/record/checkpoint` → snapshot without stopping

### Tests

```csharp
[AppMap]            // AppMap.Testing.Xunit or AppMap.Testing.NUnit
public class UserServiceTests { ... }
```

AppMaps land in `tmp/appmap/xunit/` / `tmp/appmap/nunit/`, named
`{Class}_{method}.appmap.json`. Disable test parallelization while
recording, or maps from concurrent tests will interleave (the old
prototype had the same constraint with XUnit).

### SQL

Any ADO.NET provider whose command derives from
`System.Data.Common.DbCommand` (SqlClient, Npgsql, Sqlite, MySql,
Oracle) is hooked automatically; `Execute*` calls appear as `sql_query`
events.

### .NET Framework / classic ASP.NET

`AppMap.Agent` multi-targets `net8.0` and `netstandard2.0`, so it also
runs on .NET Framework 4.6.2+, .NET Core 2.x+, and Mono.
`DOTNET_STARTUP_HOOKS` is a .NET Core 3.0+ feature; on .NET Framework
call `AppMap.AgentBootstrap.Init()` explicitly at startup (e.g. from
`Application_Start`), or just register the module below, which does it
for you. For classic ASP.NET, `AppMap.SystemWeb` provides the
`IHttpModule` equivalent of `UseAppMap()`:

```xml
<system.webServer>
  <modules>
    <add name="AppMap" type="AppMap.SystemWeb.AppMapHttpModule, AppMap.SystemWeb" />
  </modules>
</system.webServer>
```

Build with `<DebugType>portable</DebugType>` (supported since VS2017) for
source locations; classic Windows PDBs are read best-effort through the
native diasymreader binder on Windows. NativeAOT and IL-trimmed apps are
out of scope (Harmony requires a JIT).

## Configuration

`appmap.yml` is searched from the current directory upward
(`APPMAP_CONFIG_FILE` overrides). Schema, as in appmap-java:

```yaml
name: my-app          # metadata.app
appmap_dir: tmp/appmap
packages:
- path: MyApp.Services    # namespace prefix to instrument
  exclude:                # fully-qualified-name prefixes to skip
  - MyApp.Services.Internal
- path: MyApp.Domain
  methods:                # alternative: explicit allow-list with labels
  - class: .*Repository
    name: (Find|Save).*
    labels: [crud]
```

Environment variables (defaults in parentheses):

- `APPMAP_OUTPUT_DIRECTORY` — overrides `appmap_dir`
- `APPMAP_RECORDING_REQUESTS` (true) — per-request AppMaps
- `APPMAP_RECORDING_REMOTE` (true) — `/_appmap/record` endpoints
- `APPMAP_RECORD_PROCESS` (false) — whole-process recording
- `APPMAP_RECORD_PRIVATE` (false) — instrument non-public methods
- `APPMAP_RECORD_ASYNC` (true) — emit an async method's return when its
  Task completes (real elapsed and unwrapped value) rather than when the
  Task is returned
- `APPMAP_EVENT_VALUESIZE` (1024) — max captured value length
- `APPMAP_EVENT_DISABLEVALUE` (false) — never stringify values
- `APPMAP_DEFAULT_EXCLUDES` (true) — skip noise methods (`Equals`,
  `GetHashCode`, `ToString`, `CompareTo`, `Deconstruct`, `Finalize`,
  parameterless `Dispose`) and EF Core `*.Migrations` namespaces
- `APPMAP_DEBUG` (false) — agent diagnostics on stderr
- `APPMAP_DEBUG_DISABLEGIT` (false) — skip git metadata

## Labels

Labels on classMap functions are what AppMap runtime analysis rules match
on. They come from three sources, merged per method:

1. **Built-in framework hooks** — the analog of appmap-java's bundled,
   pre-labeled hooks. These methods are recorded (with labels) even though
   they are outside your packages: configuration:

   | Framework code | Label |
   |---|---|
   | `LoggerExtensions.Log*` (Microsoft.Extensions.Logging) | `log` |
   | `IAuthenticationService.AuthenticateAsync/SignInAsync/SignOutAsync` | `security.authentication` |
   | `IAuthorizationService.AuthorizeAsync` | `security.authorization` |
   | `SymmetricAlgorithm.CreateEncryptor` / `CreateDecryptor` | `crypto.encrypt` / `crypto.decrypt` |
   | `HashAlgorithm.ComputeHash[Async]` | `crypto.digest` |
   | `BinaryFormatter.Deserialize` | `deserialize.unsafe` |
   | `JsonSerializer.Deserialize`, `JsonConvert.DeserializeObject`, `XmlSerializer.Deserialize`, `DataContractSerializer.ReadObject` | `deserialize` |
   | `HttpClient.Send/SendAsync` | `http.client.request` |
   | `ISession.TryGetValue` / `Set`, `Remove`, `Clear` | `http.session.read` / `http.session.write` |
   | `IBackgroundJobClient.Create` (Hangfire) | `job.create` |

   Interface- and base-class-based rules are resolved against concrete
   implementations as assemblies load (open generics cannot be patched, so
   e.g. `SignInManager<T>` is covered via the non-generic
   `IAuthenticationService` beneath it).

2. **`[AppMap.Labels(...)]`** from the dependency-free `AppMap.Attributes`
   package — the analog of `@Labels`. On a method or a class:

   ```csharp
   using AppMap;

   [Labels("crud")]
   public Owner Add(Owner owner) { ... }
   ```

   A labeled method is recorded even when its namespace is not listed under
   packages:. The attribute is matched by full type name
   (`AppMap.LabelsAttribute`), not assembly identity.

3. **appmap.yml `methods:` entries** (see Configuration above).

## How it maps to appmap-java

- **Instrumentation.** Where the Java agent rewrites bytecode with
  Javassist at class-load time, this agent patches methods at runtime
  with Harmony: a prefix emits the `call` event, a finalizer (which runs
  on both normal and exceptional exit) emits the matching `return` event
  with `parent_id`, `elapsed`, `return_value` or `exceptions`.
  `EventTemplateRegistry` caches per-method facts at patch time so the
  hot path is cheap, like its Java namesake.
- **Recorder.** Singleton with one optional global session plus an
  `AsyncLocal` session for request recording (the Java agent uses
  `ThreadLocal`; `AsyncLocal` lets a recording follow its request across
  `await`). Events are streamed to a temp file as they happen, as the
  Java agent does, so memory does not grow with recording length; events
  mutated after being streamed (the HTTP route template, known only
  after routing) are emitted in the spec's `eventUpdates` section.
- **Source locations.** The Java agent reads `LineNumberTable` from
  bytecode; this agent reads sequence points from the portable PDB
  (`SourceLocator`), so build with `<DebugType>portable</DebugType>` to
  get `path`/`lineno` in events and the class map. Classic Windows PDBs
  fall back to the native diasymreader binder (Windows only,
  best-effort).
- **classMap.** Namespace segments become `package` nodes, nested types
  become nested `class` nodes, and only functions that produced events
  are included — same as the Java agent.

## Known limitations

- `async` methods record their `return` when the awaited `Task` completes
  (real `elapsed` and unwrapped value), but the body's resumption after
  each `await` is not yet split into separate call frames.
- Open generic methods/types are not patched (Harmony limitation).
- xUnit's `BeforeAfterTestAttribute` does not expose the test outcome,
  so `test_status` is always "succeeded" there; the NUnit integration
  reports real outcomes.
- `database_type` is inferred from the provider's type name; ADO.NET has
  no portable equivalent of JDBC's `DatabaseProductName`.
- Methods inlined by the JIT before patching, and code compiled with
  `[MethodImpl(MethodImplOptions.AggressiveInlining)]`, may be missed if
  assemblies are loaded and JIT-compiled before the agent initializes —
  prefer the startup hook over late `AgentBootstrap.Init()` calls.
