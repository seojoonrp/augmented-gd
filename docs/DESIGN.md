# Augment Mode — game design

A level becomes a **run**: attempts accumulate augments until the level is
cleared. Dying keeps your augments; clearing ends the run.

## Run

- Started from the level info screen (`AUG` button → Start). One run per level
  ID at a time; starting again on the same level offers Continue / Restart.
- Ends on `levelComplete`. Leaving the level does **not** end it.
- Practice / test-mode attempts never count and get no augments.
- Level clears made with augments currently **count as normal GD clears**.
  Decided 2026-09-16: ignore for now; revisit before any public release.
  Mechanism when we do: flip `m_isTestMode` / `m_isPracticeMode` around
  `PlayLayer::levelComplete()` (see `docs/GD-INTERNALS.md` "Does a clear count?").
- Public release note (2026-09-16): `$GEODE_SDK/AGENTS.md` states the Geode
  index does not accept AI-written mods. Private use is unaffected; any index
  submission is the user's call.

## Draft gauge (decided 2026-09-16)

Each death charges the gauge by `max(min-charge, percent reached)`. When it
reaches `draft-threshold`, a draft is queued; leftover charge carries over.
At most one draft per death. Both numbers are mod settings (defaults 10 / 100;
use ~30 for fast test loops).

Rationale: pure death-count rewards nothing for progress; pure new-best stalls
exactly when the player is stuck. This guarantees drafts when stuck and speeds
them up when progressing. Future tuning idea: raise the threshold per draft.

## Draft

Three random augments that are not yet maxed, shown on respawn. Picking is
mandatory (no close button, back key ignored). Duplicate picks level the
augment up. Pool is ~10 augments eventually; 6 now.

## Augments (current pool)

| id | name | levels | effect |
|---|---|---|---|
| `shield` | Shield | 3 | 1/2/3 shields per attempt. A hit consumes one and gives 3 s of noclip. Covers both players in dual. |
| `slowmo` | Slow-Mo | 3 | Game + music at 93/86/79 %. Toggle with the Slow-Mo hotkey (default X); toggle state persists within the run. Pause menu runs at normal speed. |
| `checkpoint` | Checkpoint | 3 | Hotkey (default Z) places up to 1/2/3 checkpoints per attempt. On death, respawn once at the latest checkpoint; the next death restarts from the beginning. Placements are consumed even if a later one supersedes them. |
| `foresight` | Foresight | 1 | Draws hitboxes (GD colours: blue solid, red hazard, green interactive, yellow player). |
| `unmirror` | Unmirror | 1 | Neutralises every mirror portal, including ones already loaded when drafted. |
| `blunt` | Blunt | 3 | Hazard hitboxes (Hazard / AnimatedHazard types: spikes, saws, …) shrink around their centre to 80/60/40 %. Solids, slopes and the player are untouched. Numbers are test values (2026-09-16); real steps will be smaller. |

Definitions live in `src/core/AugmentDef.cpp`; behaviour in `src/hooks/PlayLayerHook.cpp`.

## HUD

Top-left: gauge, deaths, live %, best %, then one line per owned augment with
its per-attempt state. Centre notices for events (SHIELD BROKEN, CHECKPOINT
PLACED, SLOW-MO ON/OFF…).

## Not decided yet

- Real draft trigger tuning (numbers above are placeholders).
- Whether augmented clears should be recorded as GD clears.
- Remaining augments and systems: candidates, difficulty tiers, rejected ideas
  and build order are in `ROADMAP.md` (2026-09-16).
- Run persistence across game restarts (currently in-memory only).
