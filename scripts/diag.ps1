# Inventory of the log lines in src/, grouped by file: what a test round can
# print, so STATUS.md can point here instead of listing them by hand. Trim a
# diagnostic by deleting the line; this list follows.
#
#   .\scripts\diag.ps1                  every log::info/warn/error in src/
#   .\scripts\diag.ps1 -Pattern Cat     only messages matching a regex
#   .\scripts\diag.ps1 -Level warn      one level only
param(
    [string]$Pattern,
    [string]$Level = "info|warn|error|debug"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$files = Get-ChildItem (Join-Path $root "src") -Recurse -Include *.cpp, *.hpp | Sort-Object FullName
$total = 0
foreach ($f in $files) {
    $lines = Get-Content $f.FullName
    $hits = @()
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -notmatch "log::($Level)\(") { continue }
        # The format string may sit on the next line(s); take the first quoted one.
        $msg = ""
        for ($j = $i; $j -lt [Math]::Min($i + 3, $lines.Count); $j++) {
            if ($lines[$j] -match '"((?:[^"\\]|\\.)*)"') { $msg = $Matches[1]; break }
        }
        if ($Pattern -and $msg -notmatch $Pattern) { continue }
        $lvl = $Matches[1]
        $hits += "{0,5}  {1,-5} {2}" -f ($i + 1), ($lines[$i] -replace '.*log::(\w+)\(.*', '$1'), $msg
    }
    if ($hits.Count -eq 0) { continue }
    Write-Output ($f.FullName.Substring($root.Length + 1) + "  ($($hits.Count))")
    $hits | ForEach-Object { Write-Output $_ }
    Write-Output ""
    $total += $hits.Count
}
Write-Output "diag: $total log line(s)"
