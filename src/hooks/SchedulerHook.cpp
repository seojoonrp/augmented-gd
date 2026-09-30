// every scheduled update (PlayLayer's too) gets dt * scales::time(), for slow-mo and brake

#include "../game/Scales.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

class $modify(AugScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt * augment::scales::time());
    }
};
