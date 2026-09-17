// wave-hitbox augment: shrink the player's collision rect while in wave mode.
// See the header for why this is a separate hook from HazardHitboxHook.

#include "PlayerHitboxHook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GameObject.hpp>

using namespace geode::prelude;

namespace {

float g_waveScale = 1.f;

} // namespace

namespace augment::player {

void setWaveScale(float scale) {
    if (std::abs(g_waveScale - scale) < 0.001f) return;
    g_waveScale = scale;
    log::info("WaveHitbox: player hitbox scale -> {:.2f}", scale);
}

float waveScale() { return g_waveScale; }

} // namespace augment::player

// Pattern from qolmod's HitboxMultiplier (HitboxMultiplier.cpp:103-131):
// scale the two size factors, then let GD build the rect as usual.
class $modify(AugPlayerRect, GameObject) {
    CCRect getObjectRect(float width, float height) {
        // Hot path: this virtual runs for far more than the player, so bail
        // out before the typeinfo_cast whenever the augment is not in play.
        if (g_waveScale >= 1.f) return GameObject::getObjectRect(width, height);

        // Per PlayerObject, so in dual only the player currently in wave mode
        // shrinks. m_isDart is GD's wave flag (start mode 4).
        auto player = typeinfo_cast<PlayerObject*>(this);
        if (!player || !player->m_isDart) return GameObject::getObjectRect(width, height);

        return GameObject::getObjectRect(width * g_waveScale, height * g_waveScale);
    }
};
