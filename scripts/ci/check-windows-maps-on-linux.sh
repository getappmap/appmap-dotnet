#!/usr/bin/env bash
# Cross-platform acceptance: index + query a Windows-recorded AppMap on Linux
# and assert every source path resolves against this (Linux) checkout.
# Usage: scripts/ci/check-windows-maps-on-linux.sh <maps-dir>
set -euo pipefail

maps_dir="${1:?usage: $0 <maps-dir>}"

appmap index --appmap-dir "$maps_dir"
map=$(find "$maps_dir" -name '*.appmap.json' | head -1)
test -n "$map" || { echo "no map in $maps_dir"; exit 1; }
appmap sequence-diagram -f json "$map" > /dev/null
echo "queried $map on Linux"

python3 - "$maps_dir" <<'PY'
import json, glob, os, sys
maps = glob.glob(os.path.join(sys.argv[1], '**', '*.appmap.json'), recursive=True)
assert maps, "no maps found"
doc = json.load(open(maps[0]))
locs = set()
def walk(n):
    if n.get('location'):
        locs.add(n['location'].split(':')[0])
    for c in n.get('children', []):
        walk(c)
for r in doc.get('classMap', []):
    walk(r)
assert locs, "Windows map carried no source locations"
missing = [p for p in locs if not os.path.isfile(p)]
print("locations:", sorted(locs))
assert not missing, f"paths do not resolve on Linux: {missing}"
print(f"OK: {len(locs)} Windows-recorded path(s) resolve against the Linux checkout")
PY
