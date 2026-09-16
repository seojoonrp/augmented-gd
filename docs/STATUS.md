# Status

Update this file at the end of every working session: what was verified in
game, what is broken, what to try next. Dates are absolute.

## Verified working in game (2026-09-16)

- Build → auto-install → mod visible in Geode mod list.
- `AUG` button on LevelInfoLayer, Start / Preview / Continue / Restart flow.
- Death counting, draft gauge, draft popup on respawn, mandatory pick, cursor
  shown during draft and hidden again afterwards.
- Shield: hit consumed, 3 s noclip, dies again after; anticheat-spike call
  passes through (no more phantom deaths / instant shield loss).
- Slow-Mo: game speed and music slow down together (default ON after pick).
- Foresight: GD-style hitboxes (user: "perfect").
- HUD lines and notices.
- Hotkeys X (Slow-Mo toggle) / Z (Checkpoint place) — user confirmed after the
  2026-09-16 fix (loader listener order + Custom Keybinds Z/X, see GD-INTERNALS).

## Broken / unverified

| Item | State | Evidence | Next step |
|---|---|---|---|
| Draft after a checkpoint respawn | Fixed, untested: `resetLevel` skips the draft when `fromCheckpoint`, so it waits for the reset that starts from 0. | User report 2026-09-16 | Place checkpoint, die with the gauge full → respawn with no popup; die again → popup on the fresh attempt. |
| Debug augment keys 1–5 | New, untested. Raw key path; setting `debug-augment-keys` (default on). | — | In a run press 1..5 → HUD "+Name LvN (DEBUG)", log `Debug grant: 'id' -> level N`; 6–9 do nothing; maxed → "MAXED" notice. Slow-Mo applies speed immediately, Unmirror runs `applyUnmirrorNow`. |
| Checkpoint respawn | Z works; respawn itself unverified. | — | Once keys work: verify respawn position/state, that a 2nd death restarts from 0, and that `m_checkpointArray` survives a normal-mode death (else the `storeCheckpoint` fallback kicks in — check the log for "Respawned from checkpoint"). |
| Unmirror | Unverified — no mirror-portal level tested. | Log said "neutralized 0 mirror portal(s)" on Retention (has none). | Replace object surgery with a `GJBaseGameLayer::toggleFlipped` hook that returns early (qolmod/xdBot pattern, `docs/refs/INDEX.md`). Then test on a mirror level. |
| Slow-Mo with Click Between Frames | Unverified interaction. | CBF (installed) derives physics steps from `CCDirector` deltas; qolmod scales those too when CBF is loaded. | If Slow-Mo feels wrong, test with CBF disabled; if that fixes it, scale `m_fActualDeltaTime`/`m_fDeltaTime` as qolmod does. |
| Draft pending when re-entering a level | Handled (`isRunning()` guard) but not tested. | — | Quit with a draft pending, re-enter, die once → popup should appear then. |

## Diagnostic logging currently in the code

These `log::info` lines are intentional and should stay until the feature is
verified, then be trimmed:
- `Debug grant: …` — number keys
- `Hotkey raw listener registered` / `Hotkey setting listeners attached to PlayLayer`
- `Hotkey X via raw|setting (frame N)` / `… duplicate in frame N, ignored` / `… ignored: draft open|no PlayLayer`
- `X ignored: …` / `Z ignored: …` — hotkey handlers' early returns
- `Foresight: tracking N objects`
- `Game speed -> …`
- `Death #N at …% -> +charge, gauge …` — keep (useful for tuning)

## Session log

- **2026-09-15** — Environment set up (winget: git/cmake/ninja/LLVM/geode CLI; VS Build Tools already present). MSYS2 GCC on PATH broke the first build → CMakeLists defaults to clang on Windows. Project scaffolded, M2–M4 implemented.
- **2026-09-16** — 5 augments + gauge + HUD implemented. Fixed: anticheat-spike death counting, slow-mo via scheduler hook + FMOD master pitch, hitboxes drawn manually (GD's own path is byte-patch-only). Still open: hotkeys. Reference mod sources cloned into `refs/`.
- **2026-09-16 (harness)** — `docs/HARNESS-PLAN.md` phases 0–3 done: `refs/` re-selected for GD 2.2081 / Geode 5 (openhack dropped as stale; qolmod, xdbot, CBF, death-tracker, cleanstartpos, betterinfo, node-ids, devtools, example-mod, geode-docs added, commits pinned, `refs/MANIFEST.md` generated); scripts `bro.ps1` / `refgrep.ps1` / `nodeids.ps1` / `mods.ps1`; `docs/refs/INDEX.md` + per-ref pages; `docs/RECIPES.md`; GD-INTERNALS extended with the findings (mirror via `toggleFlipped`, checkpoint bindings, `m_isDead` death test, clear-counting flags, key delivery order, CBF/death-tracker interference). Phase 4 (hotkey fix through the harness) is the next in-game test. No code changed this session.
- **2026-09-16 (hotkey fix)** — Found the hotkey root cause by reading loader source (no guessing): the loader's keybind listener is registered before mod binaries load and returns `Stop` when Custom Keybinds' Z/X (practice checkpoint) listeners do, so our listener never ran in a level. Fix in `PlayLayerHook.cpp` + `mod.json`: settings `"priority": -5` + node-scoped `KeybindSettingPressedEventV3` listeners in `PlayLayer::init`, and a raw `KeyboardInputEvent` listener at priority -1 from `$on_mod(Loaded)`; per-frame dedup on the layer; `toggleSlowMo` / `tryPlaceCheckpoint` now return whether they consumed the key. Builds and installs; awaiting the in-game test above.
- **2026-09-16 (hotkeys verified)** — User confirmed X/Z work. Trimmed the per-key `Raw key press` log. Added: draft no longer pops on a checkpoint respawn (waits for the from-0 reset); debug number keys 1..N grant one level of the N-th augment (`AugmentManager::grant`, `AugPlayLayer::debugGrantAugment`, setting `debug-augment-keys`). Both untested in game.
- **2026-09-16 (later)** — Planning only, no code. Wrote `docs/ROADMAP.md`: goals, difficulty tiers (T1–T4 = number of unverified GD facts), ~20 candidate augments, systems, rejected list, build order. Recorded the bindings checked in `GD-INTERNALS.md`.
