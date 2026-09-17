// wave-hitbox augment: shrink the player's collision rect while in wave mode.
//
// A different path from hazard-hitbox: GD builds the player's collision
// rects through the *other* getObjectRect overload,
// GameObject::getObjectRect(float width, float height) (win 0x1976c0), whose
// two arguments are size factors — m_vehicleSize for the outer box and 0.3f
// for the inner one (qolmod HitboxNode.cpp:417-425). Scaling the arguments
// scales the rect GD then collides with; that is exactly what qolmod's
// HitboxMultiplier does (refs/qolmod/src/Hacks/Level/HitboxMultiplier.cpp).
//
// Not covered: the player's oriented box, which GD uses instead of the rect
// when the *hazard* is rotated off the 90-degree grid. See docs/GD-INTERNALS.md
// "Player collision shape".
//
// The scale is scales::wave() (game/Scales.hpp), republished every frame by
// augments/HitboxScales.cpp because nerve makes it depend on level progress.

#include "../game/Scales.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>

using namespace geode::prelude;

// Pattern from qolmod's HitboxMultiplier (HitboxMultiplier.cpp:103-131):
// scale the two size factors, then let GD build the rect as usual.
class $modify(AugPlayerRect, GameObject) {
    CCRect getObjectRect(float width, float height) {
        // Hot path: this virtual runs for far more than the player, so bail
        // out before the typeinfo_cast whenever the augment is not in play.
        float scale = augment::scales::wave();
        if (scale >= 1.f) return GameObject::getObjectRect(width, height);

        // Per PlayerObject, so in dual only the player currently in wave mode
        // shrinks. m_isDart is GD's wave flag (start mode 4).
        auto player = typeinfo_cast<PlayerObject*>(this);
        if (!player || !player->m_isDart) return GameObject::getObjectRect(width, height);

        return GameObject::getObjectRect(width * scale, height * scale);
    }
};
