#pragma once

// Hotkeys (keys come from the "keybind" settings in mod.json) and the debug
// number keys. Two input paths feed route(); see Hotkeys.cpp for why.

namespace augment {

enum class Hotkey { SlowMo = 0, Checkpoint = 1 };
constexpr int HotkeyCount = 2;

inline char const* hotkeyName(Hotkey which) {
    return which == Hotkey::SlowMo ? "slowmo" : "checkpoint";
}

namespace hotkeys {

// Routes a press to the current level session. Returns true (= Stop) only
// when the press was acted on, so an idle key still reaches other mods.
// `source` is "raw" or "setting", for the log.
bool route(Hotkey which, char const* source);

} // namespace hotkeys

} // namespace augment
