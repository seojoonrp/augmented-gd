#pragma once

#include <Geode/Geode.hpp>

#include <span>

namespace augment {

// Berserk flame around the player icon. Object layer, but z-ordered under the
// player so the icon stays on top.
class BerserkAura : public cocos2d::CCNode {
public:
    static BerserkAura* create();

    static constexpr float FadeInSeconds = 0.1f;
    static constexpr float FadeOutSeconds = 0.25f;

    // one per player (two in dual); size = icon half-extent in object units
    struct Flame {
        cocos2d::CCPoint at;
        float size;
        bool goingLeft;
    };

    // on = window open; going off fades out instead of cutting
    void tick(float dt, std::span<Flame const> flames, bool on);
    // attempt reset
    void reset();

protected:
    bool init() override;
    void redraw(std::span<Flame const> flames);

    bool m_on = false;
    float m_age = 0.f;      // flicker + swirl clock
    float m_onAge = 0.f;
    float m_offAge = -1.f;  // < 0 = not fading out
    cocos2d::CCDrawNode* m_draw = nullptr;
};

} // namespace augment
