// Slow-mo time scaling. Every scheduled update (PlayLayer::update included)
// receives the scaled dt; scales::setTime() is what the SlowMo augment drives.

#include "../game/Scales.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

class $modify(AugScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt * augment::scales::time());
    }
};
