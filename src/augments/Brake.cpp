// brake (브레이크): hold C to run the game at 1 - tune::BrakeCut, for
// BrakeSecondsPerLevel * level real seconds per attempt. Goes through
// scales::setTimeOverride, which beats slow-mo's base speed while set.

#include "Augments.hpp"
#include "../core/Formulas.hpp"
#include "../game/LevelSession.hpp"
#include "../game/Scales.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace augment {

namespace {

class Brake : public Augment {
public:
    Brake() : Augment({ ids::Brake }) {}

    // m_held survives resets on purpose, the key can still be down
    void onAttemptStart(LevelSession& s, bool fromCheckpoint) override {
        if (!fromCheckpoint) m_left = formula::brakeBudget(s.levelOf(ids::Brake));
        this->apply(s);
    }

    void onCheckpointPlaced(LevelSession&) override { m_savedLeft = m_left; }
    void onCheckpointRespawn(LevelSession& s) override {
        m_left = m_savedLeft;
        this->apply(s);
    }

    void onGranted(LevelSession& s, std::string const& id, int) override {
        if (id != ids::Brake) return;
        // drafted mid-attempt: the new level's seconds count right away
        m_left += tune::BrakeSecondsPerLevel;
        this->apply(s);
    }

    bool onHotkey(LevelSession& s, Hotkey which, bool down) override {
        if (which != Hotkey::Brake || !s.owns(ids::Brake)) return false;
        m_held = down;
        this->apply(s);
        return true;
    }

    // Checked every frame so pause / practice / run end drop the override for
    // free. dt comes in time-scaled, the budget is real seconds.
    void onFrame(LevelSession& s, float dt) override {
        if (this->active(s)) {
            float scale = scales::time();
            m_left -= scale > 0.f ? dt / scale : dt;
            if (m_left <= 0.f) m_left = 0.f;
        }
        this->apply(s);
    }

    // Focus loss shows up as pauseGame and the key release never comes, so
    // a pause forgets the key.
    void onPause(LevelSession&) override {
        m_held = false;
        scales::setTimeOverride(0.f);
    }
    void onQuit(LevelSession&) override {
        m_held = false;
        scales::setTimeOverride(0.f);
    }

    std::string hudState(LevelSession& s, std::string const&) override {
        int lvl = s.levelOf(ids::Brake);
        return fmt::format(
            "Lv{}  {:.1f}/{:.0f}s{}  [C]", lvl, m_left, formula::brakeBudget(lvl),
            this->active(s) ? "  ACTIVE" : ""
        );
    }

private:
    bool active(LevelSession& s) const {
        return m_held && m_left > 0.f && s.runAttempt() && !s.layer()->m_isPaused;
    }

    void apply(LevelSession& s) {
        scales::setTimeOverride(this->active(s) ? formula::brakeScale() : 0.f);
    }

    bool m_held = false;
    float m_left = 0.f;
    float m_savedLeft = 0.f;
};

} // namespace

std::unique_ptr<Augment> makeBrake() { return std::make_unique<Brake>(); }

} // namespace augment
