#pragma once

#include <Geode/Geode.hpp>

#include <array>

namespace augment {

// The missile strike's picture: a reticle on the impact point, the missile
// dropping onto it, then the blast (a flash over the cleared circle, a
// fireball, two shockwave rings and a spray of sparks that arc down; white
// with a touch of red). Lives in PlayLayer's object layer (world
// space, so it stays glued to the level as it scrolls) and is redrawn from
// `tick(dt)`, which the augment calls with the game's own (time-scaled) dt,
// so the picture and the removal share one clock.
class MissileNode : public cocos2d::CCNode {
public:
    static MissileNode* create();

    // The drop takes this long; the augment removes the hazards when it lands.
    static constexpr float FallSeconds = 0.35f;
    static constexpr float BlastSeconds = 0.6f;

    // Start a drop from `from` onto `impact`; the reticle shows `radius`.
    void launch(cocos2d::CCPoint from, cocos2d::CCPoint impact, float radius);
    // Impact: switch from the drop to the blast.
    void detonate();
    // Drop whatever is in flight (attempt reset).
    void cancel();
    void tick(float dt);

    bool idle() const { return m_phase == Phase::Idle; }

protected:
    bool init() override;
    void redraw();
    void drawBlast();

    // A shard thrown out by the blast, rolled at detonate().
    struct Spark {
        cocos2d::CCPoint dir;
        float reach = 0.f;
        float life = 0.f;
    };
    static constexpr int SparkCount = 10;
    std::array<Spark, SparkCount> m_sparks;

    enum class Phase { Idle, Falling, Blast };
    Phase m_phase = Phase::Idle;
    float m_age = 0.f;
    cocos2d::CCPoint m_from;
    cocos2d::CCPoint m_impact;
    float m_radius = 0.f;
    cocos2d::CCDrawNode* m_draw = nullptr;
};

} // namespace augment
