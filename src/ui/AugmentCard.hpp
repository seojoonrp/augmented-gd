#pragma once

// The augment card: GD-button look (white rim, green body, footer band),
// name, art slot, wrapped description, footer text and level stars. Drawn
// at 140 x 210 and scaled by the caller as a whole. The draft popup shows it
// for what a pick would give; the pause menu's detail popup for what an
// augment does at the level held.

#include "../core/AugmentDef.hpp"

#include <Geode/Geode.hpp>

#include <string>

namespace augment::card {

constexpr float CardWidth = 140.f;
constexpr float CardHeight = 210.f;

// The texts come in the language the caller picked (game/Language.hpp).
struct CardFace {
    // The augment's name, across the top.
    std::string name;
    // Wrapped at the card's inner width and shrunk in steps until it fits.
    std::string description;
    // Left end of the footer band: "NEW", "Lv 2 → 3", "Lv 3".
    std::string footer;
    cocos2d::ccColor3B footerColor = { 255, 255, 255 };
    // Stars filled, of def.maxLevel.
    int pips = 0;
};

cocos2d::CCNode* augmentCard(AugmentDef const& def, CardFace const& face);

} // namespace augment::card
