#pragma once

// Static augment table + tuning constants. No Geode headers: src/core is
// compiled on the host by scripts/test.ps1 as well as into the mod.

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
struct AugmentDef {
    std::string id;
    // Display name, Korean. Drawn with the mod's own font (GD's fonts have no
    // Hangul), so never feed it to a bigFont/goldFont/chatFont label.
    std::string name;
    int maxLevel = 1;
    // Shown on the card when drafting level 1. Built from tune:: at startup
    // so the numbers on the card and in the code cannot drift apart.
    std::string initialDesc;
    // Shown when drafting level 2+ (same text for every level-up). Empty
    // when maxLevel == 1.
    std::string levelUpDesc;

    std::string const& describe(int level) const;
};

std::vector<AugmentDef> const& allAugments();
AugmentDef const* findAugment(std::string const& id);
// Display name for an id; falls back to the id itself.
std::string augmentName(std::string const& id);

// Tuning constants shared by hooks and by the card descriptions.
namespace tune {
    constexpr float NoclipSeconds = 1.5f;
    // Game speed at slow-mo level n: 1 - SlowMoStep * n.
    constexpr float SlowMoStep = 0.05f;
    // Hazard hitbox shrink at hazard-hitbox level n: HazardStep * n.
    constexpr float HazardStep = 0.05f;
    // Player hitbox shrink while in wave mode at wave-hitbox level n: WaveStep * n.
    constexpr float WaveStep = 0.10f;
    // Nerve (single level): both shrinks grow to shrink * (1 + NerveMult
    // * progress), progress = current level percent / 100.
    constexpr float NerveMult = 1.f;
    // Floor for either hitbox scale. wave-hitbox Lv5 boosted by nerve at
    // 100 % would otherwise shrink the box to nothing (0.5 * 2 = 1.0).
    constexpr float MinHitboxScale = 0.2f;
    // Cards per draft, with and without draft-count.
    constexpr int DefaultDraftCards = 3;
    constexpr int DraftCountCards = 4;
    // Cat at level n removes CatBaseCount + CatCountStep * (n - 1) hazards in
    // view every CatBaseInterval - CatIntervalStep * (n - 1) seconds.
    constexpr int CatBaseCount = 5;
    constexpr int CatCountStep = 1;
    constexpr float CatBaseInterval = 4.f;
    constexpr float CatIntervalStep = 0.5f;
    constexpr float CatMinInterval = 0.5f;
    // Draft gauge: a death charges by the percent reached, plus every percent
    // of new best times NewBestBonusMult. A draft costs GaugeThresholdStart
    // and each gauge-earned draft raises the next cost by GaugeThresholdStep
    // (the `debug-threshold` setting overrides the ramp in debug mode).
    constexpr float GaugeThresholdStart = 40.f;
    constexpr float GaugeThresholdStep = 10.f;
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
    constexpr float MissileRadiusStep = 0.5f * BlockUnits;
    // Berserker: every destroyed hazard (cat, missile, or a berserk smash
    // itself) rolls BerserkChanceBase + BerserkChanceStep * (level - 1) to
    // open a BerserkSeconds window in which touching a hazard destroys it
    // instead of dying. The window does not grow with the level.
    constexpr float BerserkChanceBase = 0.03f;
    constexpr float BerserkChanceStep = 0.01f;
    constexpr float BerserkSeconds = 2.f;
}

} // namespace augment
