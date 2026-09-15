# Print this mod's lines from the newest Geode log (plus loader errors).
#
#   .\scripts\logs.ps1              last 40 matching lines
#   .\scripts\logs.ps1 -All         every matching line
#   .\scripts\logs.ps1 -Pattern X   custom regex instead of the default
param(
    [switch]$All,
    [string]$Pattern = "Augmented GD|augmented-gd|ERROR|WARN"
)

$logDir = "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\logs"
$log = Get-ChildItem $logDir -Filter *.log | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $log) { throw "No Geode logs found in $logDir" }

Write-Output "LOG: $($log.FullName)"
$lines = Select-String -Path $log.FullName -Pattern $Pattern | ForEach-Object { $_.Line }
if ($All) { $lines } else { $lines | Select-Object -Last 40 }
