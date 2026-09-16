# Recipes — snippets in this project's style

Each recipe says where it is verified. "verified <date>" = runs in game on
this machine; "(from ref X)" = read from a working mod's source, not yet used
here. Bindings quoted were checked with `scripts\bro.ps1` on 2026-09-16.
Match `src/` conventions: `geode::prelude`, `$modify(AugX, X)`, `struct Fields`,
`m_fields.self()`, `log::info("{}", …)`.

## Hook with per-attempt state — verified 2026-09-16 (`src/hooks/PlayLayerHook.cpp`)

```cpp
#include <Geode/modify/PlayLayer.hpp>

class $modify(AugPlayLayer, PlayLayer) {
    struct Fields {
        bool deathCounted = false;   // destroyPlayer can fire twice per death
    };

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // Never block or count the anticheat call (CLAUDE.md rule 6).
        if (object == m_anticheatSpike) return PlayLayer::destroyPlayer(player, object);
        auto f = m_fields.self();
        …
        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->deathCounted = false;
        // isRunning() is false during PlayLayer::init's own resetLevel — no popups then.
    }
};
```

Alternative real-death test (from ref death-tracker `DTPlayLayer.cpp:225-259`):
call the original first, then `if (!player->m_isDead) return;`.

## Hook priority vs another mod — (from refs custom-keybinds / death-tracker)

```cpp
class $modify(AugUILayer, UILayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("UILayer::keyDown", Priority::Early);   // or Priority::Late
        // setHookPriorityPre / Post exist too (death-tracker uses Pre + Priority::First)
    }
};
```

## Skip a death (noclip) — verified 2026-09-16 (Shield)

Don't call `PlayLayer::destroyPlayer` and the player passes through. Always
forward when `object == m_anticheatSpike`; qolmod also forwards when `!player`
or `m_levelEndAnimationStarted` (`refs/qolmod/src/Hacks/Level/Noclip/Hooks.cpp:202-235`).

## Slow-mo — verified 2026-09-16 (`PlayLayerHook.cpp` top + `AugScheduler`)

```cpp
#include <Geode/modify/CCScheduler.hpp>
float g_timeScale = 1.f;

class $modify(AugScheduler, CCScheduler) {
    void update(float dt) { CCScheduler::update(dt * g_timeScale); }
};

void setGameSpeed(float scale) {          // music follows the game
    FMOD::ChannelGroup* master = nullptr;
    auto engine = FMODAudioEngine::sharedEngine();
    if (engine && engine->m_system && engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master)
        master->setPitch(scale);
}
```

Gameplay-only variant (menus stay at normal speed) — (from ref qolmod
`Speedhack/Hooks.cpp:75-83`): scale `dt` in a `GJBaseGameLayer::update(float)`
hook instead. If Click Between Frames is loaded, also multiply
`CCDirector::get()->m_fActualDeltaTime` and `m_fDeltaTime` (`Hooks.cpp:38-45`).

## Draw in world space (hitboxes) — verified 2026-09-16 (Foresight)

```cpp
auto node = CCDrawNode::create();
node->setID("foresight-hitboxes"_spr);
m_objectLayer->addChild(node, 1000);           // world coordinates, follows the camera
…
node->clear();
node->drawRect(rect.origin, rect.origin + rect.size, {0,0,0,0} /* premultiplied: no fill */, 1.f, border);
```

`GameObject::getObjectRect()` for the box; `m_objectRadius > 0` → circle.
Extras qolmod handles (`HitboxNode.cpp:159-243`): slopes via `m_slopeDirection`,
rotated boxes via `m_orientedBox->m_corners`. Sort `m_objects` by x once and
`lower_bound` per frame — there can be 10k+ objects.

## Shrink / reshape an object's collision — (from ref qolmod `AccurateHitboxes.cpp:122-163`; ours in `src/hooks/HazardHitboxHook.cpp`, unverified 2026-09-16)

```cpp
#include <Geode/modify/GameObject.hpp>
class $modify(AugGameObject, GameObject) {
    // AABB: shrink only a fresh recompute (GD rebuilds from position + size,
    // so this never compounds); cached reads already carry the shrink.
    CCRect const& getObjectRect() {                       // win 0x1976a0
        bool wasDirty = m_isObjectRectDirty;
        auto& rect = GameObject::getObjectRect();
        if (wasDirty && isTarget(this) && !(m_shouldUseOuterOb && m_orientedBox)) shrinkInPlace(m_objectRect);
        return rect;
    }
    // OBB (off-grid rotations only): same pattern as qolmod.
    void updateOrientedBox() {                            // win 0x1a1570
        bool dirty = m_isOrientedBoxDirty || !m_orientedBox;
        GameObject::updateOrientedBox();
        if (!dirty || !m_orientedBox || !isTarget(this)) return;
        auto c = m_orientedBox->m_center;
        for (auto& p : m_orientedBox->m_corners) p = c + (p - c) * scale;
        m_orientedBox->computeAxes();                     // Geode inline body, callable
        m_orientedBox->orderCorners();                    // win 0x6dda0
    }
};
// Circles: GD reads m_objectRadius inline -> write the field (PlayLayer::addObject).
// Force a recompute later: obj->m_isObjectRectDirty = obj->m_isOrientedBoxDirty = true;
```

## Mirror portals — (from ref qolmod `NoMirrorPortal.cpp:22-31`, unverified here)

```cpp
#include <Geode/modify/GJBaseGameLayer.hpp>
class $modify(AugBaseGameLayer, GJBaseGameLayer) {
    void toggleFlipped(bool flip, bool noEffects) {     // win 0x2467d0
        if (unmirrorActive()) return;                   // swallow the flip
        GJBaseGameLayer::toggleFlipped(flip, noEffects);
    }
};
```

## Keybind setting → callback

Setting in `mod.json`: `{"type": "keybind", "default": "X", "category": "gameplay"}`
(saved as `[{"key": 88, "modifiers": 0}]`).

Node-scoped (from ref custom-keybinds `UILayer.cpp:47-102`; preferred, unverified here):
```cpp
// inside PlayLayer::init after the original returned true
this->addEventListener(
    KeybindSettingPressedEventV3(Mod::get(), "keybind-slowmo"),
    [this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
        auto ref = Ref(this);
        if (!down || repeat) return ListenerResult::Propagate;
        static_cast<AugPlayLayer*>(this)->toggleSlowMo();
        return ListenerResult::Stop;
    });
```

Global (from ref qolmod `Keybinds/Hooks.cpp:51-96`; ours is identical and did
**not** fire — see `docs/GD-INTERNALS.md` "Keyboard input"):
```cpp
$execute {
    log::info("augmented-gd: $execute ran");                 // prove static init first
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        if (data.action != KeyboardInputData::Action::Press) return ListenerResult::Propagate;
        …
        return ListenerResult::Propagate;
    }).leak();                                              // without .leak() the handle dies immediately
    listenForKeybindSettingPresses("keybind-slowmo", [](Keybind const&, bool down, bool repeat, double) { … });
}
```

## Read settings / react to changes — verified (`AugmentManager.cpp:48-52`) / (from ref CBF `main.cpp:707-737`)

```cpp
auto n = Mod::get()->getSettingValue<int64_t>("draft-threshold");   // int settings are int64_t
$on_mod(Loaded) {
    listenForSettingChanges<bool>("some-flag", +[](bool v) { … });
}
```

## Button on a GD layer via node IDs — verified 2026-09-16 (`src/hooks/LevelInfoHook.cpp:12-30`)

```cpp
class $modify(AugLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        auto spr = ButtonSprite::create("AUG", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AugLevelInfoLayer::onAugment));
        btn->setID("augment-button"_spr);
        if (auto menu = this->getChildByID("left-side-menu")) { menu->addChild(btn); menu->updateLayout(); }
        return true;
    }
    void onAugment(CCObject*) { … }
};
```

IDs: `scripts\nodeids.ps1 LevelInfoLayer`. PauseLayer: hook `customSetup`, menu
`left-button-menu` (from ref death-tracker `DTPauseLayer.cpp:3-27`).

## Modal popup that must be answered — verified 2026-09-16 (`src/ui/AugmentDraftPopup.*`, `PlayLayerHook.cpp:440-470`)

- Subclass `geode::Popup`, override `keyBackClicked() {}` and `onClose(CCObject*) {}`.
- `Popup::init` puts the close button in `m_buttonMenu`; remove it before `setLayout`.
- Before `CCDirector::pause()`: `popup->m_noElasticity = true` (actions freeze).
- Show the cursor: `auto v = CCEGLView::get(); saved = v->m_bShouldHideCursor; v->showCursor(true);`
- The pick callback captures **nothing**; it uses `AugmentManager::get()` and
  `PlayLayer::get()` at call time (rule 5).
- `CCDirector::pause()` drops the frame interval to 1/4 s — restore with
  `setAnimationInterval(previous)`.

## In-level HUD — verified 2026-09-16 (`src/ui/RunHud.*`)

Ours is a node on the PlayLayer. qolmod attaches its labels to `m_uiLayer`
(`refs/qolmod/src/Labels/Hooks.cpp:9`), which is the screen-space layer GD
itself uses for the pause button — use that if the HUD ever drifts with the camera.

## Cross-DLL casts — verified 2026-09-16

`cast::typeinfo_pointer_cast<KeybindSettingV3>(Mod::get()->getSetting(key))`
and `typeinfo_cast<PlayLayer*>(node)`; never `dynamic_cast` / `dynamic_pointer_cast`
on Geode objects (`refs/geode-docs/tutorials/casting.md`).
