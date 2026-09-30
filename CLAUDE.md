# Augmented GD — agent harness

Geode mod for Geometry Dash (2.2081, Geode 5.10.1, Windows). Turns a level into a
roguelite run: die → draft an augment → get stronger → clear.
Mod ID `selenophile.augmented-gd`. The user tests in game; you cannot.

Read in this order at the start of a session:
1. `docs/STATUS.md` — what works, what's broken, what to test next (short; history is in `docs/SESSIONS.md`).
2. `docs/GD-INTERNALS.md` — verified facts about GD/Geode. **Check it before touching anything GD-internal.**
3. `docs/refs/INDEX.md` — problem → "how a working mod does it" (file:line in `refs/`).
4. `docs/DESIGN.md` — game rules and decisions.
5. `docs/RECIPES.md` — snippets in our style, each marked verified / from-ref.

`docs/HARNESS.md` explains every script and doc convention; open it when a
script's flags or a doc's rules matter. Update `docs/STATUS.md` (state) and
`docs/SESSIONS.md` (one entry) at the end of every session.

## Build & test loop

```powershell
.\scripts\build.ps1          # host tests → fontcharset → fontgen → geode build --ninja → install into GD
.\scripts\build.ps1 -Clean   # after CMake/CPM/env changes ("Unknown CMake command CPMAddPackage" → this)
.\scripts\test.ps1           # tests/core_tests.cpp against src/core, clang only, no Geode
.\scripts\check.ps1          # every $modify override must be `win ok`; doc → source cites must resolve
.\scripts\logs.ps1           # [Augmented GD] lines from the newest Geode log
.\scripts\diag.ps1 [-Pattern x]    # inventory of log:: lines in src/
.\scripts\bro.ps1 Class [member]   # binding line + hookable verdict (win ok / win inline / field)
.\scripts\refgrep.ps1 pattern [-Sdk|-All]   # rg over refs/ (+ loader source, bindings)
.\scripts\nodeids.ps1 Layer / mods.ps1 / fetch-refs.ps1 / fontcharset.ps1   # see docs/HARNESS.md
```

- Build must end with `| Done | Installed selenophile.augmented-gd.geode` and exit 0;
  `check.ps1` must report 0 errors after any hook or file move.
- `src/core/` has no Geode headers and is covered by `tests/core_tests.cpp` (gauge
  economy, formulas, roll, card text). A rule change there = a test change.
- Plain `geode`/`clang` may be missing in VS Code terminals (stale PATH). The scripts
  reload PATH from the registry; if running commands by hand, do the same first.
- The user launches GD and reports. Ask for the log (`scripts/logs.ps1`) with every
  bug report; don't guess from the description alone.
- Environment: SDK at `$env:GEODE_SDK` (`C:\Users\seojo\Documents\Geode`), CPM cache
  `C:\Users\seojo\.cpm-cache`, GD at `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash`.
  Compiler is LLVM clang (MSYS2 GCC is also on PATH and must **not** be picked — CMakeLists handles it).
- `.vscode/` is gitignored (CMake Tools: Ninja, compile_commands; clangd `-header-insertion=never`).

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
   every class member / function you use (`check.ps1` re-audits the hooks). `= inline` /
   `win inline` functions cannot be hooked on Windows.
3. **Instrument before the user tests.** Every new code path gets a `log::info` on entry
   and on each early return, so one failed test round pinpoints the stage. These are
   **temporary**: delete them as soon as the user confirms the feature (`scripts\diag.ps1`
   lists what is there). The shipped mod stays quiet (2026-09-30 cleanup, 136 → ~15
   lines): `log::info` only for run start / end, a pick and one line per counted death;
   `log::warn` for real failures (a node or sprite that should exist is missing). No
   per-frame, per-hit, per-object or "node created" lines left behind.
4. **One failed round → change approach, not parameters.** If a test shows *no* log
   line from a path, the path isn't reached; don't re-tune it.
5. Never hold a `PlayLayer*` across frames (popup callbacks, listeners): ask
   `AugmentManager::get().session()` at call time; it is null once the layer is gone.
6. Never block or count `destroyPlayer(player, m_anticheatSpike)`.
7. Geode objects: `cast::typeinfo_pointer_cast`, not `dynamic_pointer_cast`.
8. Prefer `Write`/`Edit` tools over bash heredocs for source files (quoting issues on
   this Windows setup). Python one-off patch scripts are fine **only if they contain
   no backslashes**: a heredoc on this setup turns `\n` / `\f` / `\b` into real
   control characters even inside quoted `<<'EOF'`. Anything with a backslash
   (Windows paths, `\n` in strings) goes in a script file in the scratchpad via
   `Write`, then `python file.py`.
9. Don't commit; the user commits. Don't change mod ID / name.
10. Player-facing text comes in **English and Korean**, picked by the `language`
    setting (2026-09-29): every new line needs both. Augment names / card texts
    are `LocalText { en, ko }` in `AugmentDef.cpp` (host tests check both exist
    and that English has no Hangul); any other line is `tr("English", "한국어")`
    (`src/game/Language.hpp`) where it is shown. Both languages use the mod fonts
    in `src/ui/Fonts.hpp`; GD's fonts draw nothing for Hangul (the mod's settings
    page and `about.md` are GD fonts: ASCII only there). Player-facing UI =
    `fonts::Name` / `fonts::Text` (ImcreSoojin, outlined); debug readouts (HUD
    lines) = `fonts::Debug` (Pretendard). Logs and debug HUD wording stay
    English. New Korean literals need no extra step: `build.ps1` regenerates the
    charset and re-bakes the fonts.
11. Docs cite our own sources as `` `path/File.cpp` `symbol` `` or `File.cpp::symbol`,
    never by line number (`check.ps1` warns). Line numbers are fine for `refs/` (pinned).

## Code map

```
mod.json                 id, GD/Geode versions, fonts (AugDebug generated, charset from src/; baked UI
                         fonts via resources.files, card art via resources.sprites), settings (language, sound-effects,
                         draft-bar-opacity, keybinds). **Debug mode is off for release (2026-09-30)**: the
                         `debug-mode` / `debug-readout` settings are gone from mod.json and
                         `AugmentManager.cpp` `kDebugBuild` is false. For a test round that needs debug keys, flip
                         it and paste the two settings back (JSON in docs/RECIPES.md "Debug mode"), then undo
                         both before any commit.
resources/fonts/         ImcreSoojin.ttf (UI) + Pretendard (debug HUD); gen/ (gitignored) = baked AugName/AugText
resources/gallery/       about.md screenshots, 1280 wide PNG (mod sprites, since Geode's in-game markdown only
                         draws local sprites: `![cap](selenophile.augmented-gd/0-start.png?width=300)`)
resources/augments/      one 480x280 card image per augment, named <id>.png (Geode bakes hd/sd from it)
resources/sfx/           hover / select / missile .wav, cut from source/ by scripts/sfxcut.py
resources/ui/            aug-logo.png (the mark inside the round AUG button — every new export goes through
                         scripts/logocrop.py, or its margin shrinks the mark); cat-idle-1/2 + cat-cast.png (the corner
                         cat's frames, placeholders from scripts/catgen.py); logo.png at the root = mod list
src/core/                pure C++, host-tested (tests/core_tests.cpp)
  Lang.hpp               Lang { English, Korean } + LocalText { en, ko }
  AugmentDef.*           table: ids::*, names (en + ko), maxLevel, descriptions built from tune:: at startup
                         (a late level-up text from `lateFrom` when the last levels differ, e.g. cat Lv6-7);
                         describe(level, lang) / describeAt(level, lang) = card text / that level's numbers (detail card)
  Formulas.hpp           level → effect (slowMoScale, nerveBoost, hazard/waveScale, cat*, draftCardCount)
  RunState.*             one run: gauge economy (GaugeRule injected), levels, pending drafts, slow-mo toggle
src/game/                Geode glue
  AugmentManager.*       singleton: RunState + settings + logging; owns the LevelSession
                         (beginLevel / endLevel; session() checks PlayLayer::get(), hooks use sessionFor(this))
  LevelSession.*         one PlayLayer of a run level: the Augment objects, HUD + progress marks,
                         forEachObjectInX (walks GD's live section grid, so moved objects count; no index of ours),
                         death-once, debug grants, refreshHud (10 Hz); fans lifecycle events out in table order
  DraftSession.*         draft::showNext / isOpen / abandon — popup, director pause, cursor, chaining;
                         the pending draft is taken on the pick, not on show (leaving mid-draft keeps it);
                         PlayLayerHook::pauseGame refuses to pause while isOpen()
  Scales.*               scales::time / hazard / wave globals the hot hooks read (inline getters, logging setters);
                         time = base (slow-mo) with an override layer (brake)
  Language.*             language() from the `language` setting, tr(en, ko) for lines outside the table
  Sfx.*                  sfx::play(Cue): cue table (file, gain, game-time pitch), FMOD direct, `sound-effects` setting
  RunSummary.*           summary::open: the run summary for the live session (the pause menu's run button and
                         the draft popup's top-right one; no pop-in action while the director is paused)
  Records.*              the runs' own per-level record (Geode saved values) + records::hiddenFromGd()
                         (run attempts never reach GD's normal best / New Best! / clear)
src/augments/            one file per augment behind Augment.hpp (all hooks default to no-op):
  Augment.hpp            onLevelInit (before PlayLayer::init) / onLevelReady / onObjectAdded / onBeforeReset→resume? /
                         onAttemptStart(fromCheckpoint) / onCheckpointPlaced→snapshot / onCheckpointRespawn→restore /
                         onHit→swallow / onDeath→respawning? (defers the gauge) / onFrame / onPause /
                         onGranted(id, lv) (every augment hears every grant) / onHotkey(key, down) / hudState(id) / onQuit
  Augments.*             factories + makeAllAugments() (table order)
  Shield SlowMo StartPos Foresight Unmirror(+toggleFlipped hook) HitboxScales(hazard+wave+nerve) DraftCount Cat
                         Brake (held C, scales::setTimeOverride over SlowMo's base speed)
                         Missile (timed strike on a random hazard in view, blast-radius removal)
                         Berserk (hazard destruction rolls a window that smashes hazards on contact)
  HazardRemoval.*        shared by Cat + Missile: hazard::Removed (take / restore), viewAhead / hazardsInView, touchesCircle
src/input/Hotkeys.*      Hotkey enum, hotkeys::route (draft-open guard, per-frame dedup → session->onHotkey),
                         raw KeyboardInputEvent listener at priority -1, debug number keys
src/hooks/               PlayLayerHook (lifecycle only, dispatches to the session), LevelInfoHook (round AUG button;
                         RunPromptPopup when this level has a run, or another level's run would be dropped),
                         HazardHitboxHook (hazard::isTarget + GameObject rect/OBB hooks), PlayerHitboxHook
                         (getObjectRect(w,h) for the wave player), SchedulerHook (dt * scales::time()),
                         GdRecordHook (savePercentage / showNewBest blocked on run attempts, end-screen quote),
                         PauseLayerHook (run button in place of practice → RunInfoPopup)
src/ui/                  Fonts.hpp, CardStyle.hpp (rim/panel/art slot), AugButton.hpp (round mark button),
                         AugmentCard (the card itself, shared), AugmentDraftPopup (cards, reveal from visit()),
                         RunInfoPopup (pause-menu run summary: stat chips + tile grid, hover ring, click →
                         AugmentInfoPopup = the card at the held level's text), RunHud (bottom gauge, debug-mode rows,
                         bottom-left notices), ProgressMarks (dots on GD's bar), CatNode (the corner cat's frames + doodle magic circle sprites), MissileNode (reticle / drop / blast, world space, white),
                         BerserkNode (screen frame + smash bursts), BerserkAura (fire on the player, object layer under the icon),
                         RunPromptPopup (the AUG button's two-button questions: resume() / replace())
tests/core_tests.cpp     host tests, plain asserts (scripts/test.ps1)
docs/                    DESIGN / STATUS / SESSIONS / GD-INTERNALS / RECIPES / HARNESS / ROADMAP / VISUAL-IDEAS / REFACTOR-PLAN / refs/ / gallery/
scripts/                 see docs/HARNESS.md
```

Per-run state: `RunState`. Per-level: `LevelSession`. Per-attempt: each `Augment`'s own
members, reset in `onAttemptStart`. Adding an augment = one row in `AugmentDef.cpp` + one
file in `src/augments/` + one line in `makeAllAugments()` (+ a debug key follows for free).

## Style

Match the existing files: `geode::prelude`, `$modify(AugX, X)` classes for hooks,
`Augment` subclasses in an anonymous namespace with a `makeX()` factory, `s.` for the
session, `log::info` with `{}` formatting, English identifiers and strings, Korean is
fine in chat with the user.

**Everything is tracked and public**, this harness included. The mod was built with
Claude Code and says so: the Geode index rejected v1.0.0 under `reject-vibecoded`
(2026-09-30), the user asked the index staff what would make it acceptable, and chose
to publish the whole workflow rather than keep part of it local. Never hide or scrub
AI involvement (commit trailers, these docs, the history).

Comment style is still a preference:

- Source comments are short notes: only the non-obvious *why*, 1-2 lines, not on every
  function. A file header is 0-2 lines.
- Keep dates, "the user said" provenance, change history ("was X") and `refs/...:line`
  paths out of `src/` and `tests/`: history and evidence go to `docs/SESSIONS.md`, open
  questions to `docs/GD-INTERNALS.md` / `docs/STATUS.md`. A plain credit is fine in
  source ("same trick as qolmod's safe mode").
- `docs/DESIGN.md` is plain design notes (rules, formulas, reasons). Keep it current when
  a rule changes.
- `docs/gallery/` = README screenshots (full size, 0-4 in order).
