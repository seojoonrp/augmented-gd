#pragma once

#include <Geode/Geode.hpp>

namespace augment::fonts {

// GD's fonts have no Hangul. The charset is built from the string literals
// in src/ at build time. Name/Text have the outline baked in (Geode's font
// generator ignores "outline"); setColor tints the glyphs, not the outline.
constexpr char const* Name = "AugName.fnt"_spr;    // player-facing UI
constexpr char const* Text = "AugText.fnt"_spr;    // player-facing UI, smaller
constexpr char const* Debug = "AugDebug.fnt"_spr;  // debug HUD only

} // namespace augment::fonts
