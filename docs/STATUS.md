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

## Broken / unverified

| Item | State | Evidence | Next step |
|---|---|---|---|
| Hotkeys (Slow-Mo toggle, Checkpoint place) | **Not firing.** No log line from any of the three approaches tried. | `docs/GD-INTERNALS.md` → "Keyboard input"; `docs/refs/custom-keybinds.md` "Open problem" | Our `$execute` listener equals qolmod's working one, so first prove the code runs (`log::info` in `$execute` / `$on_mod(Loaded)`, log *all* keys). Then switch to the node-scoped `addEventListener(KeybindSettingPressedEventV3(...))` in `PlayLayer::init`. |
| Checkpoint respawn | Unverified — depends on the Z key. | — | Once keys work: verify respawn position/state, that a 2nd death restarts from 0, and that `m_checkpointArray` survives a normal-mode death (else the `storeCheckpoint` fallback kicks in — check the log for "Respawned from checkpoint"). |
| Unmirror | Unverified — no mirror-portal level tested. | Log said "neutralized 0 mirror portal(s)" on Retention (has none). | Replace object surgery with a `GJBaseGameLayer::toggleFlipped` hook that returns early (qolmod/xdBot pattern, `docs/refs/INDEX.md`). Then test on a mirror level. |
| Slow-Mo with Click Between Frames | Unverified interaction. | CBF (installed) derives physics steps from `CCDirector` deltas; qolmod scales those too when CBF is loaded. | If Slow-Mo feels wrong, test with CBF disabled; if that fixes it, scale `m_fActualDeltaTime`/`m_fDeltaTime` as qolmod does. |
| Draft pending when re-entering a level | Handled (`isRunning()` guard) but not tested. | — | Quit with a draft pending, re-enter, die once → popup should appear then. |

## Diagnostic logging currently in the code

These `log::info` lines are intentional and should stay until the feature is
verified, then be trimmed:
- `Raw key … matches: …` / `Hotkey … pressed` — keyboard path
- `X ignored: …` / `Z ignored: …` — hotkey handlers' early returns
- `Foresight: tracking N objects`
- `Game speed -> …`
- `Death #N at …% -> +charge, gauge …` — keep (useful for tuning)

## Session log

- **2026-09-15** — Environment set up (winget: git/cmake/ninja/LLVM/geode CLI; VS Build Tools already present). MSYS2 GCC on PATH broke the first build → CMakeLists defaults to clang on Windows. Project scaffolded, M2–M4 implemented.
- **2026-09-16** — 5 augments + gauge + HUD implemented. Fixed: anticheat-spike death counting, slow-mo via scheduler hook + FMOD master pitch, hitboxes drawn manually (GD's own path is byte-patch-only). Still open: hotkeys. Reference mod sources cloned into `refs/`.
- **2026-09-16 (harness)** — `docs/HARNESS-PLAN.md` phases 0–3 done: `refs/` re-selected for GD 2.2081 / Geode 5 (openhack dropped as stale; qolmod, xdbot, CBF, death-tracker, cleanstartpos, betterinfo, node-ids, devtools, example-mod, geode-docs added, commits pinned, `refs/MANIFEST.md` generated); scripts `bro.ps1` / `refgrep.ps1` / `nodeids.ps1` / `mods.ps1`; `docs/refs/INDEX.md` + per-ref pages; `docs/RECIPES.md`; GD-INTERNALS extended with the findings (mirror via `toggleFlipped`, checkpoint bindings, `m_isDead` death test, clear-counting flags, key delivery order, CBF/death-tracker interference). Phase 4 (hotkey fix through the harness) is the next in-game test. No code changed this session.
- **2026-09-16 (later)** — Planning only, no code. Wrote `docs/ROADMAP.md`: goals, difficulty tiers (T1–T4 = number of unverified GD facts), ~20 candidate augments, systems, rejected list, build order. Recorded the bindings checked in `GD-INTERNALS.md`.
