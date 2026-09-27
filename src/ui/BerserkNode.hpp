#pragma once

#include <Geode/Geode.hpp>

#include <vector>

class GameObject;

namespace augment {

// The berserk window's picture: a red pulse framing the screen while the
// window is open, and a burst on every hazard it smashes. Lives in PlayLayer's
// UI layer (screen space) like CatNode, and the bursts follow their (by then
// disabled) object through the level's scroll, so each stays glued to what was
// smashed. Driven by tick(dt) with the game's own time-scaled dt, like
// MissileNode, so the picture and the window share one clock.
class BerserkNode : public cocos2d::CCNode {
public:
    static BerserkNode* create();

    static constexpr float BurstSeconds = 0.4f;

    // Seconds left of the window and its full length; left <= 0 = closed.
    void setWindow(float left, float total);
    // One burst on the hazard that was just smashed.
    void smashAt(GameObject* obj);
    void tick(float dt);

protected:
    bool init() override;
    void redraw();

    struct Burst {
        geode::Ref<GameObject> target;
        float age = 0.f;
    };

    float m_left = 0.f;
    float m_total = 0.f;
    float m_age = 0.f;   // seconds the window has been open, for the pulse
    bool m_dirty = false;   // something is drawn and needs clearing when idle
    std::vector<Burst> m_bursts;
    cocos2d::CCDrawNode* m_draw = nullptr;
};

} // namespace augment
