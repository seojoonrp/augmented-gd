#pragma once

#include <Geode/Geode.hpp>

#include <array>

namespace augment {

// Missile strike: reticle, the drop, then the blast. Object layer (world space)
// so it scrolls with the level. tick() gets the game's time-scaled dt, same
// clock as the hazard removal.
class MissileNode : public cocos2d::CCNode {
public:
    static MissileNode* create();

    // the augment removes the hazards when the drop lands
    static constexpr float FallSeconds = 0.35f;
    static constexpr float BlastSeconds = 0.6f;

    void launch(cocos2d::CCPoint from, cocos2d::CCPoint impact, float radius);
    void detonate();
    // attempt reset
    void cancel();
    void tick(float dt);

    bool idle() const { return m_phase == Phase::Idle; }

protected:
    bool init() override;
    void redraw();
    void drawBlast();

    // rolled in detonate()
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
