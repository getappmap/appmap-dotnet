#!/usr/bin/env python3
"""
App-agnostic AppMap .NET agent test harness.

Records a real .NET application under the agent, then validates every produced
AppMap with the official `appmap` CLI and asserts coverage thresholds. The
point is to prove the agent works in real codebases, not just on fixtures.

Two manifest modes:

  - "tests" (default): clone a repo, run its test suite with the agent
    attached (startup hook + process recording). Method + label coverage.
    Example: harness/targets/eshoponweb.json

  - "web": build a web app, launch it with the agent attached purely through
    environment variables (DOTNET_STARTUP_HOOKS + the zero-touch
    ASPNETCORE_HOSTINGSTARTUPASSEMBLIES HostingStartup), drive HTTP requests,
    and assert HTTP + SQL coverage in the per-request AppMaps.
    Example: harness/targets/zerotouch-web.json

Usage:
    python3 harness/run.py harness/targets/eshoponweb.json
    python3 harness/run.py harness/targets/zerotouch-web.json --keep

See harness/README.md for the manifest schema.
"""
import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def log(msg):
    print(f"[harness] {msg}", flush=True)


def run(cmd, cwd, env=None, check=True):
    log(f"$ {cmd}  (cwd={cwd})")
    result = subprocess.run(cmd, cwd=cwd, env=env, shell=True)
    if check and result.returncode != 0:
        raise SystemExit(f"command failed ({result.returncode}): {cmd}")
    return result.returncode


def build_agent():
    """Build the agent's startup hook once and return its DLL path. The build
    output also contains AppMap.AspNetCore.dll, which the hook's
    assembly-resolve handler loads for the zero-touch HostingStartup."""
    log("building the AppMap agent (startup hook + AspNetCore)")
    run("dotnet build src/AppMap.StartupHook/AppMap.StartupHook.csproj -c Release",
        cwd=REPO_ROOT)
    hook = REPO_ROOT / "src/AppMap.StartupHook/bin/Release/net8.0/AppMap.StartupHook.dll"
    if not hook.exists():
        raise SystemExit(f"startup hook not found at {hook}")
    return hook


def resolve_app_dir(manifest, workdir):
    """A target is either a repo to clone or an in-repo local_path."""
    if manifest.get("local_path"):
        app_dir = (REPO_ROOT / manifest["local_path"]).resolve()
        if not app_dir.exists():
            raise SystemExit(f"local_path not found: {app_dir}")
        return app_dir
    app_dir = workdir / manifest["name"]
    if app_dir.exists():
        shutil.rmtree(app_dir)
    ref = manifest.get("ref", "main")
    run(f"git clone --depth 1 --branch {ref} {manifest['repo']} {app_dir}", cwd=workdir)
    return app_dir


def write_config(workdir, manifest):
    """A generated appmap.yml scoped to the manifest's packages. Kept in the
    workdir so an in-repo fixture stays untouched."""
    packages = "".join(f"- path: {p}\n" for p in manifest["appmap_packages"])
    config = workdir / "appmap.yml"
    config.write_text(
        f"name: {manifest['name']}\nappmap_dir: tmp/appmap\npackages:\n{packages}")
    return config


def agent_env(hook, config, out_dir, extra=None):
    env = dict(os.environ)
    env.update({
        "DOTNET_CLI_TELEMETRY_OPTOUT": "1",
        "DOTNET_NOLOGO": "1",
        "APPMAP_CONFIG_FILE": str(config),
        "APPMAP_OUTPUT_DIRECTORY": str(out_dir),
        "DOTNET_STARTUP_HOOKS": str(hook),
    })
    if extra:
        env.update(extra)
    return env


# --- tests mode ------------------------------------------------------------

def record_tests(app_dir, manifest, hook, config, out_dir):
    env = agent_env(hook, config, out_dir, {"APPMAP_RECORD_PROCESS": "true"})
    # The record command (often `dotnet test`) may exit non-zero if some app
    # tests fail; that does not invalidate the maps, so don't treat it as fatal.
    run(manifest["record"], cwd=app_dir, env=env, check=False)
    return sorted(out_dir.rglob("*.appmap.json"))


# --- web mode --------------------------------------------------------------

def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def http_call(base_url, spec, timeout=15):
    url = base_url + spec["path"]
    data = None
    headers = {}
    if "json" in spec:
        data = json.dumps(spec["json"]).encode()
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, method=spec.get("method", "GET"),
                                 headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status
    except urllib.error.HTTPError as e:
        return e.code  # 404 etc. are still recorded maps


def wait_ready(base_url, path, timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(base_url + path, timeout=2) as resp:
                if resp.status < 500:
                    return True
        except Exception:
            time.sleep(0.5)
    return False


def record_web(app_dir, manifest, hook, config, out_dir, workdir):
    port = free_port()
    base_url = f"http://127.0.0.1:{port}"
    launch = (app_dir / manifest["launch"]).resolve()
    env = agent_env(hook, config, out_dir, {
        "APPMAP_RECORDING_REQUESTS": "true",
        "APPMAP_RECORDING_REMOTE": "false",
        # The zero-touch attach: the app references no AppMap package.
        "ASPNETCORE_HOSTINGSTARTUPASSEMBLIES": "AppMap.AspNetCore",
        "ASPNETCORE_URLS": base_url,
    })
    log_path = workdir / "server.log"
    # Launch from the app's own directory so the agent resolves the git root
    # (and relativizes source paths) the way a normally-run app would. Clean
    # any stale SQLite files first (a leftover -wal can fault the next run).
    for db in app_dir.glob("*.db*"):
        db.unlink()
    log(f"launching {launch.name} on {base_url} (zero-touch attach)")
    ready = False
    with open(log_path, "w") as logfile:
        proc = subprocess.Popen(f"dotnet {launch}", cwd=app_dir, env=env,
                                shell=True, stdout=logfile, stderr=subprocess.STDOUT)
        try:
            ready = wait_ready(base_url, manifest.get("ready_path", "/"), timeout=120)
            if ready:
                for spec in manifest["requests"]:
                    status = http_call(base_url, spec)
                    log(f"  {spec.get('method', 'GET')} {spec['path']} -> {status}")
                time.sleep(1)  # let the last request's map flush
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proc.kill()

    maps = sorted(out_dir.rglob("*.appmap.json"))
    # Always surface the app's own log: a request can 500 (e.g. the backing
    # service is unreachable) while the app is still "ready" and producing
    # maps, so the error only lives here. Diagnosable without a local repro.
    tail = log_path.read_text(errors="replace").splitlines()[-40:]
    log(f"--- {log_path.name} (tail) ---\n" + "\n".join(tail))
    if not ready:
        raise SystemExit("web app never became ready")
    return maps


# --- validation + coverage -------------------------------------------------

def structural_check(doc):
    problems = []
    for key in ("version", "metadata", "classMap", "events"):
        if key not in doc:
            problems.append(f"missing top-level '{key}'")
    events = doc.get("events", [])
    call_ids = {e["id"] for e in events if e.get("event") == "call"}
    for e in events:
        if e.get("event") == "return" and e.get("parent_id") not in call_ids:
            problems.append(f"return event {e['id']} has no matching call")
            break

    # Paths must be repo-relative with forward slashes, or a map recorded on
    # one machine/OS won't resolve on another (the R4 cross-platform
    # requirement). Guards against the SourceLocator regressing to absolute.
    def is_bad(p):
        return p.startswith("/") or "\\" in p or (len(p) > 1 and p[1] == ":")
    paths = [n["location"] for n in iter_classmap(doc) if n.get("location")]
    paths += [e["path"] for e in events if e.get("path")]
    bad = next((p for p in paths if is_bad(p)), None)
    if bad:
        problems.append(f"non-relative source path: {bad}")
    return problems


def iter_classmap(doc):
    def walk(node):
        yield node
        for child in node.get("children", []):
            yield from walk(child)
    for root in doc.get("classMap", []):
        yield from walk(root)


def cli_accepts(map_path):
    """The official CLI must be able to build a sequence diagram from it."""
    result = subprocess.run(
        f"appmap sequence-diagram -f json '{map_path}'",
        cwd=map_path.parent, shell=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return result.returncode == 0, result.stdout.decode(errors="replace")


def collect_functions(node, chain, out):
    chain = chain + [node["name"]]
    if node.get("type") == "function":
        out.append((".".join(chain), node.get("labels") or []))
    for child in node.get("children", []):
        collect_functions(child, chain, out)


def analyze(maps):
    stats = {
        "maps": len(maps), "events": 0, "http_events": 0, "sql_events": 0,
        "functions": set(), "labels": set(), "packages": set(), "failures": [],
    }
    for m in maps:
        doc = json.loads(m.read_text())
        problems = structural_check(doc)
        ok, output = cli_accepts(m)
        if not ok:
            last = output.strip().splitlines()[-1] if output.strip() else ""
            problems.append(f"appmap CLI rejected the map: {last}")
        if problems:
            stats["failures"].append((m.name, problems))

        for e in doc.get("events", []):
            stats["events"] += 1
            if e.get("http_server_request"):
                stats["http_events"] += 1
            if e.get("sql_query"):
                stats["sql_events"] += 1
        for root in doc.get("classMap", []):
            fns = []
            collect_functions(root, [], fns)
            for fqn, fn_labels in fns:
                stats["functions"].add(fqn)
                stats["labels"].update(fn_labels)
                parts = fqn.split(".")
                if len(parts) >= 2:
                    stats["packages"].add(".".join(parts[:2]))

    stats["functions"] = sorted(stats["functions"])
    stats["labels"] = sorted(stats["labels"])
    stats["packages"] = sorted(stats["packages"])
    return stats


def assert_thresholds(stats, manifest):
    t = manifest.get("thresholds", {})
    errors = [f"{name}: {probs}" for name, probs in stats["failures"]]

    checks = {
        "min_maps": "maps", "min_events": "events",
        "min_http_events": "http_events", "min_sql_events": "sql_events",
        "min_classmap_functions": ("functions", len),
    }
    for key, metric in checks.items():
        minimum = t.get(key)
        if minimum is None:
            continue
        value = len(stats[metric[0]]) if isinstance(metric, tuple) else stats[metric]
        name = metric[0] if isinstance(metric, tuple) else metric
        if value < minimum:
            errors.append(f"{name} {value} < required {minimum}")

    for label in t.get("expected_labels", []):
        if label not in stats["labels"]:
            errors.append(f"expected label '{label}' not found")
    return errors


def report(stats):
    print("\n=== coverage ===")
    print(f"maps:                 {stats['maps']}")
    print(f"events:               {stats['events']}")
    print(f"http_server_request:  {stats['http_events']}")
    print(f"sql_query:            {stats['sql_events']}")
    print(f"classMap functions:   {len(stats['functions'])}")
    print(f"packages touched:     {', '.join(stats['packages'])}")
    print(f"labels seen:          {', '.join(stats['labels']) or '(none)'}")


# --- determinism (R5) ------------------------------------------------------

def _norm_value(v):
    # Keep the shape (name + type); drop the runtime value and object_id,
    # which legitimately vary between runs.
    return {"name": v.get("name"), "class": v.get("class")}


def normalize_structure(doc):
    """A map reduced to its structure: event/method/SQL/HTTP shape and the
    classMap, with the volatile fields the spec allows to differ removed —
    ids, parent_ids, elapsed, timestamps, headers, object_ids, value text."""
    events = []
    for e in doc.get("events", []):
        n = {k: e[k] for k in ("event", "defined_class", "method_id", "static")
             if k in e}
        if e.get("http_server_request"):
            h = e["http_server_request"]
            n["http"] = {"method": h.get("request_method"),
                         "path": h.get("path_info"),
                         "route": h.get("normalized_path_info")}
        if e.get("http_server_response"):
            n["status"] = e["http_server_response"].get("status")
        if e.get("sql_query"):
            q = e["sql_query"]
            n["sql"] = {"sql": q.get("sql"), "db": q.get("database_type")}
        for vk in ("parameters", "message", "return_value", "receiver"):
            val = e.get(vk)
            if isinstance(val, list):
                n[vk] = [_norm_value(x) for x in val]
            elif isinstance(val, dict):
                n[vk] = _norm_value(val)
        if e.get("exceptions"):
            n["exceptions"] = [{"class": x.get("class")} for x in e["exceptions"]]
        events.append(n)

    def node(c):
        out = {"name": c.get("name"), "type": c.get("type")}
        if c.get("labels"):
            out["labels"] = sorted(c["labels"])
        if c.get("location"):
            out["location"] = c["location"]
        if c.get("children"):
            out["children"] = [node(ch) for ch in c["children"]]
        return out

    return {"events": events, "classMap": [node(r) for r in doc.get("classMap", [])]}


def request_key(doc):
    for e in doc.get("events", []):
        h = e.get("http_server_request")
        if h:
            return (h.get("request_method"), h.get("path_info"))
    return ("process", (doc.get("metadata", {}).get("recorder") or {}).get("type"))


def compare_determinism(maps_a, maps_b):
    def keyed(maps):
        out = {}
        for m in maps:
            doc = json.loads(m.read_text())
            out[request_key(doc)] = normalize_structure(doc)
        return out

    a, b = keyed(maps_a), keyed(maps_b)
    errors = []
    if set(a) != set(b):
        errors.append(f"the two runs recorded different requests: "
                      f"{sorted(set(a) ^ set(b))}")
    for k in sorted(set(a) & set(b)):
        if a[k] != b[k]:
            errors.append(f"structure differs between runs for {k}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("manifest", help="path to a target manifest JSON")
    parser.add_argument("--workdir", help="where to clone/record (default: temp)")
    parser.add_argument("--keep", action="store_true", help="keep the workdir")
    parser.add_argument("--hook", help="prebuilt AppMap.StartupHook.dll (skips agent build)")
    parser.add_argument("--determinism", action="store_true",
                        help="record a web target twice and assert identical structure")
    args = parser.parse_args()

    manifest = json.loads(Path(args.manifest).read_text())
    mode = manifest.get("mode", "tests")
    workdir = Path(args.workdir) if args.workdir else Path(tempfile.mkdtemp(prefix="appmap-harness-"))
    workdir.mkdir(parents=True, exist_ok=True)
    out_dir = workdir / "appmap"
    if out_dir.exists():
        shutil.rmtree(out_dir)
    log(f"target={manifest['name']} mode={mode} workdir={workdir}")

    try:
        hook = Path(args.hook) if args.hook else build_agent()
        app_dir = resolve_app_dir(manifest, workdir)
        for cmd in manifest.get("prep", []):
            run(cmd, cwd=app_dir, check=False)
        run(manifest["build"], cwd=app_dir)
        config = write_config(workdir, manifest)

        if mode == "web":
            maps = record_web(app_dir, manifest, hook, config, out_dir, workdir)
        else:
            maps = record_tests(app_dir, manifest, hook, config, out_dir)
        log(f"produced {len(maps)} AppMap(s)")
        if not maps:
            raise SystemExit("no AppMaps were produced")

        stats = analyze(maps)
        report(stats)
        errors = assert_thresholds(stats, manifest)
        if errors:
            print("\n=== FAILURES ===")
            for e in errors:
                print(f"  - {e}")
            raise SystemExit(f"\nharness failed: {len(errors)} problem(s)")
        print("\nharness passed: all maps valid and coverage thresholds met.")

        if args.determinism:
            if mode != "web":
                raise SystemExit("--determinism is supported for web targets only")
            out_dir2 = workdir / "appmap2"
            if out_dir2.exists():
                shutil.rmtree(out_dir2)
            log("recording a second time to check determinism")
            maps2 = record_web(app_dir, manifest, hook, config, out_dir2, workdir)
            diffs = compare_determinism(maps, maps2)
            if diffs:
                print("\n=== DETERMINISM FAILURES ===")
                for d in diffs:
                    print(f"  - {d}")
                raise SystemExit(f"\nharness failed: {len(diffs)} determinism diff(s)")
            print(f"determinism: all {len(maps)} maps reproduced identically "
                  "(modulo ids, timestamps, durations, and values).")
    finally:
        if not args.keep:
            shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    main()
