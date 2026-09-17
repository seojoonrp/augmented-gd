// Hotkeys (keys come from the "keybind" settings in mod.json).
//
// Why a plain listener never fired: the loader registers its own
// KeyboardInputEvent listener in queueMods(), before any mod binary is loaded,
// so at priority 0 it runs ahead of every listener a mod adds from $execute /
// $on_mod. That listener turns presses into KeybindSettingPressedEventV3 and
// returns Stop as soon as one setting's listener does. Custom Keybinds binds
// Z / X (practice checkpoints) on the PlayLayer and returns Stop whenever the
// level isn't paused, so in a level X and Z were consumed before they reached
// us (or CCKeyboardDispatcher, which is why hooking that saw nothing either).
//
// Two paths, each ahead of that:
//   1. our settings carry "priority": -5, so the loader dispatches them before
//      Custom Keybinds' for the same key; the node-scoped listener lives in
//      PlayLayer::init (PlayLayerHook.cpp) and dies with the layer.
//   2. a raw KeyboardInputEvent listener at priority -1, ahead of the loader's
//      (registered below).
// Whichever runs first handles the press; the other one hits the frame guard.

#include "Hotkeys.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/DraftSession.hpp"
#include "../game/LevelSession.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/Keyboard.hpp>

using namespace geode::prelude;

namespace augment::hotkeys {

namespace {

// One physical press (or release) can reach route() through both input
// paths in the same frame; the second one is dropped (and answered like the
// first). Indexed [hotkey][down].
unsigned g_lastFrame[HotkeyCount][2] = {};
bool g_lastHandled[HotkeyCount][2] = {};

// getSettingValue does the typeinfo cast for us; an empty result means the
// setting couldn't be resolved, so fall back to the mod.json default.
bool keybindMatches(char const* settingKey, enumKeyCodes fallback, Keybind const& pressed) {
    auto binds = Mod::get()->getSettingValue<std::vector<Keybind>>(settingKey);
    if (binds.empty()) return pressed == Keybind(fallback, KeyboardModifier::None);
    return std::ranges::contains(binds, pressed);
}

} // namespace

bool route(Hotkey which, bool down, char const* source) {
    char const* action = down ? "press" : "release";
    if (down && draft::isOpen()) {
        log::info("Hotkey {} {} via {} ignored: draft open", hotkeyName(which), action, source);
        return false;
    }
    auto session = AugmentManager::get().session();
    if (!session) {
        log::info("Hotkey {} {} via {} ignored: no run level", hotkeyName(which), action, source);
        return false;
    }

    // getTotalFrames starts at 0, so the "never" sentinel is ~0u.
    unsigned frame = CCDirector::sharedDirector()->getTotalFrames();
    int i = static_cast<int>(which);
    int d = down ? 1 : 0;
    if (g_lastFrame[i][d] == frame + 1) {
        log::info("Hotkey {} {} via {} duplicate in frame {}, ignored", hotkeyName(which), action, source, frame);
        return g_lastHandled[i][d];
    }
    log::info("Hotkey {} {} via {} (frame {})", hotkeyName(which), action, source, frame);

    bool handled = session->onHotkey(which, down);
    g_lastFrame[i][d] = frame + 1;
    g_lastHandled[i][d] = handled;
    return handled;
}

} // namespace augment::hotkeys

$on_mod(Loaded) {
    using namespace augment;
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        // Repeats are neither a press nor a release for us.
        if (data.action == KeyboardInputData::Action::Repeat) return ListenerResult::Propagate;
        bool down = data.action == KeyboardInputData::Action::Press;
        // Text fields get their keys untouched.
        if (CCIMEDispatcher::sharedDispatcher()->hasDelegate()) return ListenerResult::Propagate;

        Keybind pressed(data.key, data.modifiers);
        if (hotkeys::keybindMatches("keybind-slowmo", KEY_X, pressed)) {
            return hotkeys::route(Hotkey::SlowMo, down, "raw") ? ListenerResult::Stop : ListenerResult::Propagate;
        }
        if (hotkeys::keybindMatches("keybind-checkpoint", KEY_Z, pressed)) {
            return hotkeys::route(Hotkey::Checkpoint, down, "raw") ? ListenerResult::Stop : ListenerResult::Propagate;
        }
        if (hotkeys::keybindMatches("keybind-brake", KEY_C, pressed)) {
            return hotkeys::route(Hotkey::Brake, down, "raw") ? ListenerResult::Stop : ListenerResult::Propagate;
        }
        if (!down) return ListenerResult::Propagate;

        // Debug: 1..9 grant augments (table order), Shift+1..9 the 10th
        // onwards (Shift+1 cat, Shift+2 brake), 0 fills the gauge. Never while a draft is up.
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
    log::info("Hotkey raw listener registered (priority -1)");
}
