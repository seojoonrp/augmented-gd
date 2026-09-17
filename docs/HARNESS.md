# Harness — how an agent works on this mod

The mod is tested in game by the user only. Everything here exists to make each
of those rounds count: know GD facts before coding (docs + refs), catch what can
be caught without the game (tests, static audit), and leave a trail the next
session can pick up (STATUS / SESSIONS / GD-INTERNALS). History of how it was
built: `SESSIONS.md` (2026-09-16 "harness" entries); the original plan is
retired.

## The loop

1. Read `STATUS.md` → what is unverified and what the next test round should show.
2. Before touching a GD internal: `GD-INTERNALS.md` → `refs/INDEX.md` →
   `scripts\refgrep.ps1` → `scripts\bro.ps1` (CLAUDE.md rules 1–2).
3. Code. Every new path logs on entry and on early returns (rule 3).
4. `scripts\build.ps1` — runs the host tests, regenerates fonts, builds, installs.
   `scripts\check.ps1` after adding a hook or moving files.
5. Write the expected log lines / HUD text into the STATUS "Next step" column, then
   hand over to the user. Ask for `scripts\logs.ps1` output with every report.
6. End of session: STATUS (state only) + one SESSIONS entry + any new GD fact into
   GD-INTERNALS (marked verified / unverified) + RECIPES if a snippet is reusable.

## Scripts

| Script | Does | Notes |
|---|---|---|
| `build.ps1 [-Clean] [-Log] [-SkipTests]` | `test.ps1` → `fontcharset.ps1` → `fontgen.py` → `geode build --ninja` → install | Reloads PATH / `GEODE_SDK` from the registry (VS Code terminals are stale). Must end `\| Done \| Installed selenophile.augmented-gd.geode`. `-Clean` after CMake/CPM changes. |
| `test.ps1 [-Verbose]` | compiles `tests/core_tests.cpp` + `src/core/*.cpp` with clang (no Geode) into `build/tests/`, runs it | Plain `CHECK` asserts. Covers gauge economy, formulas, roll, table text. Non-zero exit fails the build. |
| `check.ps1 [-Bindings] [-Docs] [-Verbose]` | (1) every member defined in a `$modify(X, Base)` class is looked up in the 2.2081 bindings (Base, then parents): `win inline` / no Windows address = ERROR; (2) `path.cpp` / `File.cpp` cites in `docs/**` + `CLAUDE.md` must exist, `` `File.cpp` `sym` `` / `File.cpp::sym` must find `sym`, line-number cites into our sources warn | Skips `docs/refs/<mod>.md` (they cite the ref's tree) and any line naming a ref. Needs `build/_deps` (run a build once). |
| `diag.ps1 [-Pattern x] [-Level info\|warn]` | lists every `log::` line in `src/`, grouped by file | Replaces the hand-kept "diagnostic logging" list. Trim = delete the line. |
| `logs.ps1 [-All] [-Pattern x]` | this mod's lines (+ ERROR/WARN) from the newest Geode log | |
| `bro.ps1 Class [member]` / `-Grep x` | binding line(s) with a verdict: `win ok` / `win inline` (hook does nothing) / `geode inline` / `no win address` / `field`; lists `inline/*.cpp` impls | The answer to "can I hook / call this?". |
| `refgrep.ps1 pattern [-Ref a,b] [-Sdk] [-Bindings] [-All] [-Context n]` | rg over `docs/` + `refs/` (+ loader source, + bindings) | Refs are pinned (`fetch-refs.ps1`), so `docs/refs/*.md` line numbers stay valid. |
| `nodeids.ps1 Layer` | node IDs a layer gets from the NodeIDs mod (from `refs/node-ids` source) | |
| `mods.ps1` | mods installed in the user's GD and whether enabled | Interference analysis (CBF, death-tracker, custom-keybinds…). |
| `fetch-refs.ps1` | clones the reference mods into `refs/` at pinned commits, writes `refs/MANIFEST.md` | `refs/` is gitignored; `docs/refs/` is what is committed. |
| `fontcharset.ps1` | rebuilds the `AugDebug` charset in `mod.json` from every string literal in `src/` | Concatenated literals are fine; comments are ignored. |
| `fontgen.py` | bakes `AugName` / `AugText` (ImcreSoojin, white + black outline + shadow) sd/hd/uhd into `resources/fonts/gen/` | Windows `py -3` + Pillow + fonttools. Skips when the charset stamp is unchanged. Geode CLI's own `outline` key is a no-op. |

## Docs

| File | Holds | Rule |
|---|---|---|
| `STATUS.md` | verified list, broken/unverified table (state / evidence / **next step = the exact test**), diagnostics note | Current state only. Rewrite rows, don't append. |
| `SESSIONS.md` | one dated entry per working session | Append-only. What changed, what was verified, what is pending. |
| `GD-INTERNALS.md` | facts about GD / Geode, each with how it was verified | New fact → add it, marked **(verified)** or **(unverified)**. |
| `refs/INDEX.md` + `refs/<mod>.md` | problem → ref file:line; per-ref feature maps | Line numbers are pinned; the "Ours" column cites by file + symbol. |
| `RECIPES.md` | snippets in our style, verified / from-ref | Cite our code by file + symbol. |
| `DESIGN.md` | game rules, augment table, decisions | |
| `ROADMAP.md`, `VISUAL-IDEAS.md` | candidates by difficulty; visual feedback ideas | Planning material. |
| `REFACTOR-PLAN.md` | the 2026-09-17 refactor: diagnosis, target layout, steps, progress | Keep until step 4 is closed, then fold into SESSIONS. |

Citation format for our own sources: `` `src/game/DraftSession.cpp` `showNext` `` or
`DraftSession.cpp::showNext` — `check.ps1` verifies both and warns on line numbers.

## Code conventions the harness relies on

- `src/core/` never includes Geode: that is what makes `test.ps1` possible. Settings
  and randomness are passed in (`GaugeRule`, `std::mt19937&`).
- Hooks stay thin (`src/hooks/`); behaviour lives in `src/augments/` behind
  `Augment.hpp`. `check.ps1` audits every `$modify` member, so hooks must be
  written as `$modify` classes, not raw `Mod::get()->hook(...)` calls.
- Diagnostics are plain `log::info` — `diag.ps1` finds them; STATUS says which
  ones are still wanted.

## CI

`.github/workflows/build.yml` builds Windows only (the mod uses Windows-only input
and cursor APIs) and bakes the UI fonts first (`pip install pillow fonttools`,
`python scripts/fontgen.py`). Decided 2026-09-17; the other platforms can come
back if the mod ever targets them.
