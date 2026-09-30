// Wave hitbox augment. GD builds the player's rects through the other
// getObjectRect overload, whose args are size factors (m_vehicleSize outer, 0.3
// inner), so scaling them scales the rect. Same idea as qolmod's hitbox multiplier.
// Doesn't cover the player's oriented box (used against off-grid rotated hazards).

#include "../game/Scales.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>

using namespace geode::prelude;

class $modify(AugPlayerRect, GameObject) {
    CCRect getObjectRect(float width, float height) {
        // runs for every object, bail before the cast when the augment is off
        float scale = augment::scales::wave();
        if (scale >= 1.f) return GameObject::getObjectRect(width, height);

        // m_isDart = wave. per player, so in dual only the one in wave shrinks
        auto player = typeinfo_cast<PlayerObject*>(this);
        if (!player || !player->m_isDart) return GameObject::getObjectRect(width, height);

        return GameObject::getObjectRect(width * scale, height * scale);
    }
};
