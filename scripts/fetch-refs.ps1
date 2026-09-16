# Clone open-source Geode mods into refs/ (gitignored) so agents can read how
# a feature is *actually* done before touching GD internals, and write
# refs/MANIFEST.md (commit, date, geode/gd target, license) for every ref.
#
#   .\scripts\fetch-refs.ps1            # clone missing refs at their pinned commit
#   .\scripts\fetch-refs.ps1 -Update    # move every ref to upstream HEAD, re-pin by hand after
#
# Every ref must target GD 2.2081 / Geode 5.x (see docs/HARNESS-PLAN.md). When
# adding one, leave Pin empty for the first run, then copy the commit from
# MANIFEST.md into Pin. Read docs/refs/INDEX.md for what each ref is good for.
param([switch]$Update)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$refs = Join-Path $root "refs"
New-Item -ItemType Directory -Force $refs | Out-Null

# Order matters only for MANIFEST.md. Pin = full commit hash ("" = HEAD).
$repos = @(
    @{ Name = "custom-keybinds";      Url = "https://github.com/geode-sdk/CustomKeybinds.git";       Pin = "426a9aa1fc8811007d56866642a249c3b4a76d0d"; Purpose = "Geode 5 keybind settings, UILayer key handling in PlayLayer" }
    @{ Name = "qolmod";               Url = "https://github.com/TheSillyDoggo/GeodeMenu.git";        Pin = "98aa8a3a1cf46b192b42ae6ee369f4434f1bd81d"; Purpose = "QOLMod (installed): hitboxes, noclip, speedhack, key input, label HUD" }
    @{ Name = "xdbot";                Url = "https://github.com/Zilko/xdBot.git";                    Pin = "16ef8e86d3a295119583a1c236be7a9aab7568e4"; Purpose = "macro bot: checkpoints/frame step, PlayLayer lifecycle, slow-mo"; Note = "STALE: Geode 4 / GD 2.2074 (no newer upstream). Patterns only; re-verify every binding" }
    @{ Name = "click-between-frames"; Url = "https://github.com/theyareonit/Click-Between-Frames.git"; Pin = "9fa7c6f590945d048b8e83de71eaba30dcb0c64e"; Purpose = "installed; hooks Windows raw input directly" }
    @{ Name = "death-tracker";        Url = "https://github.com/abb2k/death-tracker.git";            Pin = "e8bc39aae1d3a91ad8d74a8d813fe77c65c3d68e"; Purpose = "installed; death counting per level" }
    @{ Name = "cleanstartpos";        Url = "https://github.com/blueblock6/CleanStartpos.git";       Pin = "ba4110687c7663d231d3771ae41d129a1eaab330"; Purpose = "installed; startpos handling" }
    @{ Name = "miscbugfixes";         Url = "https://github.com/Cvolton/miscbugfixes-geode.git";     Pin = "6ffab7baa459f230f6f0339bbca7f244a33a6dbe"; Purpose = "small targeted GD bugfix hooks; minimal `$modify examples" }
    @{ Name = "betterinfo";           Url = "https://github.com/Cvolton/betterinfo-geode.git";       Pin = "305e1e574865e1a3fe15b43315059c96776a8e68"; Purpose = "large; LevelInfoLayer buttons, popups, UI patterns" }
    @{ Name = "node-ids";             Url = "https://github.com/geode-sdk/NodeIDs.git";              Pin = "fc9b3aa12a4eee67f0d26e3f115190cba2deddf1"; Purpose = "node IDs per layer (source instead of dll grep)" }
    @{ Name = "devtools";             Url = "https://github.com/geode-sdk/DevTools.git";             Pin = "f36931acb7ba788fefc3ca748dbc98322fbb6152"; Purpose = "node tree inspector; UI debugging" }
    @{ Name = "example-mod";          Url = "https://github.com/geode-sdk/example-mod.git";          Pin = "86696b8eea13d95975ff55f97c1a827ee133f22a"; Purpose = "minimal template; style baseline" }
    @{ Name = "geode-docs";           Url = "https://github.com/geode-sdk/docs.git";                 Pin = "e7dac379c333d590e0f68a3917c528ad6c54b790"; Purpose = "official tutorials: modify, events, settings, keybinds, Popup, layout" }
)

function Get-Json($path) {
    if (Test-Path $path) { try { return Get-Content $path -Raw | ConvertFrom-Json } catch { return $null } }
    return $null
}
function Get-License($dir) {
    $f = Get-ChildItem $dir -File | Where-Object { $_.Name -match '^(LICENSE|LICENCE|COPYING)' } | Select-Object -First 1
    if (-not $f) { return "none" }
    $head = (Get-Content $f.FullName -TotalCount 20) -join " "
    if ($head -match "MIT")                  { return "MIT" }
    if ($head -match "GNU GENERAL PUBLIC")   { return "GPL" }
    if ($head -match "LESSER GENERAL")       { return "LGPL" }
    if ($head -match "Apache")               { return "Apache-2.0" }
    if ($head -match "Boost")                { return "BSL-1.0" }
    if ($head -match "Dont use my code")     { return "all-rights-reserved" }
    return ($f.Name)
}

$rows = @()
foreach ($r in $repos) {
    $dst = Join-Path $refs $r.Name
    if (-not (Test-Path $dst)) {
        Write-Output "cloning $($r.Url) -> refs/$($r.Name)"
        if ($r.Pin) {
            git init -q $dst
            git -C $dst remote add origin $r.Url
            git -C $dst fetch -q --depth 1 origin $r.Pin
            git -C $dst checkout -q FETCH_HEAD
        }
        else {
            git clone --depth 1 -q $r.Url $dst
        }
    }
    elseif ($Update) {
        Write-Output "updating refs/$($r.Name) to upstream HEAD"
        git -C $dst fetch -q --depth 1 origin HEAD
        git -C $dst checkout -q FETCH_HEAD
    }
    elseif ($r.Pin -and (git -C $dst rev-parse HEAD) -ne $r.Pin) {
        Write-Output "refs/$($r.Name): moving to pinned $($r.Pin.Substring(0,7))"
        git -C $dst fetch -q --depth 1 origin $r.Pin
        git -C $dst checkout -q FETCH_HEAD
    }

    $commit = git -C $dst rev-parse HEAD
    $date   = git -C $dst log -1 --format=%ad --date=short
    $mod    = Get-Json (Join-Path $dst "mod.json")
    $geode  = if ($mod) { $mod.geode } else { "-" }
    $gd     = if ($mod -and $mod.gd) { if ($mod.gd -is [string]) { $mod.gd } else { $mod.gd.win } } else { "-" }
    $rows  += "| $($r.Name) | $($r.Url -replace '^https://github.com/','' -replace '\.git$','') | ``$($commit.Substring(0,7))`` | $date | $geode | $gd | $(Get-License $dst) | $($r.Purpose) | $($r.Note) |"
    if (-not $r.Pin) { Write-Output "  refs/$($r.Name) unpinned -> $commit (copy into Pin)" }
}

# Refs no longer in the table are stale (wrong GD/Geode version); say so.
Get-ChildItem $refs -Directory | Where-Object { $repos.Name -notcontains $_.Name } | ForEach-Object {
    Write-Output "refs/$($_.Name) is not in the table any more - delete it (stale target version)"
}

$manifest = @(
    "# refs/ manifest (generated by scripts/fetch-refs.ps1, $(Get-Date -Format yyyy-MM-dd))"
    ""
    "Reference mods for GD 2.2081 / Geode 5.x. Read the *pattern*, don't copy code:"
    "licenses are mixed. Per-ref notes live in docs/refs/<name>.md; the"
    "problem-first index is docs/refs/INDEX.md."
    ""
    "| ref | repo | commit | date | geode | gd | license | purpose | note |"
    "|---|---|---|---|---|---|---|---|---|"
) + $rows
[IO.File]::WriteAllText((Join-Path $refs "MANIFEST.md"), ($manifest -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
Write-Output "wrote refs/MANIFEST.md. Geode SDK source (loader/src) is at $env:GEODE_SDK"
