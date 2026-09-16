# Rebuild the "charset" of every font in mod.json from the string literals in
# src/. GD's fonts have no Hangul, so the mod ships Pretendard, and Geode's
# font generator only includes the codepoints listed in the charset: this
# keeps that list equal to what the code can actually display (ASCII + every
# non-ASCII character used in a string literal). build.ps1 runs this first.
#
#   .\scripts\fontcharset.ps1          update mod.json if the charset changed
#   .\scripts\fontcharset.ps1 -Check   only report; exit 1 if mod.json is stale
param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$modJson = Join-Path $root "mod.json"
$utf8 = New-Object System.Text.UTF8Encoding $false

# Always present: printable ASCII and the bullet GD's own fonts carry.
$points = New-Object System.Collections.Generic.HashSet[int]
32..126 | ForEach-Object { [void]$points.Add($_) }
[void]$points.Add(8226)

# String literals only; comments are skipped (they hold dashes and the like
# that the game never draws). One regex, alternatives tried in order, so a
# "//" inside a literal or a quote inside a comment cannot confuse it.
$tokens = [regex]'("(?:[^"\\\r\n]|\\.)*")|(//[^\r\n]*)|(/\*[\s\S]*?\*/)'
$sources = Get-ChildItem (Join-Path $root "src") -Recurse -Include *.cpp, *.hpp
$literals = 0
foreach ($file in $sources) {
    $text = [System.IO.File]::ReadAllText($file.FullName, $utf8)
    foreach ($m in $tokens.Matches($text)) {
        if (-not $m.Groups[1].Success) { continue }
        $literals++
        $s = $m.Groups[1].Value
        $i = 0
        while ($i -lt $s.Length) {
            if ([char]::IsHighSurrogate($s, $i) -and $i + 1 -lt $s.Length) {
                $cp = [char]::ConvertToUtf32($s, $i)
                $i += 2
            }
            else {
                $cp = [int]$s[$i]
                $i += 1
            }
            if ($cp -ge 128) { [void]$points.Add($cp) }
        }
    }
}

# Compress sorted codepoints into "a-b,c,d-e".
$sorted = @($points | Sort-Object)
$parts = New-Object System.Collections.Generic.List[string]
$start = $sorted[0]; $prev = $sorted[0]
foreach ($cp in $sorted[1..($sorted.Count - 1)]) {
    if ($cp -eq $prev + 1) { $prev = $cp; continue }
    if ($start -eq $prev) { $parts.Add("$start") } else { $parts.Add("$start-$prev") }
    $start = $cp; $prev = $cp
}
if ($start -eq $prev) { $parts.Add("$start") } else { $parts.Add("$start-$prev") }
$charset = $parts -join ","

$nonAscii = ($sorted | Where-Object { $_ -ge 128 }).Count
Write-Host "fontcharset: $($sources.Count) files, $literals literals, $($sorted.Count) codepoints ($nonAscii non-ASCII)"

$json = [System.IO.File]::ReadAllText($modJson, $utf8)
$pattern = '("charset"\s*:\s*")[^"]*(")'
if (-not [regex]::IsMatch($json, $pattern)) {
    throw "mod.json has no font with a ""charset"" key (resources.fonts)"
}
$updated = [regex]::Replace($json, $pattern, { param($m) $m.Groups[1].Value + $charset + $m.Groups[2].Value })

if ($updated -eq $json) {
    Write-Host "fontcharset: mod.json is up to date"
    exit 0
}
if ($Check) {
    Write-Host "fontcharset: mod.json charset is stale (run scripts\fontcharset.ps1)"
    exit 1
}
[System.IO.File]::WriteAllText($modJson, $updated, $utf8)
Write-Host "fontcharset: mod.json charset updated"
