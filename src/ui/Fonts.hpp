#pragma once

#include <Geode/Geode.hpp>

namespace augment::fonts {

// Pretendard bitmap fonts that Geode generates at build time from
// resources/fonts (mod.json "resources.fonts"). GD's own bigFont / goldFont /
// chatFont have no Hangul, so every label that can show augment text uses
// one of these. The charset is rebuilt from the sources by
// scripts/fontcharset.ps1 (run by build.ps1), so any Korean literal in src/
// is guaranteed to have a glyph.
//
// Sizes are the UHD size; sd is a quarter of it, like GD's own fonts.
//   Name: SemiBold, sd 24 px (goldFont-sized) — augment names, notices, title.
//   Text: Regular,  sd 16 px (chatFont-sized) — descriptions, HUD lines.
constexpr char const* Name = "AugName.fnt"_spr;
constexpr char const* Text = "AugText.fnt"_spr;

// Extra kerning for name labels. CCLabelBMFont::setExtraKerning is a RobTop
// addition (Geode cocos header); the unit is font pixels of the loaded
// variant (unverified). Slightly negative = tighter Korean headings.
constexpr int NameKerning = -1;

} // namespace augment::fonts
