#pragma once

class GameObject;

// hazard-hitbox augment: hazard hitboxes shrink around their centre. GD keeps three
// collision shapes per object and reads each from a different place, so the
// GameObject hooks in HazardHitboxHook.cpp scale the first two as GD computes
// them and PlayLayerHook scales the third in place:
//   - the axis-aligned rect cached in m_objectRect (virtual getObjectRect(),
//     recomputed whenever m_isObjectRectDirty),
//   - the oriented box m_orientedBox for objects rotated off the 90-degree
//     grid (updateOrientedBox()),
//   - m_objectRadius for circular hazards (read inline by GD, so the field
//     itself is scaled when the object is added / the level changes).
// The scale is a global because getObjectRect() is hot and runs for every
// GameObject in the game; the PlayLayer hook owns it for the life of a level.
namespace augment::hazard {

// 1.0 = untouched. Logs on change.
void setScale(float scale);
float scale();

// Hazard / AnimatedHazard only. Slopes with a hazard edge are solids first
// and are left alone.
bool isTarget(GameObject* obj);

} // namespace augment::hazard
