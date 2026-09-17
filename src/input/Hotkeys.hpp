#pragma once

// Hotkeys (keys come from the "keybind" settings in mod.json) and the debug
// number keys. Two input paths feed route(); see Hotkeys.cpp for why.

namespace augment {

enum class Hotkey { SlowMo = 0, Checkpoint = 1, Brake = 2 };
constexpr int HotkeyCount = 3;

inline char const* hotkeyName(Hotkey which) {
    switch (which) {
        case Hotkey::SlowMo: return "slowmo";
        case Hotkey::Checkpoint: return "checkpoint";
        case Hotkey::Brake: return "brake";
    }
    return "?";
}

namespace hotkeys {

// Routes a press (`down`) or release to the current level session. Returns
// true (= Stop) only when the key was acted on, so an idle key still reaches
// other mods. Releases matter to held keys (brake) and are routed even while
// a draft is open, so a key let go over the popup cannot stay "held".
// `source` is "raw" or "setting", for the log.
bool route(Hotkey which, bool down, char const* source);

} // namespace hotkeys

} // namespace augment
