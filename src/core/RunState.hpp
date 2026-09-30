#pragma once

// One run: gauge economy, augment levels, pending drafts. No Geode, no
// logging, no settings (AugmentManager does those).

#include "AugmentDef.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace augment {

// defaults are tune::, tests build their own
struct GaugeRule {
    float thresholdStart = tune::GaugeThresholdStart;
    float thresholdStep = tune::GaugeThresholdStep;
    int thresholdEvery = tune::GaugeThresholdEvery;   // gauge drafts per step
    float thresholdMax = tune::GaugeThresholdMax;
    float newBestMult = tune::NewBestBonusMult;
};

struct DeathResult {
    float charge = 0.f;   // added to the gauge, bonus included
    float bonus = 0.f;    // new-best part
    int earned = 0;       // drafts queued
    // checkpoint respawn follows: nothing charged yet, the life pays when it ends
    bool deferred = false;
};

class RunState {
public:
    // less<> so per-frame lookups by ids:: literal don't build a std::string
    using Levels = std::map<std::string, int, std::less<>>;

    // --- lifecycle ---
    // queues the free opening draft
    void start(int levelID, std::string levelName);
    void end();
    bool active() const { return m_active; }
    bool isFor(int levelID) const { return m_active && m_levelID == levelID; }
    int levelID() const { return m_levelID; }
    std::string const& levelName() const { return m_levelName; }

    // --- draft gauge ---
    // Once per attempt when player 1 dies. Charge = floor(percent) + whole
    // new-best percents * mult, leftover carries over. With `respawning`
    // (checkpoint) the gauge waits and the death that ends the life pays once
    // for the life's best: 6 % then 4 % behind a checkpoint pays 6, not 10.
    DeathResult onDeath(float percent, GaugeRule const& rule, bool respawning = false);
    float gauge() const { return m_gauge; }
    // best of a life a checkpoint is holding open, 0 otherwise. the HUD folds
    // it in so the best mark never slides back.
    float lifeBest() const { return m_lifeBest; }
    float gaugeThreshold(GaugeRule const& rule) const;
    // cost of the last gauge draft, for the HUD's "30/30" while it waits
    // (the threshold has already moved on)
    float lastDraftCost(GaugeRule const& rule) const;
    // debug: tops up to the threshold, returns what it added
    float fillGauge(GaugeRule const& rule);
    int deaths() const { return m_deaths; }
    int draftsTaken() const { return m_draftsTaken; }
    float bestPercent() const { return m_bestPercent; }

    // earned but not shown yet; a big new best can earn several
    int pendingDrafts() const { return m_pendingDrafts; }
    bool hasPendingDraft() const { return m_pendingDrafts > 0; }
    void takePendingDraft();
    void dropPendingDrafts();
    // the ones the gauge paid for (not the opening draft); the HUD shows a full bar while any wait
    int pendingGaugeDrafts() const { return m_pendingGaugeDrafts; }

    // --- augments ---
    int levelOf(std::string_view id) const;
    bool has(std::string_view id) const { return this->levelOf(id) > 0; }
    Levels const& augments() const { return m_levels; }
    // false = everything maxed (MAX gauge)
    bool anyDraftable() const;
    int levelsLeft() const;
    // up to `count` random augments below max level
    std::vector<AugmentDef const*> rollDraft(std::size_t count, std::mt19937& rng) const;
    std::size_t draftCardCount() const;
    // debug keys: +1 level without counting a draft. 0 = unknown id
    int grant(std::string const& id);
    // grant + counts a draft
    int applyPick(std::string const& id);

    // --- slow-mo toggle, kept across attempts ---
    bool slowMoEnabled() const { return m_slowMoEnabled; }
    void toggleSlowMo() { m_slowMoEnabled = !m_slowMoEnabled; }

    // --- formula:: at this run's levels ---
    float slowMoScale() const;
    // progress = level percent / 100, pass 0 outside a run
    float nerveBoost(float progress) const;
    float hazardScale(float progress) const;
    float waveScale(float progress) const;
    int catCount() const;
    float catInterval() const;
    float missileInterval() const;
    float missileRadius() const;
    float berserkChance() const;
    float berserkSeconds() const;

private:
    // cost of gauge draft `n`, 0-based
    static float costOf(int n, GaugeRule const& rule);

    bool m_active = false;
    int m_levelID = 0;
    std::string m_levelName;

    int m_deaths = 0;
    int m_draftsTaken = 0;
    // the free opening draft isn't counted, so it doesn't raise the cost
    int m_gaugeDrafts = 0;
    float m_bestPercent = 0.f;
    float m_lifeBest = 0.f;
    float m_gauge = 0.f;
    int m_pendingDrafts = 0;
    int m_pendingGaugeDrafts = 0;

    Levels m_levels;
    bool m_slowMoEnabled = true;
};

} // namespace augment
