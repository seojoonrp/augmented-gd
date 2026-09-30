// slow-mo (나무늘보): game speed 1 - 0.05 * level during run attempts, X
// toggles it for the rest of the run. SchedulerHook reads scales::time(),
// FMOD pitch follows it.

#include "Augments.hpp"
#include "../game/LevelSession.hpp"
#include "../game/AugmentManager.hpp"
#include "../game/Scales.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {

class SlowMo : public Augment {
public:
    SlowMo() : Augment({ ids::SlowMo }) {}

    // every frame, so pause / practice / run end fall back to 1x on their own
    void onFrame(LevelSession& s, float) override { this->apply(s); }
    void onGranted(LevelSession& s, std::string const& id, int) override {
        if (id == ids::SlowMo) this->apply(s);
    }
    // pause menu runs at normal speed, next frame re-applies
    void onPause(LevelSession&) override { scales::setTime(1.f); }
    void onQuit(LevelSession&) override { scales::setTime(1.f); }

    bool onHotkey(LevelSession& s, Hotkey which, bool down) override {
        if (which != Hotkey::SlowMo || !down) return false;
        auto& mgr = s.mgr();
        if (!s.runAttempt() || !mgr.has(ids::SlowMo)) return false;
        mgr.toggleSlowMo();
        this->apply(s);
        return true;
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        auto& mgr = s.mgr();
        return fmt::format(
            "Lv{}  {} ({:.0f}%)  [X]", mgr.levelOf(ids::SlowMo),
            mgr.slowMoEnabled() ? "ON" : "OFF", mgr.slowMoScale() * 100.f
        );
    }

private:
    void apply(LevelSession& s) {
        float want = 1.f;
        auto& mgr = s.mgr();
        if (s.runAttempt() && !s.layer()->m_isPaused && mgr.slowMoEnabled()) {
            want = mgr.slowMoScale();
        }
        scales::setTime(want);
    }
};

} // namespace

std::unique_ptr<Augment> makeSlowMo() { return std::make_unique<SlowMo>(); }

} // namespace augment
