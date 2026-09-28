#pragma once

// The mod's round button face: the user's mark (resources/ui/aug-logo.png)
// in a green GD circle. The level info screen starts runs with it and the
// pause menu of a run level opens the run summary with the same face.

#include <Geode/Geode.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

namespace augment {

inline geode::CircleButtonSprite* augButtonSprite(geode::CircleBaseSize size, float logoScale = 1.f) {
    auto logo = cocos2d::CCSprite::create("aug-logo.png"_spr);
    if (!logo) geode::log::warn("AUG button: no logo sprite, the circle will be empty");
    auto spr = geode::CircleButtonSprite::create(logo, geode::CircleBaseColor::Green, size);
    spr->setTopRelativeScale(logoScale);
    return spr;
}

} // namespace augment
