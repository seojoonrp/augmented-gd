# List the node IDs the geode.node-ids mod assigns on a layer, from its source
# in refs/node-ids (run scripts\fetch-refs.ps1 first). Use these with
# getChildByID / querySelector instead of child indices.
#
#   .\scripts\nodeids.ps1 LevelInfoLayer
#   .\scripts\nodeids.ps1 PauseLayer
#   .\scripts\nodeids.ps1              # which layers are covered
param([Parameter(Position = 0)][string]$Layer)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$src  = Join-Path $root "refs\node-ids\src"
if (-not (Test-Path $src)) { throw "refs/node-ids missing - run scripts\fetch-refs.ps1" }

if (-not $Layer) {
    Get-ChildItem $src -Filter *.cpp | ForEach-Object { $_.BaseName } | Sort-Object
    return
}
$file = Join-Path $src "$Layer.cpp"
if (-not (Test-Path $file)) { throw "no refs/node-ids/src/$Layer.cpp (node-ids does not cover this layer)" }
Write-Output "refs/node-ids/src/$Layer.cpp"
# Every ID is a string literal handed to setID / setIDSafe / setIDs. Print the
# literal with its line so the structure (menus vs. buttons) stays visible.
Select-String -Path $file -Pattern '"([a-z0-9\-]+)"' -AllMatches | ForEach-Object {
    $ln = $_.LineNumber
    foreach ($m in $_.Matches) { "{0,5}: {1}" -f $ln, $m.Groups[1].Value }
}
