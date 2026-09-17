# Reference index — "how does a working mod do X?"

Problem-first index into `refs/` (clone with `scripts/fetch-refs.ps1`; commits
are pinned in that script, so the line numbers below stay valid). One page per
ref lives next to this file. Everything here was read from source on
2026-09-16; **(verified)** means also confirmed in our own build/in game.

Lookup order for any GD-internal question:
1. this file → the ref page → the cited lines;
2. `scripts\refgrep.ps1 <symbol>` (add `-Sdk` for loader source, `-All` for bindings);
3. `scripts\bro.ps1 <Class> <member>` before using/hooking anything;
4. still unsure → say so and mark **(unverified)** in `docs/GD-INTERNALS.md`.

| Problem | Best ref (file:line) | What it shows | Ours |
|---|---|---|---|
| **Key input, Geode 5** — global listener | [qolmod](qolmod.md) `src/Keybinds/Hooks.cpp:51-96` | `$on_mod(Loaded) { KeyboardInputEvent().listen(fn, priority).leak(); }` — pass a **negative priority** to run before the loader's keybind listener | `PlayLayerHook.cpp` `$on_mod(Loaded)`, priority -1 |
| Key input — node-scoped listener (auto-removed with the node) | [custom-keybinds](custom-keybinds.md) `src/UILayer.cpp:47-102` (PauseLayer), `:321-329` (UILayer) | `this->addEventListener(KeybindSettingPressedEventV3(Mod::get(), "id"), [](Keybind const&, bool down, bool repeat, double ts){…})` + `"priority"` in mod.json to win same-key conflicts | `AugPlayLayer::init` |
| Key input — who eats what | loader `platform/windows/input.cpp:492-520`, `loader/LoaderImpl.cpp:409-455` (registered in `queueMods`, before mod binaries), `include/Geode/loader/Event.hpp:1045-1058` | order: key-filtered listeners → unfiltered by ascending priority (loader's = 0, registered first) → IME → `CCKeyboardDispatcher`; loader `Stop`s if any `KeybindSettingPressedEventV3` listener did; custom-keybinds `Stop`s Z/X in every unpaused level | see [custom-keybinds](custom-keybinds.md) "Solved" |
| DevTools variant | [devtools](devtools.md) `src/backend.cpp:504` | same listener registered from `$on_mod(Loaded)` instead of `$execute` | — |
| **Noclip / skip a death** | [qolmod](qolmod.md) `src/Hacks/Level/Noclip/Hooks.cpp:151-172`, `:202-235` | don't call `PlayLayer::destroyPlayer`; always pass through when `object == m_anticheatSpike`, `!player`, or `m_levelEndAnimationStarted` | Shield **(verified)** |
| **Real-death detection** | [death-tracker](death-tracker.md) `src/hooks/DTPlayLayer.cpp:225-259` | call original first, then `if (!player->m_isDead) return;` — anticheat-spike and noclipped calls leave `m_isDead` false | we use a per-attempt flag + spike check **(verified)**; `m_isDead` is the simpler test |
| Death → respawn chain | [qolmod](qolmod.md) `src/Hacks/Level/RespawnTime.cpp:74-98` | `destroyPlayer` schedules an action **tag 0x10** → `PlayLayer::delayedResetLevel()` → `resetLevel()`; replace the action to change respawn delay | draft popup lives in `resetLevel` hook; `delayedResetLevel` is an alternative entry |
| **Speedhack / slow-mo** | [qolmod](qolmod.md) `src/Hacks/Speedhack/Hooks.cpp:14-49`, `:75-83`, `Speedhack.cpp:172-185` | `CCScheduler::update(dt*v)`; gameplay-only variant scales `GJBaseGameLayer::update(dt)` instead (menus stay normal speed); music via `m_system->getMasterChannelGroup` → `setPitch`; **with CBF loaded also scale `CCDirector::m_fActualDeltaTime`/`m_fDeltaTime`** | scheduler hook + master pitch **(verified)**; CBF interaction not handled |
| **Hitboxes** — own draw node | [qolmod](qolmod.md) `src/Hacks/Level/Hitboxes/HitboxNode.cpp:140-243` (objects), `:397-436` (player), `:587-608` (visibility), `Hooks.cpp:8-18` (attach) | `getObjectRect()` with dirty-flag save/restore, `m_objectRadius` → circle, `m_objectType == Slope` → triangle by `m_slopeDirection` (+hazard hypotenuse), `m_orientedBox->m_corners` for rotated, node `insertBefore(node, m_uiLayer)` | Foresight **(verified)**; slopes/oriented boxes not handled |
| **Hitbox geometry — change what GD collides with** | [qolmod](qolmod.md) `src/Hacks/Level/AccurateHitboxes.cpp:122-163` (OBB), `:169-181` (force OBB path), `ShowTrajectory/Hooks.cpp:61-64` (rect vs OBB test) | hook `GameObject::updateOrientedBox()`, rewrite `m_orientedBox->m_corners`, `computeAxes()` + `orderCorners()`; collision is `m_orientedBox->overlaps(player OBB)` when `m_orientedBox && m_shouldUseOuterOb`, else `getObjectRect().intersectsRect`; grid-aligned objects never use the OBB | hazard-hitbox, ex-Blunt (`HazardHitboxHook.cpp`, verified 2026-09-16) — adds a `getObjectRect()` hook for the AABB and scales `m_objectRadius` |
| **Player hitbox — change what the player collides with** | [qolmod](qolmod.md) `src/Hacks/Level/HitboxMultiplier.cpp:103-131` | hook the *other* overload `GameObject::getObjectRect(float w, float h)` (`win 0x1976c0`) and multiply both arguments — they are size factors (`m_vehicleSize`, `0.3f`), not sizes; guard with `typeinfo_cast<PlayerObject*>(this)`. Registered as `SafeModeTrigger::Attempt`, so it changes real collision | wave-hitbox (`PlayerHitboxHook.cpp`, `m_isDart` gate) |
| **Remove / disable an object at runtime** | [xdbot](xdbot.md) `src/hacks/show_trajectory.cpp:322-350`; Geode inline `bindings/2.2081/inline/GameObject.cpp:335` (`GameObject::destroyObject`); [qolmod](qolmod.md) `src/Hacks/Level/AutoCollectCoins.cpp:33` | `m_isDisabled = m_isDisabled2 = true` makes `collisionCheckObjects` skip the object; GD's own `destroyObject()` inline is those two flags + `setOpacity(0)`; the layer-level `GJBaseGameLayer::destroyObject(obj)` (`win 0x216090`) adds the break effect | cat (`PlayLayerHook.cpp::catSweep`, restore in `catRestore`) **(unverified)** |
| Hitboxes — GD's own path is inlined | `docs/GD-INTERNALS.md` "Hitboxes"; [miscbugfixes](miscbugfixes.md) `src/MirrorShowHitboxesFix.cpp` | only byte-patches or flag tricks reach GD's `m_debugDrawNode`; miscbugfixes hooks `CCDrawNode::drawPolygon` while `updateDebugDraw` runs | — |
| **Mirror portals** | [qolmod](qolmod.md) `src/Hacks/Level/NoMirrorPortal.cpp:22-31` | hook `GJBaseGameLayer::toggleFlipped(bool flip, bool noEffects)` and return early (`win 0x2467d0`) | Unmirror neutralises objects **(unverified)** — switch to this hook |
| **Checkpoints in normal mode** | [qolmod](qolmod.md) `src/Hacks/Level/PracticeComplete.cpp:22-63`; [xdbot](xdbot.md) `src/practice_fixes/play_layer.cpp:28-110` | hooks on `PlayLayer::storeCheckpoint`, `loadFromCheckpoint(CheckpointObject*)`, `CheckpointObject::init`; `m_isPracticeMode` flipped inside `levelComplete` decides whether a clear counts | Checkpoint augment **(unverified)** |
| Start-pos style respawn | [qolmod](qolmod.md) `src/Hacks/Level/StartposSwitcher.cpp:133-163` | `m_currentCheckpoint = nullptr; setStartPosObject(obj); resetLevel(); startMusic();` | — |
| **Does an augmented clear count?** | [click-between-frames](click-between-frames.md) `src/main.cpp:241-253` (safe mode); qolmod `PracticeComplete.cpp:44-52` | `m_isTestMode = true` around `PlayLayer::levelComplete()` blocks progress; `m_isPracticeMode = false` makes a practice clear count | open design question in `DESIGN.md` |
| **Buttons on LevelInfoLayer** | [death-tracker](death-tracker.md) `src/hooks/DTLevelInfoLayer.cpp:10-32`; [betterinfo](betterinfo.md) `src/hooks/LevelInfoLayer.cpp` | `getChildByID("other-menu")` → `addChild(btn)` → `updateLayout()`; IDs via `scripts\nodeids.ps1 LevelInfoLayer` | `LevelInfoHook.cpp` **(verified)** |
| Buttons on PauseLayer | [death-tracker](death-tracker.md) `src/hooks/DTPauseLayer.cpp:3-27` | hook `PauseLayer::customSetup`, `getChildByID("left-button-menu")`, `updateLayout()` | not used |
| In-level HUD / labels | [qolmod](qolmod.md) `src/Labels/Hooks.cpp:9` | container added to `m_uiLayer` (screen space, survives camera) | `RunHud` on PlayLayer |
| Node IDs for any layer | [node-ids](node-ids.md), `scripts\nodeids.ps1 <Layer>` | `PlayLayer`, `UILayer`, `PauseLayer`, `EndLevelLayer`, `LevelInfoLayer` all covered | — |
| Hook priority vs other mods | death-tracker `DTPlayLayer.cpp:21-24`; custom-keybinds `UILayer.cpp:110-112`; CBF `main.cpp:325-328` | `static void onModify(auto& self) { (void)self.setHookPriorityPre("PlayLayer::destroyPlayer", Priority::First); }` / `setHookPriority(name, Priority::Late)` | `AugPlayLayer` uses defaults |
| Settings: read / listen | CBF `src/main.cpp:707-737`; geode-docs `mods/settings.md:355` (keybind type) | `Mod::get()->getSettingValue<T>("k")`, `listenForSettingChanges<T>("k", fn)` in `$on_mod(Loaded)` | `AugmentManager` reads gauge numbers |
| Popups / layouts / fields / events (API tutorials) | [geode-docs](geode-docs.md) | `tutorials/popup.md`, `layouts.md`, `fields.md`, `events.md:237-253` (`addEventListener`), `hookpriority.md`, `nodetree.md` | — |
| Installed-mod interference | `scripts\mods.ps1`; [click-between-frames](click-between-frames.md); [death-tracker](death-tracker.md) | CBF hooks `CCScheduler::update`, `GJBaseGameLayer::update/handleButton` (VeryEarly) and reads `CCDirector` deltas; death-tracker hooks `destroyPlayer` at `Priority::First` and logs "Noclip Detected!" when our Shield skips the original | — |

## Refs at a glance

| ref | target | use for | trust |
|---|---|---|---|
| [qolmod](qolmod.md) | Geode 5.7 / 2.2081, installed | noclip, speedhack, hitboxes, mirror, checkpoints, labels, keybinds | high (runs on this machine) |
| [custom-keybinds](custom-keybinds.md) | 5.9 / 2.2081, installed | node-scoped keybind listeners, what swallows keys in PlayLayer | high |
| [click-between-frames](click-between-frames.md) | 5.3 / 2.2081, installed | scheduler/delta interaction, safe mode, hook priorities | high |
| [death-tracker](death-tracker.md) | 5.10.1 / 2.2081, installed | real-death detection, LevelInfo/Pause buttons | high |
| [xdbot](xdbot.md) | **Geode 4.4 / 2.2074** | checkpoint internals, PlayLayer lifecycle | patterns only; re-verify every binding with `bro.ps1` |
| [betterinfo](betterinfo.md) | 5.10.1 / 2.2081 | UI patterns, PlayLayer attempt logging | medium (large) |
| [miscbugfixes](miscbugfixes.md) | 5.8 / 2.2081 | minimal `$modify` style, debug-draw and mirror quirks | high |
| [node-ids](node-ids.md) | 5.8 / 2.2081, installed | node IDs | high |
| [devtools](devtools.md) | 5.8 / 2.2081 | key listener from `$on_mod(Loaded)`, node inspection | high |
| [geode-docs](geode-docs.md) | docs `main` (may already describe Geode 6 — we're on 5.10.1) | API tutorials | cross-check with `$GEODE_SDK/loader/include` |
| [cleanstartpos](cleanstartpos.md), [example-mod](example-mod.md) | 5.10.1 / 2.2081 | small; startpos in editor, template | low relevance |
