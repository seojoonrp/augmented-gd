#pragma once

// Hazard removal shared by cat, missile and berserk: the "in view, ahead of
// the player" scan and take / put-back. Removal is GD's own destroyObject()
// (both disabled flags + opacity 0); collision skips either flag. Everything
// goes back before each reset, ahead of GD's own reset.

#include <Geode/Geode.hpp>

#include <cstddef>
#include <vector>

class GameObject;
class PlayLayer;

namespace augment {

class LevelSession;

namespace hazard {

// hazards removed this attempt, with the opacity each had
class Removed {
public:
    void take(GameObject* obj);
    void restore();
    std::size_t size() const { return m_entries.size(); }

private:
    struct Entry { geode::Ref<GameObject> obj; unsigned char opacity; };
    std::vector<Entry> m_entries;
};

// The screen's extent in object-layer x, cut to the side the player is
// heading (so it flips for platformer going left), plus the screen rect with
// a margin. contains() goes through the real node transform, so zoom, offset
// and rotation all count. `lead` pushes the near edge further ahead.
struct View {
    cocos2d::CCRect screen;
    float lo = 0.f;
    float hi = 0.f;
    bool contains(GameObject* obj, cocos2d::CCNode* fallbackParent) const;
};
View viewAhead(PlayLayer* layer, float lead = 0.f);

// live hazards in `view` (not disabled, not the anticheat spike), from GD's section grid
std::vector<GameObject*> hazardsInView(LevelSession& s, View const& view);

// circle objects (saws) by radius, everything else by its axis-aligned rect
bool touchesCircle(GameObject* obj, cocos2d::CCPoint centre, float radius);

} // namespace hazard
} // namespace augment
