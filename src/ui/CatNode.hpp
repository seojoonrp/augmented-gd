#pragma once

#include <Geode/Geode.hpp>

#include <vector>

class GameObject;

namespace augment {

// The cat's on-screen presence, in doodles on purpose (placeholder art from
// scripts/catgen.py; the polished first version looked "4K", user
// 2026-09-30). The cat itself sits in the bottom-right corner: two idle
// frames in turn, and the wand swing for a moment on every cast. Every
// hazard it removes gets a magic circle: a wobbly ring round a lopsided
// star, two shaky frames flipping. It appears with a quick flash of light
// behind it and spins away shrinking and fading at the end (user
// 2026-09-30); the cat itself never scales. Lives in PlayLayer's UI layer (screen
// space); each circle is moved every frame onto its target's current screen
// position, so it stays glued to the object that scrolls past. Replaced the
// lasers the cat used to fire (user 2026-09-27: magic, not lasers).
class CatNode : public cocos2d::CCNode {
public:
    static CatNode* create();

    // One magic circle per target, all cast now, and the wand swing when
    // there is at least one. Targets are followed by reference; a dead one
    // just drops out.
    void castAt(std::vector<GameObject*> const& targets);

protected:
    bool init() override;
    void update(float dt) override;
    // Builds the three frames in the corner; false (and no cat) when a
    // sprite is missing.
    bool buildMascot();
    // Shows exactly one frame: the swing while it lasts, else the idle one
    // m_idleClock is on.
    void stepMascot(float dt);
    // Ages the circles, drops the finished ones, and puts each of the rest
    // on its target showing the frame its age is on.
    void stepSigils(float dt);

    struct Sigil {
        geode::Ref<GameObject> target;
        cocos2d::CCSprite* frames[2] = { nullptr, nullptr };   // children of this
        cocos2d::CCSprite* flash = nullptr;   // child of this, under every circle; may be null
        float age = 0.f;
        float phase = 0.f;   // offsets the flip, so the circles do not shake in step
        float angle = 0.f;   // its random turn, degrees
        float scale = 1.f;   // sprite scale at full size
    };
    std::vector<Sigil> m_active;

    cocos2d::CCSprite* m_idle[2] = { nullptr, nullptr };
    cocos2d::CCSprite* m_cast = nullptr;
    cocos2d::CCSprite* m_shown = nullptr;
    float m_idleClock = 0.f;   // game seconds; the idle frame flips every kIdleFrame
    float m_castLeft = 0.f;    // game seconds of the swing still to show
};

} // namespace augment
