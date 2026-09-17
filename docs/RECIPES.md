# Recipes — snippets in this project's style

Each recipe says where it is verified. "verified <date>" = runs in game on
this machine; "(from ref X)" = read from a working mod's source, not yet used
here. Bindings quoted were checked with `scripts\bro.ps1` on 2026-09-16.
Match `src/` conventions: `geode::prelude`, `$modify(AugX, X)`, `struct Fields`,
`m_fields.self()`, `log::info("{}", …)`.

## Hook with per-attempt state — verified 2026-09-16 (`src/hooks/PlayLayerHook.cpp`; in this mod per-attempt state now lives in an `Augment` subclass, see `src/augments/Shield.cpp` `onAttemptStart`)

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

## Slow-mo — verified 2026-09-16 (`src/hooks/SchedulerHook.cpp` + `src/game/Scales.cpp` `setTime`; driven by `src/augments/SlowMo.cpp`)

```cpp
#include <Geode/modify/CCScheduler.hpp>
inline float g_time = 1.f;               // scales::time() in Scales.hpp

class $modify(AugScheduler, CCScheduler) {
    void update(float dt) { CCScheduler::update(dt * g_time); }
};

void setTime(float scale) {               // music follows the game
    g_time = scale;
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

## Shrink the *player* collision — verified 2026-09-17 (from ref qolmod `HitboxMultiplier.cpp:103-131`; ours in `src/hooks/PlayerHitboxHook.cpp`, namespace `augment::player`)

A different overload from the one below: the player's rects come from
`getObjectRect(width, height)`, whose arguments are size *factors*
(`m_vehicleSize`, `0.3f`). Scale the arguments, let GD build the rect.

```cpp
class $modify(AugPlayerRect, GameObject) {
    CCRect getObjectRect(float width, float height) {     // win 0x1976c0
        if (g_scale >= 1.f) return GameObject::getObjectRect(width, height);  // hot: bail first
        auto player = typeinfo_cast<PlayerObject*>(this);
        if (!player || !player->m_isDart) return GameObject::getObjectRect(width, height);
        return GameObject::getObjectRect(width * g_scale, height * g_scale);
    }
};
// m_isDart = wave mode, per PlayerObject (dual: each half checked separately).
// Does NOT cover the player's oriented box (rotated hazards) - see GD-INTERNALS.
```

## Shrink / reshape an object's collision — verified 2026-09-16 (from ref qolmod `AccurateHitboxes.cpp:122-163`; ours in `src/hooks/HazardHitboxHook.cpp`, namespace `augment::hazard`)

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

## Read settings / react to changes — verified (`src/game/AugmentManager.cpp` `gaugeRule`) / (from ref CBF `main.cpp:707-737`)

```cpp
auto n = Mod::get()->getSettingValue<int64_t>("debug-threshold");   // int settings are int64_t
$on_mod(Loaded) {
    listenForSettingChanges<bool>("some-flag", +[](bool v) { … });
}
```

## Button on a GD layer via node IDs — verified 2026-09-16 (`src/hooks/LevelInfoHook.cpp` `init`)

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

## Modal popup that must be answered — verified 2026-09-16 (`src/ui/AugmentDraftPopup.*`, `src/game/DraftSession.cpp` `showNext`)

- Subclass `geode::Popup`, override `keyBackClicked() {}` and `onClose(CCObject*) {}`.
- `Popup::init` puts the close button in `m_buttonMenu`; remove it before `setLayout`.
- Before `CCDirector::pause()`: `popup->m_noElasticity = true` (actions freeze).
- Show the cursor: `auto v = CCEGLView::get(); saved = v->m_bShouldHideCursor; v->showCursor(true);`
- The pick callback captures **nothing**; it uses `AugmentManager::get()` and
  `PlayLayer::get()` at call time (rule 5).
- `CCDirector::pause()` drops the frame interval to 1/4 s — restore with
  `setAnimationInterval(previous)`.

## In-level HUD — verified 2026-09-16 (`src/ui/RunHud.*`)

Ours is a node added to `m_uiLayer` (qolmod does the same for its labels,
`refs/qolmod/src/Labels/Hooks.cpp:9`): screen space, never drifts with the
camera. `CCLayerColor::create(color, w, h)` is the cheapest filled rect
(positions by its bottom-left; layers ignore the anchor point); resize with
`setContentSize`. Pool labels/rows and compare strings before `setString`
when refreshing every frame.

## Decorate GD's progress bar — verified in game 2026-09-17 (`src/ui/ProgressMarks.*`, `PlayLayerHook.cpp` `attachToProgressBar`)

`m_progressBar` does not exist yet when `PlayLayer::init` returns on online
levels; hook `setupHasCompleted` too and attach once (`GD-INTERNALS.md`
"Progress bar" has the geometry).

```cpp
void attachToProgressBar(char const* where) {
    auto f = m_fields.self();
    if (!f->hud || f->marks) return;          // once
    if (!m_progressBar) { log::info("ProgressBar not there yet at {}", where); return; }
    f->hud->attachGauge(m_progressBar, m_progressFill, m_percentageLabel);
    f->marks = ProgressMarks::create(m_progressBar, m_progressFill);   // bar->addChild(this, 10)
}
bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
    if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
    // ...
    this->attachToProgressBar("init");        // works for local levels
    return true;
}
void setupHasCompleted() {
    PlayLayer::setupHasCompleted();           // online levels: bar is created in here
    if (this->isRunLevel()) this->attachToProgressBar("setupHasCompleted");
}
```

Track geometry from the fill, never from constants:
```cpp
bool fillIsChild = fill && fill->getParent() == bar;
float inset = fillIsChild ? fill->getPositionX() : 2.f;
float fillH = fillIsChild ? fill->getContentSize().height : 8.f;
float fillBottom = fillIsChild ? fill->getPositionY() - fill->getAnchorPoint().y * fillH : (bar.height - fillH) / 2;
float x = inset + (bar.width - 2 * inset) * percent / 100.f;
```

## Twin of a GD node (same look, mirrored position) — verified in game 2026-09-17 (`src/ui/RunHud.cpp` `attachGauge`)

Copy the frame and transform instead of loading a texture by name (the
standalone `GJ_progressBar_001.png` file is not the in-level bar):

```cpp
auto twin = CCSprite::createWithSpriteFrame(bar->displayFrame());
twin->setScaleX(bar->getScaleX()); twin->setScaleY(bar->getScaleY());
twin->setAnchorPoint(bar->getAnchorPoint()); twin->setColor(bar->getColor());
twin->setPosition(CCPoint(bar->getPositionX(), winSize.height - bar->getPositionY()));  // bottom edge
// CCPoint(...) not { ... }: `pos = { x, y }` is ambiguous with Geode's cocos headers.
auto label = CCLabelBMFont::create("", percentLabel->getFntFile());   // GD's percent font
label->setScale(percentLabel->getScale());
```

A per-frame eased value lives in an `update(float)` override after
`scheduleUpdate()` — it runs on the scheduler, so it freezes with the
director during drafts and slows with slow-mo, which is fine for a HUD.

## Korean text / custom font — verified in game 2026-09-17 (`src/ui/Fonts.hpp`, `mod.json`, `scripts/fontcharset.ps1`, `scripts/fontgen.py`)

```json
"resources": {
    "fonts": { "AugDebug": { "path": "resources/fonts/Pretendard-Regular.ttf", "size": 64, "charset": "32-126,8226,…" } },
    "files": [ "resources/fonts/gen/*.fnt", "resources/fonts/gen/*.png" ]
}
```
```cpp
// src/ui/Fonts.hpp
constexpr char const* Name  = "AugName.fnt"_spr;   // ImcreSoojin 24 sd, white + black outline + shadow (baked)
constexpr char const* Text  = "AugText.fnt"_spr;   // ImcreSoojin 16 sd, same look
constexpr char const* Debug = "AugDebug.fnt"_spr;  // Pretendard 16 sd, plain (Geode-generated)
// anywhere
auto label = CCLabelBMFont::create("결계인가?", fonts::Name);   // UTF-8 literal, file saved as UTF-8
label->setColor({ 255, 215, 60 });                              // tints the white; the outline stays black
auto wrapped = CCLabelBMFont::create(text, fonts::Text, widthInFontUnits / scale, kCCTextAlignmentCenter);
wrapped->setScale(scale);
wrapped->setWidth(widthInFontUnits / smallerScale);             // re-wraps; getContentSize() updates
popup->setTitle("증강 선택", fonts::Name, 0.8f);
```
- Geode fonts: `size` is the UHD pixel size (sd = size / 4). 96 ≈ goldFont, 64 ≈ chatFont.
  Its `outline` key does nothing (CLI 3.9.0), so outlined fonts are baked by
  `scripts/fontgen.py` (`FONTS` table: name / size / outline / shadow in UHD px)
  into `resources/fonts/gen/` (gitignored) and shipped as plain files.
  `build.ps1` runs it; it's a no-op when the ttf/charset/params are unchanged
  (`--force` to redo). Needs `py -3 -m pip install pillow fonttools`.
- Never put Korean in a `bigFont` / `goldFont` / `chatFont` label (draws nothing).
- New Korean literal in `src/` → just build; `build.ps1` regenerates the charset
  and re-bakes the fonts. Text that is *not* a literal (read from a file, typed
  by the user) needs the full Hangul range instead.
- Wrapping: **the `width` argument / `setWidth()` never wrapped either mod
  font in game** (verified 2026-09-17). Use `wrapText(text, font, maxWidth)`
  from `AugmentDraftPopup.cpp`: it measures words with throwaway labels and
  inserts newlines (Korean text has spaces between words; explicit `\n` is
  kept); then `create(wrapped, font, kCCLabelAutomaticWidth, kCCTextAlignmentCenter)`.
  `maxWidth` is in label units, i.e. divide the point width by the scale.
- Fit-to-slot: re-wrap at `inner / scale`, lower `scale` a step while
  `getContentSize().height * scale > slot` (`AugmentDraftPopup::createCard`).

## GD-style card / button panel — verified in game 2026-09-17 (`src/ui/AugmentDraftPopup.cpp`)

```cpp
// square02b_001 is a plain white rounded square, corner radius ~9 pt at scale 1;
// scale the slices to get the radius you want, then size in unscaled units.
NineSlice* roundedBox(CCSize size, ccColor3B color, float radius, GLubyte opacity = 255) {
    float const scale = radius / 9.f;
    auto box = NineSlice::create("square02b_001.png");
    box->setScale(scale); box->setContentSize(size / scale);
    box->setColor(color); box->setOpacity(opacity);
    return box;
}
auto rim  = roundedBox({ w + 4, h + 4 }, ccWHITE, 8.f);          // white rim, 2 pt
auto body = NineSlice::create("GJ_button_01.png");               // GD's green button: black ring + flat (122,222,45) fill
body->setContentSize({ w, h });                                  // default insets = a third of the 40 pt texture
auto band = roundedBox({ w - 5, 20 }, ccBLACK, 4.f, 70);         // darker footer strip
auto frame = roundedBox({ 120, 70 }, ccBLACK, 4.5f);             // bordered panel: outer box …
auto panel = roundedBox({ 116, 66 }, ccWHITE, 4.5f - 2.f);       // … + inner box, radius minus border
```
- Both sprites are files in `Resources/` (sd/hd/uhd), not sheet frames, so
  `NineSlice::create(file)` is right; `square02b_001` is what the loader's own
  mod list uses for tinted panels (`loader/src/ui/mods/list/ModItem.cpp:97`).
- A bordered panel = black box + inner box 2× border smaller with radius
  reduced by the border, so the border stays even around the corners.
- Animating inside a `CCMenuItemSpriteExtra` without moving its touch area:
  wrap the visual in a fixed-size holder node (`setContentSize`), make the
  holder the item's sprite, and move/scale/rotate the visual.
- Popup with floating content: `m_bgSprite->setVisible(false)` + `setOpacity(160)`
  on the popup (it is the dimming `CCLayerColor`), title via `setTitle`.

## Cross-DLL casts — verified 2026-09-16

`cast::typeinfo_pointer_cast<KeybindSettingV3>(Mod::get()->getSetting(key))`
and `typeinfo_cast<PlayLayer*>(node)`; never `dynamic_cast` / `dynamic_pointer_cast`
on Geode objects (`refs/geode-docs/tutorials/casting.md`).
