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

## Object collision shapes (hazard-hitbox, formerly "Blunt") — verified in game 2026-09-16

GD keeps three collision shapes per `GameObject`, read from different places:

| Shape | Where GD reads it | How we scale it |
|---|---|---|
| Axis-aligned rect `m_objectRect` | virtual `getObjectRect()` (`win 0x1976a0`, hookable) recomputes it when `m_isObjectRectDirty`; `getObjectRectPointer()` is `win inline` = `if (dirty) getObjectRect(); return &m_objectRect;` | hook `getObjectRect()`, shrink `m_objectRect` in place **only when it was dirty before the call** (fresh recompute from position/size → never compounds). `HazardHitboxHook.cpp` |
| Oriented box `m_orientedBox` (`OBB2D`) — only for rotations off the 90° grid | `updateOrientedBox()` (`win 0x1a1570`, hookable) rebuilds it when `m_isOrientedBoxDirty` or `m_orientedBox == nullptr`; collision uses `m_orientedBox->overlaps(player->m_orientedBox)` when `m_orientedBox && m_shouldUseOuterOb` (qolmod trajectory `ShowTrajectory/Hooks.cpp:61`, AllModesPlatformer `:47`) | hook `updateOrientedBox()`, move `m_corners[i]` toward `m_center`, then `computeAxes()` (Geode inline body) + `orderCorners()` — exactly what qolmod AccurateHitboxes does with its own corners (`refs/qolmod/src/Hacks/Level/AccurateHitboxes.cpp:122-163`) and that changes real collision |
| Circle `m_objectRadius` (saws) | `getObjectRadius()` is `win inline` = `m_objectRadius * max(m_scaleX, m_scaleY)`; test is `GJBaseGameLayer::playerCircleCollision(PlayerObject*, GameObject*)` (`win 0x211df0`, hookable, unused) | write the field: `m_objectRadius *= scale` in `PlayLayer::addObject` and by ratio when the level changes |

- Grid-aligned objects (rotation % 90 == 0) never use the OBB — qolmod nudges
  their rotation by 1° in `PlayLayer::addObject` to force the OBB path
  (`AccurateHitboxes.cpp:169-181`). For them the AABB *is* the hitbox.
- `dirtifyObjectRect()` (`win inline`) is just `m_isObjectRectDirty = m_isOrientedBoxDirty = true`;
  setting both flags forces a recompute through the hooks on next use.
- `getObjectRect()` when dirty also clears `m_isObjectRectDirty` and sets
  `m_boxOffsetCalculated` (qolmod restores both after peeking, `HitboxNode.cpp:140-157`).
- Player rects come from `getObjectRect(float w, float h)` (by value:
  `m_vehicleSize` / `0.3f`), a different virtual — untouched by the hook above.
  `PlayerObject` does not override `getObjectRect()`.
- Open questions the hazard-hitbox log answers: is `getObjectRect()` the only path that
  fills `m_objectRect` (counter `HazardHitbox: shrunk N rects`), is the returned
  reference `m_objectRect` (`[returned ref is NOT m_objectRect]` marker), does
  nothing reset `m_objectRadius` during an attempt.

## Player collision shape (wave-hitbox) — verified in game 2026-09-17

The player is a `GameObject` but its rects come from the **other** overload,
`GameObject::getObjectRect(float width, float height)` (`win 0x1976c0`,
hookable, returns by value). Both arguments are *size factors*, not sizes:
qolmod reads the outer box with `getObjectRect(m_vehicleSize, m_vehicleSize)`
and the inner one with `getObjectRect(0.3f, 0.3f)`
(`refs/qolmod/src/Hacks/Level/Hitboxes/HitboxNode.cpp:417-425`).

So the player hitbox **is** reachable: hook that overload, multiply both
arguments, let GD build the rect — confirmed in our own build (wave-hitbox
works, user 2026-09-17). qolmod's HitboxMultiplier does exactly this
(`refs/qolmod/src/Hacks/Level/HitboxMultiplier.cpp:103-131`) and registers
`SafeModeTrigger::Attempt` for it, i.e. it changes real collision, not just the
drawing. This supersedes the older "player rect is inlined, rejected" note in
`ROADMAP.md` (`tiny`). Ours: `src/hooks/PlayerHitboxHook.cpp`, gated on
`typeinfo_cast<PlayerObject*>(this)` and `m_isDart`.

- `PlayerObject::m_isDart` is the wave flag (start mode 4 —
  `refs/cleanstartpos/src/CreateStartPos.cpp:49`, CBF `main.cpp:422`). It is per
  `PlayerObject`, so in dual each half is checked on its own.
- `PlayerObject` does **not** override either `getObjectRect`, so one
  `$modify(GameObject)` covers it.
- **Not covered (unverified):** the player's oriented box. GD compares
  `obj->m_orientedBox->overlaps(player->m_orientedBox)` when the *hazard* is
  rotated off the 90-degree grid, and where the player OBB gets its size is
  unknown — probably the no-argument `getObjectRect()`, which this hook does not
  touch. Grid-aligned spikes (the common wave hazard) use the AABB, so the
  augment should still bite; if it visibly does nothing against *rotated*
  hazards only, that is this gap, and the fix is to extend
  `updateOrientedBox()` to `PlayerObject` the way `HazardHitboxHook` does for
  hazards.

## Removing an object at runtime (cat) — from Geode inline source + xdBot, unverified in game

GD's own primitive is `GameObject::destroyObject()` (Geode inline,
`bindings/2.2081/inline/GameObject.cpp:335`):
`m_isDisabled = true; m_isDisabled2 = true; setOpacity(0);` — the sprite
vanishes and the object is skipped by collision: xdBot's trajectory sim sets
exactly those two flags on every object it wants the fake player to pass
through around `GJBaseGameLayer::collisionCheckObjects`
(`refs/xdbot/src/hacks/show_trajectory.cpp:322-350`). Related inlines:
`disableObject()` = the same + `triggerActivated(0)`; `makeInvisible()` /
`makeVisible()` toggle `m_isDisabled2` + `m_isInvisible` + opacity (so
`m_isDisabled2` reads as "not part of the level right now").
`GJBaseGameLayer::destroyObject(GameObject*)` (`win 0x216090`, the layer-level
version with the break effect) is what qolmod calls on coins
(`AutoCollectCoins.cpp:33`); untried here — could give a visual for the cat.

- `GameObject::setOpacity` is virtual (`win 0x198800`), so opacity 0 is
  expected to cover the detail/glow sprites; **(unverified)** whether the glow
  batch sprite actually follows. `m_particle` is left alone (particles are
  claimed from a pool via `claimParticle()`, hiding one could hit another
  object).
- Whether `GameObject::resetObject()` (`win 0x1906d0`) clears the flags on
  reset is **(unverified)**; `PlayLayerHook.cpp::catRestore()` restores
  flags + the recorded opacity itself before every `resetLevel`, and logs how
  many were still disabled (0 there = GD reset them first, so the fallback
  could go).
- Oddity: inline `updateUnmodifiedPositions()` (`GameObject.cpp:192`) clears
  `m_isDisabled` — either a misnamed field or an editor-only path; the cat
  sets both flags like GD does, so one surviving is enough.

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

Approach (implemented; placement + single respawn **verified in game 2026-09-16**,
multi-respawn model unverified): `markCheckpoint()` / `resetLevel()` wrapped in a
temporary `m_isPracticeMode = true`. Our own `std::vector<Ref<CheckpointObject>>`
is the source of truth; before a checkpoint reset GD's `m_checkpointArray` is
rebuilt from it if it diverged, and `m_currentCheckpoint` is pointed at the
target. See `PlayLayerHook.cpp::resetLevel` / `syncCheckpointArray` / `consumeCheckpoint`.

Facts read from Geode's inline implementations (= reverse-engineered GD code,
`build/_deps/bindings-src/bindings/2.2081/inline/`), 2026-09-16:
- `PlayerObject::removePlacedCheckpoint()` (`PlayerObject.cpp:549`) =
  `if (m_checkpointTimeout) { m_playLayer->removeCheckpoint(false); m_checkpointTimeout = false; }`,
  located right before `playerDestroyed` → **GD deletes a checkpoint placed
  less than 0.1 s before the death** (`updateCheckpointTest` clears
  `m_checkpointTimeout` after `.1f` s, `:722-727`). So `m_checkpointArray` can
  legitimately lose our newest checkpoint during a death — never rely on it
  alone. Whether `PlayLayer::markCheckpoint()` (our path) sets the timeout is
  **(unverified)**; `PlayerObject::tryPlaceCheckpoint` (`win 0x3a32d0`) is GD's
  auto-checkpoint path.
- Same site shows `removeCheckpoint(bool first)` with `false` = **remove the
  newest** checkpoint, callable mid-death without triggering a reset. Used by
  us to consume a checkpoint right after respawning at it. **(from GD's own
  usage; our call unverified)**
- `getLastCheckpoint()` / `loadLastCheckpoint()` (`PlayLayer.cpp:132, 205`)
  read `m_checkpointArray->lastObject()` with no null check on the array.
- `queueCheckpoint()` (`:214`) just sets `m_tryPlaceCheckpoint = true`; GD's own
  Z key defers placement to the next update that way. We call
  `markCheckpoint()` directly from the key handler (between frames).
- `GJBaseGameLayer::removeAllCheckpoints()` is an empty inline; the real one
  is `PlayLayer::removeAllCheckpoints()` `win 0x3b8040` (virtual).
- qolmod's StartposSwitcher sets `m_currentCheckpoint = nullptr` before
  `resetLevel()` to force a start-pos respawn (`StartposSwitcher.cpp:149`) →
  GD's `resetLevel` consults `m_currentCheckpoint`. We set it explicitly on
  both branches.
- `PlayerObject::m_pendingCheckpoint` exists (`GeometryDash.bro:14788`);
  purpose unknown.

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

## Keyboard input on Windows (Geode 5)

Facts (all read from loader source, `$GEODE_SDK/loader/src` / `include`):
- Geode 5 replaces GD's key handling with its own raw-input path
  (`platform/windows/input.cpp:457-535`): `KeyboardInputEvent(keyCode).send(data)`
  first, then IME, then `CCKeyboardDispatcher::dispatchKeyboardMSG` — the
  latter **only if no listener returned `Stop`**. So a hook on
  `dispatchKeyboardMSG` / `UILayer::keyDown` never sees a key some listener ate.
- Delivery order inside `GlobalEvent::send` (`loader/Event.hpp:1045-1058`):
  key-filtered listeners (`KeyboardInputEvent(KEY_X).listen`) first — if one
  returns `Stop` the unfiltered ones never run — then unfiltered listeners
  (`KeyboardInputEvent().listen`), ordered by ascending `priority` (second
  argument of `listen`, default 0; `Port::addReceiver`, `Event.hpp:122-137`),
  equal priority = registration order.
- **The loader's keybind-settings listener is registered in `queueMods()`
  (`LoaderImpl.cpp:409`), before any mod binary is loaded (`loadModGraph`,
  `:546`).** At priority 0 it therefore runs *before* every listener a mod adds
  from `$execute` / `$on_mod(Loaded)`. It sends `KeybindSettingPressedEventV3`
  for every setting bound to the key, in ascending mod.json `"priority"`
  order (`onKeybindSettingChanged`, `:1440-1449`; default 0), and returns
  `Stop` as soon as one setting's listener does (`:440`).
- CustomKeybinds (installed) binds `place-checkpoint` = Z and
  `delete-checkpoint` = X (its `mod.json:80-92`, priority 0) with node-scoped
  listeners on `GJBaseGameLayer` that return `Stop` whenever the level isn't
  paused, practice mode or not (`refs/custom-keybinds/src/UILayer.cpp:229-242`).
- **Root cause of "hotkeys never fire" (2026-09-16):** in a level, X/Z went
  loader listener → CustomKeybinds `Stop` → event stopped. Our unfiltered
  listener (registered later, priority 0) never ran, `dispatchKeyboardMSG` was
  never called, and `listenForKeybindSettingPresses` on our own setting lost
  to CustomKeybinds' same-key setting that was dispatched first. All three
  failed attempts had this single cause. Outside a level the keys would have
  reached us — nobody tested there.
- Fix (both in `PlayLayerHook.cpp`, either alone suffices, per-frame dedup on
  the layer): our keybind settings carry `"priority": -5` and are handled by
  node-scoped `addEventListener(KeybindSettingPressedEventV3(Mod::get(), "…"))`
  in `PlayLayer::init` (CustomKeybinds' own pattern); plus a raw
  `KeyboardInputEvent().listen(fn, -1)` from `$on_mod(Loaded)` that runs ahead
  of the loader's listener. Return `Stop` only when the press was acted on so
  an idle X/Z still reaches CustomKeybinds. **Verified in game 2026-09-16.**
- `mod.json` keybind settings: `"default"` string or array, `"priority"` int
  (lower dispatched first), `"category"`; read with
  `Mod::get()->getSettingValue<std::vector<Keybind>>("key")`.
- RTTI across DLLs is unreliable: `cast::typeinfo_pointer_cast`, never
  `dynamic_pointer_cast`, on Geode objects.
- CustomKeybinds overrides `UILayer::handleKeypress` completely (only Escape
  passes), so hooking `handleKeypress` sees nothing; `UILayer::keyDown`
  (`win 0x4cde50`) still sees every key that reaches the dispatcher.
- Working references: qolmod unfiltered listener from `$execute`
  (`refs/qolmod/src/Keybinds/Hooks.cpp:51-96`), DevTools from `$on_mod(Loaded)`
  (`refs/devtools/src/backend.cpp:504`), CustomKeybinds node-scoped
  (`refs/custom-keybinds/src/UILayer.cpp:47-102, 321-329`).

## Fonts & Korean text — verified in game 2026-09-17 (Geode-generated and baked-outline fonts)

- GD's `bigFont` / `goldFont` / `chatFont` (`Resources/*.fnt`) carry
  `32-126,8226` only: a Korean string in them draws nothing. `CCLabelBMFont`
  itself is UTF-8 aware; it only needs the glyphs.
- `mod.json` `resources.fonts.<Name> { path, size, charset, outline? }` makes
  Geode CLI (3.9.0) convert a TTF/OTF at package time
  (`refs/geode-docs/mods/resources.md:74-105`). Verified output: `<Name>.fnt/.png`,
  `<Name>-hd.*`, `<Name>-uhd.*` where **`size` is the UHD size** and hd / sd
  are ½ / ¼ (GD's own: bigFont 32/64/128, goldFont 24/48/96, chatFont 16/32/56).
  Every `.fnt` says `file="<Name>.png"`; cocos adds the -hd/-uhd suffix itself.
  Atlases are single-page and NPOT (e.g. 1063×1393 for 227 glyphs at 96);
  `base=` is wrong-looking (3/7/15) but cocos2d 2.x ignores it (uses lineHeight).
- Charset syntax: `32-126,8226,44032-44033,…` — ranges with `-`, comma-separated,
  decimal codepoints. The full Hangul block (44032-55203, 11 172 glyphs) would
  blow the atlas at a readable size, so `scripts/fontcharset.ps1` lists only the
  codepoints found in string literals under `src/` (comments skipped) and
  rewrites the charset in `mod.json`; `build.ps1` runs it first. The CLI caches
  fonts by content hash (`.geode_cache`), so a charset change rebuilds them.
- Standalone check without a full build:
  `geode package resources <dir with mod.json> <out>` (needs `description` in
  mod.json). `Add-Type System.Drawing` + draw the png over black to eyeball it
  (the atlas is white-on-transparent).
- Use in code: `CCLabelBMFont::create(text, "AugName.fnt"_spr)`; `_spr` is a
  constexpr literal, so `constexpr char const* Name = "AugName.fnt"_spr;` works
  (`src/ui/Fonts.hpp`). `Popup::setTitle(title, font, scale)` takes the font too.
- Letter spacing: `CCLabelBMFont::setExtraKerning(int)` is a RobTop addition
  (`Geode/cocos/label_nodes/CCLabelBMFont.h:327`, `CC_SYNTHESIZE_NV`). Whether
  GD applies it per glyph and in which unit is **(unverified)**; no longer used
  (the baked fonts carry their own spacing).
- **`outline` is a no-op in Geode CLI 3.9.0.** `mod_file.rs` deserialises
  `BitmapFont { path, charset, size, outline, color }`, but in
  `src/util/bmfont.rs` the SDF outline code is commented out and
  `generate_char` writes a flat `color` + alpha glyph (read from the v3.9.0 tag
  on GitHub). Only `color` works. Outlined GD-style text therefore comes from
  `scripts/fontgen.py` (Pillow `stroke_width` / `stroke_fill` + an offset black
  copy for the shadow), which writes the same file set Geode would
  (`<Name>.fnt/.png`, `-hd`, `-uhd`, page `file="<Name>.png"`) and ships it via
  `resources.files` — the CLI copies `files` into the same
  `resources/<mod.id>/` folder as generated fonts (`package.rs:234-237`), so
  `"AugName.fnt"_spr` resolves identically.
- BMFont semantics cocos relies on (`CCLabelBMFont::createFontChars`): glyph
  quad at `(pen + xoffset, lineTop - yoffset)`, then `pen += xadvance`;
  `lineHeight` is the only line metric used (`base` ignored). With an outline
  baked into the glyph the ring extends `outline` px past the ink on every
  side, so `xadvance` must grow by `outline` or the next glyph's black ring
  paints over this glyph's white (Hangul in ImcreSoojin has zero side bearings).
  Whitespace: Pillow's `getbbox(ch, stroke_width=n)` is non-empty for a space
  (the stroke pads it) — check the rendered alpha and emit `width=0`.
- ImcreSoojin (`resources/fonts/ImcreSoojin.ttf`, "아임크리수진"): 17 363
  glyphs, all 11 172 Hangul syllables, ★ ☆ → present, • (U+2022) absent.
  hhea ascent 910 / descent 250 per 1000 em → lineHeight 1.16 em.
- **The `width` argument does not wrap mod fonts** (verified in game
  2026-09-17, user-confirmed for both the Geode-generated Pretendard fnt and
  the baked ImcreSoojin fnt): `CCLabelBMFont::create(text, fnt, width, align)`
  and `setWidth()` left every description on one line. Whether GD's own fonts
  wrap with it is untested; RobTop's `updateLabel` is in the GD binary, so the
  reason isn't readable. Verified workaround: `AugmentDraftPopup.cpp`
  `wrapText()` measures words with throwaway labels (`getContentSize().width`
  comes from the advances and is correct) and inserts `\n`; a label created
  with `kCCLabelAutomaticWidth` honours newlines.
- `CCNode::getPosition()` in Geode's cocos headers resolves ambiguously when
  assigned into a `CCPoint` with `=` (clang: "operand types CCPoint and void");
  use `getPositionX()/Y()` or keep the point you set.
- Pretendard (OFL) is installed per-user at
  `%LOCALAPPDATA%\Microsoft\Windows\Fonts\Pretendard-*.ttf`; static TTFs, so no
  variable-font question. License text is shipped in `resources/fonts/`.

## Director pause vs. animation — verified in game 2026-09-17 (reveal plays while paused)

`CCDirector::pause()` (what `AugmentManager::pauseGameForDraft` calls) makes
`drawScene` skip `m_pScheduler->update()`, and `CCActionManager` is driven by
the scheduler, so **no cocos action runs while a draft is open** — including
`FLAlertLayer::show()`'s elastic pop-in (hence `m_noElasticity = true`) and
`CCMenuItemSpriteExtra`'s press bounce. `visit()` / `draw()` still run every
frame (the animation interval is restored to the real one), so a node can
animate itself from an overridden `visit()` with `std::chrono::steady_clock`
(`AugmentDraftPopup::stepReveal`).

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
