#pragma once

#include "AugmentDef.hpp"

#include <map>
#include <string>
#include <vector>

class GJGameLevel;

namespace augment {

// Holds the state of the current run. Singleton so UI callbacks never need to
// hold a pointer to a PlayLayer (which may be gone by the time they fire).
class AugmentManager {
public:
    static AugmentManager& get();

    // --- run lifecycle ---
    void startRun(GJGameLevel* level);
    void endRun();
    bool isRunActive() const { return m_active; }
    bool isRunFor(int levelID) const { return m_active && m_levelID == levelID; }
    int levelID() const { return m_levelID; }
    std::string const& levelName() const { return m_levelName; }

    // --- draft gauge ---
    // Called once per attempt when player 1 dies. Charges the gauge by the
    // percent reached (at least `min-charge`); a full gauge queues a draft.
    void onDeath(float percent);
    bool hasPendingDraft() const { return m_pendingDraft; }
    void clearPendingDraft() { m_pendingDraft = false; }
    float gauge() const { return m_gauge; }
    float gaugeThreshold() const;
    int deaths() const { return m_deaths; }
    int draftsTaken() const { return m_draftsTaken; }
    float bestPercent() const { return m_bestPercent; }

    // --- augments ---
    int levelOf(std::string const& id) const;
    bool has(std::string const& id) const { return this->levelOf(id) > 0; }
    std::map<std::string, int> const& augments() const { return m_levels; }
    // Up to `count` random augments that are not yet maxed. Stub augments
    // are drafted like any other; only their level is recorded.
    std::vector<AugmentDef const*> rollDraft(size_t count = 3) const;
    // Picking an augment increments its level (or adds it at level 1).
    void applyPick(std::string const& id);
    // Same level bump without counting a draft (debug keys). Returns the new
    // level, or 0 for an unknown id.
    int grant(std::string const& id);

    // --- slow-mo toggle (persists across attempts within a run) ---
    bool slowMoEnabled() const { return m_slowMoEnabled; }
    void toggleSlowMo() { m_slowMoEnabled = !m_slowMoEnabled; }
    // 1.0 when the run has no slow-mo; otherwise the level's speed scale.
    float slowMoScale() const;
    // 1.0 when the run has no hazard-hitbox; otherwise the level's hazard hitbox scale.
    float hazardScale() const;

    // --- gameplay pause used while the draft popup is up ---
    // Pauses CCDirector without dropping the frame rate to 4 fps.
    void pauseGameForDraft();
    // No-op if not paused by us, so it is safe to call from PlayLayer::onQuit.
    void resumeGameAfterDraft();
    bool isGamePausedForDraft() const { return m_directorPaused; }
    void setCursorWasHidden(bool hidden) { m_cursorWasHidden = hidden; }
    bool cursorWasHidden() const { return m_cursorWasHidden; }

private:
    AugmentManager() = default;

    float minCharge() const;

    bool m_active = false;
    int m_levelID = 0;
    std::string m_levelName;

    int m_deaths = 0;
    int m_draftsTaken = 0;
    float m_bestPercent = 0.f;
    float m_gauge = 0.f;
    bool m_pendingDraft = false;

    std::map<std::string, int> m_levels;
    bool m_slowMoEnabled = true;

    bool m_directorPaused = false;
    bool m_cursorWasHidden = false;
};

} // namespace augment
