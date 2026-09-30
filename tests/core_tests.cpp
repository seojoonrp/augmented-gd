// Host-side tests for src/core (no Geode, no GD). Run with scripts/test.ps1.
// Plain asserts on purpose: nothing to install, nothing to fetch.

#include "core/AugmentDef.hpp"
#include "core/Formulas.hpp"
#include "core/RunState.hpp"

#include <cmath>
#include <cstdio>
#include <initializer_list>
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

// Every Hangul syllable block; an English text must have none (a pasted
// Korean phrase would draw fine and read wrong).
bool hasHangul(std::string const& s) {
    // UTF-8 lead bytes 0xEA-0xED cover U+A000-U+DFFF, which holds the block.
    for (unsigned char c : s) {
        if (c >= 0xEA && c <= 0xED) return true;
    }
    return false;
}

void testTable() {
    auto const& defs = allAugments();
    CHECK(defs.size() == 13);
    std::set<std::string> ids;
    for (auto const& d : defs) {
        CHECK(!d.id.empty());
        CHECK(d.maxLevel >= 1);
        // Both languages, all the way through.
        for (auto const* text : { &d.name, &d.initialDesc }) {
            CHECK(!text->en.empty());
            CHECK(!text->ko.empty());
        }
        CHECK((d.maxLevel == 1) == d.levelUpDesc.en.empty());
        CHECK((d.maxLevel == 1) == d.levelUpDesc.ko.empty());
        CHECK(!hasHangul(d.name.en) && !hasHangul(d.initialDesc.en) && !hasHangul(d.levelUpDesc.en));
        CHECK(hasHangul(d.name.ko) && hasHangul(d.initialDesc.ko));
        // A late level-up text comes with its level, in both languages.
        CHECK((d.lateFrom > 0) == !d.lateLevelUpDesc.en.empty());
        CHECK((d.lateFrom > 0) == !d.lateLevelUpDesc.ko.empty());
        if (d.lateFrom > 0) CHECK(d.lateFrom > 2 && d.lateFrom <= d.maxLevel);
        CHECK(!hasHangul(d.lateLevelUpDesc.en));
        ids.insert(d.id);
    }
    CHECK(ids.size() == defs.size());
    CHECK(findAugment(ids::Cat) != nullptr);
    CHECK(findAugment("nope") == nullptr);
    CHECK(augmentName("nope", Lang::English) == "nope");
    CHECK(augmentName(ids::Shield, Lang::Korean) == "결계인가?");
    CHECK(augmentName(ids::Shield, Lang::English) == "Shield");
    CHECK(augmentName(ids::Missile, Lang::English) == "Air Raid");

    // Numbers on the cards come from tune::, formatted the way the design
    // table writes them.
    auto ko = [](char const* id) { return findAugment(id)->initialDesc.ko; };
    auto koUp = [](char const* id) { return findAugment(id)->levelUpDesc.ko; };
    CHECK(contains(ko(ids::Shield), "1.5초간"));
    CHECK(contains(ko(ids::SlowMo), "5% 감소"));
    CHECK(contains(koUp(ids::SlowMo), "5% 더 감소"));
    CHECK(contains(ko(ids::HazardHitbox), "5% 감소"));
    CHECK(contains(ko(ids::WaveHitbox), "10% 감소"));
    CHECK(contains(ko(ids::Nerve), "기존의 (120 + X/2)%가"));
    CHECK(contains(ko(ids::DraftCount), "4개씩"));
    CHECK(contains(ko(ids::Cat), "4초마다"));
    CHECK(contains(ko(ids::Cat), "위험 요소 5개"));
    CHECK(contains(koUp(ids::Cat), "2개 더"));
    CHECK(contains(koUp(ids::Cat), "0.5초 감소"));
    CHECK(contains(ko(ids::Brake), "60% 감소"));
    CHECK(contains(ko(ids::Brake), "최대 7초씩"));
    CHECK(contains(koUp(ids::Brake), "7초 더"));
    CHECK(contains(ko(ids::Missile), "6초마다"));
    CHECK(contains(ko(ids::Missile), "반경 3칸"));
    CHECK(contains(koUp(ids::Missile), "1칸 커지고"));
    CHECK(contains(koUp(ids::Missile), "0.5초 감소"));
    CHECK(contains(ko(ids::Berserk), "3% 확률로"));
    CHECK(contains(ko(ids::Berserk), "2.5초간"));
    CHECK(contains(koUp(ids::Berserk), "1.5% 증가"));

    // The English texts quote the same numbers, with their units.
    auto en = [](char const* id) { return findAugment(id)->initialDesc.en; };
    auto enUp = [](char const* id) { return findAugment(id)->levelUpDesc.en; };
    CHECK(contains(en(ids::Shield), "noclip for 1.5 seconds"));
    CHECK(contains(en(ids::SlowMo), "5% slower"));
    CHECK(contains(enUp(ids::SlowMo), "another 5% slower"));
    CHECK(contains(en(ids::HazardHitbox), "shrink by 5%"));
    CHECK(contains(en(ids::WaveHitbox), "shrinks by 10%"));
    CHECK(contains(en(ids::Nerve), "[Threat Removal]"));
    CHECK(contains(en(ids::Nerve), "[Wave Breaker]"));
    CHECK(contains(en(ids::Nerve), "at (120 + X/2)% of"));
    CHECK(contains(en(ids::DraftCount), "4 cards"));
    CHECK(contains(en(ids::Cat), "Every 4 seconds"));
    CHECK(contains(en(ids::Cat), "removes 5 random hazards"));
    CHECK(contains(enUp(ids::Cat), "2 more hazards"));
    CHECK(contains(enUp(ids::Cat), "0.5 seconds"));
    CHECK(contains(en(ids::Brake), "by 60%"));
    CHECK(contains(en(ids::Brake), "Up to 7 seconds"));
    CHECK(contains(enUp(ids::Brake), "7 seconds longer"));
    CHECK(contains(en(ids::Missile), "Every 6 seconds"));
    CHECK(contains(en(ids::Missile), "within 3 blocks"));
    CHECK(contains(enUp(ids::Missile), "grows by 1 block and"));
    CHECK(contains(enUp(ids::Missile), "0.5 seconds sooner"));
    CHECK(contains(en(ids::Berserk), "3% chance"));
    CHECK(contains(en(ids::Berserk), "2.5 seconds of berserk"));
    CHECK(contains(enUp(ids::Berserk), "up by 1.5%"));

    // describe(): initial for level 1, level-up text after, initial again
    // when there is no level-up text; in the language asked for.
    auto const& slow = *findAugment(ids::SlowMo);
    CHECK(&slow.describe(1, Lang::Korean) == &slow.initialDesc.ko);
    CHECK(&slow.describe(2, Lang::Korean) == &slow.levelUpDesc.ko);
    CHECK(&slow.describe(2, Lang::English) == &slow.levelUpDesc.en);
    auto const& fore = *findAugment(ids::Foresight);
    CHECK(&fore.describe(2, Lang::English) == &fore.initialDesc.en);
    // Cat's last levels grow the count only, and say so (user, 2026-09-30).
    auto const& cat = *findAugment(ids::Cat);
    CHECK(cat.maxLevel == 7);
    CHECK(cat.lateFrom == 6);
    CHECK(&cat.describe(5, Lang::Korean) == &cat.levelUpDesc.ko);
    CHECK(&cat.describe(6, Lang::Korean) == &cat.lateLevelUpDesc.ko);
    CHECK(&cat.describe(7, Lang::English) == &cat.lateLevelUpDesc.en);
    CHECK(contains(cat.lateLevelUpDesc.ko, "4개 더 제거합니다"));
    CHECK(!contains(cat.lateLevelUpDesc.ko, "쿨타임"));
    CHECK(contains(cat.lateLevelUpDesc.en, "4 more hazards"));
    CHECK(&slow.describe(5, Lang::Korean) == &slow.levelUpDesc.ko);   // no late text: same all the way
    // Max levels (user, 2026-09-30).
    CHECK(findAugment(ids::Shield)->maxLevel == 5);
    CHECK(findAugment(ids::SlowMo)->maxLevel == 3);   // stays 3 (user, 2026-09-30)
    CHECK(findAugment(ids::HazardHitbox)->maxLevel == 7);
    CHECK(findAugment(ids::WaveHitbox)->maxLevel == 7);
}

// describeAt(): the detail card's text carries that level's numbers.
void testDescribeAt() {
    // Level 1 and single-level augments read exactly like the draft card.
    for (auto lang : { Lang::English, Lang::Korean }) {
        for (auto const& d : allAugments()) {
            auto const& initial = d.initialDesc.in(lang);
            CHECK(d.describeAt(1, lang) == initial);
            CHECK(d.describeAt(0, lang) == initial);
            if (d.maxLevel == 1) CHECK(d.describeAt(3, lang) == initial);
            // Every level has a text, and the numbers differ from level 1's.
            for (int lv = 2; lv <= d.maxLevel; lv++) {
                CHECK(d.describeAt(lv, lang) != initial);
                CHECK(hasHangul(d.describeAt(lv, lang)) == (lang == Lang::Korean));
            }
        }
    }

    auto at = [](char const* id, int level) { return findAugment(id)->describeAt(level, Lang::Korean); };
    CHECK(contains(at(ids::Shield, 3), "보호막이 3개"));
    CHECK(contains(at(ids::Shield, 3), "1.5초간"));
    CHECK(contains(at(ids::SlowMo, 3), "15% 감소"));
    CHECK(contains(at(ids::SlowMo, 3), "X를 눌러"));
    CHECK(contains(at(ids::StartPos, 3), "최대 3번"));
    CHECK(findAugment(ids::StartPos)->maxLevel == 3);
    CHECK(contains(at(ids::HazardHitbox, 5), "25% 감소"));
    CHECK(contains(at(ids::WaveHitbox, 5), "50% 감소"));
    CHECK(contains(at(ids::Cat, 3), "3초마다"));
    CHECK(contains(at(ids::Cat, 3), "위험 요소 9개"));
    CHECK(contains(at(ids::Cat, 6), "2초마다"));
    CHECK(contains(at(ids::Cat, 6), "위험 요소 17개"));
    CHECK(contains(at(ids::Cat, 7), "2초마다"));
    CHECK(contains(at(ids::Cat, 7), "위험 요소 21개"));
    CHECK(contains(at(ids::Shield, 5), "보호막이 5개"));
    CHECK(contains(at(ids::HazardHitbox, 7), "35% 감소"));
    CHECK(contains(at(ids::WaveHitbox, 7), "70% 감소"));
    CHECK(contains(at(ids::Cat, 4), "2.5초마다"));
    CHECK(contains(at(ids::Brake, 2), "최대 14초씩"));
    CHECK(contains(at(ids::Brake, 2), "60% 감소"));
    CHECK(contains(at(ids::Missile, 3), "5초마다"));
    CHECK(contains(at(ids::Missile, 3), "반경 5칸"));
    CHECK(contains(at(ids::Missile, 2), "반경 4칸"));
    CHECK(contains(at(ids::Berserk, 2), "4.5% 확률로"));
    CHECK(contains(at(ids::Berserk, 3), "6% 확률로"));
    CHECK(contains(at(ids::Berserk, 3), "2.5초간"));
    // Past the cap reads as the cap.
    CHECK(at(ids::Shield, 9) == at(ids::Shield, 5));
    CHECK(at(ids::StartPos, 5) == at(ids::StartPos, 3));

    auto atEn = [](char const* id, int level) { return findAugment(id)->describeAt(level, Lang::English); };
    CHECK(contains(atEn(ids::Shield, 3), "Get 3 shields"));
    CHECK(contains(atEn(ids::Shield, 3), "noclip for 1.5 seconds"));
    CHECK(contains(atEn(ids::SlowMo, 3), "15% slower"));
    CHECK(contains(atEn(ids::StartPos, 3), "up to 3 times"));
    CHECK(contains(atEn(ids::HazardHitbox, 5), "by 25%"));
    CHECK(contains(atEn(ids::WaveHitbox, 5), "by 50%"));
    CHECK(contains(atEn(ids::Cat, 3), "Every 3 seconds"));
    CHECK(contains(atEn(ids::Cat, 3), "removes 9 random"));
    CHECK(contains(atEn(ids::Cat, 4), "Every 2.5 seconds"));
    CHECK(contains(atEn(ids::Cat, 7), "Every 2 seconds"));
    CHECK(contains(atEn(ids::Cat, 7), "removes 21 random"));
    CHECK(contains(atEn(ids::Brake, 2), "Up to 14 seconds"));
    CHECK(contains(atEn(ids::Missile, 3), "Every 5 seconds"));
    CHECK(contains(atEn(ids::Missile, 3), "within 5 blocks"));
    CHECK(contains(atEn(ids::Missile, 2), "within 4 blocks"));
    CHECK(contains(atEn(ids::Berserk, 3), "6% chance"));
    CHECK(atEn(ids::Shield, 9) == atEn(ids::Shield, 5));
}

// ---------------------------------------------------------------- formulas

void testFormulas() {
    CHECK_NEAR(formula::slowMoScale(0), 1.f);
    CHECK_NEAR(formula::slowMoScale(3), 0.85f);

    // (120 + X/2) % at X % progress (user, 2026-09-30).
    CHECK_NEAR(formula::nerveBoost(0, 1.f), 1.f);
    CHECK_NEAR(formula::nerveBoost(1, 0.f), 1.2f);
    CHECK_NEAR(formula::nerveBoost(1, 0.5f), 1.45f);
    CHECK_NEAR(formula::nerveBoost(1, 1.f), 1.7f);
    CHECK_NEAR(formula::nerveBoost(1, 7.f), 1.7f);    // progress clamped

    CHECK_NEAR(formula::hazardScale(0, 0, 0.f), 1.f);
    CHECK_NEAR(formula::hazardScale(1, 0, 0.f), 0.95f);
    CHECK_NEAR(formula::hazardScale(5, 0, 0.f), 0.75f);
    CHECK_NEAR(formula::hazardScale(5, 1, 1.f), 1.f - 0.25f * 1.7f);
    CHECK_NEAR(formula::hazardScale(7, 0, 0.f), 0.65f);
    CHECK_NEAR(formula::hazardScale(7, 1, 1.f), 1.f - 0.35f * 1.7f);
    CHECK_NEAR(formula::waveScale(5, 0, 0.f), 0.5f);
    CHECK_NEAR(formula::waveScale(7, 0, 0.f), 0.3f);
    CHECK_NEAR(formula::waveScale(5, 1, 0.f), 1.f - 0.5f * 1.2f);
    // wave Lv5 + nerve at 100 % would be 1 - 0.85, Lv7 + nerve anywhere
    // 1 - 0.84 or less: floored.
    CHECK_NEAR(formula::waveScale(5, 1, 1.f), tune::MinHitboxScale);
    CHECK_NEAR(formula::waveScale(7, 1, 0.f), tune::MinHitboxScale);

    CHECK(formula::catCount(0) == 0);
    CHECK(formula::catCount(1) == 5);
    CHECK(formula::catCount(2) == 7);
    CHECK(formula::catCount(5) == 13);
    // Lv6-7: four more each, the interval stays at Lv5's (user, 2026-09-30).
    CHECK(formula::catCount(6) == 17);
    CHECK(formula::catCount(7) == 21);
    CHECK_NEAR(formula::catInterval(0), 0.f);
    CHECK_NEAR(formula::catInterval(1), 4.f);
    CHECK_NEAR(formula::catInterval(5), 2.f);
    CHECK_NEAR(formula::catInterval(6), 2.f);
    CHECK_NEAR(formula::catInterval(7), 2.f);
    CHECK_NEAR(formula::catInterval(50), 2.f);

    CHECK_NEAR(formula::missileInterval(0), 0.f);
    CHECK_NEAR(formula::missileInterval(1), 6.f);
    CHECK_NEAR(formula::missileInterval(5), 4.f);
    CHECK_NEAR(formula::missileInterval(50), tune::MissileMinInterval);
    CHECK_NEAR(formula::missileRadius(0), 0.f);
    CHECK_NEAR(formula::missileRadius(1), 90.f);
    CHECK_NEAR(formula::missileRadius(2), 120.f);
    CHECK_NEAR(formula::missileRadius(5), 210.f);

    CHECK(formula::draftCardCount(false) == 3);
    CHECK(formula::draftCardCount(true) == 4);

    CHECK_NEAR(formula::brakeScale(), 0.4f);
    CHECK_NEAR(formula::brakeBudget(0), 0.f);
    CHECK_NEAR(formula::brakeBudget(1), 7.f);
    CHECK_NEAR(formula::brakeBudget(3), 21.f);

    CHECK_NEAR(formula::berserkChance(0), 0.f);
    CHECK_NEAR(formula::berserkChance(1), 0.03f);
    CHECK_NEAR(formula::berserkChance(2), 0.045f);
    CHECK_NEAR(formula::berserkChance(3), 0.06f);
    CHECK_NEAR(formula::berserkChance(500), 1.f);   // a probability, so clamped
    // The window is flat across levels; only the chance grows.
    CHECK_NEAR(formula::berserkSeconds(0), 0.f);
    CHECK_NEAR(formula::berserkSeconds(1), 2.5f);
    CHECK_NEAR(formula::berserkSeconds(3), 2.5f);
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
    CHECK_NEAR(r.charge, 4.f);
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

// The gauge holds whole numbers, so what the HUD reads is what it has, and
// landing exactly on the cost drafts (user, 2026-09-30: 2.2 + 3.3 + 12.1 %
// with their bonuses was 29.6, shown as "30/30", and no draft came).
void testGaugeWholeCharge() {
    auto s = freshRun();
    GaugeRule rule;
    rule.thresholdStart = 30.f;   // the cost at the time of the report
    CHECK_NEAR(s.onDeath(2.2f, rule).charge, 4.f);    // 2 + 2
    CHECK_NEAR(s.onDeath(3.3f, rule).charge, 4.f);    // 3 + 1
    auto r = s.onDeath(12.1f, rule);                  // 12 + 9
    CHECK_NEAR(r.charge, 21.f);
    CHECK(r.earned == 0);
    CHECK(s.gauge() == 29.f);                         // exactly, and it reads 29/30
    r = s.onDeath(1.9f, rule);                        // 1 more: exactly 30
    CHECK(r.earned == 1);
    CHECK(s.gauge() == 0.f);

    // Straight onto the cost in one death: 15 + 15.
    auto t = freshRun();
    CHECK(t.onDeath(15.f, rule).earned == 1);
    CHECK(t.gauge() == 0.f);
    // Just under it: 14 + 14, whatever the decimals.
    auto u = freshRun();
    CHECK(u.onDeath(14.99f, rule).earned == 0);
    CHECK(u.gauge() == 28.f);
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
    CHECK_NEAR(s.gaugeThreshold(rule), 20.f);

    // 15 % fresh = 15 + 15 = 30 >= 20: one draft, 10 left, and the second
    // draft costs 20 again.
    auto r = s.onDeath(15.f, rule);
    CHECK(r.earned == 1);
    CHECK_NEAR(s.gauge(), 10.f);
    CHECK_NEAR(s.gaugeThreshold(rule), 20.f);
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

    // The opening draft did not count toward the ramp: two gauge drafts in,
    // the third still costs 20 (it would be 25 if the opening one counted).
    s.fillGauge(rule);
    CHECK(s.onDeath(0.f, rule).earned == 1);
    CHECK_NEAR(s.gaugeThreshold(rule), 20.f);
}

// 20, 20, 20, 25, 25, 25 ... 65, 65, 65, 70, and flat from there (user,
// 2026-09-30).
void testGaugeThresholdCap() {
    auto s = freshRun();
    GaugeRule rule;
    // Before any gauge draft, the "last cost" is simply the next one.
    CHECK_NEAR(s.lastDraftCost(rule), 20.f);
    // Thirty gauge drafts walk the ramp up to its ceiling, three per step.
    for (int i = 0; i < 30; i++) {
        float const cost = 20.f + 5.f * static_cast<float>(i / 3);
        CHECK_NEAR(s.gaugeThreshold(rule), cost);
        s.fillGauge(rule);
        CHECK(s.onDeath(0.f, rule).earned == 1);
        CHECK_NEAR(s.lastDraftCost(rule), cost);
    }
    CHECK_NEAR(s.gaugeThreshold(rule), 70.f);
    for (int i = 0; i < 3; i++) {
        s.fillGauge(rule);
        CHECK(s.onDeath(0.f, rule).earned == 1);
        CHECK_NEAR(s.gaugeThreshold(rule), 70.f);   // stays there
        CHECK_NEAR(s.lastDraftCost(rule), 70.f);
    }
    CHECK(s.draftsTaken() == 0);                    // queued, not taken
}

void testGaugeMultiDraft() {
    auto s = freshRun();
    GaugeRule rule;
    // 32 % fresh = 32 + 32 = 64 pays 20 + 20 + 20, 4 left (< 25).
    auto r = s.onDeath(32.f, rule);
    CHECK(r.earned == 3);
    CHECK_NEAR(s.gauge(), 4.f);
    CHECK(s.pendingDrafts() == 4);         // opening + these three
    CHECK(s.pendingGaugeDrafts() == 3);
    CHECK_NEAR(s.gaugeThreshold(rule), 25.f);
    // The HUD's "cost/cost" while they wait is the last one earned.
    CHECK_NEAR(s.lastDraftCost(rule), 20.f);

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
    CHECK(t.onDeath(32.f, plain, true).earned == 0);
    CHECK(t.pendingGaugeDrafts() == 0);
    CHECK(t.onDeath(1.f, plain).earned == 3);   // 32 + 32 bonus pays 20 + 20 + 20

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

// The run's last levels: a draft shows only what is left, and the gauge
// never queues more drafts than there are levels to give (user, 2026-09-30).
void testGaugeLastLevels() {
    auto s = freshRun();
    s.takePendingDraft();   // the opening draft, as if picked
    for (auto const& d : allAugments()) {
        int const keep = d.id == ids::Cat ? 2 : 0;   // cat: two levels left
        for (int i = 0; i < d.maxLevel - keep; i++) s.grant(d.id);
    }
    CHECK(s.levelsLeft() == 2);
    CHECK(s.anyDraftable());
    GaugeRule rule;
    // 100 % fresh = 200: enough for many drafts, but only two levels are left.
    auto r = s.onDeath(100.f, rule);
    CHECK(r.earned == 2);
    CHECK(s.pendingDrafts() == 2);
    std::mt19937 rng{ 7 };
    auto roll = s.rollDraft(4, rng);
    CHECK(roll.size() == 1);
    CHECK(roll[0]->id == ids::Cat);
    // Picking both: nothing left, nothing queued any more.
    s.takePendingDraft(); s.applyPick(ids::Cat);
    s.takePendingDraft(); s.applyPick(ids::Cat);
    CHECK(s.levelsLeft() == 0);
    CHECK(!s.anyDraftable());
    CHECK(s.onDeath(100.f, rule).earned == 0);
    CHECK(s.pendingDrafts() == 0);

    // One level left while the opening draft still waits: that draft
    // covers it, so the gauge queues none.
    auto t = freshRun();
    for (auto const& d : allAugments()) {
        int const keep = d.id == ids::Shield ? 1 : 0;
        for (int i = 0; i < d.maxLevel - keep; i++) t.grant(d.id);
    }
    CHECK(t.pendingDrafts() == 1);
    CHECK(t.onDeath(100.f, rule).earned == 0);
    CHECK(t.pendingDrafts() == 1);
}

void testFillGauge() {
    auto s = freshRun();
    GaugeRule rule;
    s.onDeath(5.f, rule);            // gauge 10
    CHECK_NEAR(s.fillGauge(rule), 10.f);
    CHECK_NEAR(s.gauge(), 20.f);
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
    CHECK(s.grant(ids::Shield) == 4);
    CHECK(s.grant(ids::Shield) == 5);
    CHECK(s.grant(ids::Shield) == 5);   // capped at maxLevel
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
    for (int i = 0; i < findAugment(ids::Shield)->maxLevel; i++) s.grant(ids::Shield);
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
    CHECK_NEAR(s.hazardScale(0.f), 1.f - 0.05f * 1.2f);
    CHECK_NEAR(s.hazardScale(1.f), 1.f - 0.05f * 1.7f);
    CHECK_NEAR(s.nerveBoost(0.5f), 1.45f);
    s.grant(ids::Cat); s.grant(ids::Cat);
    CHECK(s.catCount() == 7);
    CHECK_NEAR(s.catInterval(), 3.5f);
    CHECK_NEAR(s.missileRadius(), 0.f);
    s.grant(ids::Missile); s.grant(ids::Missile); s.grant(ids::Missile);
    CHECK_NEAR(s.missileInterval(), 5.f);
    CHECK_NEAR(s.missileRadius(), 150.f);
    CHECK_NEAR(s.berserkChance(), 0.f);
    CHECK_NEAR(s.berserkSeconds(), 0.f);
    s.grant(ids::Berserk); s.grant(ids::Berserk);
    CHECK_NEAR(s.berserkChance(), 0.045f);
    CHECK_NEAR(s.berserkSeconds(), tune::BerserkSeconds);
}

} // namespace

int main() {
    testTable();
    testDescribeAt();
    testFormulas();
    testLifecycle();
    testGaugeNoFloor();
    testGaugeNewBestWholePercents();
    testGaugeWholeCharge();
    testGaugeNewBestBonusBounded();
    testGaugeRamp();
    testGaugeThresholdCap();
    testGaugeMultiDraft();
    testGaugeCheckpointLife();
    testGaugeStopsWhenNothingDraftable();
    testGaugeLastLevels();
    testFillGauge();
    testGrantAndPick();
    testRollDraft();
    testEffectsAtLevels();

    std::printf("core tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
