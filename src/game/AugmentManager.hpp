#pragma once

#include "../core/RunState.hpp"

#include <memory>
#include <string>
#include <vector>

class GJGameLevel;
class PlayLayer;

namespace augment {

class LevelSession;

// Geode-side owner of the current run: wraps RunState with settings and
// logging, and owns the LevelSession while a run level is loaded. Singleton
// so UI callbacks never need to hold a pointer to a PlayLayer (which may be
// gone by the time they fire): they ask for session() at call time.
class AugmentManager {
public:
    static AugmentManager& get();

    // --- level session (see LevelSession.hpp) ---
    // Called by the PlayLayer hook around the layer's life. beginLevel
    // replaces any stale session (a layer that went without onQuit).
    LevelSession& beginLevel(PlayLayer* layer, int levelID);
    void endLevel();
    // The live session, or null when no run level is loaded or the layer it
    // was made for is no longer the current PlayLayer.
    LevelSession* session() const;
    // For the PlayLayer hook itself: the session made for `layer`, or null.
    // No PlayLayer::get() check, because inside PlayLayer::init GD may not
    // have published the layer yet.
    LevelSession* sessionFor(PlayLayer* layer) const;

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

private:
    AugmentManager();
    ~AugmentManager();
    GaugeRule gaugeRule() const;

    RunState m_state;
    std::unique_ptr<LevelSession> m_session;
};

} // namespace augment
