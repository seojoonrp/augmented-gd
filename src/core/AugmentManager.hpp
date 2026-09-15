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

    // --- progress ---
    // Called once per attempt when player 1 dies. Sets the pending-draft flag
    // when the death threshold is reached.
    void onDeath(float percent);
    bool hasPendingDraft() const { return m_pendingDraft; }
    void clearPendingDraft() { m_pendingDraft = false; }
    int deaths() const { return m_deaths; }
    int draftsTaken() const { return m_draftsTaken; }
    float bestPercent() const { return m_bestPercent; }

    // --- augments ---
    int levelOf(std::string const& id) const;
    std::map<std::string, int> const& augments() const { return m_levels; }
    // Up to `count` random augments that are not yet maxed.
    std::vector<AugmentDef const*> rollDraft(size_t count = 3) const;
    // Picking an augment increments its level (or adds it at level 1).
    void applyPick(std::string const& id);

    // --- gameplay pause used while the draft popup is up ---
    // Pauses CCDirector without dropping the frame rate to 4 fps.
    void pauseGameForDraft();
    // No-op if not paused by us, so it is safe to call from PlayLayer::onQuit.
    void resumeGameAfterDraft();
    bool isGamePausedForDraft() const { return m_directorPaused; }

private:
    AugmentManager() = default;

    int deathsPerDraft() const;

    bool m_active = false;
    int m_levelID = 0;
    std::string m_levelName;

    int m_deaths = 0;
    int m_deathsSinceDraft = 0;
    int m_draftsTaken = 0;
    float m_bestPercent = 0.f;
    bool m_pendingDraft = false;

    std::map<std::string, int> m_levels;

    bool m_directorPaused = false;
};

} // namespace augment
