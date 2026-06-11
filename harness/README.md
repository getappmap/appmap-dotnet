# AppMap .NET agent test harness

A deeper-than-fixtures check that the agent works in real, **unmodified**
codebases: it records a .NET application (or its test suite) under the agent,
then validates every produced AppMap with the official
[`appmap`](https://www.npmjs.com/package/@appland/appmap) CLI and asserts
coverage thresholds. It is the .NET analog of running appmap-java against a
large reference app.

Two modes:

- **`tests`** — clone a repo and record its test suite (method + label
  coverage). Default target: Microsoft's
  [eShopOnWeb](https://github.com/dotnet-architecture/eShopOnWeb) reference
  app, recorded with zero source changes.
- **`web`** — launch a web app and record live HTTP traffic (HTTP + SQL
  coverage). Default target: an in-repo fixture that **references no AppMap
  package at all** — the agent attaches purely through environment variables.

## Run it

```sh
npm install -g @appland/appmap --ignore-scripts
python3 harness/run.py harness/targets/eshoponweb.json     # tests mode
python3 harness/run.py harness/targets/zerotouch-web.json  # web mode
```

`tests` mode builds the agent, clones the target, runs its `prep`/`build`
steps, runs the `record` command with the agent attached
(`DOTNET_STARTUP_HOOKS` + `APPMAP_RECORD_PROCESS`), then validates and reports:

```
=== coverage ===
maps:                 2
events:               1020
classMap functions:   125
packages touched:     Microsoft.eShopWeb, System.Security
labels seen:          crypto.digest

harness passed: all maps valid and coverage thresholds met.
```

`web` mode reports HTTP and SQL event counts instead:

```
=== coverage ===
maps:                 5
events:               52
http_server_request:  5
sql_query:            4
classMap functions:   6
packages touched:     ZeroTouchWeb.Widget, ZeroTouchWeb.WidgetContext, ZeroTouchWeb.WidgetService
```

Flags: `--workdir DIR` (where to clone/record), `--keep` (don't delete it),
`--hook PATH` (use a prebuilt `AppMap.StartupHook.dll`, skipping the agent
build).

## Zero-touch attach (web mode)

The web fixture (`fixtures/ZeroTouchWeb`) is an ordinary ASP.NET Core +
EF Core/SQLite app. It has **no** `using AppMap`, no package reference, and
never calls `app.UseAppMap()`. The harness records it by setting only:

```sh
DOTNET_STARTUP_HOOKS=…/AppMap.StartupHook.dll      # method + SQL instrumentation
ASPNETCORE_HOSTINGSTARTUPASSEMBLIES=AppMap.AspNetCore   # prepends UseAppMap() via IStartupFilter
APPMAP_RECORDING_REQUESTS=true
```

`AppMap.AspNetCore` carries `[assembly: HostingStartup(...)]`; ASP.NET Core
loads it (resolved from the agent directory by the startup hook's
assembly-resolve handler) and runs an `IStartupFilter` that inserts the
AppMap middleware at the front of the pipeline. This is the .NET equivalent
of a Java `-javaagent` auto-registering its servlet filter — drop the agent
next to any ASP.NET Core app and get per-request HTTP→method→SQL maps with no
code change.

## What it validates

For every AppMap produced:

- **Structural**: the `version` / `metadata` / `classMap` / `events` keys
  are present and every `return` event has a matching `call` (no dangling
  frames).
- **Tooling**: the official `appmap sequence-diagram` accepts the map
  (this is what caught the `class_map` vs `classMap` bug originally).

Then, across all maps, it asserts the manifest's coverage thresholds:
total events, distinct `classMap` functions, and that each expected label
was produced (e.g. `crypto.digest` from eShop's password hashing).

## Adding a target

A target manifest is JSON:

```json
{
  "name": "eShopOnWeb",
  "repo": "https://github.com/dotnet-architecture/eShopOnWeb.git",
  "ref": "main",
  "prep": ["echo '{\"version\":\"1.0\",\"libraries\":[]}' > src/Web/libman.json"],
  "appmap_packages": ["Microsoft.eShopWeb"],
  "build": "dotnet build tests/UnitTests/UnitTests.csproj -c Release && ...",
  "record": "dotnet test tests/UnitTests/UnitTests.csproj -c Release --no-build ; ...",
  "thresholds": {
    "min_maps": 1,
    "min_events": 150,
    "min_classmap_functions": 40,
    "expected_labels": ["crypto.digest"]
  }
}
```

Common fields:

- `name` — used for the generated `appmap.yml`.
- `local_path` (in-repo target) or `repo`/`ref` (cloned target).
- `prep` runs before `build` (non-fatal). eShopOnWeb needs its `libman.json`
  neutralized because the client-side JS restore reaches cdnjs, which
  CI/sandbox networks often block — server code is unaffected.
- `appmap_packages` become the `packages:` of a generated `appmap.yml`.
- `thresholds` — any of `min_maps`, `min_events`, `min_http_events`,
  `min_sql_events`, `min_classmap_functions`, `expected_labels`.

`tests` mode adds `record` (run with the agent attached; a non-zero exit
from a failing app test does not fail the harness — only invalid maps or
unmet thresholds do).

`web` mode adds `launch` (the built DLL to run), `ready_path` (polled until
the server answers), and `requests` (the HTTP calls to drive, each
`{method, path, json?}`). See `targets/zerotouch-web.json`.

## Known scope

- `tests` mode records via process recording, so it captures instrumented
  methods and label coverage but not HTTP/SQL (eShopOnWeb's tests use the
  in-memory provider). `web` mode covers HTTP + SQL. A natural extension is
  a `web` target pointed at a large real app (eShop with Postgres), which
  needs container infrastructure in CI.
- Findings-level (RCA-style) assertions — planting an N+1 / unauthenticated
  endpoint / logged secret and asserting AppMap's analysis flags it — build
  on the events and labels this harness already exercises and are the next
  step up in depth.
