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
    // percent reached plus a bonus for any new best; a full gauge queues a
    // draft. Returns the new-best bonus (0 when the best did not move) so the
    // caller can show it.
    float onDeath(float percent);
    // Drafts earned but not yet shown. A big new best can earn several at
    // once; the popup chains them, taking one per clearPendingDraft().
    int pendingDrafts() const { return m_pendingDrafts; }
    bool hasPendingDraft() const { return m_pendingDrafts > 0; }
    void clearPendingDraft() {
        if (m_pendingDrafts > 0) m_pendingDrafts--;
        if (m_pendingGaugeDrafts > m_pendingDrafts) m_pendingGaugeDrafts = m_pendingDrafts;
    }
    // Pending drafts the gauge paid for (the free opening draft is not one).
    // While any is waiting the HUD shows the gauge as full.
    int pendingGaugeDrafts() const { return m_pendingGaugeDrafts; }
    float gauge() const { return m_gauge; }
    // Cost of the next draft: rises with every draft the gauge has paid for,
    // or the fixed `debug-threshold` setting in debug mode.
    float gaugeThreshold() const;
    // The `debug-mode` setting: number keys grant augments, fixed threshold.
    static bool debugMode();
    // Debug: tops the gauge up to the threshold so the next death drafts.
    // Returns the charge added.
    float debugFillGauge();
    int deaths() const { return m_deaths; }
    int draftsTaken() const { return m_draftsTaken; }
    float bestPercent() const { return m_bestPercent; }

    // --- augments ---
    int levelOf(std::string const& id) const;
    bool has(std::string const& id) const { return this->levelOf(id) > 0; }
    std::map<std::string, int> const& augments() const { return m_levels; }
    // Up to `count` random augments that are not yet maxed.
    std::vector<AugmentDef const*> rollDraft(size_t count) const;
    // Cards the next draft shows: 3, or DraftCountCards once draft-count is owned.
    size_t draftCardCount() const;
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
    // --- hitbox shrinks (hazard-hitbox / wave-hitbox, boosted by nerve) ---
    // `progress` is the current level percent / 100, clamped to 0..1. Nerve
    // grows both shrinks the further into the level the player is, so every
    // caller has to say where the player is right now; pass 0 outside a run.
    // 1.0 means "untouched"; the result never goes below tune::MinHitboxScale.
    float nerveBoost(float progress) const;
    float hazardScale(float progress) const;
    // Applies while the player is in wave mode only; the hook checks that.
    float waveScale(float progress) const;
    // --- cat: hazards removed per sweep and seconds between sweeps (0 when unowned) ---
    int catCount() const;
    float catInterval() const;

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

    bool m_active = false;
    int m_levelID = 0;
    std::string m_levelName;

    int m_deaths = 0;
    int m_draftsTaken = 0;
    // Drafts the gauge paid for; the free one at run start is not among them
    // and so does not raise the threshold.
    int m_gaugeDrafts = 0;
    float m_bestPercent = 0.f;
    float m_gauge = 0.f;
    int m_pendingDrafts = 0;
    int m_pendingGaugeDrafts = 0;

    std::map<std::string, int> m_levels;
    bool m_slowMoEnabled = true;

    bool m_directorPaused = false;
    bool m_cursorWasHidden = false;
};

} // namespace augment
