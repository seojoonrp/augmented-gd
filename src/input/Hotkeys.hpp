#pragma once

// Hotkeys from the "keybind" settings in mod.json, plus the debug number keys.

namespace augment {

enum class Hotkey { SlowMo = 0, Checkpoint = 1, Brake = 2 };
constexpr int HotkeyCount = 3;

namespace hotkeys {

// Sends a press (`down`) or release to the current level session. True
// (= Stop) only if something used the key, so idle keys still reach other
// mods. Releases get through even with a draft open, so a held brake can't
// get stuck.
bool route(Hotkey which, bool down);

} // namespace hotkeys

} // namespace augment
