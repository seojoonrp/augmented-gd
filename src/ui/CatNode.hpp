#pragma once

#include <Geode/Geode.hpp>

#include <vector>

class GameObject;

namespace augment {

// Corner cat + a magic circle on each hazard it removes (placeholder doodle art).
// UI layer (screen space): circles are moved onto their target every frame so
// they follow it while the level scrolls.
class CatNode : public cocos2d::CCNode {
public:
    static CatNode* create();

    // Targets are held by ref; if one goes away its circle just drops.
    void castAt(std::vector<GameObject*> const& targets);

protected:
    bool init() override;
    void update(float dt) override;
    // false (and no cat) if a sprite is missing
    bool buildMascot();
    void stepMascot(float dt);
    void stepSigils(float dt);

    struct Sigil {
        geode::Ref<GameObject> target;
        cocos2d::CCSprite* frames[2] = { nullptr, nullptr };
        cocos2d::CCSprite* flash = nullptr;   // may be null
        float age = 0.f;
        float phase = 0.f;   // so the circles don't flip in sync
        float angle = 0.f;   // degrees
        float scale = 1.f;   // at full size
    };
    std::vector<Sigil> m_active;

    cocos2d::CCSprite* m_idle[2] = { nullptr, nullptr };
    cocos2d::CCSprite* m_cast = nullptr;
    cocos2d::CCSprite* m_shown = nullptr;
    float m_idleClock = 0.f;   // game seconds
    float m_castLeft = 0.f;
};

} // namespace augment
