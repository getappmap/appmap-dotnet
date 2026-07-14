# Validates the classic-ASP.NET (System.Web) IHttpModule on a real IIS host
# (IIS Express): deploys harness/fixtures/SystemWebApp with the agent DLLs in
# bin/, injects the binding redirects the netstandard2.0 agent needs under
# .NET Framework, records a request, and asserts a per-request AppMap with an
# http_server_request event. Run from the repo root on Windows.
$ErrorActionPreference = "Stop"

$site = Join-Path ([System.IO.Path]::GetTempPath()) "appmap-systemweb-site"
if (Test-Path $site) { Remove-Item -Recurse -Force $site }
New-Item -ItemType Directory -Force (Join-Path $site "bin") | Out-Null
Copy-Item harness/fixtures/SystemWebApp/* $site -Recurse -Force
Copy-Item src/AppMap.SystemWeb/bin/Release/net472/*.dll (Join-Path $site "bin") -Force

# The agent is netstandard2.0; under .NET Framework its transitive System.*
# package assemblies need binding redirects or they throw FileLoadException at
# runtime (and the module silently records nothing). Generate them from the
# actual deployed assemblies.
$deps = ""
Get-ChildItem (Join-Path $site "bin\*.dll") | ForEach-Object {
  try {
    $an = [System.Reflection.AssemblyName]::GetAssemblyName($_.FullName)
    $tokenBytes = $an.GetPublicKeyToken()
    if ($tokenBytes -and $tokenBytes.Length -gt 0) {
      $token = ($tokenBytes | ForEach-Object { $_.ToString("x2") }) -join ""
      $deps += "<dependentAssembly><assemblyIdentity name='$($an.Name)' publicKeyToken='$token' culture='neutral' /><bindingRedirect oldVersion='0.0.0.0-$($an.Version)' newVersion='$($an.Version)' /></dependentAssembly>"
    }
  } catch {}
}
$runtime = "<runtime><assemblyBinding xmlns='urn:schemas-microsoft-com:asm.v1'>$deps</assemblyBinding></runtime>"
$cfg = Join-Path $site "web.config"
(Get-Content $cfg -Raw) -replace '</configuration>', "$runtime</configuration>" | Set-Content $cfg
Write-Host "injected $((($deps -split '</dependentAssembly>').Count) - 1) binding redirect(s)"

$env:APPMAP_CONFIG_FILE = Join-Path $site "appmap.yml"
$env:APPMAP_OUTPUT_DIRECTORY = Join-Path $site "tmp\appmap"
$env:APPMAP_DEBUG = "true"

$exe = "C:\Program Files\IIS Express\iisexpress.exe"
if (-not (Test-Path $exe)) { $exe = "C:\Program Files (x86)\IIS Express\iisexpress.exe" }
if (-not (Test-Path $exe)) { throw "IIS Express not found" }

$out = Join-Path $site "iisexpress.out.log"
$err = Join-Path $site "iisexpress.err.log"
$proc = Start-Process $exe -ArgumentList "/path:$site","/port:8088" -PassThru `
  -RedirectStandardOutput $out -RedirectStandardError $err
try {
  $served = $false
  for ($i = 0; $i -lt 30; $i++) {
    try {
      (Invoke-WebRequest "http://localhost:8088/Default.aspx" -UseBasicParsing).Content | Out-Null
      $served = $true; break
    } catch { Start-Sleep -Seconds 1 }
  }
  Start-Sleep -Seconds 1   # let the end-request hook flush the map
} finally {
  Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
Write-Host "--- iisexpress stdout ---"; Get-Content $out -ErrorAction SilentlyContinue
Write-Host "--- iisexpress stderr ---"; Get-Content $err -ErrorAction SilentlyContinue
if (-not $served) { throw "IIS Express never served the app" }

$maps = Get-ChildItem -Recurse (Join-Path $site "tmp\appmap") -Filter *.appmap.json -ErrorAction SilentlyContinue
if (-not $maps) { throw "no AppMap produced by the System.Web module" }
$json = Get-Content $maps[0].FullName -Raw | ConvertFrom-Json
$http = $json.events | Where-Object { $_.http_server_request } | Select-Object -First 1
if (-not $http) { throw "map has no http_server_request event" }
Write-Host "System.Web module recorded $($http.http_server_request.request_method) $($http.http_server_request.path_info)"
