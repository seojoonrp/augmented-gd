#pragma once

// wave-hitbox augment: the player's own hitbox shrinks while it is in wave
// (dart) mode. This is a different path from hazard-hitbox: GD builds the
// player's collision rects through the *other* getObjectRect overload,
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
// The scale is a global for the same reason as the hazard one: the hook is on
// a hot virtual. PlayLayer owns it and republishes it every frame, because
// nerve makes it depend on how far into the level the player is.
namespace augment::player {

// 1.0 = untouched. Logs on change.
void setWaveScale(float scale);
float waveScale();

} // namespace augment::player
