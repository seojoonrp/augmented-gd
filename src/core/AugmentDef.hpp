#pragma once

// Static augment table + tuning constants. No Geode headers: src/core is
// compiled on the host by scripts/test.ps1 as well as into the mod.

#include "Lang.hpp"

#include <string>
#include <vector>

namespace augment {

// Stable IDs used by the hooks to look up run state. These match the "코드"
// column of the design table (docs/DESIGN.md).
namespace ids {
    constexpr char const* Shield       = "shield";
    constexpr char const* SlowMo       = "slow-mo";
    constexpr char const* StartPos     = "startpos";
    constexpr char const* Foresight    = "foresight";
    constexpr char const* Unmirror     = "unmirror";
    constexpr char const* HazardHitbox = "hazard-hitbox";
    constexpr char const* WaveHitbox   = "wave-hitbox";
    constexpr char const* Nerve        = "nerve";
    constexpr char const* DraftCount   = "draft-count";
    constexpr char const* Cat          = "cat";
    constexpr char const* Brake        = "brake";
    constexpr char const* Missile      = "missile";
    constexpr char const* Berserk      = "berserker";
}

// Static definition of one augment. Runtime state (current level, etc.)
// lives in RunState, not here.
// Every text comes in English and Korean (Lang.hpp) and is drawn with the
// mod's own fonts (GD's fonts have no Hangul), so never feed one to a
// bigFont/goldFont/chatFont label.
struct AugmentDef {
    std::string id;
    // Display name.
    LocalText name;
    int maxLevel = 1;
    // Shown on the card when drafting level 1. Built from tune:: at startup
    // so the numbers on the card and in the code cannot drift apart.
    LocalText initialDesc;
    // Shown when drafting level 2+ (same text for every level-up). Empty
    // when maxLevel == 1.
    LocalText levelUpDesc;
    // Shown instead of levelUpDesc when drafting `lateFrom` or later, for an
    // augment whose last levels grow differently (cat: count only). Empty
    // and 0 otherwise.
    LocalText lateLevelUpDesc{};
    int lateFrom = 0;

    // Draft card text for drafting `level`: initial at 1, level-up after
    // (the late one from lateFrom on).
    std::string const& describe(int level, Lang lang) const;
    // What the augment does *at* `level`, with that level's numbers (the
    // pause menu's detail card): "Get 3 shields every attempt." Level 1
    // (and any single-level augment) is the initial text.
    std::string describeAt(int level, Lang lang) const;
};

std::vector<AugmentDef> const& allAugments();
AugmentDef const* findAugment(std::string const& id);
// Display name for an id; falls back to the id itself.
std::string augmentName(std::string const& id, Lang lang);

// Tuning constants shared by hooks and by the card descriptions.
namespace tune {
    constexpr float NoclipSeconds = 1.5f;
    // Game speed at slow-mo level n: 1 - SlowMoStep * n.
    constexpr float SlowMoStep = 0.05f;
    // Hazard hitbox shrink at hazard-hitbox level n: HazardStep * n.
    constexpr float HazardStep = 0.05f;
    // Player hitbox shrink while in wave mode at wave-hitbox level n: WaveStep * n.
    constexpr float WaveStep = 0.10f;
    // Nerve (single level): at X % progress both shrinks are (120 + X/2) %
    // of themselves, i.e. shrink * (1 + NerveBase + NerveSlope * progress),
    // progress = current level percent / 100 (user, 2026-09-30; it was
    // 1 + progress). The card text quotes NerveSlope as "X/2".
    constexpr float NerveBase = 0.2f;
    constexpr float NerveSlope = 0.5f;
    // Floor for either hitbox scale. wave-hitbox Lv7 boosted by nerve would
    // otherwise shrink the box to almost nothing (0.7 * 1.2 = 0.84 at 0 %).
    constexpr float MinHitboxScale = 0.2f;
    // Cards per draft, with and without draft-count.
    constexpr int DefaultDraftCards = 3;
    constexpr int DraftCountCards = 4;
    // Cat at level n removes CatBaseCount + CatCountStep * (n - 1) hazards in
    // view every CatBaseInterval - CatIntervalStep * (n - 1) seconds, up to
    // CatLateLevel - 1. From CatLateLevel on a level adds CatLateCountStep
    // hazards and leaves the interval alone (user, 2026-09-30: Lv6-7 = +4).
    constexpr int CatBaseCount = 5;
    constexpr int CatCountStep = 2;   // was 1 (user, 2026-09-29)
    constexpr float CatBaseInterval = 4.f;
    constexpr float CatIntervalStep = 0.5f;
    constexpr float CatMinInterval = 0.5f;
    constexpr int CatLateLevel = 6;
    constexpr int CatLateCountStep = 4;
    // Draft gauge: a death charges by the whole percent reached, plus every
    // percent of new best times NewBestBonusMult. A draft costs
    // GaugeThresholdStart and every GaugeThresholdEvery gauge-earned drafts
    // raise the next cost by GaugeThresholdStep until GaugeThresholdMax,
    // where the ramp stops (20, 20, 20, 25, 25, 25 … 65, 65, 65, 70, 70 …
    // — user, 2026-09-30; 30, 30, 35, 35 … felt too slow).
    constexpr float GaugeThresholdStart = 20.f;
    constexpr float GaugeThresholdStep = 5.f;
    constexpr int GaugeThresholdEvery = 3;
    constexpr float GaugeThresholdMax = 70.f;
    constexpr float NewBestBonusMult = 1.f;
    // Brake: game speed 1 - BrakeCut while the key is held (slow-mo is
    // ignored, the cut is absolute), for BrakeSecondsPerLevel * level real
    // seconds per attempt.
    constexpr float BrakeCut = 0.6f;
    constexpr float BrakeSecondsPerLevel = 7.f;
    // One GD grid block, in object-layer units; the missile text quotes
    // its radius in blocks.
    constexpr float BlockUnits = 30.f;
    // Missile at level n: every MissileBaseInterval - MissileIntervalStep
    // * (n - 1) seconds a strike on a random hazard in view removes every
    // hazard within MissileBaseRadius + MissileRadiusStep * (n - 1) units.
    constexpr float MissileBaseInterval = 6.f;
    constexpr float MissileIntervalStep = 0.5f;
    constexpr float MissileMinInterval = 1.f;
    constexpr float MissileBaseRadius = 3.f * BlockUnits;
    constexpr float MissileRadiusStep = 1.f * BlockUnits;   // was 0.5 blocks (user, 2026-09-29)
    // Berserker: every destroyed hazard (cat, missile, or a berserk smash
    // itself) rolls BerserkChanceBase + BerserkChanceStep * (level - 1) to
    // open a BerserkSeconds window in which touching a hazard destroys it
    // instead of dying. The window does not grow with the level.
    constexpr float BerserkChanceBase = 0.03f;
    constexpr float BerserkChanceStep = 0.015f;   // was 0.01 (user, 2026-09-30)
    constexpr float BerserkSeconds = 2.5f;   // was 2 (user, 2026-09-29)
}

} // namespace augment
