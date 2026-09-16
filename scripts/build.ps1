# Build + install the mod into GD. Safe to run from any shell (VS Code
# terminals often carry a stale PATH; this reloads it from the registry).
#
#   .\scripts\build.ps1            incremental build
#   .\scripts\build.ps1 -Clean     wipe build/ first (needed after CMake/CPM changes)
#   .\scripts\build.ps1 -Log       tail the newest Geode log after building
param(
    [switch]$Clean,
    [switch]$Log
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
$env:GEODE_SDK = [Environment]::GetEnvironmentVariable("GEODE_SDK", "User")
$env:CPM_SOURCE_CACHE = [Environment]::GetEnvironmentVariable("CPM_SOURCE_CACHE", "User")

if (-not $env:GEODE_SDK) { throw "GEODE_SDK is not set. Run 'geode sdk install' first." }

if ($Clean -and (Test-Path build)) {
    Remove-Item -Recurse -Force build
}

# Keep the mod fonts' charset in step with the Korean text in src/ (mod.json
# resources.fonts); Geode regenerates the fonts when it changes.
& "$PSScriptRoot\fontcharset.ps1"

# geode writes warnings to stderr; under "Stop" PS 5.1 would abort on them.
$ErrorActionPreference = "Continue"
geode build --ninja
$ErrorActionPreference = "Stop"
if ($LASTEXITCODE -ne 0) { throw "geode build failed with exit code $LASTEXITCODE" }

if ($Log) {
    & "$PSScriptRoot\logs.ps1"
}
