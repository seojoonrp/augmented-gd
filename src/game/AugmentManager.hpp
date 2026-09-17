#pragma once

#include "../core/RunState.hpp"

#include <string>
#include <vector>

class GJGameLevel;

namespace augment {

// Geode-side owner of the current run: wraps RunState with settings, logging
// and the game pause used while a draft is up. Singleton so UI callbacks
// never need to hold a pointer to a PlayLayer (which may be gone by the
// time they fire).
class AugmentManager {
public:
    static AugmentManager& get();

    // Read access to the run; every mutation goes through the methods below
    // so it gets logged in one place.
    RunState const& state() const { return m_state; }

    // --- run lifecycle ---
    void startRun(GJGameLevel* level);
    void endRun();
    bool isRunActive() const { return m_state.active(); }
    bool isRunFor(int levelID) const { return m_state.isFor(levelID); }
    int levelID() const { return m_state.levelID(); }
    std::string const& levelName() const { return m_state.levelName(); }

    // --- draft gauge ---
    // Called once per attempt when player 1 dies. Returns the new-best bonus
    // (0 when the best did not move) so the caller can show it.
    float onDeath(float percent);
    int pendingDrafts() const { return m_state.pendingDrafts(); }
    bool hasPendingDraft() const { return m_state.hasPendingDraft(); }
    void clearPendingDraft() { m_state.takePendingDraft(); }
    void dropPendingDrafts() { m_state.dropPendingDrafts(); }
    int pendingGaugeDrafts() const { return m_state.pendingGaugeDrafts(); }
    float gauge() const { return m_state.gauge(); }
    // Cost of the next draft: the rising ramp, or the fixed `debug-threshold`
    // setting in debug mode.
    float gaugeThreshold() const { return m_state.gaugeThreshold(this->gaugeRule()); }
    // The `debug-mode` setting: number keys grant augments, fixed threshold.
    static bool debugMode();
    // Debug: tops the gauge up to the threshold so the next death drafts.
    // Returns the charge added.
    float debugFillGauge();
    int deaths() const { return m_state.deaths(); }
    int draftsTaken() const { return m_state.draftsTaken(); }
    float bestPercent() const { return m_state.bestPercent(); }

    // --- augments ---
    int levelOf(std::string const& id) const { return m_state.levelOf(id); }
    bool has(std::string const& id) const { return m_state.has(id); }
    std::map<std::string, int> const& augments() const { return m_state.augments(); }
    std::vector<AugmentDef const*> rollDraft(size_t count) const;
    size_t draftCardCount() const { return m_state.draftCardCount(); }
    // Picking an augment increments its level (or adds it at level 1).
    void applyPick(std::string const& id);
    // Same level bump without counting a draft (debug keys). Returns the new
    // level, or 0 for an unknown id.
    int grant(std::string const& id);

    // --- slow-mo toggle (persists across attempts within a run) ---
    bool slowMoEnabled() const { return m_state.slowMoEnabled(); }
    void toggleSlowMo() { m_state.toggleSlowMo(); }
    float slowMoScale() const { return m_state.slowMoScale(); }
    // --- hitbox shrinks (hazard-hitbox / wave-hitbox, boosted by nerve) ---
    // `progress` is the current level percent / 100; pass 0 outside a run.
    float nerveBoost(float progress) const { return m_state.nerveBoost(progress); }
    float hazardScale(float progress) const { return m_state.hazardScale(progress); }
    float waveScale(float progress) const { return m_state.waveScale(progress); }
    // --- cat: hazards removed per sweep and seconds between sweeps (0 when unowned) ---
    int catCount() const { return m_state.catCount(); }
    float catInterval() const { return m_state.catInterval(); }

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
    GaugeRule gaugeRule() const;

    RunState m_state;
    bool m_directorPaused = false;
    bool m_cursorWasHidden = false;
};

} // namespace augment
