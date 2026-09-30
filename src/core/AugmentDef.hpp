#pragma once

// Augment table + tuning. No Geode headers in src/core: it also builds on the
// host for the tests.

#include "Lang.hpp"

#include <string>
#include <vector>

namespace augment {

// keep these stable, the card art is named after them
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

// Static data only; levels live in RunState.
// All text needs the mod fonts (GD's have no Hangul), never a bigFont/goldFont/chatFont label.
struct AugmentDef {
    std::string id;
    LocalText name;
    int maxLevel = 1;
    // built from tune:: so the card can't drift from the code
    LocalText initialDesc;
    // empty when maxLevel == 1
    LocalText levelUpDesc;
    // replaces levelUpDesc from `lateFrom` on (cat's last levels only add count)
    LocalText lateLevelUpDesc{};
    int lateFrom = 0;

    // draft card text for drafting `level`
    std::string const& describe(int level, Lang lang) const;
    // what it does at `level`, with that level's numbers (pause menu detail card)
    std::string describeAt(int level, Lang lang) const;
};

std::vector<AugmentDef> const& allAugments();
AugmentDef const* findAugment(std::string const& id);
// falls back to the id
std::string augmentName(std::string const& id, Lang lang);

namespace tune {
    constexpr float NoclipSeconds = 1.5f;
    // per level: speed 1 - SlowMoStep * n, shrink HazardStep * n / WaveStep * n
    constexpr float SlowMoStep = 0.05f;
    constexpr float HazardStep = 0.05f;
    constexpr float WaveStep = 0.10f;
    // nerve: shrink * (1 + NerveBase + NerveSlope * progress), progress in 0..1.
    // the card text shows NerveSlope as "X/2".
    constexpr float NerveBase = 0.2f;
    constexpr float NerveSlope = 0.5f;
    // wave Lv7 + nerve would leave almost no hitbox otherwise
    constexpr float MinHitboxScale = 0.2f;
    constexpr int DefaultDraftCards = 3;
    constexpr int DraftCountCards = 4;
    // cat: count and interval step each level until CatLateLevel, after that
    // only CatLateCountStep more hazards per level
    constexpr int CatBaseCount = 5;
    constexpr int CatCountStep = 2;
    constexpr float CatBaseInterval = 4.f;   // seconds
    constexpr float CatIntervalStep = 0.5f;
    constexpr float CatMinInterval = 0.5f;
    constexpr int CatLateLevel = 6;
    constexpr int CatLateCountStep = 4;
    // draft cost ramp: 20, 20, 20, 25, 25, 25 ... 70, then flat
    constexpr float GaugeThresholdStart = 20.f;
    constexpr float GaugeThresholdStep = 5.f;
    constexpr int GaugeThresholdEvery = 3;
    constexpr float GaugeThresholdMax = 70.f;
    constexpr float NewBestBonusMult = 1.f;
    // brake: absolute cut (ignores slow-mo), real seconds per level per attempt
    constexpr float BrakeCut = 0.6f;
    constexpr float BrakeSecondsPerLevel = 7.f;
    // one grid block in object-layer units
    constexpr float BlockUnits = 30.f;
    constexpr float MissileBaseInterval = 6.f;   // seconds
    constexpr float MissileIntervalStep = 0.5f;
    constexpr float MissileMinInterval = 1.f;
    constexpr float MissileBaseRadius = 3.f * BlockUnits;
    constexpr float MissileRadiusStep = 1.f * BlockUnits;
    // berserker: every destroyed hazard rolls the chance; the window doesn't scale
    constexpr float BerserkChanceBase = 0.03f;
    constexpr float BerserkChanceStep = 0.015f;
    constexpr float BerserkSeconds = 2.5f;
}

} // namespace augment
