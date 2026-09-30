# List the mods installed in the user's GD (id, version, geode/gd target, source
# URL, enabled?). Use it to reason about hook interference and to know which
# refs correspond to what is actually running on this machine.
#
#   .\scripts\mods.ps1
$ErrorActionPreference = "Stop"
$gd   = "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash"
$mods = Join-Path $gd "geode\mods"
if (-not (Test-Path $mods)) { throw "no geode/mods under $gd" }

$saved = Join-Path $gd "geode\mods\..\..\geode\mods\..\..\geode\saved.json"
# Enabled flags live in the loader's own save file (should-load-<id>).
$saved = Join-Path $env:LOCALAPPDATA "GeometryDash\geode\mods\geode.loader\saved.json"
$disabled = @()
if (Test-Path $saved) {
    try {
        $j = Get-Content $saved -Raw | ConvertFrom-Json
        foreach ($p in $j.PSObject.Properties) {
            if ($p.Name -like "should-load-*" -and -not $p.Value) { $disabled += $p.Name.Substring(12) }
        }
    } catch {}
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$rows = foreach ($f in Get-ChildItem $mods -Filter *.geode) {
    $zip = [IO.Compression.ZipFile]::OpenRead($f.FullName)
    try {
        $entry = $zip.GetEntry("mod.json")
        $sr = New-Object IO.StreamReader($entry.Open())
        $m = $sr.ReadToEnd() | ConvertFrom-Json
        $sr.Dispose()
    } finally { $zip.Dispose() }
    $gdv = if ($m.gd -is [string]) { $m.gd } elseif ($m.gd) { $m.gd.win } else { "-" }
    $src = if ($m.links -and $m.links.source) { $m.links.source } elseif ($m.repository) { $m.repository } else { "-" }
    [pscustomobject]@{
        id      = $m.id
        version = $m.version
        geode   = $m.geode
        gd      = $gdv
        enabled = if ($disabled -contains $m.id) { "no" } else { "yes" }
        source  = $src -replace '^https://github.com/', ''
    }
}
$rows | Sort-Object id | Format-Table -AutoSize
