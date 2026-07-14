#!/usr/bin/env bash
# Smoke-tests the appmap-dotnet runner: launches the PetClinic example (which
# has no agent reference and no UseAppMap call) via the runner, drives a
# request, and asserts a per-request map with HTTP + SQL events was recorded.
# Run from the repo root.
set -euo pipefail

dotnet build src/AppMap.Runner/AppMap.Runner.csproj -c Release
dotnet build examples/PetClinic/PetClinic.csproj -c Release

runner=src/AppMap.Runner/bin/Release/net8.0/AppMap.Runner.dll
cd examples/PetClinic
rm -rf tmp petclinic.db*

port=$((20000 + RANDOM % 20000))
ASPNETCORE_URLS="http://127.0.0.1:$port" APPMAP_RECORDING_REMOTE=false \
  dotnet "../../$runner" -- dotnet bin/Release/net8.0/PetClinic.dll > runner-smoke.log 2>&1 &
pid=$!
trap 'kill $pid 2>/dev/null || true' EXIT

for _ in $(seq 1 30); do
  curl -fs "http://127.0.0.1:$port/owners" >/dev/null 2>&1 && break
  sleep 1
done
curl -fs "http://127.0.0.1:$port/owners" >/dev/null
sleep 1
kill $pid 2>/dev/null || true
wait $pid 2>/dev/null || true

python3 - <<'PY'
import json, glob, sys
maps = sorted(glob.glob('tmp/appmap/request_recording/*owners*.appmap.json'))
if not maps:
    print(open('runner-smoke.log').read()[-2000:])
    sys.exit("runner smoke: no per-request map was produced")
doc = json.load(open(maps[0]))
http = any(e.get('http_server_request') for e in doc['events'])
sql = any(e.get('sql_query') for e in doc['events'])
assert http, "no http_server_request event"
assert sql, "no sql_query event"
print(f"runner smoke OK: {maps[0]} has HTTP + SQL events")
PY
rm -rf tmp petclinic.db* runner-smoke.log
