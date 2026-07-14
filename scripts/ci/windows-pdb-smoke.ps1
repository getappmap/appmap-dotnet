# Records HelloAppMap with a native (full) Windows PDB — forcing SourceLocator
# down the WindowsPdbReader/diasymreader path — and asserts the recorded map
# carries repo-relative source locations. Run from the repo root on Windows.
$ErrorActionPreference = "Stop"

Push-Location examples/HelloAppMap
try {
  dotnet build -c Release -p:DebugType=full
  if ($LASTEXITCODE -ne 0) { throw "build failed" }

  $hook = Resolve-Path "$PWD/../../src/AppMap.StartupHook/bin/Release/net8.0/AppMap.StartupHook.dll"
  $env:DOTNET_STARTUP_HOOKS = $hook
  $env:APPMAP_RECORD_PROCESS = "true"
  $env:APPMAP_DEBUG = "true"
  dotnet run -c Release --no-build
  if ($LASTEXITCODE -ne 0) { throw "run failed" }

  $map = Get-ChildItem -Recurse tmp/appmap/process_recording/*.appmap.json |
    Select-Object -First 1
  if (-not $map) { throw "no AppMap was produced" }
  $json = Get-Content $map.FullName -Raw | ConvertFrom-Json
  $locations = @()
  function Walk($node) {
    if ($node.location) { $script:locations += $node.location }
    foreach ($child in $node.children) { Walk $child }
  }
  foreach ($root in $json.classMap) { Walk $root }
  Write-Host "resolved $($locations.Count) source location(s):"
  $locations | ForEach-Object { Write-Host "  $_" }
  if ($locations.Count -eq 0) {
    throw "Windows PDB fallback produced no source locations"
  }
  # Paths must be repo-relative with forward slashes, not the build machine's
  # C:\...; that is what lets a Windows recording be queried on Linux.
  $abs = $locations | Where-Object { $_ -match '\\' -or $_ -match '^[A-Za-z]:[\\/]' }
  if ($abs) { throw "absolute/backslash path(s) leaked: $abs" }
} finally {
  Pop-Location
}
