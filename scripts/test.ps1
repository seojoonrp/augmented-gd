# Compile and run the host-side tests for src/core (no Geode, no GD). Uses the
# same LLVM clang as the mod build; nothing else to install.
#
#   .\scripts\test.ps1            build + run
#   .\scripts\test.ps1 -Verbose   show the compiler command
param([switch]$Verbose)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")

$out = Join-Path $root "build\tests"
New-Item -ItemType Directory -Force $out | Out-Null
$exe = Join-Path $out "core_tests.exe"

$sources = @(Join-Path $root "tests\core_tests.cpp") + (Get-ChildItem (Join-Path $root "src\core") -Filter *.cpp | ForEach-Object { $_.FullName })
$args = @("-std=c++23", "-O1", "-Wall", "-Wextra", "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
          "-I", (Join-Path $root "src"), "-o", $exe) + $sources
if ($Verbose) { Write-Output ("clang++ " + ($args -join " ")) }

$ErrorActionPreference = "Continue"   # clang warnings go to stderr
& clang++ @args
$ErrorActionPreference = "Stop"
if ($LASTEXITCODE -ne 0) { throw "test build failed with exit code $LASTEXITCODE" }

& $exe
if ($LASTEXITCODE -ne 0) { throw "core tests failed" }
