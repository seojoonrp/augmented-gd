// brake (브레이크): while C is held the game runs at 1 - tune::BrakeCut
// (40 %), for BrakeSecondsPerLevel * level real seconds per attempt. The
// speed goes through scales::setTimeOverride, which wins over the slow-mo
// base speed while set, so the two never fight over the scheduler.

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

    // The key may still be physically down across a reset, so m_held
    // survives it; only the budget is per attempt (a checkpoint respawn
    // continues the attempt, like shield's charges).
    void onAttemptStart(LevelSession& s, bool fromCheckpoint) override {
        if (!fromCheckpoint) {
            m_left = formula::brakeBudget(s.levelOf(ids::Brake));
            m_emptyNoticed = false;
        }
        this->apply(s);
    }

    // The checkpoint keeps the seconds left when it was placed.
    void onCheckpointPlaced(LevelSession&) override { m_savedLeft = m_left; }
    void onCheckpointRespawn(LevelSession& s) override {
        m_left = m_savedLeft;
        m_emptyNoticed = false;
        log::info("Brake: restored from checkpoint, {:.1f}s left", m_left);
        this->apply(s);
    }

    void onGranted(LevelSession& s, std::string const& id, int level) override {
        if (id != ids::Brake) return;
        // Drafted mid-attempt: the new level's extra seconds are usable now.
        m_left += tune::BrakeSecondsPerLevel;
        log::info("Brake: level {} -> {:.1f}s left this attempt", level, m_left);
        this->apply(s);
    }

    bool onHotkey(LevelSession& s, Hotkey which, bool down) override {
        if (which != Hotkey::Brake) return false;
        if (!s.owns(ids::Brake)) {
            if (down) log::info("C ignored: brake not owned");
            return false;
        }
        m_held = down;
        log::info("Brake {}: {:.1f}s left, runAttempt={}", down ? "held" : "released", m_left, s.runAttempt());
        this->apply(s);
        return true;
    }

    // Re-evaluated every frame so pause / practice / run end all release
    // the override without special cases. dt arrives time-scaled; the
    // budget counts real seconds.
    void onFrame(LevelSession& s, float dt) override {
        if (this->active(s)) {
            float scale = scales::time();
            m_left -= scale > 0.f ? dt / scale : dt;
            if (m_left <= 0.f) {
                m_left = 0.f;
                if (!m_emptyNoticed) {
                    m_emptyNoticed = true;
                    log::info("Brake: budget used up this attempt");
                    s.notice("BRAKE EMPTY", { 255, 140, 120 });
                }
            }
        }
        this->apply(s);
    }

    // Focus loss reaches us as pauseGame(unfocused) and the release never
    // arrives, so a pause forgets the key; the player presses again.
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
    bool m_emptyNoticed = false;
};

} // namespace

std::unique_ptr<Augment> makeBrake() { return std::make_unique<Brake>(); }

} // namespace augment
