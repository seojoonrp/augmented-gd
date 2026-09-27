#pragma once

// The state of one run and the rules that move it: gauge economy, augment
// levels, pending drafts. Pure C++ (no Geode, no logging, no settings): the
// game-side AugmentManager feeds it settings and logs what it returns, and
// scripts/test.ps1 runs it on the host.

#include "AugmentDef.hpp"

#include <cstddef>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace augment {

// How the draft gauge charges and what a draft costs. Defaults are tune::;
// the tests build their own.
struct GaugeRule {
    float thresholdStart = tune::GaugeThresholdStart;
    float thresholdStep = tune::GaugeThresholdStep;
    // The ramp stops here: every draft past it costs this much.
    float thresholdMax = tune::GaugeThresholdMax;
    float newBestMult = tune::NewBestBonusMult;
};

struct DeathResult {
    float charge = 0.f;   // percent + bonus added to the gauge
    float bonus = 0.f;    // new-best part of that (0 when the best did not move)
    int earned = 0;       // drafts this death queued
    // A checkpoint respawn follows, so nothing was charged yet (charge,
    // bonus and earned are all 0): the life pays once, when it ends.
    bool deferred = false;
};

class RunState {
public:
    // --- lifecycle ---
    // Queues the free opening draft (not gauge-earned).
    void start(int levelID, std::string levelName);
    void end();
    bool active() const { return m_active; }
    bool isFor(int levelID) const { return m_active && m_levelID == levelID; }
    int levelID() const { return m_levelID; }
    std::string const& levelName() const { return m_levelName; }

    // --- draft gauge ---
    // Once per attempt when player 1 dies. Charges by the percent reached
    // (no floor) plus the new-best delta in whole percents times the bonus
    // multiplier (bestPercent itself keeps the decimals); while
    // the gauge covers the (rising) cost and something is still draftable,
    // a draft is queued and the cost rises. Leftover charge carries over.
    //
    // `respawning` = a checkpoint respawn follows this death, so the life
    // goes on: the death is counted, the gauge waits, and the percent is
    // remembered. The death that really ends the life charges for the best
    // percent of the whole life, once (user, 2026-09-27: dying at 6 % and
    // then at 4 % behind one checkpoint must pay 6, not 10).
    DeathResult onDeath(float percent, GaugeRule const& rule, bool respawning = false);
    float gauge() const { return m_gauge; }
    // Best percent of the life in progress, 0 when no death is waiting on a
    // checkpoint respawn. What the next settling death will charge for; the
    // HUD folds it into the best it shows, so the mark never slides back.
    float lifeBest() const { return m_lifeBest; }
    // Cost of the next draft under `rule`: the ramp, capped at its ceiling.
    float gaugeThreshold(GaugeRule const& rule) const;
    // Debug: tops the gauge up to the threshold. Returns the charge added.
    float fillGauge(GaugeRule const& rule);
    int deaths() const { return m_deaths; }
    int draftsTaken() const { return m_draftsTaken; }
    float bestPercent() const { return m_bestPercent; }

    // Drafts earned but not yet shown. A big new best can earn several at
    // once; the popup chains them, taking one per takePendingDraft().
    int pendingDrafts() const { return m_pendingDrafts; }
    bool hasPendingDraft() const { return m_pendingDrafts > 0; }
    void takePendingDraft();
    void dropPendingDrafts();
    // Pending drafts the gauge paid for (the free opening draft is not one).
    // While any is waiting the HUD shows the gauge as full.
    int pendingGaugeDrafts() const { return m_pendingGaugeDrafts; }

    // --- augments ---
    int levelOf(std::string const& id) const;
    bool has(std::string const& id) const { return this->levelOf(id) > 0; }
    std::map<std::string, int> const& augments() const { return m_levels; }
    // Any augment below its max level?
    bool anyDraftable() const;
    // Up to `count` random augments that are not yet maxed.
    std::vector<AugmentDef const*> rollDraft(std::size_t count, std::mt19937& rng) const;
    // Cards the next draft shows: 3, or DraftCountCards once draft-count is owned.
    std::size_t draftCardCount() const;
    // Level bump without counting a draft (debug keys). Returns the new
    // level, or 0 for an unknown id.
    int grant(std::string const& id);
    // Picking an augment: grant + one more draft taken. Returns the new level.
    int applyPick(std::string const& id);

    // --- slow-mo toggle (persists across attempts within a run) ---
    bool slowMoEnabled() const { return m_slowMoEnabled; }
    void toggleSlowMo() { m_slowMoEnabled = !m_slowMoEnabled; }

    // --- effects at the current levels (formula:: with this run's levels) ---
    // 1.0 when the run has no slow-mo; otherwise the level's speed scale.
    float slowMoScale() const;
    // `progress` = current level percent / 100. Nerve grows both shrinks the
    // further into the level the player is; pass 0 outside a run.
    float nerveBoost(float progress) const;
    float hazardScale(float progress) const;
    float waveScale(float progress) const;
    int catCount() const;
    float catInterval() const;
    float missileInterval() const;
    float missileRadius() const;
    // Berserker: the per-destroyed-hazard roll and how long a hit roll lasts.
    float berserkChance() const;
    float berserkSeconds() const;

private:
    bool m_active = false;
    int m_levelID = 0;
    std::string m_levelName;

    int m_deaths = 0;
    int m_draftsTaken = 0;
    // Drafts the gauge paid for; the free one at run start is not among them
    // and so does not raise the threshold.
    int m_gaugeDrafts = 0;
    float m_bestPercent = 0.f;
    // Best percent of the life that a checkpoint respawn is holding open;
    // 0 when nothing is deferred.
    float m_lifeBest = 0.f;
    float m_gauge = 0.f;
    int m_pendingDrafts = 0;
    int m_pendingGaugeDrafts = 0;

    std::map<std::string, int> m_levels;
    bool m_slowMoEnabled = true;
};

} // namespace augment
