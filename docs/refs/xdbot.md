# xdbot — Zilko/xdBot (macro bot)

Pinned `16ef8e8` (2025-05-29), **Geode 4.4.0, GD 2.2074** — no newer upstream
exists. GD 2.2074 → 2.2081 kept most PlayLayer internals, but the Geode 4 API
differs (events, settings). Use for *which GD function does what*; run
`scripts\bro.ps1` on every symbol before using it, and don't copy Geode API
calls.

## Files

| File | Hooks | Purpose |
|---|---|---|
| `src/main.cpp:37-190` | `PlayLayer::init/resetLevel/postUpdate/onQuit/pauseGame` | attempt lifecycle for recording/playback |
| `src/main.cpp:193-430` | `GJBaseGameLayer::processCommands/handleButton` | input replay per physics frame |
| `src/main.cpp:434-470` | `PauseLayer::onPracticeMode/onNormalMode/onQuit` | practice toggles |
| `src/practice_fixes/play_layer.cpp:7-27` | `GJBaseGameLayer::toggleFlipped`, `checkpointActivated` | no-mirror (forces `flip=false`), checkpoint cancel |
| `src/practice_fixes/play_layer.cpp:28-60` | `CheckpointObject::init` (Windows) / `create` (others) | snapshot per-player state when a checkpoint object is made |
| `src/practice_fixes/play_layer.cpp:63-110` | `PlayLayer::storeCheckpoint`, `loadFromCheckpoint` | drop a checkpoint (`cancelCheckpoint`) or restore extra state after GD's own load |
| `src/hacks/other.cpp:14-70` | `CCScheduler::update` | speedhack + FMOD pitch (same idea as ours) |
| `src/hacks/frame_stepper.cpp:23-50` | `CCDirector::drawScene` | frame stepping: skip `drawScene`, call `getScheduler()->update(1/TPS)` manually |
| `src/hacks/frame_stepper.cpp:53-60` | `PlayLayer::setupHasCompleted` | "level fully loaded" hook point |
| `src/hacks/show_trajectory.cpp` | `PlayLayer`, `PlayerObject`, `HardStreak`, `GameObject` | simulate the player ahead (a heavy "foresight") |

## Facts confirmed against 2.2081 bindings (`bro.ps1`, 2026-09-16)

- `PlayLayer::storeCheckpoint(CheckpointObject*)` `win 0x3b74a0`,
  `loadFromCheckpoint(CheckpointObject*)` `win 0x3b7640`,
  `markCheckpoint()` `win 0x3b7570`, `resetLevelFromStart()` `win 0x3b8d10`,
  `removeCheckpoint(bool first)` `win 0x3b7f00`, `delayedResetLevel()` `win 0x3b8cf0`.
- `getLastCheckpoint()`, `loadLastCheckpoint()`, `queueCheckpoint()`,
  `CheckpointObject::create()` are `win inline` → callable via Geode's inline
  impl, **not hookable**. `CheckpointObject::init()` is hookable (`win 0x77de0`).
- Fields: `m_currentCheckpoint`, `m_checkpointArray`, `m_tryPlaceCheckpoint`,
  `m_activatedCheckpoint` on PlayLayer.

## Traps

- Geode 4 code: `Mod::get()->getSavedValue`, old event API. Ignore API, keep logic.
- It byte-reads GD memory (`seedAddr`) for RNG — never do that here.
