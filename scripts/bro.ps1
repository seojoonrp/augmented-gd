# Look a class / member up in the Geode bindings and say whether it is hookable
# on Windows. Use this before writing any $modify or calling any GD function
# (CLAUDE.md rule 2).
#
#   .\scripts\bro.ps1 PlayLayer                 # whole class block
#   .\scripts\bro.ps1 PlayLayer destroyPlayer   # matching lines only + verdict
#   .\scripts\bro.ps1 CCScheduler update        # cocos classes work too
#   .\scripts\bro.ps1 -Grep "m_isPracticeMode"  # free-text search across all .bro files
#
# Verdicts:
#   win 0x...      hookable / callable on Windows
#   win inline     GD inlined it -> hooking does NOTHING; Geode's impl is in inline/*.cpp
#   = inline       Geode-provided body (header), not a GD function -> not hookable
#   (no win)       not bound on Windows; for Cocos2d.bro entries this is still callable
#                  via libcocos2d.dll, for GD classes it is not usable
#   field          data member; offsets are handled by Geode, just use it
param(
    [Parameter(Position = 0)][string]$Class,
    [Parameter(Position = 1)][string]$Member,
    [string]$Grep
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bro  = Join-Path $root "build\_deps\bindings-src\bindings\2.2081"
if (-not (Test-Path $bro)) { throw "bindings not found at $bro - run scripts\build.ps1 once" }
$files = Get-ChildItem $bro -Filter *.bro

if ($Grep) {
    foreach ($f in $files) {
        Select-String -Path $f.FullName -Pattern $Grep | ForEach-Object { "$($f.Name):$($_.LineNumber): $($_.Line.Trim())" }
    }
    return
}
if (-not $Class) { throw "usage: bro.ps1 <Class> [Member] | -Grep <pattern>" }

function Verdict($line, $file) {
    if ($line -match '^\s*(class|struct)\s')           { return "" }
    if ($line -match '=\s*win inline')                 { return "  <- win inline: NOT hookable, see inline/" }
    if ($line -match '=\s*inline\s*;')                 { return "  <- geode inline body: not hookable" }
    if ($line -match '=\s*win 0x')                     { return "  <- win ok" }
    if ($line -match '\(.*\)\s*(const)?\s*(=|;)' -and $line -notmatch 'win ')    { if ($file -eq "Cocos2d.bro") { return "  <- no win address, but cocos: hookable via libcocos2d.dll" } else { return "  <- no win address: NOT usable on Windows" } }
    if ($line -match '^\s*[\w:<>\*&,\s]+\s+m_\w+\s*;') { return "  <- field" }
    return ""
}

$found = $false
foreach ($f in $files) {
    $lines = Get-Content $f.FullName
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -notmatch "^class (\w+::)*$Class\b") { continue }
        $found = $true
        Write-Output "$($f.Name):$($i + 1)"
        for ($j = $i; $j -lt $lines.Count; $j++) {
            $l = $lines[$j]
            if (-not $Member -or $j -eq $i -or $l -match "\b$Member\b") {
                Write-Output ("{0,6}: {1}{2}" -f ($j + 1), $l, (Verdict $l $f.Name))
            }
            if ($l -match '^}') { break }
        }
        break
    }
}
if (-not $found) {
    Write-Output "class $Class not found in 2.2081 bindings. Try: bro.ps1 -Grep $Class"
    return
}
if ($Member) {
    $impls = Get-ChildItem (Join-Path $bro "inline") -Filter *.cpp | Select-String -Pattern "${Class}::${Member}\b"
    foreach ($m in $impls) { Write-Output "inline impl: inline/$($m.Filename):$($m.LineNumber)" }
}
