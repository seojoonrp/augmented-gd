# Clone open-source Geode mods into refs/ (gitignored) so agents can read how
# a feature is *actually* done before touching GD internals. See docs/GD-INTERNALS.md.
#
#   .\scripts\fetch-refs.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$refs = Join-Path $root "refs"
New-Item -ItemType Directory -Force $refs | Out-Null

$repos = @{
    # hitboxes (byte-patched!), noclip, speedhack, startpos, labels, replays
    "openhack"        = "https://github.com/prevter/OpenHack.git"
    # Geode 5 keybind settings + UILayer key handling in PlayLayer
    "custom-keybinds" = "https://github.com/geode-sdk/CustomKeybinds.git"
    # small targeted GD bugfix hooks; good examples of minimal $modify usage
    "miscbugfixes"    = "https://github.com/Cvolton/miscbugfixes-geode.git"
}

foreach ($name in $repos.Keys) {
    $dst = Join-Path $refs $name
    if (Test-Path $dst) {
        Write-Output "refs/$name already present, pulling"
        git -C $dst pull -q --ff-only
    }
    else {
        Write-Output "cloning $($repos[$name]) -> refs/$name"
        git clone --depth 1 -q $repos[$name] $dst
    }
}
Write-Output "done. Geode SDK source (loader/src) is at $env:GEODE_SDK"
