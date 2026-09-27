#pragma once

#include <Geode/Geode.hpp>

#include <vector>

namespace augment {

// The berserk window's aura on the player: a flame that licks around the icon
// and streams out behind it. Lives in PlayLayer's object layer like ShieldNode
// (so every point is an object-layer coordinate) but **below the player's own
// z order**, so the icon stays on top of it. The augment feeds it the players'
// positions and the game's own (time-scaled) dt from onFrame.
class BerserkAura : public cocos2d::CCNode {
public:
    static BerserkAura* create();

    static constexpr float FadeInSeconds = 0.1f;
    static constexpr float FadeOutSeconds = 0.25f;

    // One entry per player to wrap (two in dual mode). `size` is the icon's
    // half-extent in object units; `goingLeft` points the flame's tail the
    // other way.
    struct Flame {
        cocos2d::CCPoint at;
        float size;
        bool goingLeft;
    };

    // `on` = the berserk window is open. Going false fades the flame out
    // rather than cutting it.
    void tick(float dt, std::vector<Flame> const& flames, bool on);
    // Attempt reset: nothing drawn, no fade left over.
    void reset();

protected:
    bool init() override;
    void redraw(std::vector<Flame> const& flames);

    bool m_on = false;
    float m_age = 0.f;      // drives the flicker and the swirl
    float m_onAge = 0.f;    // since the flame appeared (fade in)
    float m_offAge = -1.f;  // < 0 = not fading out
    cocos2d::CCDrawNode* m_draw = nullptr;
};

} // namespace augment
