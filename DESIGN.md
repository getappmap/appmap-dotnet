# AppMap .NET Agent — Design Spec

> Status: working prototype, validated end-to-end (unit tests + the official
> `@appland/appmap` CLI + a real Microsoft reference app + a live web app) on
> Linux and Windows CI. This document is written for review and for a possible
> move into the AppMap org repo (`getappmap/appmap-dotnet`).

## 1. Purpose

A runtime agent that records the execution of a .NET application into the
[AppMap JSON](https://github.com/getappmap/appmap) format — the .NET
counterpart of `appmap-java`. It produces the same artifact (events +
classMap + metadata) the rest of the AppMap toolchain already consumes
(CLI, analysis rules, sequence diagrams, the VS Code/IntelliJ extensions).

The design goal throughout is **parity of behaviour with `appmap-java`**, so
that someone who knows the Java agent can predict what this one does. Where a
mechanism has no .NET equivalent, the closest idiomatic one was chosen and
the trade-off documented.

### Goals
- Emit AppMaps the official `@appland/appmap` CLI accepts unmodified.
- Record the four `appmap-java` modes: process, request, remote, test.
- Instrument by `appmap.yml` package config, with built-in framework hooks
  and `[Labels]` attributes feeding the analysis label taxonomy.
- Attach to an application with **no source changes** (parity with
  `-javaagent`).
- Support modern .NET (net8.0) and legacy (.NET Framework 4.6.2+ via
  netstandard2.0).

### Non-goals (current)
- NativeAOT / IL-trimmed apps (Harmony needs a JIT).
- Splitting an `async` method body into per-`await` frames (see §11).
- Bytecode-level rewrite at load time (we patch at runtime instead — §4).

## 2. Dependencies & licensing

| Dependency | Use | License |
|---|---|---|
| [Lib.Harmony](https://github.com/pardeike/Harmony) 2.3.3 | runtime method patching | MIT |
| YamlDotNet 15.1.2 | `appmap.yml` parsing | MIT |
| System.Text.Json / System.Reflection.Metadata | output + PDB reading (netstandard2.0 only; inbox on net8.0) | MIT |

The agent itself is `PackageLicenseExpression = MIT`. No GPL/LGPL in the
dependency graph. Harmony is the only non-trivial third-party runtime
dependency and is the main thing to vet (see §12).

## 3. Project layout

12 projects. Each maps to an `appmap-java` concept:

| Project | Responsibility | `appmap-java` analog |
|---|---|---|
| `AppMap.Agent` | Core: config, instrumentation, recorder, output, source locations | `com.appland.appmap.*` |
| `AppMap.Attributes` | Dependency-free `[Labels]` attribute | `@Labels` |
| `AppMap.StartupHook` | `DOTNET_STARTUP_HOOKS` entry point | `premain` |
| `AppMap.AspNetCore` | HTTP middleware, remote-recording endpoints, **zero-touch HostingStartup** | servlet filter |
| `AppMap.SystemWeb` | `IHttpModule` for classic ASP.NET (.NET Framework) | servlet filter (legacy) |
| `AppMap.Testing.Xunit` / `.NUnit` | `[AppMap]` per-test recording | JUnit integration |
| `AppMap.Agent.Tests` | unit tests (42) | agent unit tests |
| `harness/` (Python + fixture) | record real apps, validate with the CLI | `appmap-java`'s integration harness |

## 4. Instrumentation

**Mechanism.** Where the Java agent rewrites bytecode with Javassist at
class-load time, this agent patches methods at runtime with Harmony: a
**prefix** emits the `call` event; a **finalizer** (runs on both normal and
exceptional return) emits the matching `return` event with `parent_id`,
`elapsed`, and `return_value` or `exceptions`.

**Why runtime patching, not a profiler/ICorProfiler.** A profiler-based
IL-rewriter would be more powerful but is a native component per-architecture
and a much larger surface. Harmony gives load-time-agnostic patching in pure
managed code, multi-targets to .NET Framework, and is what the original
prototype reached for. The cost is the set of BCL methods Harmony can't
patch (intrinsics, `[RequiresDynamicCode]`), handled explicitly below.

**Selection pipeline** (`Instrumentor`):
1. `AppMapConfig` parses `appmap.yml` → package prefixes, exclude lists,
   explicit `methods:` rules with labels.
2. On assembly load (and for already-loaded assemblies), each candidate
   type's methods are matched against the config.
3. Matched methods are patched via `HookPatcher`, which registers a
   per-method `EventTemplate` (defined_class, method_id, static, params,
   labels, source location) **at patch time** so the hot path only fills in
   values — the same "compute once" strategy as the Java agent.

**Robustness (the part worth scrutinising).** Real BCL usage in eShopOnWeb
surfaced that Harmony throws `InvalidProgramException` when asked to patch
certain methods (JIT intrinsics like `RandomNumberGenerator.GetBytes`,
`[RequiresDynamicCode]` helpers like `JsonSerializer.Deserialize`,
abstract-base crypto signatures). The agent:
- Skips methods with no IL body and open generics up front.
- Wraps every `harmony.Patch` in a try/catch: a failure leaves the method
  uninstrumented (no corruption — the exception is at patch time, before any
  IL is applied) and logs a calm "skipping (not instrumentable)" at debug.
- Built-in hook rules that proved consistently un-patchable were removed
  rather than left to spam (see `BuiltinHooks` comments + `BACKLOG.md`).

Harmony's robustness on arbitrary BCL methods is the historical worry with
this approach. The current design is defensive-by-default: patch failures are
contained per-method, and **the agent must never prevent the host app from
starting** (`AgentBootstrap` wraps the whole init in a catch).

**SQL** (`SqlHooks`). Every concrete `System.Data.Common.DbCommand`
implementation found in loaded provider assemblies has its `Execute*` /
`Execute*Async` overrides patched to emit `sql_query` events — the analog of
the JDBC `Statement` hooks. New provider assemblies are caught via an
`AssemblyLoad` handler. `database_type` is inferred from the provider type
name (ADO.NET has no portable `DatabaseProductName`).

**Built-in framework hooks** (`BuiltinHooks`). Pre-labeled rules for logging,
auth, crypto, deserialization, HTTP client, session, Hangfire — recorded
with labels even outside the user's packages. Interface/base-class rules are
resolved against concrete implementations as assemblies load (open generics
can't be patched, so e.g. `SignInManager<T>` is covered via the non-generic
`IAuthenticationService` beneath it). Full table in `README.md` §Labels.

## 5. Recording model (`Recorder`, `Recording`)

A singleton `Recorder` holds one optional **global** session plus an
`AsyncLocal` **request** session. The Java agent uses `ThreadLocal`;
`AsyncLocal` is the .NET fix for "a recording must follow its request across
`await`". Events stream to a temp file as they happen, so memory doesn't grow
with recording length (as in the Java agent). Events mutated after being
streamed — the HTTP route template, known only after routing — are emitted in
the spec's `eventUpdates` section.

Four modes (all `appmap-java` parity):
- **Process** — `APPMAP_RECORD_PROCESS=true`, written at process exit.
- **Request** — one AppMap per HTTP request (`AppMapMiddleware`).
- **Remote** — `GET/POST/DELETE /_appmap/record` (`RemoteRecordingMiddleware`).
- **Test** — `[AppMap]` on xUnit/NUnit classes, one map per test.

## 6. Async handling

When `APPMAP_RECORD_ASYNC=true` (default), an `async` method whose return
type is a `Task`/`ValueTask` records its `return` event when the returned
task **completes**, not when the task is handed back — so `elapsed` is the
real wall-clock duration and `return_value` is the unwrapped result. Implemented
in `AsyncResult` + the method finalizer attaching a continuation. (Not yet
done: splitting the post-`await` resumption into separate frames — §11.)

## 7. Output & CLI compatibility

`AppMapSerializer` writes the AppMap document: `version`, `metadata`
(language, client, git via `GitMetadata`, recorder), `classMap`, `events`.
classMap is built from events only (functions that produced no event are
omitted) with namespace→`package`, nested type→`class`, same as Java.

**Compatibility is enforced, not assumed.** The harness runs
`appmap sequence-diagram` over every generated map and fails if the official
CLI rejects it. (This is what caught an early `class_map` vs `classMap`
casing bug.)

## 8. Zero-touch attach (HostingStartup) — newest piece

Goal: attach to an **unmodified** ASP.NET Core app — no package reference, no
`app.UseAppMap()` — matching the `-javaagent` experience.

```
DOTNET_STARTUP_HOOKS=…/AppMap.StartupHook.dll          # instrumentation (method + SQL)
ASPNETCORE_HOSTINGSTARTUPASSEMBLIES=AppMap.AspNetCore  # middleware auto-registration
```

How it works:
1. `AppMap.AspNetCore` carries `[assembly: HostingStartup(typeof(AppMapHostingStartup))]`.
   ASP.NET Core loads any assembly named in `ASPNETCORE_HOSTINGSTARTUPASSEMBLIES`
   and runs its `IHostingStartup.Configure` before the app's own startup.
2. `AppMapHostingStartup` registers an `IStartupFilter` that prepends
   `app.UseAppMap()` to the pipeline — so the middleware brackets the whole
   request, exactly as a manual first-line `UseAppMap()` would.
3. **Assembly resolution.** The app has no reference to `AppMap.AspNetCore`,
   so ASP.NET Core's `Assembly.Load("AppMap.AspNetCore")` would normally
   fail. The startup hook (already loaded, very early) installs an
   `AssemblyResolve` handler that serves AppMap assemblies from the agent
   directory. Because the hook ships `AppMap.AspNetCore.dll` alongside itself,
   one directory is the entire agent deployment.
4. `UseAppMap()` is idempotent (guards via `app.Properties`) so the
   HostingStartup and a hand-written call can't double-register.

This is the .NET-idiomatic equivalent of an APM agent's auto-instrumentation,
using only first-class framework extension points (no IL injection into the
app, no profiler).

## 9. Source locations

`SourceLocator` reads method sequence points from the **portable PDB** via
`System.Reflection.Metadata` to populate `path`/`lineno` on events and
classMap. Build with `<DebugType>portable</DebugType>`. Classic **Windows
PDBs** fall back to the native `diasymreader` COM binder
(`WindowsPdbReader`, Windows-only, best-effort) — this COM interop path is
validated on a real Windows runner in CI (it resolves locations from a native
`full` PDB; the assertion is hard-failing).

PDBs embed the **absolute build-machine path**, so `SourceLocator` relativizes
every path against the repo root (git root, then the appmap.yml directory) and
normalizes to forward slashes — like `appmap-java`. Without this a map
recorded on Windows (`C:\agent\work\repo\src\X.cs`) would not resolve against
the same repo checked out on Linux; with it, both sides see
`src/X.cs`. The harness asserts no map carries an absolute or backslash path.

## 10. Test harness (`harness/`)

App-agnostic, manifest-driven. Records real apps under the agent and
validates every map with the official CLI plus coverage thresholds. Two modes:

- **`tests` mode** — clone a repo, run its test suite with the agent attached
  (process recording, scoped by namespace). Method + label coverage.
  Demonstrated target: **Microsoft's eShopOnWeb**, unmodified →
  **2 maps, ~1020 events, 125 classMap functions, `crypto.digest` label**,
  all CLI-valid.
- **`web` mode** — build a web app, launch it with the zero-touch attach
  (env vars only), drive HTTP requests, assert HTTP + SQL coverage.
  Demonstrated target: `fixtures/ZeroTouchWeb`, an ASP.NET Core + EF
  Core/SQLite app **with no AppMap reference** → **5 maps, 5
  `http_server_request`, 4 `sql_query`**, capturing full HTTP→method→SQL
  chains (`GET /widgets` → `WidgetService.All` → SQLite `SELECT`).

Both run as jobs in `.github/workflows/harness.yml`.

## 11. Known limitations (honest list)

- `async` records the `return` at task completion, but post-`await`
  resumption is not split into separate call frames.
- Open generic methods/types are not patched (Harmony limitation).
- xUnit's `BeforeAfterTestAttribute` doesn't expose the test outcome, so
  `test_status` is always "succeeded" there; NUnit reports real outcomes.
- `database_type` is inferred from the provider type name.
- `random.secure` / `crypto.sign/verify` labels are deferred: their methods
  on abstract BCL crypto bases are intrinsic-backed and currently un-patchable
  (need concrete-type targeting). See `BACKLOG.md`.
- NativeAOT / full IL-trimming unsupported.

## 12. Review guide — what to scrutinise

For a code review, the load-bearing / highest-risk areas, in order:

1. **`Instrumentation/HookPatcher.cs` + `MethodHooks.cs`** — the Harmony
   prefix/finalizer, `__state` threading, re-entrancy guard (`inHook`),
   exception-path correctness. The hot path's allocation/locking.
2. **`Instrumentation/BuiltinHooks.cs` + `SqlHooks.cs`** — which BCL methods
   are patched, and the graceful-skip behaviour for un-patchable ones. The
   `AssemblyLoad` scanning cost on large apps.
3. **`Record/Recorder.cs` + `Recording.cs`** — thread-safety of the global
   vs `AsyncLocal` sessions, the streaming writer, `eventUpdates`.
4. **`Util/SourceLocator.cs` + `WindowsPdbReader.cs`** — the PDB readers,
   especially the hand-rolled diasymreader COM vtable.
5. **Zero-touch attach** (`AppMap.AspNetCore/AppMapHostingStartup.cs`,
   `StartupHook/StartupHook.cs`) — assembly-resolution correctness and the
   idempotency guard.
6. **Value capture** (`Output/Value.cs`) — stringification limits, cycles,
   and not triggering side effects in user `ToString()`.

## 13. Porting notes (to `getappmap/appmap-dotnet`)

- The csproj already declares `PackageId=AppMap.Agent`, `MIT`, and
  `RepositoryUrl=https://github.com/getappmap/appmap-dotnet` — it's structured
  to drop in.
- The harness's Python orchestrator + the eShopOnWeb/ZeroTouchWeb targets are
  self-contained and could become the integration-test job.
- Things to harden before shipping: a broader BCL-patchability denylist
  (driven by running against more real apps), perf benchmarking of the hot
  path under load, and a decision on whether to invest in an
  `ICorProfiler`-based rewriter for the cases Harmony can't reach.
- Test coverage today is 42 unit tests + 2 CI integration jobs; a port should
  expand unit coverage around the Recorder concurrency and value capture.
