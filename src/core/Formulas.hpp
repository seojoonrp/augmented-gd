#pragma once

// level -> effect, shared by the hooks, the HUD and the host tests.
// level 0 (unowned) gives the neutral value: 1 for scales, 0 otherwise.

#include "AugmentDef.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace augment::formula {

inline float slowMoScale(int slowMoLevel) {
    return 1.f - tune::SlowMoStep * static_cast<float>(slowMoLevel);
}

// 1.2 at the start of the level, 1.7 at the end
inline float nerveBoost(int nerveLevel, float progress) {
    if (nerveLevel <= 0) return 1.f;
    return 1.f + tune::NerveBase + tune::NerveSlope * std::clamp(progress, 0.f, 1.f);
}

// shrink = how much is cut off, 0 = untouched
inline float shrinkToScale(float shrink) {
    return std::clamp(1.f - shrink, tune::MinHitboxScale, 1.f);
}

inline float hazardScale(int hazardLevel, int nerveLevel, float progress) {
    float shrink = tune::HazardStep * static_cast<float>(hazardLevel);
    return shrinkToScale(shrink * nerveBoost(nerveLevel, progress));
}

// wave mode only, the hook checks that
inline float waveScale(int waveLevel, int nerveLevel, float progress) {
    float shrink = tune::WaveStep * static_cast<float>(waveLevel);
    return shrinkToScale(shrink * nerveBoost(nerveLevel, progress));
}

inline int catCount(int catLevel) {
    if (catLevel <= 0) return 0;
    int const early = std::min(catLevel, tune::CatLateLevel - 1);
    int const late = catLevel - early;
    return tune::CatBaseCount + tune::CatCountStep * (early - 1) + tune::CatLateCountStep * late;
}

// seconds; the late levels don't touch it
inline float catInterval(int catLevel) {
    if (catLevel <= 0) return 0.f;
    int const early = std::min(catLevel, tune::CatLateLevel - 1);
    return std::max(tune::CatMinInterval, tune::CatBaseInterval - tune::CatIntervalStep * static_cast<float>(early - 1));
}

inline float missileInterval(int missileLevel) {
    if (missileLevel <= 0) return 0.f;
    return std::max(tune::MissileMinInterval, tune::MissileBaseInterval - tune::MissileIntervalStep * static_cast<float>(missileLevel - 1));
}

// object-layer units
inline float missileRadius(int missileLevel) {
    if (missileLevel <= 0) return 0.f;
    return tune::MissileBaseRadius + tune::MissileRadiusStep * static_cast<float>(missileLevel - 1);
}

// 0..1, per destroyed hazard
inline float berserkChance(int berserkLevel) {
    if (berserkLevel <= 0) return 0.f;
    float chance = tune::BerserkChanceBase + tune::BerserkChanceStep * static_cast<float>(berserkLevel - 1);
    return std::clamp(chance, 0.f, 1.f);
}

inline float berserkSeconds(int berserkLevel) {
    if (berserkLevel <= 0) return 0.f;
    return tune::BerserkSeconds;
}

// independent of slow-mo
inline float brakeScale() {
    return 1.f - tune::BrakeCut;
}

// real seconds per attempt
inline float brakeBudget(int brakeLevel) {
    if (brakeLevel <= 0) return 0.f;
    return tune::BrakeSecondsPerLevel * static_cast<float>(brakeLevel);
}

inline std::size_t draftCardCount(bool hasDraftCount) {
    return static_cast<std::size_t>(hasDraftCount ? tune::DraftCountCards : tune::DefaultDraftCards);
}

} // namespace augment::formula
