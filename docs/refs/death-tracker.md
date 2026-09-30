# death-tracker — abb2k/death-tracker

Pinned `e8bc39a` (2026-09-10), Geode 5.10.1, GD 2.2081. **Installed (v3.0.9).**
Counts deaths per level and per percent. Its hooks run on the user's machine
next to ours, so both the patterns *and* the interference matter.

## Files (`src/hooks/`)

| File | Hooks | Purpose |
|---|---|---|
| `DTPlayLayer.cpp:21-24` | `onModify`: `setHookPriority("PlayLayer::levelComplete", -9999)`, `setHookPriorityPre("PlayLayer::destroyPlayer", Priority::First)` | runs its `destroyPlayer` before everyone else's |
| `DTPlayLayer.cpp:26-60` | `PlayLayer::init` | per-level metadata; a `SaveDeletionEvent().listen` handle stored in Fields |
| `DTPlayLayer.cpp:225-259` | `PlayLayer::destroyPlayer` | **real-death test**: call original, then `if (!player->m_isDead) return;`. First object ever passed is remembered as the anticheat object; any later call that leaves the player alive is logged as "Noclip Detected!" |
| `DTPlayLayer.cpp:297-330` | `PlayLayer::levelComplete` | same guard, then records a 100 % run |
| `DTPlayLayer.cpp:190-210` | `PlayLayer::resetLevel` | optional "reset counts as death" using `getActualProgress` |
| `DTGJBaseGameLayer.cpp:6` | `GJBaseGameLayer::checkpointActivated` | checkpoint-object tracking |
| `DTLevelInfoLayer.cpp:6-35` | `LevelInfoLayer::init` | button: sprite `GJ_plainBtn_001.png` + `miniSkull_001.png`, `CCMenuItemSpriteExtra::create(s, nullptr, this, menu_selector(...))`, `setID`, `getChildByID("other-menu")->addChild`, position from `favorite-button`, `updateLayout()` |
| `DTPauseLayer.cpp:3-27` | `PauseLayer::customSetup` | same recipe on `left-button-menu` |
| `DTEndLevelLayer.cpp:6` | `EndLevelLayer::customSetup` | same recipe on the end screen |

## Interference with us

- Its `destroyPlayer` hook has `Priority::First`, so the chain is
  death-tracker → **ours** → GD. When Shield swallows a death (no original
  call), death-tracker sees `m_isDead == false` and logs `Noclip Detected!`,
  then flags the level's stats as cheated. Cosmetic for us; worth knowing when
  reading the log.
- Its `levelComplete` runs last (`-9999`), after ours.
