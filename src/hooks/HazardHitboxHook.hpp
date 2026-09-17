#pragma once

class GameObject;

// hazard-hitbox augment: hazard hitboxes shrink around their centre. GD keeps three
// collision shapes per object and reads each from a different place, so the
// GameObject hooks in HazardHitboxHook.cpp scale the first two as GD computes
// them and augments/HitboxScales.cpp scales the third in place:
//   - the axis-aligned rect cached in m_objectRect (virtual getObjectRect(),
//     recomputed whenever m_isObjectRectDirty),
//   - the oriented box m_orientedBox for objects rotated off the 90-degree
//     grid (updateOrientedBox()),
//   - m_objectRadius for circular hazards (read inline by GD, so the field
//     itself is scaled when the object is added / the level changes).
// The scale itself is scales::hazard() (game/Scales.hpp): getObjectRect() is
// hot and runs for every GameObject in the game.
namespace augment::hazard {

// Hazard / AnimatedHazard only. Slopes with a hazard edge are solids first
// and are left alone.
bool isTarget(GameObject* obj);

} // namespace augment::hazard
