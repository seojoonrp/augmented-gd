#pragma once

#include <Geode/Geode.hpp>

#include <vector>

class GameObject;

namespace augment {

// The cat's on-screen presence: a placeholder square in the bottom-right
// corner (art comes later) that casts a small magic circle on every hazard it
// removes — two rings, runes around the rim and a star inside, spinning as
// they fade. Lives in PlayLayer's UI layer (screen space); each circle is
// redrawn every frame at its target's current screen position, so it stays
// glued to the object that scrolls past while it fades. Replaced the lasers
// the cat used to fire (user 2026-09-27: magic, not lasers).
class CatNode : public cocos2d::CCNode {
public:
    static CatNode* create();

    // One magic circle per target, all cast now. Targets are followed by
    // reference; a dead one just drops out.
    void castAt(std::vector<GameObject*> const& targets);

protected:
    bool init() override;
    void update(float dt) override;
    void redrawSigils();

    struct Sigil {
        geode::Ref<GameObject> target;
        float age = 0.f;
    };

    cocos2d::CCDrawNode* m_body = nullptr;    // the square
    cocos2d::CCDrawNode* m_sigils = nullptr;  // child of this, drawn in screen space
    std::vector<Sigil> m_active;
};

} // namespace augment
