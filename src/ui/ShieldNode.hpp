#pragma once

#include <Geode/Geode.hpp>

#include <span>

namespace augment {

// Shield bubble around the player while a charge is up, ring burst when a hit
// breaks it. Object layer, so positions are object-layer coords.
class ShieldNode : public cocos2d::CCNode {
public:
    static ShieldNode* create();

    static constexpr float BreakSeconds = 0.4f;
    static constexpr float FadeInSeconds = 0.25f;

    // one per player (two in dual)
    struct Bubble {
        cocos2d::CCPoint center;
        float radius;
    };

    // up = charge ready; false -> true fades the bubble in
    void tick(float dt, std::span<Bubble const> bubbles, bool up);
    void shatter();
    // attempt reset: bubble up at once, no fade
    void reset();

protected:
    bool init() override;
    void redraw(std::span<Bubble const> bubbles);

    bool m_up = false;
    float m_upAge = 0.f;
    float m_breakAge = -1.f;   // < 0 = no burst playing
    cocos2d::CCDrawNode* m_draw = nullptr;
};

} // namespace augment
