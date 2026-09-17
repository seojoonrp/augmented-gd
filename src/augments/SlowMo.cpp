// slow-mo (나무늘보): game speed 1 - 0.05 * level while a run attempt is
// playing; X toggles it for the rest of the run. The scale itself is
// scales::time(), read by the CCScheduler hook (SchedulerHook.cpp) and
// mirrored onto the FMOD master pitch.

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

    // Re-evaluated every frame so pause / practice / run end all fall back
    // to normal speed without special cases.
    void onFrame(LevelSession& s, float) override { this->apply(s); }
    void onGranted(LevelSession& s, std::string const& id, int) override {
        if (id == ids::SlowMo) this->apply(s);
    }
    // Pause menu at normal speed; the next frame re-applies slow-mo.
    void onPause(LevelSession&) override { scales::setTime(1.f); }
    void onQuit(LevelSession&) override { scales::setTime(1.f); }

    bool onHotkey(LevelSession& s, Hotkey which, bool down) override {
        if (which != Hotkey::SlowMo || !down) return false;
        auto& mgr = s.mgr();
        if (!s.runAttempt() || !mgr.has(ids::SlowMo)) {
            log::info("X ignored: runAttempt={} slowmoLv={}", s.runAttempt(), mgr.levelOf(ids::SlowMo));
            return false;
        }
        mgr.toggleSlowMo();
        log::info("Slow-mo toggled -> {}", mgr.slowMoEnabled() ? "ON" : "OFF");
        this->apply(s);
        s.notice(mgr.slowMoEnabled() ? "SLOW-MO ON" : "SLOW-MO OFF", { 255, 220, 120 });
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
