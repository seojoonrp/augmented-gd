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
- **Missile** (공습경보, 12th augment, strike on a random hazard ≥ 300 units
  ahead every 6 s, 3-block blast) — user: "괜찮은데" then "딱 괜찮은듯" with the
  300 lead (2026-09-20). The cat on the shared `HazardRemoval` scan is covered
  by the same rounds.
- **Shield bubble** (white bubble around the icon, ring burst on the absorbed hit, icon blinks 100/50/100 % through noclip via `PlayerObject::setOpacity`) — user: "잘 된다" (2026-09-27).

## Broken / unverified

| Item | State | Evidence | Next step |
|---|---|---|---|
| Unmirror on a mirror level | `GJBaseGameLayer::toggleFlipped` hook (refs pattern), fine on levels without portals; never run on one with. | — | Debug **5** on a mirror-portal level: portals do nothing, log `Unmirror: mirror portal ignored at p%`; drafting mid-mirror straightens the view (`Unmirror: drafted, un-flipped`). |
| **Draft gauge v2** | Opening draft moves in-level (`PlayLayer::startGame` hook, assumed once per load); no floor, new-best bonus, 40 +10 ramp, `debug-mode` keys — the economy itself is now host-tested (`tests/core_tests.cpp`), the in-game wiring is not. | Tests green; user saw the info-screen version render. | Start → level fades in → draft over the paused first frame (log `startGame: run level true, draft pending true`) → pick → play; HUD `0/40`. Die at a new best (whole percent, e.g. 4.4 → 5.0 = `+1`; 4.1 → 4.4 = nothing) → yellow `NEW BEST` (the number is the gold `+X` beside the icon), log `Death #n at p% -> +p (+X new best), gauge g/40`. Gauge draft → `/50` next. Threshold 20, die at 60 % fresh → popups chain (`Draft: showing N cards, M more pending`), game resumes after the last pick. Key 0 → `GAUGE FULL +X (DEBUG)`, next death drafts. |
| **Death reward animation** | `RunHud::playDeathReward`: numbers beside the dead icon (`+normal` green, `+bonus` gold), particles into the gauge, green fill then gold fill, gold blends in; driven by `RunHud::update` (scheduler), cut by `settleGauge` at attempt start. The centre notice is now just `NEW BEST`. Round 2 (particles readable, stale gold text fixed): user "잘 된다". Round 3: readout `DRAFT!` while a gauge draft waits (was `50/50`), new best in whole percents — untested. | Rounds 1–2 verified 2026-09-20. | Die at 25 % fresh: `+25` pops beside the icon, dots fly into the bar, the bar grows during the death delay (not after respawn), log `RunHud reward: +25 (+0 gold) at (x, y) -> tip (tx, ty) …` (tip must be on the bottom gauge: y ≈ 8, x between its ends), then `Attempt start N s after the death` (if `RunHud reward: cut at …` appears, the sequence is longer than GD's delay — shorten `kFlyTime`/`kFillTime`). Die at a new best: second gold `+X` under the green one, gold particles after the green ones, bar fills green then gold, gold turns green ~0.2 s later. |
| **HUD v2d/e (bottom gauge + dots)** | Gauge mirrored to the bottom edge, `charge/cost` in GD's percent font, card-green fill; marks are rimmed dots inside the track. | v2c screenshot only. | Bottom-centre bar identical to the top one, `0/40` at run start, fills with deaths, `DRAFT!` while a gauge draft waits (the count-up ends at `40/40` then flips to `DRAFT!`). White best dot, green checkpoint dots. Log `RunHud gauge: … label copied` (`fallback` = percent label hidden in GD settings). |
| **Wave icon shrink** | wave-hitbox now also scales the player node (`m_vehicleSize × scale`) and `m_waveTrail->m_waveSize` every frame while in wave, restores once on leaving wave / run end (`HitboxScales.cpp` `syncPlayerVisual`). | Builds. | Debug **7** (wave-hitbox), enter a wave section: icon and trail visibly smaller (90 % at Lv1, more with nerve later in the level), log once `WaveHitbox: player 1 icon x0.90 (node scale 0.90)` (0.54 if mini); leave wave → normal size, log `WaveHitbox: player 1 icon restored`. Mini portal inside wave: size snaps instead of animating (accepted). Dual: only the wave half shrinks. |
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
