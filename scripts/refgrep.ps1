# One search over everything an agent may cite: docs/ (our notes), refs/ (reference mods), and
# optionally the Geode loader source and the bindings. Build output, resources
# and .git are excluded. Answers "how does a working mod do X?".
#
#   .\scripts\refgrep.ps1 "destroyPlayer"              # docs/ + refs/
#   .\scripts\refgrep.ps1 "KeyboardInputEvent" -Sdk    # + $GEODE_SDK/loader (src + include)
#   .\scripts\refgrep.ps1 "m_checkpointArray" -All     # + bindings too
#   .\scripts\refgrep.ps1 "\$modify\(.*PlayLayer" -Ref qolmod,xdbot   # limit to some refs
#   .\scripts\refgrep.ps1 "setTimeScale" -Context 3
param(
    [Parameter(Mandatory, Position = 0)][string]$Pattern,
    [string[]]$Ref,
    [switch]$Sdk,
    [switch]$Bindings,
    [switch]$All,
    [int]$Context = 0,
    [switch]$CaseSensitive
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ($All) { $Sdk = $true; $Bindings = $true }

# "-Ref a,b" arrives as one string when invoked with -File; split it.
$Ref = $Ref | ForEach-Object { $_ -split "," } | Where-Object { $_ }
$paths = @(Join-Path $root "docs")   # our own notes cite refs; always included
if ($Ref) { $paths += $Ref | ForEach-Object { Join-Path $root "refs\$_" } }
else      { $paths += Join-Path $root "refs" }
if ($Sdk) {
    if (-not $env:GEODE_SDK) { throw "GEODE_SDK not set" }
    $paths += Join-Path $env:GEODE_SDK "loader\src"
    $paths += Join-Path $env:GEODE_SDK "loader\include\Geode"
}
if ($Bindings) { $paths += Join-Path $root "build\_deps\bindings-src\bindings\2.2081" }
$missing = $paths | Where-Object { -not (Test-Path $_) }
if ($missing) { throw "not found: $($missing -join ', ') (run scripts/fetch-refs.ps1?)" }

$rg = Get-Command rg -ErrorAction SilentlyContinue
if ($rg) {
    $args = @("-n", "--no-heading", "--color", "never",
              "-g", "!.git", "-g", "!resources", "-g", "!previews", "-g", "!build", "-g", "!*.png", "-g", "!*.plist", "-g", "!*.fnt",
              "-g", "!refs/geode-docs/src/*.js")
    if (-not $CaseSensitive) { $args += "-i" }
    if ($Context -gt 0) { $args += @("-C", "$Context") }
    & $rg.Source @args -e $Pattern @paths | ForEach-Object { $_ -replace [regex]::Escape($root + "\"), "" }
}
else {
    # Fallback: slower, no context, but works on a bare PowerShell.
    Get-ChildItem $paths -Recurse -File -Include *.cpp, *.hpp, *.h, *.mm, *.md, *.json, *.bro, *.txt |
        Where-Object { $_.FullName -notmatch '\\(\.git|resources|previews|build)\\' } |
        Select-String -Pattern $Pattern -CaseSensitive:$CaseSensitive |
        ForEach-Object { "$($_.Path -replace [regex]::Escape($root + '\'), ''):$($_.LineNumber): $($_.Line.Trim())" }
}
