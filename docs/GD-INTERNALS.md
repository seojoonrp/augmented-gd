# GD / Geode internals — verified facts

Everything here was confirmed either by reading the SDK/bindings source or by
an in-game test on **GD 2.2081 / Geode 5.10.1 / Windows**. Facts that are only
*believed* are marked **(unverified)**. When you learn something new, add it
here with how it was verified.

## How to look things up

| Question | Where |
|---|---|
| Does class X have member/function Y? | `build/_deps/bindings-src/bindings/2.2081/GeometryDash.bro` (GD) and `Cocos2d.bro` (cocos). `awk '/^class PlayLayer /,/^}/' GeometryDash.bro \| grep foo` |
| What does an `= inline` / `win inline` function do? | `build/_deps/bindings-src/bindings/2.2081/inline/*.cpp` (Geode's hand-written impl; on Windows GD's copy is inlined, so **hooking it does nothing**) |
| Geode API (Popup, Layout, settings, events) | `$GEODE_SDK/loader/include/Geode/**` and impl in `$GEODE_SDK/loader/src/**` |
| Enum values (`GameObjectType`, key codes) | `build/_deps/bindings-src/bindings/include/Geode/Enums.hpp`, `Geode/cocos/robtop/keyboard_dispatcher/CCKeyboardDelegate.h` |
| Node IDs added by node-ids | `grep -a -o` the string in `geode/unzipped/geode.node-ids/geode.node-ids.dll` |
| How a working mod does it | `refs/` (run `scripts/fetch-refs.ps1`) — **read this before guessing GD behaviour** |

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
- After a death GD calls `resetLevel()` on respawn. Our hook is where draft
  popups are shown.
- `levelComplete()` fires on clear. `onQuit()` fires when leaving.
- `postUpdate(float dt)` runs every frame after physics; good place for
  per-frame work (timers, HUD, drawing).
- `getCurrentPercent()` returns 0–100 as float and is accurate at death time.
- Practice / test attempts: `m_isPracticeMode`, `m_isTestMode` on GJBaseGameLayer.

## Skipping a death (noclip)

Not calling `PlayLayer::destroyPlayer` for a real death works: the player
passes through. Used by Shield. (verified)

## Slow-mo / time scale

- `CCScheduler::setTimeScale` exists in the header; **untested** whether GD's
  scheduler honours it. We hook `CCScheduler::update(float dt)` and call the
  original with `dt * scale` instead (OpenHack pattern). Verified: game slows.
- Music: `FMODAudioEngine::sharedEngine()->m_system->getMasterChannelGroup(&g); g->setPitch(scale)`.
  Verified to stay in sync. (`m_globalChannel->setPitch` did **not** slow music.)

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

## Mirror portals

`GameObjectType::InverseMirrorPortal (14)` / `NormalMirrorPortal (15)`.
Setting `m_objectType = Decoration` + `setVisible(false)` + `m_isHide = true`
neutralises a portal at runtime. **(unverified in game — no mirror level tested yet)**

## Checkpoints in normal mode

Approach (implemented, **unverified** because the Z key never fired):
`markCheckpoint()` / `resetLevel()` wrapped in a temporary `m_isPracticeMode = true`,
keeping a `Ref<CheckpointObject>` from `getLastCheckpoint()` at death time in case
GD clears `m_checkpointArray` on a normal-mode death, and re-`storeCheckpoint`ing it
before the reset. See `PlayLayerHook.cpp::resetLevel`.

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

Tried and **did not fire** (no log line at all, in this order):
1. `$modify(CCKeyboardDispatcher)::dispatchKeyboardMSG` — even with
   `setHookPriorityBefore(..., "geode.custom-keybinds")`.
2. `listenForKeybindSettingPresses("keybind-slowmo", ...)` in `$execute`.
3. `KeyboardInputEvent().listen(...)` in `$execute`, logging every X/Z press.

Since (3) logged nothing, either the listener isn't registered the way we think
(`$execute` timing? `GlobalEvent` default filter?) or key events reach GD by a
path that bypasses `KeyboardInputEvent` on this machine.

Next things to try, in order:
1. Log *any* key inside the `KeyboardInputEvent` listener (not just X/Z) and log
   from `$execute` itself, to prove registration and delivery separately.
2. Hook `UILayer::keyDown(enumKeyCodes, double)` — the object GD's PlayLayer
   uses for gameplay keys; CustomKeybinds overrides `UILayer::handleKeypress`
   one level below it, so `keyDown` still sees everything.
3. Depend on `geode.custom-keybinds` and register bindables through its API.
4. Test with custom-keybinds and QOLMod disabled to rule out interference.

## Misc

- Node IDs on `LevelInfoLayer` (node-ids): `left-side-menu`, `right-side-menu`, `back-menu`.
- `Popup::init` puts the close button inside `m_buttonMenu`; remove it before
  applying a layout to that menu.
- `CCDirector::pause()` drops the frame interval to 1/4 s; restore it with
  `setAnimationInterval(previous)` right after. `resume()` restores the saved
  original. Actions freeze while paused → `m_noElasticity = true` on popups.
- Cursor: `CCEGLView::get()->showCursor(bool)`, previous state in `m_bShouldHideCursor`.
