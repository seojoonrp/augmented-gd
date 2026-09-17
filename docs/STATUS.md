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

## Broken / unverified

| Item | State | Evidence | Next step |
|---|---|---|---|
| **Refactor 2b + 4 (startpos, cat, session HUD, unmirror hook)** | Built 2026-09-17. `src/augments/StartPos.cpp` / `Cat.cpp` replace the last inline code in `PlayLayerHook.cpp`; `LevelSession::refreshHud` rebuilds text at 10 Hz; unmirror = `GJBaseGameLayer::toggleFlipped` hook. Intended change: practice mode on a run level no longer wipes the player's own checkpoints. | Builds; `check.ps1` 16 hooks `win ok`; core tests 582/582. | Debug on. **3** ×2 + **Z**: place, die → `CHECKPOINT` respawn, place again, die → newest, die → older, die → from 0; log `Checkpoint placed (n/lvl)…` / `Checkpoint: death with … -> respawn` / `Respawned from checkpoint` / `Checkpoint: consumed (… )` with no `still on top!`; green dots on GD's bar follow placements. **Shift+1** cat: `CAT -n` notices every 4 s, spikes ahead vanish, `Cat: removed n/5 …`, on death `Cat: restored K hazards`. HUD rows update ~10×/s (timers still read smoothly). **5** on a mirror level: portals do nothing, log `Unmirror: mirror portal ignored at p%`; drafting mid-mirror straightens the view (`Unmirror: drafted, un-flipped`). Practice mode on a run level: own checkpoints survive resets. Quit → next level normal. No `beginLevel: replacing a session` in the log. |
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
