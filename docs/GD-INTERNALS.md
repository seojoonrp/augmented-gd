# GD / Geode internals — verified facts

Everything here was confirmed either by reading the SDK/bindings source or by
an in-game test on **GD 2.2081 / Geode 5.10.1 / Windows**. Facts that are only
*believed* are marked **(unverified)**. When you learn something new, add it
here with how it was verified.

## How to look things up

| Question | Where |
|---|---|
| Does class X have member/function Y? Is it hookable? | `scripts\bro.ps1 <Class> [member]` — prints the binding line with a verdict (`win ok` / `win inline` / field). Raw files: `build/_deps/bindings-src/bindings/2.2081/*.bro` |
| What does an `= inline` / `win inline` function do? | `build/_deps/bindings-src/bindings/2.2081/inline/*.cpp` (Geode's hand-written impl; on Windows GD's copy is inlined, so **hooking it does nothing**) |
| Geode API (Popup, Layout, settings, events) | `$GEODE_SDK/loader/include/Geode/**` and impl in `$GEODE_SDK/loader/src/**` |
| Enum values (`GameObjectType`, key codes) | `build/_deps/bindings-src/bindings/include/Geode/Enums.hpp`, `Geode/cocos/robtop/keyboard_dispatcher/CCKeyboardDelegate.h` |
| How a working mod does it | **`docs/refs/INDEX.md`** (problem → ref file:line), then `scripts\refgrep.ps1 <symbol> [-Sdk|-All]` over `refs/` (+ loader source, bindings). Clone with `scripts/fetch-refs.ps1`. |
| Node IDs on a layer | `scripts\nodeids.ps1 <Layer>` (from the NodeIDs source in `refs/node-ids`) |
| Which mods are installed / enabled | `scripts\mods.ps1` |

A binding entry with no `win` address in `Cocos2d.bro` is still hookable on
Windows (linked from `libcocos2d.dll`); an entry marked `win inline` is not.

## PlayLayer lifecycle (verified in game)

- `PlayLayer::init` → calls `resetLevel()` **before the scene is running**
  (`isRunning()` is false). Don't show popups from there.
- `destroyPlayer(player, object)` is called:
  - once per real death, for `m_player1` (and `m_player2` in dual mode);
  - **at the start of every attempt with `object == m_anticheatSpike`**.
    That call is not a death. Blocking it flags the level; counting it gives
    phantom deaths at 0%. Always pass it straight through.
  - possibly more than once per attempt for the same death → guard with a
    per-attempt flag in `Fields`.
- After a death GD queues a `CCSequence` with **tag `0x10`** on the PlayLayer
  that ends in `delayedResetLevel()` → `resetLevel()` (qolmod `RespawnTime.cpp:74-98`
  replaces that action to change the respawn delay). Our `resetLevel` hook is
  where draft popups are shown; `delayedResetLevel` (`win 0x3b8cf0`) is an
  alternative, pre-reset hook point. **(from refs, not yet used here)**
- Simplest real-death test (death-tracker `DTPlayLayer.cpp:225-259`): call the
  original `destroyPlayer` and then check `player->m_isDead` — the anticheat-
  spike call and noclipped calls leave it `false`. **(from refs)**
- `levelComplete()` fires on clear. `onQuit()` fires when leaving.
- `postUpdate(float dt)` runs every frame after physics; good place for
  per-frame work (timers, HUD, drawing).
- `getCurrentPercent()` returns 0–100 as float and is accurate at death time.
- Practice / test attempts: `m_isPracticeMode`, `m_isTestMode` on GJBaseGameLayer.

## Skipping a death (noclip)

Not calling `PlayLayer::destroyPlayer` for a real death works: the player
passes through. Used by Shield. (verified)
qolmod does the same and additionally always forwards when `!player` or
`m_levelEndAnimationStarted` (`refs/qolmod/src/Hacks/Level/Noclip/Hooks.cpp:202-235`).
Side effect on this machine: death-tracker (hook priority `First`) sees
`m_isDead == false` after our swallow and logs `Noclip Detected!` — harmless.

## Slow-mo / time scale

- `CCScheduler::setTimeScale` exists in the header; **untested** whether GD's
  scheduler honours it. We hook `CCScheduler::update(float dt)` and call the
  original with `dt * scale` instead (OpenHack pattern). Verified: game slows.
- Music: `FMODAudioEngine::sharedEngine()->m_system->getMasterChannelGroup(&g); g->setPitch(scale)`.
  Verified to stay in sync. (`m_globalChannel->setPitch` did **not** slow music.)
- qolmod's "gameplay only" speedhack scales `dt` inside
  `GJBaseGameLayer::update(float)` (`win 0x237850`) instead of the scheduler,
  so menus/popups keep normal speed (`refs/qolmod/src/Hacks/Speedhack/Hooks.cpp:75-83`).
  **(from refs, unverified here)**
- Click Between Frames (installed) computes physics steps from
  `CCDirector::m_fActualDeltaTime` / `m_fDeltaTime`; qolmod multiplies both by
  the speed factor when CBF is loaded (`Hooks.cpp:38-45`). We don't — if
  Slow-Mo ever feels off, test with CBF disabled first. **(unverified)**

## Hitboxes

- GD only draws hitboxes when `(m_isPracticeMode && m_isDebugDrawEnabled) || (m_hitboxesOnDeath && m_playerDied)`.
  That check is **inlined inside GD's own functions** (`PlayLayer::shouldDebugDraw` is
  `win inline`). Setting the flags around `updateDebugDraw()` does nothing; forcing
  the game variable `0166` through `updateDebugDrawSettings()` does nothing.
  OpenHack byte-patches the `je` instructions (`refs/openhack/src/shared/hacks/hitboxes/hitboxes.cpp`).
- What works: draw them ourselves into our own `CCDrawNode` added to
  `m_objectLayer` (world coordinates). `GameObject::getObjectRect()` gives the box,
  `m_objectRadius > 0` means GD treats it as a circle. Colour by `m_objectType`.
  `CCDrawNode` expects **premultiplied alpha** — a translucent fill renders as
  solid; use `{0,0,0,0}` for no fill. Implemented in `PlayLayerHook.cpp::drawHitboxes`.
- `m_objects` can be 10k+ objects; sort once by x and `lower_bound` per frame.
- qolmod's node (`refs/qolmod/src/Hacks/Level/Hitboxes/HitboxNode.cpp:140-243`)
  also handles what ours skips: slopes (`m_objectType == GameObjectType::Slope`,
  triangle from `m_slopeDirection`, hazard edge when `m_slopeIsHazard`), rotated
  objects (`m_orientedBox->m_corners[0..3]`), and it only calls
  `getObjectRect()` when `m_isObjectRectDirty`, restoring `m_isObjectRectDirty`
  and `m_boxOffsetCalculated` afterwards so GD's cache is untouched. Player:
  `getObjectRect(m_vehicleSize, m_vehicleSize)` and `getObjectRect(0.3f, 0.3f)`.
  **(from refs)**

## Mirror portals

`GameObjectType::InverseMirrorPortal (14)` / `NormalMirrorPortal (15)`.
Setting `m_objectType = Decoration` + `setVisible(false)` + `m_isHide = true`
neutralises a portal at runtime. **(unverified in game — no mirror level tested yet)**

Better: hook `GJBaseGameLayer::toggleFlipped(bool flip, bool noEffects)`
(`win 0x2467d0`) and return without calling the original — qolmod's
NoMirrorPortal (`refs/qolmod/src/Hacks/Level/NoMirrorPortal.cpp:22-31`) and
xdBot (`toggleFlipped` with `flip = false`) both do this. No object surgery,
covers portals spawned later too. **(from refs, unverified here)**

## Checkpoints in normal mode

Approach (implemented, **unverified** because the Z key never fired):
`markCheckpoint()` / `resetLevel()` wrapped in a temporary `m_isPracticeMode = true`,
keeping a `Ref<CheckpointObject>` from `getLastCheckpoint()` at death time in case
GD clears `m_checkpointArray` on a normal-mode death, and re-`storeCheckpoint`ing it
before the reset. See `PlayLayerHook.cpp::resetLevel`.

Bindings (2.2081, `bro.ps1`): `storeCheckpoint(CheckpointObject*)` `win 0x3b74a0`,
`loadFromCheckpoint(CheckpointObject*)` `win 0x3b7640`, `markCheckpoint()`
`win 0x3b7570`, `removeCheckpoint(bool)` `win 0x3b7f00`, `resetLevelFromStart()`
`win 0x3b8d10`, `CheckpointObject::init()` `win 0x77de0` — all hookable.
`getLastCheckpoint()`, `loadLastCheckpoint()`, `queueCheckpoint()`,
`CheckpointObject::create()` are `win inline` (callable, not hookable).
How other mods use them: xdBot hooks `storeCheckpoint` / `loadFromCheckpoint` /
`CheckpointObject::init` (`refs/xdbot/src/practice_fixes/play_layer.cpp:28-110`);
qolmod's StartposSwitcher respawns with
`m_currentCheckpoint = nullptr; setStartPosObject(obj); resetLevel(); startMusic();`
(`refs/qolmod/src/Hacks/Level/StartposSwitcher.cpp:133-163`).

## Does a clear count? (`m_isPracticeMode` / `m_isTestMode` at `levelComplete`)

GD decides at `PlayLayer::levelComplete()` time: CBF's safe mode sets
`m_isTestMode = true` around the original call to block saving progress
(`refs/click-between-frames/src/main.cpp:241-253`); qolmod's "1 Attempt
Practice" sets `m_isPracticeMode = false` there to make a practice clear count
(`refs/qolmod/src/Hacks/Level/PracticeComplete.cpp:44-52`). So whichever way
DESIGN.md decides, it is a one-line hook. **(from refs, unverified here)**

## Keyboard input on Windows (Geode 5) — OPEN PROBLEM

Facts:
- Geode 5 replaces GD's key handling with its own raw-input path
  (`$GEODE_SDK/loader/src/platform/windows/input.cpp`). It sends
  `KeyboardInputEvent(keyCode).send(data)` first, then
  `CCKeyboardDispatcher::dispatchKeyboardMSG(...)`.
- `mod.json` supports `"type": "keybind"` settings; Geode turns presses into
  `KeybindSettingPressedEventV3(modID, settingKey)` from a `KeyboardInputEvent`
  listener in `LoaderImpl.cpp` (~line 409). CustomKeybinds uses exactly this
  (`refs/custom-keybinds/src/UILayer.cpp`).
- RTTI across DLLs is unreliable: use `cast::typeinfo_pointer_cast`, never
  `dynamic_pointer_cast`, on Geode objects.
- Delivery order inside Geode (`loader/include/Geode/loader/Event.hpp:1045-1058`,
  `platform/windows/input.cpp:492-520`): key-filtered listeners
  (`KeyboardInputEvent(KEY_X).listen`) first — if one returns `Stop`, the
  unfiltered listeners (`KeyboardInputEvent().listen`) never run — then IME,
  then `CCKeyboardDispatcher::dispatchKeyboardMSG`.
- The loader's own listener (`LoaderImpl.cpp:409-455`) turns keybind settings
  into `KeybindSettingPressedEventV3` and returns `Stop` only if a listener
  for that setting returned `Stop`.
- Working code on this machine: qolmod registers the unfiltered listener from
  `$execute` with `.leak()` and also uses `listenForKeybindSettingPresses`
  (`refs/qolmod/src/Keybinds/Hooks.cpp:51-96`); DevTools registers from
  `$on_mod(Loaded)` (`refs/devtools/src/backend.cpp:504`); CustomKeybinds uses
  node-scoped `this->addEventListener(KeybindSettingPressedEventV3(Mod::get(), "id"), lambda)`
  in `PauseLayer::customSetup` / `UILayer::init` (`refs/custom-keybinds/src/UILayer.cpp:47-102, 321-329`).
- CustomKeybinds overrides `UILayer::handleKeypress` completely (only Escape
  passes), so hooking `handleKeypress` sees nothing; `UILayer::keyDown`
  (`win 0x4cde50`) still sees every key.
- Our `$execute` listener is the same code as qolmod's, and the saved settings
  hold X=88 / Z=90 — so "no log line from three approaches" most likely means
  the code never ran (stale install? `$execute` not linked?) rather than a
  wrong API.

Tried and **did not fire** (no log line at all, in this order):
1. `$modify(CCKeyboardDispatcher)::dispatchKeyboardMSG` — even with
   `setHookPriorityBefore(..., "geode.custom-keybinds")`.
2. `listenForKeybindSettingPresses("keybind-slowmo", ...)` in `$execute`.
3. `KeyboardInputEvent().listen(...)` in `$execute`, logging every X/Z press.

Since (3) logged nothing, either the listener isn't registered the way we think
(`$execute` timing? `GlobalEvent` default filter?) or key events reach GD by a
path that bypasses `KeyboardInputEvent` on this machine.

Next things to try, in order (see `docs/refs/custom-keybinds.md` "Open problem"):
1. `log::info` at the top of the `$execute` block and in `$on_mod(Loaded)`, and
   log *any* key inside the listener (not just X/Z) — proves the binary,
   registration and delivery separately.
2. Node-scoped listener in `PlayLayer::init`:
   `this->addEventListener(KeybindSettingPressedEventV3(Mod::get(), "keybind-slowmo"), [this](Keybind const&, bool down, bool repeat, double){ … })`
   — CustomKeybinds' pattern; no global state, auto-removed with the layer.
3. Hook `UILayer::keyDown(enumKeyCodes, double)` (`win 0x4cde50`) — sees every
   key even with CustomKeybinds installed.
4. Test with qolmod and custom-keybinds disabled (`scripts\mods.ps1`) to rule
   out a `Stop` from another listener.

## Misc

- Node IDs on `LevelInfoLayer` (node-ids): `left-side-menu`, `right-side-menu`, `back-menu`.
- `Popup::init` puts the close button inside `m_buttonMenu`; remove it before
  applying a layout to that menu.
- `CCDirector::pause()` drops the frame interval to 1/4 s; restore it with
  `setAnimationInterval(previous)` right after. `resume()` restores the saved
  original. Actions freeze while paused → `m_noElasticity = true` on popups.
- Cursor: `CCEGLView::get()->showCursor(bool)`, previous state in `m_bShouldHideCursor`.
- Hook priority vs other mods: `static void onModify(auto& self) { (void)self.setHookPriorityPre("PlayLayer::destroyPlayer", Priority::First); }`
  (death-tracker) or `setHookPriority("UILayer::handleKeypress", Priority::Late)`
  (CustomKeybinds). node-ids assigns IDs at `Priority::VeryEarlyPost`, so a
  default-priority `init` hook already sees them.
- Adding a button to a GD menu: `getChildByID("other-menu")->addChild(btn); menu->updateLayout();`
  (death-tracker `DTLevelInfoLayer.cpp:6-35`, `DTPauseLayer.cpp:3-27` for `left-button-menu`).
- In-level HUD that ignores the camera: add it to `m_uiLayer` (qolmod labels,
  `refs/qolmod/src/Labels/Hooks.cpp:9`).
- Settings: `Mod::get()->getSettingValue<T>("key")`, live updates with
  `listenForSettingChanges<T>("key", fn)` from `$on_mod(Loaded)` (CBF `main.cpp:707-737`).
