# Status

Current state only — rewrite rows, don't append. History and reasoning:
`SESSIONS.md`. Every "Next step" is the exact in-game test, with the log lines
and HUD text it should produce (`scripts/logs.ps1` with every report).

## Verified working in game

- Build → auto-install → mod visible in Geode mod list.
- `AUG` button on LevelInfoLayer, Start / Preview / Continue / Restart flow.
- Death counting, draft gauge, draft popup on respawn, mandatory pick, cursor
  shown during draft and hidden again afterwards.
- Shield (hit consumed, noclip window, anticheat spike passes through), Slow-Mo
  (game + music, X toggle), Foresight (GD-style hitboxes, "perfect"), hotkeys X / Z,
  hazard-hitbox, wave-hitbox, nerve, draft-count (all 2026-09-16/17).
- Checkpoint: placing (Z) and a single respawn (2026-09-16).
- Draft card UI v2: baked outlined ImcreSoojin fonts, GD-button cards, pips,
  fan-out reveal, own word-wrap (2026-09-17, "다 잘 된다").
- HUD v2 skeleton: gauge bar, augment rows, notices, progress-bar marks once
  attached from `setupHasCompleted` (2026-09-17 screenshots).
- **Refactor steps 1 and 2a** (2026-09-17): pure `src/core` + host tests, and the
  augment-module split for shield / slow-mo / foresight / unmirror / draft-count /
  hitbox scales — user: "다 괜찮아", "괜찮음".
- **Brake** (브레이크, 11th augment, hold C → 40 %, 7 s/level real time) and
  **checkpoint v3** (newest placement only, one respawn then from 0; shield
  charges + brake seconds restored on respawn) — user: "잘 된다 굳" (2026-09-17).
- **Refactor 2b + 4** (2026-09-17): startpos and cat as `Augment` modules, HUD text
  from the session at 10 Hz, unmirror as a `toggleFlipped` hook — user: "다 잘 되는듯"
  (regular levels; a mirror-portal level is still to be tried, row below).

## Broken / unverified

| Item | State | Evidence | Next step |
|---|---|---|---|
| Unmirror on a mirror level | `GJBaseGameLayer::toggleFlipped` hook (refs pattern), fine on levels without portals; never run on one with. | — | Debug **5** on a mirror-portal level: portals do nothing, log `Unmirror: mirror portal ignored at p%`; drafting mid-mirror straightens the view (`Unmirror: drafted, un-flipped`). |
| **Draft gauge v2** | Opening draft moves in-level (`PlayLayer::startGame` hook, assumed once per load); no floor, new-best bonus, 40 +10 ramp, `debug-mode` keys — the economy itself is now host-tested (`tests/core_tests.cpp`), the in-game wiring is not. | Tests green; user saw the info-screen version render. | Start → level fades in → draft over the paused first frame (log `startGame: run level true, draft pending true`) → pick → play; HUD `0/40`. Die at a new best → yellow `NEW BEST +X`, log `Death #n at p% -> +p (+X new best), gauge g/40`. Gauge draft → `/50` next. Threshold 20, die at 60 % fresh → popups chain (`Draft: showing N cards, M more pending`), game resumes after the last pick. Key 0 → `GAUGE FULL +X (DEBUG)`, next death drafts. |
| **HUD v2d/e (bottom gauge + dots)** | Gauge mirrored to the bottom edge, `charge/cost` in GD's percent font, card-green fill; marks are rimmed dots inside the track. | v2c screenshot only. | Bottom-centre bar identical to the top one, `0/40` at run start, fills with deaths, `40/40` while a gauge draft waits. White best dot, green checkpoint dots. Log `RunHud gauge: … label copied` (`fallback` = percent label hidden in GD settings). |
| Draft after a checkpoint respawn | The draft waits for the reset that starts from 0 (`resume` flag from `onBeforeReset`). | — | Place checkpoint, die with the gauge full → respawn, no popup; die again → popup on the fresh attempt. |
| New numbers (design table) | noclip 1.5 s, slow-mo 5 %/level, startpos max 5, hazard-hitbox 5 %/level — formulas host-tested, in-game readouts not. | Tests green. | Grant with debug keys, read the HUD (`나무늘보 Lv1 ON (95%)`, `위협제거 Lv1 hazards 95%`); shield noclip counts from 1.5. |
| Slow-Mo with Click Between Frames | Unverified interaction. | CBF derives physics steps from `CCDirector` deltas; qolmod scales those too when CBF is loaded. | If Slow-Mo feels wrong, test with CBF disabled; if that fixes it, scale `m_fActualDeltaTime`/`m_fDeltaTime` as qolmod does. |
| Draft pending when re-entering a level | Handled (`isRunning()` guard), untested. | — | Quit with a draft pending, re-enter, die once → popup then. |
| CI workflow | Rewritten 2026-09-17 to Windows-only + font baking; never run. | — | Push and read the Actions log; `python scripts/fontgen.py` needs Pillow + fonttools (installed in the step). |

## Diagnostics

`scripts\diag.ps1` lists every `log::` line. All of them are wanted until the
rows above are verified; the ones to trim afterwards are the per-level
`ProgressBar not there yet` / `Attached gauge + marks` / `RunHud gauge: …` /
`ProgressMarks: …` lines, `startGame: …`, and the hotkey `via raw|setting`
traces. Keep for tuning: `Death #…`, `Draft: showing…`, the scale-change lines,
the checkpoint and cat lines.
