# Backlog

## 1. Deep agent test harness (parity with appmap-java's)

Status: **shipped** in `harness/` — an app-agnostic, manifest-driven harness
that records real, unmodified .NET apps under the agent and validates every
map with the official CLI plus coverage thresholds. Two modes, both wired
into `.github/workflows/harness.yml`:

- **`tests` mode** — method + label coverage. Demonstrated against
  Microsoft's eShopOnWeb (2 maps, ~1k events, 125 classMap functions,
  `crypto.digest` label).
- **`web` mode** — ✅ **HTTP + SQL coverage**, via the zero-touch
  HostingStartup attach. Demonstrated against an in-repo fixture
  (`fixtures/ZeroTouchWeb`) that references no AppMap package: 5 maps,
  5 `http_server_request`, 4 `sql_query`, full HTTP→method→SQL chains. The
  agent attaches with only `DOTNET_STARTUP_HOOKS` +
  `ASPNETCORE_HOSTINGSTARTUPASSEMBLIES=AppMap.AspNetCore`.

Remaining depth to add:

- **Runtime code analysis (RCA-style) assertions**: plant a deliberate
  N+1, an unauthenticated endpoint, a logged secret, and a
  `BinaryFormatter.Deserialize`, then assert AppMap's analysis flags each.
  Builds on the events + label taxonomy the harness already exercises.
  (`@appland/scanner` provides the rules: `n-plus-one-query`, `secret-in-log`,
  `deserialization-of-untrusted-data`, etc.)
- **A large real app in `web` mode**: eShop/nopCommerce against a real
  relational DB (Postgres/SQL Server), which needs container infra in CI.

## 1a. Study gap-analysis follow-ups (R1–R8)

From a live zero-touch run against eShopOnWeb on SQL Server 2022. Done:

- ✅ **SqlHooks partial-load fix** — `Assembly.GetTypes()` threw
  `ReflectionTypeLoadException` on `Microsoft.Data.SqlClient` (1 unloadable
  type on Linux) and the wholesale catch discarded all 644 loadable types,
  including `SqlCommand` → **0 SQL captured**. Now patches the loadable
  subset (0 → 36 `sql_query` against SQL Server). SQLite-only harness is why
  it slipped; **add a SQL Server web-mode target** to close the gap.
- ✅ **R4 Gap A — relative source paths**. `SourceLocator` now emits paths
  relative to the repo root with forward slashes; harness guards it.
- ✅ **SQL Server `web` target** (`fixtures/SqlServerWeb` +
  `targets/sqlserver-web.json` + a `sql-server-web` CI job with an mssql
  service container). Regression-guards the SqlHooks fix against the actual
  provider; the partial-load extraction is also unit-tested directly so the
  guard holds even where the load doesn't fault.

- ✅ **R4 Gap B — cross-platform acceptance test**. CI records a map on the
  Windows runner, uploads it, and a Linux job (`cross-platform-query` in
  `ci.yml`) indexes + queries it and asserts every source path resolves
  against the Linux checkout. Proves record-on-Windows / query-on-Linux end
  to end, unblocked by Gap A.

Open:

- **R1** — IIS / `System.Web` `IHttpModule` smoke on a Windows runner.
- **R5** — determinism assertion: two identical replays → identical map
  structure modulo volatile fields (ids, timestamps, durations).

Original scope notes (for the deeper passes):

- **Read the appmap-java harness first** (`appmap-java`'s `agent/test`,
  its Spring PetClinic smoke tests, and the BATS/CI integration jobs) and
  match its coverage point for point: process/request/remote/test
  recording, classMap correctness, label coverage, exception capture,
  SQL capture, HTTP normalization.
- **Clone a large public .NET application** (candidates:
  `dotnet/eShop`, `nopSolutions/nopCommerce`, `OrchardCMS/OrchardCore`,
  `abpframework/abp` samples) and run the agent against its full test
  suite and seeded HTTP scenarios in CI.
- **Validate output against the official tooling**, not just our own
  serializer tests: `@appland/appmap` CLI `index`/`stats`/
  `sequence-diagram` must accept every generated map (this caught the
  `classMap` key bug); fail CI on validation errors.
- **Runtime code analysis (RCA-style) assertions**: run AppMap's analysis
  rules over the generated maps and assert expected findings appear —
  e.g. introduce a deliberate N+1 query, an unauthenticated endpoint, a
  logged secret, and a `BinaryFormatter.Deserialize` call in the fixture
  app, then assert each is flagged. This exercises the label taxonomy
  end to end the way AppMap's own scanner does.
- **Matrix**: Debug/Release, with/without portable PDBs, xUnit + NUnit
  recorders, SQLite + SQL Server providers, startup-hook vs
  `UseAppMap()` attach.

## 2. Older .NET support — DONE (validation on Windows outstanding)

- ~~Multi-target `AppMap.Agent` to `net8.0;netstandard2.0`~~ done
- ~~`AppMap.SystemWeb`: `IHttpModule` port of the middleware~~ done
  (compiles against net472; needs a smoke test on a real IIS/Windows box)
- ~~Document `AgentBootstrap.Init()` attach for .NET Framework~~ done
- ~~Windows PDB fallback via diasymreader~~ done — best-effort COM
  binder. Now exercised by the `windows` job in `.github/workflows/ci.yml`,
  which builds HelloAppMap with `<DebugType>full</DebugType>` (a native
  PDB) and asserts source locations resolve. Watch the first Windows CI
  run; if the COM vtable layout is off, that job fails with "no source
  locations" and needs a fix iteration on a real Windows box.
- ~~Multi-target the xUnit/NUnit integrations to `net462`~~ done
- Non-goals: NativeAOT / IL-trimmed apps (Harmony requires a JIT)

## 3. Recording quality

- ~~Streaming serialization to a temp file during recording~~ done,
  with `eventUpdates` for post-hoc mutations (route templates)
- ~~Bundled default excludes for common noise~~ done
  (`APPMAP_DEFAULT_EXCLUDES`)
- ~~Record async completions (real elapsed + unwrapped value)~~ done
  (`APPMAP_RECORD_ASYNC`). Still future: split each `await` segment into
  separate call frames (needs `MoveNext` state-machine instrumentation).
- ~~More built-in labels~~ added `http.client.request` and XML
  `deserialize`. `random.secure` and `crypto.sign/verify` were reverted:
  their methods on the abstract BCL crypto bases
  (`RandomNumberGenerator`, `AsymmetricAlgorithm`/`ECDsa`/`DSA`) are
  intrinsic-backed and make Harmony throw `InvalidProgramException` at
  patch time (caught, but noisy). Re-add them by targeting the concrete
  algorithm types instead of the abstract base. Still wanted: `secret`,
  `job.cancel`, `crypto.set_key`.
