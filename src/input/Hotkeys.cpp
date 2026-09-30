// Two input paths, because the loader's own key listener runs before anything a
// mod adds at priority 0, and Custom Keybinds stops Z / X in levels there:
//   1. our keybind settings have "priority": -5 (listeners in PlayLayerHook.cpp)
//   2. a raw KeyboardInputEvent listener at -1, below
// Whichever fires first handles the key, the frame guard drops the other.

#include "Hotkeys.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/DraftSession.hpp"
#include "../game/LevelSession.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/Keyboard.hpp>

#include <span>

using namespace geode::prelude;

namespace augment::hotkeys {

namespace {

// last frame each [hotkey][down] was routed, and what it returned then
unsigned g_lastFrame[HotkeyCount][2] = {};
bool g_lastHandled[HotkeyCount][2] = {};

// empty = setting couldn't be read, use the mod.json default.
// span because this runs on every key event and a vector would copy the list
bool keybindMatches(char const* settingKey, enumKeyCodes fallback, Keybind const& pressed) {
    auto binds = Mod::get()->getSettingValue<std::span<Keybind const>>(settingKey);
    if (binds.empty()) return pressed == Keybind(fallback, KeyboardModifier::None);
    return std::ranges::contains(binds, pressed);
}

} // namespace

bool route(Hotkey which, bool down) {
    if (down && draft::isOpen()) return false;
    auto session = AugmentManager::get().session();
    if (!session) return false;

    // stored as frame + 1 so the zeroed arrays never match frame 0
    unsigned frame = CCDirector::sharedDirector()->getTotalFrames();
    int i = static_cast<int>(which);
    int d = down ? 1 : 0;
    if (g_lastFrame[i][d] == frame + 1) return g_lastHandled[i][d];

    bool handled = session->onHotkey(which, down);
    g_lastFrame[i][d] = frame + 1;
    g_lastHandled[i][d] = handled;
    return handled;
}

} // namespace augment::hotkeys

$on_mod(Loaded) {
    using namespace augment;
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        if (data.action == KeyboardInputData::Action::Repeat) return ListenerResult::Propagate;
        bool down = data.action == KeyboardInputData::Action::Press;
        // leave text fields alone
        if (CCIMEDispatcher::sharedDispatcher()->hasDelegate()) return ListenerResult::Propagate;

        Keybind pressed(data.key, data.modifiers);
        if (hotkeys::keybindMatches("keybind-slowmo", KEY_X, pressed)) {
            return hotkeys::route(Hotkey::SlowMo, down) ? ListenerResult::Stop : ListenerResult::Propagate;
        }
        if (hotkeys::keybindMatches("keybind-checkpoint", KEY_Z, pressed)) {
            return hotkeys::route(Hotkey::Checkpoint, down) ? ListenerResult::Stop : ListenerResult::Propagate;
        }
        if (hotkeys::keybindMatches("keybind-brake", KEY_C, pressed)) {
            return hotkeys::route(Hotkey::Brake, down) ? ListenerResult::Stop : ListenerResult::Propagate;
        }
        if (!down) return ListenerResult::Propagate;

        // debug: 1-9 grant augments in table order, Shift+1-9 the 10th on
        // (Shift+1 cat, 2 brake, 3 missile, 4 berserk), 0 fills the gauge
        bool shift = data.modifiers == KeyboardModifier::Shift;
        if (data.key >= KEY_Zero && data.key <= KEY_Nine && (data.modifiers == KeyboardModifier::None || shift)
            && AugmentManager::debugMode() && !draft::isOpen()) {
            if (auto session = AugmentManager::get().session()) {
                bool handled = false;
                if (data.key == KEY_Zero) {
                    if (!shift) handled = session->debugFillGauge();
                }
                else {
                    handled = session->debugGrant(data.key - KEY_One + (shift ? 9 : 0));
                }
                if (handled) return ListenerResult::Stop;
            }
        }
        return ListenerResult::Propagate;
    }, -1).leak();
}
