#pragma once

// Shared by the augments that take hazards out of an attempt (cat, missile):
// the "in view, ahead of the player" scan and the take / put-back bookkeeping.
// Removal is GD's own GameObject::destroyObject() (m_isDisabled +
// m_isDisabled2 + opacity 0, Geode inline source): collisionCheckObjects
// skips objects with either flag set (xdBot's trajectory sim relies on that)
// and opacity 0 takes the sprite with it. Everything is put back before every
// reset, ahead of GD's own reset so it still gets the last word.

#include <Geode/Geode.hpp>

#include <cstddef>
#include <vector>

class GameObject;
class PlayLayer;

namespace augment {

class LevelSession;

namespace hazard {

// Hazards removed this attempt, each with the opacity it had.
class Removed {
public:
    void take(GameObject* obj);
    // Puts every object back; returns how many were still disabled (0 = GD
    // reset them itself first).
    int restore();
    std::size_t size() const { return m_entries.size(); }
    bool empty() const { return m_entries.empty(); }

private:
    struct Entry { geode::Ref<GameObject> obj; unsigned char opacity; };
    std::vector<Entry> m_entries;
};

// The window's extent in object-layer x, cut to the player's side of travel,
// plus the screen rect (with a margin) each object is tested against.
// "In view" = the object's screen position is inside the window, computed
// through the real node transform so camera zoom, offset and rotation all
// count. "Ahead" = further along in x than the player (behind them in
// platformer mode when they are heading left); `lead` pushes the near edge
// that many units further ahead.
struct View {
    cocos2d::CCRect screen;
    float lo = 0.f;
    float hi = 0.f;
    bool contains(GameObject* obj, cocos2d::CCNode* fallbackParent) const;
};
View viewAhead(PlayLayer* layer, float lead = 0.f);

// Live hazards (Hazard / AnimatedHazard, not disabled, not the anticheat
// spike) inside `view`, from the session's x index.
std::vector<GameObject*> hazardsInView(LevelSession& s, View const& view);

// Does the object's collision shape touch the circle? Circle objects (saws)
// by radius, everything else by its axis-aligned rect.
bool touchesCircle(GameObject* obj, cocos2d::CCPoint centre, float radius);

} // namespace hazard
} // namespace augment
