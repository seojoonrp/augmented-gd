#pragma once

#include <Geode/Geode.hpp>

#include <vector>

class GameObject;

namespace augment {

// Berserk window: red pulsing frame round the screen + a burst on each smashed
// hazard. UI layer (screen space); bursts follow their object as it scrolls.
class BerserkNode : public cocos2d::CCNode {
public:
    static BerserkNode* create();

    static constexpr float BurstSeconds = 0.4f;

    // seconds left / full length; left <= 0 = closed
    void setWindow(float left, float total);
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
    float m_age = 0.f;   // since the window opened (pulse)
    bool m_dirty = false;
    std::vector<Burst> m_bursts;
    cocos2d::CCDrawNode* m_draw = nullptr;
};

} // namespace augment
