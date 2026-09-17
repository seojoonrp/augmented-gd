#pragma once

#include <Geode/Geode.hpp>

#include <vector>

class GameObject;

namespace augment {

// The cat's on-screen presence: a placeholder square in the bottom-right
// corner (art comes later) that fires a laser at every hazard it removes.
// Lives in PlayLayer's UI layer (screen space); the lasers are redrawn each
// frame from the square to the targets' current screen positions, so they
// stay glued to objects that scroll past while they fade.
class CatNode : public cocos2d::CCNode {
public:
    static CatNode* create();

    // One laser per target, all fired now. Targets are followed by
    // reference; a dead one just drops out.
    void fireAt(std::vector<GameObject*> const& targets);

protected:
    bool init() override;
    void update(float dt) override;
    void redrawLasers();

    struct Laser {
        geode::Ref<GameObject> target;
        float age = 0.f;
    };

    cocos2d::CCDrawNode* m_body = nullptr;    // the square
    cocos2d::CCDrawNode* m_lasers = nullptr;  // child of this, origin = the square's centre
    std::vector<Laser> m_active;
};

} // namespace augment
