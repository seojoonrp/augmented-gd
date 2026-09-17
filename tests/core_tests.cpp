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
    CHECK(defs.size() == 10);
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
    CHECK(contains(findAugment(ids::Nerve)->levelUpDesc, "각각 1.5X% 증가"));
    CHECK(contains(findAugment(ids::DraftCount)->initialDesc, "4개씩"));
    CHECK(contains(findAugment(ids::Cat)->initialDesc, "4초마다"));
    CHECK(contains(findAugment(ids::Cat)->initialDesc, "장애물 5개"));
    CHECK(contains(findAugment(ids::Cat)->levelUpDesc, "한 개 더"));
    CHECK(contains(findAugment(ids::Cat)->levelUpDesc, "0.5초 감소"));

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
    CHECK_NEAR(formula::nerveBoost(2, 1.f), 2.5f);
    CHECK_NEAR(formula::nerveBoost(2, 7.f), 2.5f);    // progress clamped
    CHECK_NEAR(formula::nerveBoost(9, 1.f), 2.5f);    // level clamped to the table

    CHECK_NEAR(formula::hazardScale(0, 0, 0.f), 1.f);
    CHECK_NEAR(formula::hazardScale(1, 0, 0.f), 0.95f);
    CHECK_NEAR(formula::hazardScale(5, 0, 0.f), 0.75f);
    CHECK_NEAR(formula::hazardScale(5, 2, 1.f), 1.f - 0.25f * 2.5f);
    CHECK_NEAR(formula::waveScale(5, 0, 0.f), 0.5f);
    // wave Lv5 + nerve Lv2 at 100 % would be 1 - 1.25: floored.
    CHECK_NEAR(formula::waveScale(5, 2, 1.f), tune::MinHitboxScale);

    CHECK(formula::catCount(0) == 0);
    CHECK(formula::catCount(1) == 5);
    CHECK(formula::catCount(5) == 9);
    CHECK_NEAR(formula::catInterval(0), 0.f);
    CHECK_NEAR(formula::catInterval(1), 4.f);
    CHECK_NEAR(formula::catInterval(5), 2.f);
    CHECK_NEAR(formula::catInterval(50), tune::CatMinInterval);

    CHECK(formula::draftCardCount(false) == 3);
    CHECK(formula::draftCardCount(true) == 4);
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
    CHECK_NEAR(s.gaugeThreshold(rule), 40.f);

    // 25 % fresh = 25 + 25 = 50 >= 40: one draft, 10 left, cost now 50.
    auto r = s.onDeath(25.f, rule);
    CHECK(r.earned == 1);
    CHECK_NEAR(s.gauge(), 10.f);
    CHECK_NEAR(s.gaugeThreshold(rule), 50.f);
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

    // The opening draft did not raise the cost: still 50 after one gauge draft.
    CHECK_NEAR(s.gaugeThreshold(rule), 50.f);
}

void testGaugeMultiDraft() {
    auto s = freshRun();
    GaugeRule rule;
    rule.fixedThreshold = 20.f;
    // 60 % fresh = 60 + 60 = 120 = 6 drafts at a fixed cost of 20.
    auto r = s.onDeath(60.f, rule);
    CHECK(r.earned == 6);
    CHECK_NEAR(s.gauge(), 0.f);
    CHECK(s.pendingDrafts() == 7);
    CHECK(s.pendingGaugeDrafts() == 6);
    CHECK_NEAR(s.gaugeThreshold(rule), 20.f);   // fixed: no ramp

    // With the ramp: 120 pays 40 + 50 = 90, 30 left (< 60).
    auto t = freshRun();
    r = t.onDeath(60.f, GaugeRule{});
    CHECK(r.earned == 2);
    CHECK_NEAR(t.gauge(), 30.f);
    CHECK_NEAR(t.gaugeThreshold(GaugeRule{}), 60.f);

    t.dropPendingDrafts();
    CHECK(t.pendingDrafts() == 0);
    CHECK(t.pendingGaugeDrafts() == 0);
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
    CHECK_NEAR(s.fillGauge(rule), 30.f);
    CHECK_NEAR(s.gauge(), 40.f);
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
}

} // namespace

int main() {
    testTable();
    testFormulas();
    testLifecycle();
    testGaugeNoFloor();
    testGaugeNewBestBonusBounded();
    testGaugeRamp();
    testGaugeMultiDraft();
    testGaugeStopsWhenNothingDraftable();
    testFillGauge();
    testGrantAndPick();
    testRollDraft();
    testEffectsAtLevels();

    std::printf("core tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
