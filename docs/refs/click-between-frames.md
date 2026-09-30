# click-between-frames — theyareonit/Click-Between-Frames (CBF)

Pinned `9fa7c6f` (2026-03-20), Geode 5.3.0, GD 2.2081, MIT. **Installed (v1.5.0).**
Sub-frame input timing. Relevant to us mainly because it sits on the same
hooks as Slow-Mo.

## Files

| File | Hooks | Purpose |
|---|---|---|
| `src/main.cpp:296-303` | `CCEGLView::pollEvents` (Windows) | per-frame bookkeeping before input is polled |
| `src/main.cpp:306-320` | `CCScheduler::update` | reads `geode::base::getCocos() + 0x1a84d8` (a cocos timer) for precise frame time, then calls original |
| `src/main.cpp:325-370` | `GJBaseGameLayer::update`, `getModifiedDelta`, `handleButton` at `Priority::VeryEarly` | splits the frame into physics steps and replays queued inputs at the right step |
| `src/main.cpp:241-253` | `PlayLayer::levelComplete`, `showNewBest` | **safe mode**: `m_isTestMode = true` around `levelComplete()` so the clear isn't saved |
| `src/main.cpp:707-750` | `$on_mod(Loaded)` | settings via `getSettingValue<T>` + `listenForSettingChanges<T>`; a raw address hook `Mod::get()->hook(base + 0x71ef0, Slerp2D, ...)` |
| `src/windows.cpp` | — | Wine/Linux path only (shared-memory input daemon); on real Windows it uses Geode's input events like everyone else |

## Interference with us

- Both CBF and we hook `CCScheduler::update`; order is registration order
  (no priority set by either). Our hook scales `dt`; CBF computes physics
  steps from `CCDirector` deltas, so **under Slow-Mo CBF's step count may not
  match the scaled dt**. qolmod's fix: when CBF is loaded, also multiply
  `CCDirector::get()->m_fActualDeltaTime` and `m_fDeltaTime`
  (`refs/qolmod/src/Hacks/Speedhack/Hooks.cpp:38-45`). Test Slow-Mo with
  CBF disabled once to see whether it matters.
- `GJBaseGameLayer::handleButton` at `VeryEarly`: irrelevant unless we hook input.
