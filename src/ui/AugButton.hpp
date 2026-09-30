#pragma once

// green GD circle with the mod's mark (level info and pause menu buttons)

#include <Geode/Geode.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

namespace augment {

inline geode::CircleButtonSprite* augButtonSprite(geode::CircleBaseSize size, float logoScale = 1.f) {
    auto logo = cocos2d::CCSprite::create("aug-logo.png"_spr);
    if (!logo) geode::log::warn("AUG button: aug-logo.png missing");
    auto spr = geode::CircleButtonSprite::create(logo, geode::CircleBaseColor::Green, size);
    spr->setTopRelativeScale(logoScale);
    return spr;
}

} // namespace augment
