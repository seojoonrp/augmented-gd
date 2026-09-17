# Static audit, no game needed. Two checks:
#
#   1. Bindings: every member a `$modify(Ours, Base)` class defines is looked up
#      in the 2.2081 bindings (Base, then its parents). A member that GD
#      inlined (`win inline`) or that has no Windows address is an ERROR — the
#      hook would silently do nothing (CLAUDE.md rule 2). Helpers that are not
#      bindings at all are listed as info.
#   2. Docs: every `path.cpp` / `path.hpp` our docs mention must exist, and a
#      `` `file.cpp` `symbol` `` pair must find `symbol` in that file. Line
#      ranges into our own sources are a WARNING (they rot; cite symbols).
#      Lines that cite a reference mod are skipped.
#
#   .\scripts\check.ps1            both checks, exit 1 on any error
#   .\scripts\check.ps1 -Bindings  / -Docs   one of them
#   .\scripts\check.ps1 -Verbose   also print every OK binding
param(
    [switch]$Bindings,
    [switch]$Docs,
    [switch]$Verbose
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $Bindings -and -not $Docs) { $Bindings = $true; $Docs = $true }
$errors = 0
$warnings = 0

function Fail($msg) { Write-Output "ERROR  $msg"; $script:errors++ }
function Warn($msg) { Write-Output "WARN   $msg"; $script:warnings++ }
function Info($msg) { if ($Verbose) { Write-Output "ok     $msg" } }

# ---------------------------------------------------------------- bindings

if ($Bindings) {
    $broDir = Join-Path $root "build\_deps\bindings-src\bindings\2.2081"
    if (-not (Test-Path $broDir)) { throw "bindings not found at $broDir - run scripts\build.ps1 once" }

    # class name -> @{ File; Parents; Lines }
    $classes = @{}
    foreach ($f in Get-ChildItem $broDir -Filter *.bro) {
        $lines = Get-Content $f.FullName
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -notmatch '^class\s+((?:\w+::)*)(\w+)\s*(?::\s*([^{]+))?\{') { continue }
            $name = $Matches[2]
            $parents = @()
            if ($Matches[3]) {
                $parents = $Matches[3] -split "," | ForEach-Object { ($_.Trim() -replace '^.*::', '') } | Where-Object { $_ }
            }
            $body = @()
            for ($j = $i + 1; $j -lt $lines.Count -and $lines[$j] -notmatch '^}'; $j++) { $body += $lines[$j] }
            $classes[$name] = @{ File = $f.Name; Parents = $parents; Lines = $body }
        }
    }

    # Returns @{ Class; Line; Verdict } or $null when no binding declares it.
    function Find-Member($class, $member, $depth = 0) {
        if ($depth -gt 8 -or -not $classes.ContainsKey($class)) { return $null }
        $c = $classes[$class]
        foreach ($l in $c.Lines) {
            if ($l -notmatch "\b$member\s*\(") { continue }
            if ($l -match '^\s*//') { continue }
            $verdict = "ok"
            if ($l -match '=\s*win inline')                          { $verdict = "win inline" }
            elseif ($l -match '=\s*inline\s*;')                      { $verdict = "geode inline" }
            elseif ($l -match '=\s*win 0x')                          { $verdict = "ok" }
            elseif ($l -notmatch '\bwin\b') {
                if ($c.File -eq "Cocos2d.bro") { $verdict = "ok" } else { $verdict = "no win address" }
            }
            return @{ Class = $class; Line = $l.Trim(); Verdict = $verdict }
        }
        foreach ($p in $c.Parents) {
            $r = Find-Member $p $member ($depth + 1)
            if ($r) { return $r }
        }
        return $null
    }

    $src = Get-ChildItem (Join-Path $root "src") -Recurse -Include *.cpp, *.hpp
    foreach ($f in $src) {
        $lines = Get-Content $f.FullName
        $rel = $f.FullName.Substring($root.Length + 1)
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -notmatch '\$modify\(\s*(\w+)\s*,\s*(\w+)\s*\)') { continue }
            $ours = $Matches[1]; $base = $Matches[2]
            if (-not $classes.ContainsKey($base)) { Fail "${rel}:$($i + 1) `$modify base '$base' not in bindings"; continue }
            $depth = 0
            for ($j = $i; $j -lt $lines.Count; $j++) {
                $l = $lines[$j]
                # Member definitions sit at exactly 4 spaces of indent inside the class.
                if ($depth -eq 1 -and $l -match '^    (?=\S)(?!struct\b|static\b|//|using\b|template\b)[\w:<>&\*,\s]+?[\s\*&](\w+)\s*\([^;]*\)\s*(const)?\s*(override)?\s*\{?\s*$') {
                    $member = $Matches[1]
                    $r = Find-Member $base $member
                    if (-not $r) { Info "${rel}:$($j + 1) $ours::$member - helper (no binding)" }
                    elseif ($r.Verdict -eq "ok") { Info "${rel}:$($j + 1) $base::$member -> $($r.Class) win ok" }
                    else { Fail "${rel}:$($j + 1) $ours::$member overrides $($r.Class)::$member which is '$($r.Verdict)' - hook does nothing`n         $($r.Line)" }
                }
                $depth += ([regex]::Matches($l, '\{')).Count - ([regex]::Matches($l, '\}')).Count
                if ($j -gt $i -and $depth -le 0) { break }
            }
        }
    }
}

# ---------------------------------------------------------------- docs

if ($Docs) {
    $srcFiles = Get-ChildItem (Join-Path $root "src") -Recurse -Include *.cpp, *.hpp
    $byName = @{}
    foreach ($f in $srcFiles) { $byName[$f.Name] = $f.FullName }
    $refNames = "qolmod|xdbot|xdBot|CBF|click-between-frames|death-tracker|custom-keybinds|betterinfo|node-ids|devtools|miscbugfixes|cleanstartpos|geode-docs|example-mod|loader\b|refs/|Geode's|bindings/"

    # docs/refs/<mod>.md pages cite that mod's own tree; only INDEX.md mixes in ours.
    $docFiles = @(Get-ChildItem (Join-Path $root "docs") -Recurse -Include *.md |
        Where-Object { $_.DirectoryName -notmatch '\\refs$' -or $_.Name -eq "INDEX.md" })
    $docFiles += Get-Item (Join-Path $root "CLAUDE.md")
    foreach ($d in $docFiles) {
        $rel = $d.FullName.Substring($root.Length + 1)
        $lines = Get-Content $d.FullName
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $l = $lines[$i]
            if ($l -match $refNames) { continue }
            foreach ($m in [regex]::Matches($l, '([\w./-]*?)(\w+\.(?:cpp|hpp))(?::(\d+)(?:-(\d+))?)?')) {
                $prefix = $m.Groups[1].Value; $name = $m.Groups[2].Value
                if ($prefix -and $prefix -notmatch '^(src/|\./)?[\w/]*$') { continue }
                if (-not $byName.ContainsKey($name)) {
                    if ($prefix -match '^src/') { Fail "${rel}:$($i + 1) cites $prefix$name which does not exist" }
                    continue
                }
                if ($prefix -match '^src/' -and -not (Test-Path (Join-Path $root ($prefix + $name)))) {
                    Fail "${rel}:$($i + 1) cites $prefix$name but the file lives at $($byName[$name].Substring($root.Length + 1))"
                }
                if ($m.Groups[3].Success) {
                    Warn "${rel}:$($i + 1) cites our source by line (${name}:$($m.Groups[3].Value)) - lines rot, cite a symbol"
                }
                # `file.cpp` `symbol` (same line, any number of them)
                foreach ($s in [regex]::Matches($l.Substring($m.Index + $m.Length), '``(\w+)``|`(\w+)`')) {
                    $sym = if ($s.Groups[1].Success) { $s.Groups[1].Value } else { $s.Groups[2].Value }
                    if ($sym -match '\.(cpp|hpp)$' -or $sym.Length -lt 4) { continue }
                    if ((Get-Content $byName[$name] -Raw) -notmatch "\b$sym\b") {
                        Warn "${rel}:$($i + 1) '$sym' not found in $name"
                    }
                }
            }
        }
    }
}

Write-Output ""
Write-Output "check: $errors error(s), $warnings warning(s)"
if ($errors -gt 0) { exit 1 }
