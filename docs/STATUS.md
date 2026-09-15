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
| Hotkeys (Slow-Mo toggle, Checkpoint place) | **Not firing.** No log line from any of the three approaches tried. | `docs/GD-INTERNALS.md` → "Keyboard input" | Try the list there, starting with logging *all* keys to split "listener not registered" from "event not delivered". Then `UILayer::keyDown` hook. |
| Checkpoint respawn | Unverified — depends on the Z key. | — | Once keys work: verify respawn position/state, that a 2nd death restarts from 0, and that `m_checkpointArray` survives a normal-mode death (else the `storeCheckpoint` fallback kicks in — check the log for "Respawned from checkpoint"). |
| Unmirror | Unverified — no mirror-portal level tested. | Log said "neutralized 0 mirror portal(s)" on Retention (has none). | Test on a level with mirror portals. If the portal still flips, also call `removeObjectFromSection(obj)`. |
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
