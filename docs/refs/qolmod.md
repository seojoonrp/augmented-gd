# qolmod — TheSillyDoggo/GeodeMenu (QOLMod)

Pinned `98aa8a3` (2026-07-06), Geode 5.7.1, GD 2.2081. **Installed on the
user's machine (v2.8.6)** — everything it does is known to work on this setup.
License: "Dont use my code unless I let you" → patterns only, never copy.

## Layout

```
src/Client/          Module system (MODULE_SETUP / SUBMIT_HACK) — each hack is a class with getRealEnabled()
src/Hacks/Level/     per-hack files; the ones we care about are listed below
src/Hacks/Speedhack/ scheduler + FMOD pitch
src/Keybinds/        KeyboardInputEvent listener + its own keybind manager
src/Labels/          in-level label HUD (attached to m_uiLayer)
src/GUI/, src/UI/    ImGui-less custom menu (AndroidUI)
```

Hooks are normal `$modify` but the class names are aliased in `Hooks.hpp`
files (e.g. `class $modify(SpeedhackScheduler, CCScheduler)` in
`Speedhack/Hooks.hpp:15`), so grep `Hooks.hpp` when `rg '\$modify'` finds
fewer targets than expected.

## Feature map

| Feature | File:line | Mechanism |
|---|---|---|
| Key input (Geode ≥ 2.2081 branch) | `src/Keybinds/Hooks.cpp:51-96` | `$execute { KeyboardInputEvent().listen(+[](KeyboardInputData const& e){ … return ListenerResult::Propagate; }).leak(); listenForKeybindSettingPresses("open-menu-keybind", …); }`. Ignores keys while `CCIMEDispatcher::sharedDispatcher()->hasDelegate()` (text input focused). Modifiers from `e.modifiers & KeyboardModifier::Shift/Control/Alt/Super`. |
| Key input (older GD) | `src/Keybinds/Hooks.cpp:12-48` | `$modify(CCKeyboardDispatcher)::dispatchKeyboardMSG` — the pre-Geode-5 way; kept under `#if GEODE_COMP_GD_VERSION < 22081`. |
| Noclip | `src/Hacks/Level/Noclip/Hooks.cpp:151-172` | `destroyPlayer` hook: remembers `p1` as death object, then either forwards or swallows. `shouldPlayerRegularDie` (`:202-235`) forwards when `go == m_anticheatSpike`, `!pl`, `m_levelEndAnimationStarted`, or the per-player toggle is off. Death ticks counted in `postUpdate` (`:181`). |
| Speedhack | `src/Hacks/Speedhack/Hooks.cpp:14-49` | `CCScheduler::update(dt * value)`. If `syzzi.click_between_frames` is loaded it also multiplies `CCDirector::get()->m_fActualDeltaTime` and `m_fDeltaTime` (CBF reads them). Music: `getMasterChannel()->setPitch(v)` only when the value changes (`Speedhack.cpp:172-185` gets the group via `FMODAudioEngine::sharedEngine()->m_system->getMasterChannelGroup`). |
| Speedhack, gameplay only | `src/Hacks/Speedhack/Hooks.cpp:75-83` | scales `dt` in `GJBaseGameLayer::update(float)` instead of the scheduler → menus/popups run at normal speed. Also `pauseTarget` override (`:52-68`) to keep "unpausable" nodes running. |
| Hitboxes | `src/Hacks/Level/Hitboxes/Hooks.cpp:8-18` | own `HitboxNode` (a `CCDrawNode` subclass) created in `GJBaseGameLayer::init`, `setZOrder(2)`, `insertBefore(node, m_uiLayer)`. |
| Hitbox geometry | `HitboxNode.cpp:140-157` | `getObjectRect()` is only called when `m_isObjectRectDirty`, saving/restoring `m_isObjectRectDirty` and `m_boxOffsetCalculated` so GD's cache isn't disturbed; otherwise reads `m_objectRect`. |
| | `HitboxNode.cpp:159-243` | circle if `m_objectRadius != 0` (radius × max scale); slope (`m_objectType == GameObjectType::Slope`) → triangle from `m_slopeDirection` (cases 0/7, 3/6, 1/5), hazard hypotenuse if `m_slopeIsHazard`; rotated objects use `m_orientedBox->m_corners[0..3]`. |
| Accurate spike hitboxes (collision geometry edit) | `src/Hacks/Level/AccurateHitboxes.cpp:122-163` | `$modify(GameObject)::updateOrientedBox()`: `dirty = m_isOrientedBoxDirty \|\| !m_orientedBox` before the original; afterwards, if dirty and the object qualifies (size table / IDs 392, 458, 459), rebuilds `m_orientedBox->m_corners[0..3]` from `m_center`, `m_width*m_scaleX`, `m_height*m_scaleY`, `-rotation` (0.49 half-size, "0.50 makes levels impossible"), then `computeAxes()` + `orderCorners()`. Also marks the player's OBB. Proves corner edits change real collision. |
| | `AccurateHitboxes.cpp:169-181` | `PlayLayer::addObject`: if `(int)rotation % 90 == 0` → `setRotation(rot + 1)` so the object gets an OBB at all — grid-aligned objects collide by AABB only. |
| Rect vs OBB test (mirrors GD) | `ShowTrajectory/Hooks.cpp:61-64`, `AllModesPlatformer.cpp:47-50` | `obj->m_orientedBox && obj->m_shouldUseOuterOb ? obj->m_orientedBox->overlaps(player->m_orientedBox) : player->getObjectRect().intersectsRect(obj->getObjectRect())` |
| Player hitbox multiplier (collision, not drawing) | `src/Hacks/Level/HitboxMultiplier.cpp:103-131` | `$modify(GameObject)::getObjectRect(float p0, float p1)`: multiplies `p0`/`p1` before calling the original — for `typeinfo_cast<PlayerObject*>(this)` by the player factor, otherwise by a solid/hazard factor picked from `m_objectType`. The module declares `setSafeModeTrigger(SafeModeTrigger::Attempt)`, so this path really is what GD collides with. (Its player branch has a typo, `p1 *= p0 *= mult`, which squares the width — don't copy the arithmetic.) |
| Player hitbox | `HitboxNode.cpp:397-436` | `plr->getObjectRect(m_vehicleSize, m_vehicleSize)` (big) and `getObjectRect(0.3f, 0.3f)` (inner). |
| Hitbox visibility rule | `HitboxNode.cpp:587-608` | hides GD's `m_debugDrawNode` outside the editor; own node visible if enabled or (`show on death` && `m_player1->m_isDead`). |
| Mirror portals | `src/Hacks/Level/NoMirrorPortal.cpp:22-31` | `GJBaseGameLayer::toggleFlipped(bool, bool)` → return without calling original. |
| Practice checkpoints ↔ clears | `src/Hacks/Level/PracticeComplete.cpp:22-63` | tracks `loadFromCheckpoint` per attempt; in `levelComplete` sets `m_isPracticeMode = hasRespawnedWithCheckpoint` so a no-checkpoint practice run counts as a real clear (and re-collects coins). Shows that `m_isPracticeMode` at `levelComplete` time is what GD checks. |
| Checkpoint state fix | `src/Hacks/Level/CheckpointFix/Hooks.cpp` | extends `PlayerCheckpoint` with fields via `$modify` and hooks `PlayerObject::saveToCheckpoint/loadFromCheckpoint` to save extra player state. |
| StartPos switching | `src/Hacks/Level/StartposSwitcher.cpp:133-163` | `m_currentCheckpoint = nullptr; setStartPosObject(obj); if (m_isPracticeMode) resetLevelFromStart(); resetLevel(); startMusic();` |
| Respawn delay | `src/Hacks/Level/RespawnTime.cpp:74-98` | after `destroyPlayer`, GD has queued an action with **tag `0x10`**; stop it and run `CCDelayTime → PlayLayer::delayedResetLevel` yourself. |
| Label HUD | `src/Labels/Hooks.cpp:9` | `m_uiLayer->addChild(LabelContainerLayer::create())` in `PlayLayer::init`; refreshed from `resetLevel` (`:14`) and `handleButton` (`:22`). |

## Traps

- Restrictive license — read, don't paste.
- QOLMod's own keybinds run in the same unfiltered `KeyboardInputEvent`
  listener list as ours; it returns `Stop` for keys it handles
  (`KeybindManager::processMSG`, `src/Keybinds/KeybindManager.cpp:38`). Default
  binds don't include X/Z, but the user can bind anything.
- Its hitbox node lives on `GJBaseGameLayer` (screen space via `insertBefore`),
  ours on `m_objectLayer` (world space). Both work; don't mix coordinate spaces.
