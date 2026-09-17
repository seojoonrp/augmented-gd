// unmirror (멀미약): mirror portals do nothing. GD flips the view through
// GJBaseGameLayer::toggleFlipped(flip, noEffects) (win 0x2467d0, checked
// with bro.ps1); the hook below refuses `flip = true` while the augment is
// owned, the way qolmod's NoMirrorPortal (refs/qolmod/src/Hacks/Level/
// NoMirrorPortal.cpp) and xdBot (forces flip = false) do. Un-flips still go
// through, so a level that was already mirrored when the augment was
// drafted straightens out at the next portal / reset. No object surgery,
// covers portals spawned later too. (from refs, unverified in game)

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
        // Straighten out now, so the pick is felt at once. Calling this with
        // flip = false while not mirrored is what cleanstartpos / miscbugfixes
        // do after every reset, so it is safe when there is nothing to undo.
        s.layer()->toggleFlipped(false, true);
        log::info("Unmirror: drafted, un-flipped (no-op if the view was straight)");
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
                log::info("Unmirror: mirror portal ignored at {:.1f}%", session->percent());
                return;
            }
        }
        GJBaseGameLayer::toggleFlipped(flip, noEffects);
    }
};
