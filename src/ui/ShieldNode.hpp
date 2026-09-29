#pragma once

#include <Geode/Geode.hpp>

#include <span>

namespace augment {

// The shield's picture: a white bubble around the player while a charge is
// up, a burst (the ring expanding outward) when a hit breaks it. Lives in
// PlayLayer's object layer like MissileNode, so every point is an
// object-layer coordinate; the augment feeds it the players' positions and
// the game's own (time-scaled) dt from onFrame.
class ShieldNode : public cocos2d::CCNode {
public:
    static ShieldNode* create();

    static constexpr float BreakSeconds = 0.4f;
    static constexpr float FadeInSeconds = 0.25f;

    // One entry per player to wrap (two in dual mode).
    struct Bubble {
        cocos2d::CCPoint center;
        float radius;
    };

    // `up` = a charge is ready; going false -> true fades the bubble in.
    void tick(float dt, std::span<Bubble const> bubbles, bool up);
    // A hit took a charge: drop the bubble and play the burst.
    void shatter();
    // Attempt reset: no burst in flight, bubble shown at once.
    void reset();

protected:
    bool init() override;
    void redraw(std::span<Bubble const> bubbles);

    bool m_up = false;
    float m_upAge = 0.f;       // since the bubble (re)appeared; drives fade-in + shimmer
    float m_breakAge = -1.f;   // < 0 = no burst playing
    cocos2d::CCDrawNode* m_draw = nullptr;
};

} // namespace augment
