// unmirror (멀미약): mirror portals do nothing. The hook below refuses
// toggleFlipped(flip = true) while owned, same idea as qolmod's
// NoMirrorPortal. Un-flips still go through, so an already mirrored level
// straightens out at the next portal or reset.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace augment {

namespace {

class Unmirror : public Augment {
public:
    Unmirror() : Augment({ ids::Unmirror }) {}

    void onGranted(LevelSession& s, std::string const& id, int) override {
        if (id != ids::Unmirror) return;
        // straighten out right away; flip = false on a straight view is a no-op
        s.layer()->toggleFlipped(false, true);
    }
};

} // namespace

std::unique_ptr<Augment> makeUnmirror() { return std::make_unique<Unmirror>(); }

} // namespace augment

class $modify(AugMirrorLayer, GJBaseGameLayer) {
    void toggleFlipped(bool flip, bool noEffects) {
        if (flip) {
            auto session = augment::AugmentManager::get().session();
            if (session && static_cast<GJBaseGameLayer*>(session->layer()) == this && session->runAttempt() && session->owns(augment::ids::Unmirror)) {
                return;
            }
        }
        GJBaseGameLayer::toggleFlipped(flip, noEffects);
    }
};
