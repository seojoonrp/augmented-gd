# Build + install the mod into GD. Safe to run from any shell (VS Code
# terminals often carry a stale PATH; this reloads it from the registry).
#
#   .\scripts\build.ps1            incremental build
#   .\scripts\build.ps1 -Clean     wipe build/ first (needed after CMake/CPM changes)
#   .\scripts\build.ps1 -Log       tail the newest Geode log after building
#   .\scripts\build.ps1 -SkipTests don't run the host tests (scripts/test.ps1) first
param(
    [switch]$Clean,
    [switch]$Log,
    [switch]$SkipTests
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

# src/core is pure C++: its tests run on the host in a few seconds and catch
# economy / formula regressions before the user has to boot the game.
if (-not $SkipTests) {
    & "$PSScriptRoot\test.ps1"
}

# Keep the mod fonts' charset in step with the Korean text in src/ (mod.json
# resources.fonts); Geode regenerates the fonts when it changes.
& "$PSScriptRoot\fontcharset.ps1"

# Bake the outlined UI fonts (ImcreSoojin) from that charset. Needs the
# Windows Python with Pillow: py -3 -m pip install pillow fonttools
py -3 "$PSScriptRoot\fontgen.py"
if ($LASTEXITCODE -ne 0) { throw "fontgen.py failed with exit code $LASTEXITCODE" }

# geode writes warnings to stderr; under "Stop" PS 5.1 would abort on them.
$ErrorActionPreference = "Continue"
geode build --ninja
$ErrorActionPreference = "Stop"
if ($LASTEXITCODE -ne 0) { throw "geode build failed with exit code $LASTEXITCODE" }

if ($Log) {
    & "$PSScriptRoot\logs.ps1"
}
