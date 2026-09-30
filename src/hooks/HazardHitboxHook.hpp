#pragma once

class GameObject;

// Hazard hitbox shrink. GD collides with one of three shapes per object: the
// rect from getObjectRect(), m_orientedBox when rotated off the 90-degree grid
// (both hooked in HazardHitboxHook.cpp), and m_objectRadius for round hazards,
// which GD reads inline, so augments/HitboxScales.cpp scales that field directly.
namespace augment::hazard {

// Hazard / AnimatedHazard only (slopes with a hazard edge are solids, left alone)
bool isTarget(GameObject* obj);

} // namespace augment::hazard
