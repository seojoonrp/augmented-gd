// Host-side tests for src/core (no Geode, no GD). Run with scripts/test.ps1.
// Plain asserts on purpose: nothing to install, nothing to fetch.

#include "core/AugmentDef.hpp"
#include "core/Formulas.hpp"
#include "core/RunState.hpp"

#include <cmath>
#include <cstdio>
#include <random>
#include <set>
#include <string>

using namespace augment;

namespace {

int g_failed = 0;
int g_checks = 0;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { g_failed++; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_NEAR(a, b) do { \
    g_checks++; \
    if (std::fabs((a) - (b)) > 1e-4f) { g_failed++; std::printf("FAIL %s:%d  %s = %g, expected %g\n", __FILE__, __LINE__, #a, (double)(a), (double)(b)); } \
} while (0)

bool contains(std::string const& hay, std::string const& needle) {
    return hay.find(needle) != std::string::npos;
}

RunState freshRun() {
    RunState s;
    s.start(1, "test");
    return s;
}

// ---------------------------------------------------------------- table

void testTable() {
    auto const& defs = allAugments();
    CHECK(defs.size() == 13);
    std::set<std::string> ids;
    for (auto const& d : defs) {
        CHECK(!d.id.empty());
        CHECK(!d.name.empty());
        CHECK(d.maxLevel >= 1);
        CHECK(!d.initialDesc.empty());
        CHECK((d.maxLevel == 1) == d.levelUpDesc.empty());
        ids.insert(d.id);
    }
    CHECK(ids.size() == defs.size());
    CHECK(findAugment(ids::Cat) != nullptr);
    CHECK(findAugment("nope") == nullptr);
    CHECK(augmentName("nope") == "nope");
    CHECK(augmentName(ids::Shield) == "결계인가?");

    // Numbers on the cards come from tune::, formatted the way the design
    // table writes them.
    CHECK(contains(findAugment(ids::Shield)->initialDesc, "1.5초간"));
    CHECK(contains(findAugment(ids::SlowMo)->initialDesc, "5% 감소"));
    CHECK(contains(findAugment(ids::SlowMo)->levelUpDesc, "5% 더 감소"));
    CHECK(contains(findAugment(ids::HazardHitbox)->initialDesc, "5% 감소"));
    CHECK(contains(findAugment(ids::WaveHitbox)->initialDesc, "10% 감소"));
    CHECK(contains(findAugment(ids::Nerve)->initialDesc, "각각 X% 증가"));
    CHECK(contains(findAugment(ids::DraftCount)->initialDesc, "4개씩"));
    CHECK(contains(findAugment(ids::Cat)->initialDesc, "4초마다"));
    CHECK(contains(findAugment(ids::Cat)->initialDesc, "장애물 5개"));
    CHECK(contains(findAugment(ids::Cat)->levelUpDesc, "한 개 더"));
    CHECK(contains(findAugment(ids::Cat)->levelUpDesc, "0.5초 감소"));
    CHECK(contains(findAugment(ids::Brake)->initialDesc, "60% 감소"));
    CHECK(contains(findAugment(ids::Brake)->initialDesc, "최대 7초씩"));
    CHECK(contains(findAugment(ids::Brake)->levelUpDesc, "7초 더"));
    CHECK(contains(findAugment(ids::Missile)->initialDesc, "6초마다"));
    CHECK(contains(findAugment(ids::Missile)->initialDesc, "반경 3칸"));
    CHECK(contains(findAugment(ids::Missile)->levelUpDesc, "0.5칸 커지고"));
    CHECK(contains(findAugment(ids::Missile)->levelUpDesc, "0.5초 감소"));
    CHECK(contains(findAugment(ids::Berserk)->initialDesc, "3% 확률로"));
    CHECK(contains(findAugment(ids::Berserk)->initialDesc, "2초간"));
    CHECK(contains(findAugment(ids::Berserk)->levelUpDesc, "1% 증가"));

    // describe(): initial for level 1, level-up text after, initial again
    // when there is no level-up text.
    auto const& slow = *findAugment(ids::SlowMo);
    CHECK(&slow.describe(1) == &slow.initialDesc);
    CHECK(&slow.describe(2) == &slow.levelUpDesc);
    auto const& fore = *findAugment(ids::Foresight);
    CHECK(&fore.describe(2) == &fore.initialDesc);
}

// ---------------------------------------------------------------- formulas

void testFormulas() {
    CHECK_NEAR(formula::slowMoScale(0), 1.f);
    CHECK_NEAR(formula::slowMoScale(3), 0.85f);

    CHECK_NEAR(formula::nerveBoost(0, 1.f), 1.f);
    CHECK_NEAR(formula::nerveBoost(1, 0.f), 1.f);
    CHECK_NEAR(formula::nerveBoost(1, 0.5f), 1.5f);
    CHECK_NEAR(formula::nerveBoost(1, 1.f), 2.f);
    CHECK_NEAR(formula::nerveBoost(1, 7.f), 2.f);     // progress clamped

    CHECK_NEAR(formula::hazardScale(0, 0, 0.f), 1.f);
    CHECK_NEAR(formula::hazardScale(1, 0, 0.f), 0.95f);
    CHECK_NEAR(formula::hazardScale(5, 0, 0.f), 0.75f);
    CHECK_NEAR(formula::hazardScale(5, 1, 1.f), 1.f - 0.25f * 2.f);
    CHECK_NEAR(formula::waveScale(5, 0, 0.f), 0.5f);
    // wave Lv5 + nerve at 100 % would be 1 - 1.0: floored.
    CHECK_NEAR(formula::waveScale(5, 1, 1.f), tune::MinHitboxScale);

    CHECK(formula::catCount(0) == 0);
    CHECK(formula::catCount(1) == 5);
    CHECK(formula::catCount(5) == 9);
    CHECK_NEAR(formula::catInterval(0), 0.f);
    CHECK_NEAR(formula::catInterval(1), 4.f);
    CHECK_NEAR(formula::catInterval(5), 2.f);
    CHECK_NEAR(formula::catInterval(50), tune::CatMinInterval);

    CHECK_NEAR(formula::missileInterval(0), 0.f);
    CHECK_NEAR(formula::missileInterval(1), 6.f);
    CHECK_NEAR(formula::missileInterval(5), 4.f);
    CHECK_NEAR(formula::missileInterval(50), tune::MissileMinInterval);
    CHECK_NEAR(formula::missileRadius(0), 0.f);
    CHECK_NEAR(formula::missileRadius(1), 90.f);
    CHECK_NEAR(formula::missileRadius(5), 150.f);

    CHECK(formula::draftCardCount(false) == 3);
    CHECK(formula::draftCardCount(true) == 4);

    CHECK_NEAR(formula::brakeScale(), 0.4f);
    CHECK_NEAR(formula::brakeBudget(0), 0.f);
    CHECK_NEAR(formula::brakeBudget(1), 7.f);
    CHECK_NEAR(formula::brakeBudget(3), 21.f);

    CHECK_NEAR(formula::berserkChance(0), 0.f);
    CHECK_NEAR(formula::berserkChance(1), 0.03f);
    CHECK_NEAR(formula::berserkChance(2), 0.04f);
    CHECK_NEAR(formula::berserkChance(3), 0.05f);
    CHECK_NEAR(formula::berserkChance(500), 1.f);   // a probability, so clamped
    // The window is flat across levels; only the chance grows.
    CHECK_NEAR(formula::berserkSeconds(0), 0.f);
    CHECK_NEAR(formula::berserkSeconds(1), 2.f);
    CHECK_NEAR(formula::berserkSeconds(3), 2.f);
}

// ---------------------------------------------------------------- run lifecycle

void testLifecycle() {
    RunState s;
    CHECK(!s.active());
    CHECK(s.onDeath(50.f, GaugeRule{}).charge == 0.f);   // inactive: nothing happens
    CHECK(s.deaths() == 0);

    s.start(42, "Level");
    CHECK(s.active());
    CHECK(s.isFor(42));
    CHECK(!s.isFor(43));
    CHECK(s.levelName() == "Level");
    // The free opening draft is queued but not gauge-earned.
    CHECK(s.pendingDrafts() == 1);
    CHECK(s.pendingGaugeDrafts() == 0);
    CHECK_NEAR(s.gauge(), 0.f);
    CHECK(s.slowMoEnabled());

    s.grant(ids::Shield);
    s.toggleSlowMo();
    s.end();
    CHECK(!s.active());
    CHECK(s.pendingDrafts() == 0);
    // Levels survive end() (Continue on the info screen reads them) but a
    // new start() wipes everything.
    CHECK(s.levelOf(ids::Shield) == 1);
    s.start(42, "Level");
    CHECK(s.levelOf(ids::Shield) == 0);
    CHECK(s.slowMoEnabled());
    CHECK(s.deaths() == 0);
}

// ---------------------------------------------------------------- gauge

void testGaugeNoFloor() {
    auto s = freshRun();
    GaugeRule rule;
    auto r = s.onDeath(3.f, rule);
    // First death is always a new best: 3 + 3 bonus.
    CHECK_NEAR(r.bonus, 3.f);
    CHECK_NEAR(r.charge, 6.f);
    CHECK_NEAR(s.gauge(), 6.f);
    CHECK(r.earned == 0);
    CHECK(s.deaths() == 1);
    CHECK_NEAR(s.bestPercent(), 3.f);

    // Same spot again: no bonus, no floor.
    r = s.onDeath(3.f, rule);
    CHECK_NEAR(r.bonus, 0.f);
    CHECK_NEAR(r.charge, 3.f);
    CHECK_NEAR(s.gauge(), 9.f);

    // Below the best: still just the percent.
    r = s.onDeath(1.f, rule);
    CHECK_NEAR(r.bonus, 0.f);
    CHECK_NEAR(s.gauge(), 10.f);
}

void testGaugeNewBestWholePercents() {
    auto s = freshRun();
    GaugeRule rule;
    s.onDeath(4.1f, rule);
    CHECK_NEAR(s.bestPercent(), 4.1f);
    // Same whole percent, further along: the best moves, no bonus.
    auto r = s.onDeath(4.4f, rule);
    CHECK_NEAR(r.bonus, 0.f);
    CHECK_NEAR(r.charge, 4.4f);
    CHECK_NEAR(s.bestPercent(), 4.4f);
    // Crossing into 5 %: one whole percent of bonus, not 0.6.
    r = s.onDeath(5.0f, rule);
    CHECK_NEAR(r.bonus, 1.f);
    // Decimals never add up to a bonus on their own.
    r = s.onDeath(5.9f, rule);
    CHECK_NEAR(r.bonus, 0.f);
    r = s.onDeath(7.2f, rule);
    CHECK_NEAR(r.bonus, 2.f);
}

void testGaugeNewBestBonusBounded() {
    auto s = freshRun();
    GaugeRule rule;
    rule.thresholdStart = 100000.f;   // never draft, just sum
    float bonuses = 0.f;
    for (float p : { 10.f, 5.f, 30.f, 30.f, 60.f, 100.f, 20.f }) bonuses += s.onDeath(p, rule).bonus;
    CHECK_NEAR(bonuses, 100.f * rule.newBestMult);
    CHECK_NEAR(s.bestPercent(), 100.f);
}

void testGaugeRamp() {
    auto s = freshRun();
    GaugeRule rule;
    CHECK_NEAR(s.gaugeThreshold(rule), 30.f);

    // 25 % fresh = 25 + 25 = 50 >= 30: one draft, 20 left, cost now 35.
    auto r = s.onDeath(25.f, rule);
    CHECK(r.earned == 1);
    CHECK_NEAR(s.gauge(), 20.f);
    CHECK_NEAR(s.gaugeThreshold(rule), 35.f);
    CHECK(s.pendingDrafts() == 2);         // opening + this one
    CHECK(s.pendingGaugeDrafts() == 1);

    // Taking drafts: the opening one first leaves the gauge one still full.
    s.takePendingDraft();
    CHECK(s.pendingDrafts() == 1);
    CHECK(s.pendingGaugeDrafts() == 1);
    s.takePendingDraft();
    CHECK(s.pendingDrafts() == 0);
    CHECK(s.pendingGaugeDrafts() == 0);
    s.takePendingDraft();                   // never negative
    CHECK(s.pendingDrafts() == 0);

    // The opening draft did not raise the cost: still 35 after one gauge draft.
    CHECK_NEAR(s.gaugeThreshold(rule), 35.f);
}

// 30, 35, 40 ... 95, 100, and flat from there (user, 2026-09-27).
void testGaugeThresholdCap() {
    auto s = freshRun();
    GaugeRule rule;
    // Fourteen gauge drafts walk the ramp up to its ceiling.
    for (int i = 0; i < 14; i++) {
        CHECK_NEAR(s.gaugeThreshold(rule), 30.f + 5.f * static_cast<float>(i));
        s.fillGauge(rule);
        CHECK(s.onDeath(0.f, rule).earned == 1);
    }
    CHECK_NEAR(s.gaugeThreshold(rule), 100.f);
    s.fillGauge(rule);
    CHECK(s.onDeath(0.f, rule).earned == 1);
    CHECK_NEAR(s.gaugeThreshold(rule), 100.f);   // stays there
    CHECK(s.draftsTaken() == 0);                 // queued, not taken
}

void testGaugeMultiDraft() {
    auto s = freshRun();
    GaugeRule rule;
    // 60 % fresh = 60 + 60 = 120 pays 30 + 35 + 40, 15 left (< 45).
    auto r = s.onDeath(60.f, rule);
    CHECK(r.earned == 3);
    CHECK_NEAR(s.gauge(), 15.f);
    CHECK(s.pendingDrafts() == 4);         // opening + these three
    CHECK(s.pendingGaugeDrafts() == 3);
    CHECK_NEAR(s.gaugeThreshold(rule), 45.f);

    auto t = freshRun();
    t.onDeath(60.f, rule);
    t.dropPendingDrafts();
    CHECK(t.pendingDrafts() == 0);
    CHECK(t.pendingGaugeDrafts() == 0);
}

// A life is one visit to the level: deaths a checkpoint brings the player
// back from charge nothing, and the death that really ends the life pays once
// for the furthest point the whole life reached.
void testGaugeCheckpointLife() {
    auto s = freshRun();
    GaugeRule rule;
    rule.thresholdStart = 100000.f;   // never draft, just charge

    auto r = s.onDeath(6.f, rule, true);
    CHECK(r.deferred);
    CHECK_NEAR(r.charge, 0.f);
    CHECK_NEAR(r.bonus, 0.f);
    CHECK(r.earned == 0);
    CHECK(s.deaths() == 1);           // it still is a death
    CHECK_NEAR(s.gauge(), 0.f);
    CHECK_NEAR(s.bestPercent(), 0.f); // the bonus is not spent yet either
    CHECK_NEAR(s.lifeBest(), 6.f);

    // 6 (+6 new best), not 6 + 4: the 4 % death only ends the life.
    r = s.onDeath(4.f, rule);
    CHECK(!r.deferred);
    CHECK_NEAR(r.bonus, 6.f);
    CHECK_NEAR(r.charge, 12.f);
    CHECK_NEAR(s.gauge(), 12.f);
    CHECK_NEAR(s.bestPercent(), 6.f);
    CHECK_NEAR(s.lifeBest(), 0.f);

    // Several respawns in one life: one payment, at the furthest point.
    s.onDeath(20.f, rule, true);
    s.onDeath(12.f, rule, true);
    CHECK_NEAR(s.lifeBest(), 20.f);
    CHECK_NEAR(s.gauge(), 12.f);
    r = s.onDeath(8.f, rule);
    CHECK_NEAR(r.charge, 20.f + 14.f);   // 20, and 20 - 6 whole new percents
    CHECK_NEAR(s.bestPercent(), 20.f);

    // A life that stayed below the best pays its own percent only.
    s.onDeath(10.f, rule, true);
    r = s.onDeath(3.f, rule);
    CHECK_NEAR(r.bonus, 0.f);
    CHECK_NEAR(r.charge, 10.f);

    // Drafts wait for the settling death as well.
    auto t = freshRun();
    GaugeRule plain;
    CHECK(t.onDeath(60.f, plain, true).earned == 0);
    CHECK(t.pendingGaugeDrafts() == 0);
    CHECK(t.onDeath(1.f, plain).earned == 3);   // 60 + 60 bonus pays 30 + 35 + 40

    // Ending the run drops a life nothing was paid for.
    auto u = freshRun();
    u.onDeath(30.f, rule, true);
    CHECK_NEAR(u.lifeBest(), 30.f);
    u.end();
    CHECK_NEAR(u.lifeBest(), 0.f);
}

void testGaugeStopsWhenNothingDraftable() {
    auto s = freshRun();
    for (auto const& d : allAugments()) {
        for (int i = 0; i < d.maxLevel; i++) s.grant(d.id);
    }
    CHECK(!s.anyDraftable());
    auto r = s.onDeath(100.f, GaugeRule{});
    CHECK(r.earned == 0);
    CHECK_NEAR(s.gauge(), 200.f);   // charge kept, no draft queued
}

void testFillGauge() {
    auto s = freshRun();
    GaugeRule rule;
    s.onDeath(5.f, rule);            // gauge 10
    CHECK_NEAR(s.fillGauge(rule), 20.f);
    CHECK_NEAR(s.gauge(), 30.f);
    CHECK_NEAR(s.fillGauge(rule), 0.f);
    // Next death (any percent) drafts.
    CHECK(s.onDeath(0.f, rule).earned == 1);

    RunState idle;
    CHECK_NEAR(idle.fillGauge(rule), 0.f);
}

// ---------------------------------------------------------------- augments

void testGrantAndPick() {
    auto s = freshRun();
    CHECK(s.grant("nope") == 0);
    CHECK(s.grant(ids::Shield) == 1);
    CHECK(s.grant(ids::Shield) == 2);
    CHECK(s.grant(ids::Shield) == 3);
    CHECK(s.grant(ids::Shield) == 3);   // capped at maxLevel
    CHECK(s.draftsTaken() == 0);        // grants are not picks

    CHECK(s.applyPick(ids::SlowMo) == 1);
    CHECK(s.applyPick("nope") == 0);
    CHECK(s.draftsTaken() == 1);
    CHECK(s.has(ids::SlowMo));
    CHECK(!s.has(ids::Cat));
    CHECK(s.augments().size() == 2);
}

void testRollDraft() {
    auto s = freshRun();
    std::mt19937 rng{ 1234 };

    auto roll = s.rollDraft(3, rng);
    CHECK(roll.size() == 3);
    std::set<AugmentDef const*> unique(roll.begin(), roll.end());
    CHECK(unique.size() == 3);

    // Maxed augments never show up.
    for (int i = 0; i < 3; i++) s.grant(ids::Shield);
    for (int i = 0; i < 100; i++) {
        for (auto d : s.rollDraft(4, rng)) CHECK(d->id != ids::Shield);
    }

    // Count is capped by what is left.
    auto t = freshRun();
    for (auto const& d : allAugments()) {
        if (d.id != ids::Cat) for (int i = 0; i < d.maxLevel; i++) t.grant(d.id);
    }
    auto last = t.rollDraft(3, rng);
    CHECK(last.size() == 1);
    CHECK(last[0]->id == ids::Cat);

    CHECK(s.draftCardCount() == 3);
    s.grant(ids::DraftCount);
    CHECK(s.draftCardCount() == 4);
}

void testEffectsAtLevels() {
    auto s = freshRun();
    CHECK_NEAR(s.slowMoScale(), 1.f);
    CHECK_NEAR(s.hazardScale(1.f), 1.f);
    CHECK_NEAR(s.waveScale(1.f), 1.f);
    CHECK(s.catCount() == 0);

    s.grant(ids::SlowMo); s.grant(ids::SlowMo);
    CHECK_NEAR(s.slowMoScale(), 0.9f);
    s.grant(ids::HazardHitbox);
    s.grant(ids::Nerve);
    CHECK_NEAR(s.hazardScale(0.f), 0.95f);
    CHECK_NEAR(s.hazardScale(1.f), 0.9f);
    CHECK_NEAR(s.nerveBoost(0.5f), 1.5f);
    s.grant(ids::Cat); s.grant(ids::Cat);
    CHECK(s.catCount() == 6);
    CHECK_NEAR(s.catInterval(), 3.5f);
    CHECK_NEAR(s.missileRadius(), 0.f);
    s.grant(ids::Missile); s.grant(ids::Missile); s.grant(ids::Missile);
    CHECK_NEAR(s.missileInterval(), 5.f);
    CHECK_NEAR(s.missileRadius(), 120.f);
    CHECK_NEAR(s.berserkChance(), 0.f);
    CHECK_NEAR(s.berserkSeconds(), 0.f);
    s.grant(ids::Berserk); s.grant(ids::Berserk);
    CHECK_NEAR(s.berserkChance(), 0.04f);
    CHECK_NEAR(s.berserkSeconds(), tune::BerserkSeconds);
}

} // namespace

int main() {
    testTable();
    testFormulas();
    testLifecycle();
    testGaugeNoFloor();
    testGaugeNewBestWholePercents();
    testGaugeNewBestBonusBounded();
    testGaugeRamp();
    testGaugeThresholdCap();
    testGaugeMultiDraft();
    testGaugeCheckpointLife();
    testGaugeStopsWhenNothingDraftable();
    testFillGauge();
    testGrantAndPick();
    testRollDraft();
    testEffectsAtLevels();

    std::printf("core tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
