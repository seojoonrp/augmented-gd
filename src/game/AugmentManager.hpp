#pragma once

#include "../core/RunState.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class GJGameLevel;
class PlayLayer;

namespace augment {

class LevelSession;

// Owns the run (RunState + settings + logging) and the LevelSession.
// Singleton so callbacks ask for session() at call time instead of holding a
// PlayLayer that may be gone.
class AugmentManager {
public:
    static AugmentManager& get();

    // --- level session ---
    // PlayLayer hook, around the layer's life. beginLevel replaces a stale
    // session (a layer that never got onQuit).
    LevelSession& beginLevel(PlayLayer* layer, int levelID);
    void endLevel();
    // null unless a run level is loaded and its layer is still PlayLayer::get()
    LevelSession* session() const;
    // for the PlayLayer hook: no PlayLayer::get() check, GD may not have set it yet inside init
    LevelSession* sessionFor(PlayLayer* layer) const;

    // read-only; changes go through the methods below
    RunState const& state() const { return m_state; }

    // --- run lifecycle ---
    void startRun(GJGameLevel* level);
    void endRun();
    bool isRunActive() const { return m_state.active(); }
    bool isRunFor(int levelID) const { return m_state.isFor(levelID); }
    // Only the AUG button enters a run level: it arms the next PlayLayer of
    // that level, GD's Play button disarms, anything else plays normally and
    // leaves the run parked.
    void armRunEntry(int levelID);
    void disarmRunEntry();
    // for PlayLayer::init; clears the flag either way
    bool takeRunEntry(int levelID);
    int levelID() const { return m_state.levelID(); }
    std::string const& levelName() const { return m_state.levelName(); }

    // --- draft gauge ---
    // once per attempt when player 1 dies. respawning = a checkpoint brings it
    // back, the charge waits (deferred result)
    DeathResult onDeath(float percent, bool respawning);
    int pendingDrafts() const { return m_state.pendingDrafts(); }
    bool hasPendingDraft() const { return m_state.hasPendingDraft(); }
    void clearPendingDraft() { m_state.takePendingDraft(); }
    void dropPendingDrafts() { m_state.dropPendingDrafts(); }
    int pendingGaugeDrafts() const { return m_state.pendingGaugeDrafts(); }
    float gauge() const { return m_state.gauge(); }
    // gauge reads MAX and pays nothing
    bool allMaxed() const { return !m_state.anyDraftable(); }
    float gaugeThreshold() const { return m_state.gaugeThreshold(this->gaugeRule()); }
    // the HUD shows "cost/cost" while that draft waits
    float lastDraftCost() const { return m_state.lastDraftCost(this->gaugeRule()); }
    // `debug-mode` setting: number keys grant augments / fill the gauge
    static bool debugMode();
    // tops the gauge up so the next death drafts; returns what it added
    float debugFillGauge();
    int deaths() const { return m_state.deaths(); }
    int draftsTaken() const { return m_state.draftsTaken(); }
    float bestPercent() const { return m_state.bestPercent(); }
    float lifeBest() const { return m_state.lifeBest(); }

    // --- augments ---
    int levelOf(std::string_view id) const { return m_state.levelOf(id); }
    bool has(std::string_view id) const { return m_state.has(id); }
    RunState::Levels const& augments() const { return m_state.augments(); }
    std::vector<AugmentDef const*> rollDraft(size_t count) const;
    size_t draftCardCount() const { return m_state.draftCardCount(); }
    void applyPick(std::string const& id);
    // debug keys: level up without counting a draft. 0 = unknown id
    int grant(std::string const& id);

    // --- slow-mo, toggle kept across attempts ---
    bool slowMoEnabled() const { return m_state.slowMoEnabled(); }
    void toggleSlowMo() { m_state.toggleSlowMo(); }
    float slowMoScale() const { return m_state.slowMoScale(); }
    // --- hitbox scales; progress = percent / 100, 0 outside a run ---
    float nerveBoost(float progress) const { return m_state.nerveBoost(progress); }
    float hazardScale(float progress) const { return m_state.hazardScale(progress); }
    float waveScale(float progress) const { return m_state.waveScale(progress); }
    // --- cat / missile / berserker, 0 when unowned ---
    int catCount() const { return m_state.catCount(); }
    float catInterval() const { return m_state.catInterval(); }
    float missileInterval() const { return m_state.missileInterval(); }
    float missileRadius() const { return m_state.missileRadius(); }   // object-layer units
    float berserkChance() const { return m_state.berserkChance(); }
    float berserkSeconds() const { return m_state.berserkSeconds(); }

private:
    AugmentManager();
    ~AugmentManager();
    GaugeRule gaugeRule() const;

    RunState m_state;
    std::unique_ptr<LevelSession> m_session;
    int m_armedLevel = 0;   // 0 = nothing armed
};

} // namespace augment
