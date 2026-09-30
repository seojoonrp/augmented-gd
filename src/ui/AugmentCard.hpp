#pragma once

// The augment card, drawn at full size; callers scale it.

#include "../core/AugmentDef.hpp"

#include <Geode/Geode.hpp>

#include <string>

namespace augment::card {

constexpr float CardWidth = 140.f;
constexpr float CardHeight = 210.f;

// texts already in the caller's language
struct CardFace {
    std::string name;
    std::string description;   // wrapped and shrunk to fit
    std::string footer;        // left of the footer band, e.g. "NEW" or "Lv 3"
    cocos2d::ccColor3B footerColor = { 255, 255, 255 };
    int pips = 0;              // filled stars, out of def.maxLevel
};

cocos2d::CCNode* augmentCard(AugmentDef const& def, CardFace const& face);

} // namespace augment::card
