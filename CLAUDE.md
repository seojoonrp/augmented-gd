# Augmented GD — agent harness

Geode mod for Geometry Dash (2.2081, Geode 5.10.1, Windows). Turns a level into a
roguelite run: die → draft an augment → get stronger → clear.
Mod ID `selenophile.augmented-gd`. The user tests in game; you cannot.

Read in this order at the start of a session:
1. `docs/STATUS.md` — what works, what's broken, what to try next.
2. `docs/GD-INTERNALS.md` — verified facts about GD/Geode. **Check it before touching anything GD-internal.**
3. `docs/refs/INDEX.md` — problem → "how a working mod does it" (file:line in `refs/`).
4. `docs/DESIGN.md` — game rules and decisions.
5. `docs/RECIPES.md` — snippets in our style, each marked verified / from-ref.

Update `docs/STATUS.md` at the end of every session (verified / broken / next).

## Build & test loop

```powershell
.\scripts\build.ps1          # fontcharset + fontgen + geode build --ninja + install into GD (fixes stale PATH)
.\scripts\build.ps1 -Clean   # after CMake/CPM/env changes ("Unknown CMake command CPMAddPackage" → this)
.\scripts\logs.ps1           # [Augmented GD] lines from the newest Geode log
.\scripts\fetch-refs.ps1     # clone reference mods into refs/ (gitignored, pinned commits, writes refs/MANIFEST.md)
.\scripts\bro.ps1 Class [member]   # binding line + hookable verdict (win ok / win inline / field)
.\scripts\refgrep.ps1 pattern [-Sdk|-All]   # rg over refs/ (+ loader source, bindings)
.\scripts\nodeids.ps1 Layer  # node IDs for a layer (from NodeIDs source)
.\scripts\mods.ps1           # mods installed in the user's GD, enabled or not
.\scripts\fontcharset.ps1    # rebuild mod.json font charset from Korean literals in src/ (build.ps1 runs it)
```

- Build must end with `| Done | Installed selenophile.augmented-gd.geode` and exit 0.
- Plain `geode`/`clang` may be missing in VS Code terminals (stale PATH). The scripts
  reload PATH from the registry; if running commands by hand, do the same first.
- The user launches GD and reports. Ask for the log (`scripts/logs.ps1`) with every
  bug report; don't guess from the description alone.
- Environment facts: SDK at `$env:GEODE_SDK` (`C:\Users\seojo\Documents\Geode`),
  CPM cache `C:\Users\seojo\.cpm-cache`, GD at
  `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash`. Compiler is LLVM
  clang (MSYS2 GCC is also on PATH and must **not** be picked — CMakeLists handles it).
- `.vscode/` is gitignored; it holds CMake Tools (Ninja, compile_commands) and clangd
  (`-header-insertion=never`) settings. Recreate if missing.

## Working rules (learned the hard way)

1. **GD internals: read a reference mod first, don't guess.** GD is closed source and
   its checks are often inlined assembly. Two features (hitboxes, hotkeys) burned
   several test rounds on plausible-but-wrong assumptions. Start at
   `docs/refs/INDEX.md`, then `scripts\refgrep.ps1` over `refs/` and
   `$GEODE_SDK/loader/src`. If nothing there covers it, say so and mark the
   approach **(unverified)** in the reply and in `docs/GD-INTERNALS.md`.
   Refs are read for *patterns*; licenses are mixed (qolmod is all-rights-reserved),
   so never paste their code.
2. **Verify bindings, never recall them.** Run `scripts\bro.ps1 Class member` for
   every class member / function you use. `= inline` / `win inline` functions
   cannot be hooked on Windows.
3. **Instrument before the user tests.** Every new code path gets a `log::info` on entry
   and on each early return, so one failed test round pinpoints the stage. Trim once
   verified (list in `docs/STATUS.md`).
4. **One failed round → change approach, not parameters.** If a test shows *no* log
   line from a path, the path isn't reached; don't re-tune it.
5. Popup callbacks must not capture `PlayLayer*` (may be gone). Use
   `AugmentManager` and `PlayLayer::get()` at call time.
6. Never block or count `destroyPlayer(player, m_anticheatSpike)`.
7. Geode objects: `cast::typeinfo_pointer_cast`, not `dynamic_pointer_cast`.
8. Prefer `Write`/`Edit` tools over bash heredocs for source files (quoting issues on
   this Windows setup). Python one-off patch scripts are fine **only if they contain
   no backslashes**: a heredoc on this setup turns `\n` / `\f` / `\b` into real
   control characters even inside quoted `<<'EOF'`. Anything with a backslash
   (Windows paths, `\n` in strings) goes in a script file in the scratchpad via
   `Write`, then `python file.py`.
9. Don't commit; the user commits. Don't change mod ID / name.
10. In-game augment text is Korean (names, descriptions, popup title) and must
    use the mod fonts in `src/ui/Fonts.hpp`; GD's fonts draw nothing for Hangul.
    Player-facing UI = `fonts::Name` / `fonts::Text` (ImcreSoojin, outlined);
    debug readouts (HUD lines) = `fonts::Debug` (Pretendard). Logs, notices and
    HUD wording stay English. New Korean literals need no extra step:
    `build.ps1` regenerates the charset and re-bakes the fonts.

## Code map

```
mod.json                     id, GD/Geode versions, fonts (resources.fonts = AugDebug, charset generated;
                             resources.files = baked UI fonts), settings (gauge numbers, keybinds, debug keys)
resources/fonts/             ImcreSoojin.ttf (UI) + Pretendard-Regular.ttf (debug HUD); gen/ (gitignored) holds
                             AugName/AugText sd/hd/uhd baked by scripts/fontgen.py (white, black outline, shadow)
CMakeLists.txt               forces clang on Windows, then standard Geode setup
src/main.cpp                 entry (load log only)
src/core/AugmentDef.*        static augment table: ids::*, Korean names, initial / level-up
                             descriptions, maxLevel, tune::* (steps, nerve mult, draft cards)
src/core/AugmentManager.*    run state singleton: run lifecycle, gauge, augment levels,
                             slow-mo toggle, director pause/resume for drafts, cursor state
src/ui/Fonts.hpp             fonts::Name / Text (ImcreSoojin, outlined) / Debug (Pretendard)
src/ui/AugmentDraftPopup.*   geode::Popup, bg hidden, GD-button-style cards (140x210: name / image box /
                             description fit-to-slot / footer pips), no close, mandatory pick; whole card
                             scaled when draft-count makes it 4; fan-out reveal driven from visit()
                             (director is paused -> cocos actions don't run)
src/ui/RunHud.*              top-left text lines + centre notice (mod fonts)
src/hooks/LevelInfoHook.cpp  AUG button → Start / Preview / Continue / Restart
src/hooks/PlayLayerHook.cpp  everything in-level: death counting, shield/noclip,
                             checkpoint, slow-mo (CCScheduler hook + FMOD pitch),
                             foresight (own CCDrawNode), unmirror, HUD refresh,
                             draft popup on resetLevel, hotkeys (node-scoped keybind
                             listeners in init + raw listener in $on_mod(Loaded)),
                             both hitbox scales published per frame (applyHitboxScales /
                             updateHitboxScales — nerve makes them depend on level progress)
src/hooks/HazardHitboxHook.* hazard-hitbox: global hazard scale (augment::hazard) + GameObject hooks
                             (getObjectRect AABB in place, updateOrientedBox OBB corners)
src/hooks/PlayerHitboxHook.* wave-hitbox: global player scale (augment::player) + GameObject hook on
                             the getObjectRect(w, h) overload, gated on PlayerObject + m_isDart
docs/                        STATUS / GD-INTERNALS / DESIGN / RECIPES / HARNESS-PLAN (keep current)
docs/refs/                   INDEX (problem → ref file:line) + one page per reference mod
scripts/                     build / logs / fetch-refs / bro / refgrep / nodeids / mods / fontcharset /
                             fontgen.py (Windows `py -3` + Pillow; Geode CLI's font "outline" is a no-op)
refs/                        reference mod sources (gitignored; MANIFEST.md lists pins)
```

Per-attempt state lives in `AugPlayLayer::Fields`; per-run state in `AugmentManager`.
Charges are derived (`level - used`) so an augment drafted mid-run works next attempt.

## Style

Match the existing files: `geode::prelude`, `$modify(AugX, X)` classes with a
`struct Fields`, `m_fields.self()`, comments only where the *why* isn't obvious,
`log::info` with `{}` formatting, English identifiers and strings, Korean is fine
in chat with the user.
