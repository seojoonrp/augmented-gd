# Refactor plan — codebase + harness (started 2026-09-17)

Behaviour-preserving until step 4. Each step is one build; in-game regression
only where marked. Tick items with the date as they land.

## Diagnosis

Code
- `src/hooks/PlayLayerHook.cpp` (1065 lines) holds every augment's mechanics,
  hotkeys, debug keys, HUD text and the draft flow. Adding an augment touches
  six places (`Fields`, `resetLevel`, `postUpdate`, `refreshHud`,
  `debugGrantAugment`, the pick callback).
- "On granted" side effects are duplicated between `debugGrantAugment` and the
  pick callback (and differ: SlowMo only in one).
- `AugmentManager` mixes run state, gauge economy, per-augment formulas and
  director pause / cursor state; reads settings directly, so it cannot run
  outside GD.
- Numbers live twice: `tune::` constants and the literal card text.
- Three "global scale, log on change" copies (`g_timeScale`, `hazard::g_scale`,
  `player::g_waveScale`).
- `refreshHud` rebuilds ~10 fmt strings per frame; augment lookups go through
  `std::map<std::string,int>`.
- No automated verification at all.

Harness
- Session start reads ~1000 lines; half of STATUS is an append-only log.
- `HARNESS-PLAN.md` is a finished plan; nothing describes the harness as-is.
- Docs cite our own sources by `file:line`, which rots on every edit.
- Rule 2 (binding verdicts) is manual; the STATUS "diagnostic logging" list is
  hand-maintained and already drifting.
- CI workflow is the Geode template (5 platforms, gitignored baked fonts).

## Target layout

```
src/core/      no Geode headers; host-compilable and unit-tested
  AugmentDef   table (id, name, maxLevel, description templates) + tune:: + formulas
  RunState     run state, gauge economy, levels, pending drafts (settings injected)
  Draft        rollDraft(state, rng, count)
src/game/      Geode glue
  AugmentManager   singleton wrapping RunState + settings + logging
  DraftSession     director pause/resume, cursor, popup chaining
  Scales           time / hazard / wave global scales in one place
src/augments/  one file per augment behind a common interface
  Augment.hpp      onAttemptStart / onDeath / onFrame / onGranted / hudState / onQuit
  Shield SlowMo StartPos Foresight Unmirror HitboxScales(hazard+wave+nerve) Cat DraftCount
src/hooks/     PlayLayerHook (lifecycle only, dispatches), GameObjectHooks, SchedulerHook, LevelInfoHook
src/input/     Hotkeys (raw + setting paths, frame dedup, debug keys)
src/ui/        unchanged
tests/         core/ unit tests, no dependencies
```

Decisions
- Augment instances are owned by a per-level session reachable through
  `AugmentManager::get().session()`, not by `$modify` Fields, so popup
  callbacks / hotkeys never need the `AugPlayLayer` type (CLAUDE.md rule 5).
  Session is created in `PlayLayer::init`, dropped in `onQuit`, and every
  use checks `PlayLayer::get()` identity.
- Descriptions are fmt templates over `tune::` so every number has one home.

## Steps

- [x] **0. Safety net** (2026-09-17) (no in-game test): `docs/REFACTOR-PLAN.md`,
  `scripts/check.ps1` (binding verdict audit of every `$modify` override +
  doc → source reference check), `scripts/diag.ps1` (inventory of `log::`
  lines). User commits before step 1.
- [x] **1. Core extraction + tests** (2026-09-17, built; in-game smoke pending) (in-game smoke ×1: start run → die →
  draft → HUD numbers unchanged): `RunState`, formulas into `AugmentDef`,
  description templates, `tests/` + `scripts/test.ps1`, `build.ps1 -Test`.
- [x] **2a. Augment modules** (2026-09-17, built; regression pending) (in-game regression ×1):
  `Augment` interface, `DraftSession`, `Scales`, `input/Hotkeys.cpp`; move
  shield, slow-mo, foresight, unmirror, draft-count — and HitboxScales
  (hazard+wave+nerve) too, since it only needs `onGranted` fan-out.
- [x] **2b. Stateful augments** (2026-09-17, built; regression pending): startpos (checkpoint
  array sync; drops `hotkeys::legacyRoute`), cat (restore). HUD slots then
  come entirely from the session and `refreshHud` moves into it.
- [x] **3. Docs** (2026-09-17): STATUS = current state only, log →
  `SESSIONS.md`; `HARNESS-PLAN.md` → `HARNESS.md`; our-source cites by
  file + symbol; CLAUDE.md code map; CI decision.
- [~] **4. Follow-ups that change behaviour**: Unmirror via `toggleFlipped`
  hook and the HUD refresh throttle done 2026-09-17 (untested). Not done, on
  purpose: run persistence is a feature (ROADMAP T2, now a one-function
  serialisation of `RunState`), enum ids are optional.

## Progress

- 2026-09-17: plan written.
- 2026-09-17: step 0 — `scripts/check.ps1` (bindings audit: all 14 hooks `win ok`;
  doc cites: 0 errors, 2 line-number warnings left for step 3), `scripts/diag.ps1`.
- 2026-09-17: step 1 — `src/core/{AugmentDef,Formulas,RunState}` pure; `AugmentManager`
  moved to `src/game/` with the same public API (only `dropPendingDrafts()` added and the
  preview draft now uses `tune::DefaultDraftCards`). Descriptions built from `tune::`.
  `tests/core_tests.cpp` 582 checks pass via `scripts/test.ps1`; `build.ps1` runs it first.
  Verified in game by the user the same day.
- 2026-09-17: step 2a — see STATUS. Design notes: the session is made before
  `PlayLayer::init` (`onLevelInit` publishes globals objects read as they are
  created); hooks look it up with `sessionFor(this)`, everyone else with
  `session()` (checks `PlayLayer::get()`); every augment hears every grant
  (`onGranted(id, level)`), which is how nerve reaches HitboxScales without
  wiring. `draft::close()` restores the cursor, `draft::abandon()` (quit / run
  start / run end) does not — a hidden cursor in a menu would be worse than
  a visible one in a level.
- 2026-09-17: 2a verified by the user. Steps 2b, 3 and the two step-4 items done
  in one go (user: "나머지 그냥 한번에 다 해줘"); see SESSIONS.md for the details.
  `PlayLayerHook.cpp` is now 170 lines with nothing augment-specific in it.
