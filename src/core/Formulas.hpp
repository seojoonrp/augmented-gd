#pragma once

// Level → effect. Pure functions of augment levels so the hooks, the HUD and
// the host tests all compute the same numbers.

#include "AugmentDef.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace augment::formula {

// Game speed at a slow-mo level; 1.0 when unowned.
inline float slowMoScale(int slowMoLevel) {
    return 1.f - tune::SlowMoStep * static_cast<float>(slowMoLevel);
}

// What nerve multiplies both hitbox shrinks by. `progress` is level percent
// / 100, clamped to 0..1. 1.0 when nerve is unowned.
inline float nerveBoost(int nerveLevel, float progress) {
    if (nerveLevel <= 0) return 1.f;
    // NerveMult has one entry per nerve level; clamp in case maxLevel grows
    // before the table does.
    int idx = std::clamp(nerveLevel, 1, static_cast<int>(std::size(tune::NerveMult))) - 1;
    return 1.f + tune::NerveMult[idx] * std::clamp(progress, 0.f, 1.f);
}

// shrink is "how much is cut off", so 0 = untouched; floored at MinHitboxScale.
inline float shrinkToScale(float shrink) {
    return std::clamp(1.f - shrink, tune::MinHitboxScale, 1.f);
}

inline float hazardScale(int hazardLevel, int nerveLevel, float progress) {
    float shrink = tune::HazardStep * static_cast<float>(hazardLevel);
    return shrinkToScale(shrink * nerveBoost(nerveLevel, progress));
}

// Applies while the player is in wave mode only; the hook checks that.
inline float waveScale(int waveLevel, int nerveLevel, float progress) {
    float shrink = tune::WaveStep * static_cast<float>(waveLevel);
    return shrinkToScale(shrink * nerveBoost(nerveLevel, progress));
}

// Hazards removed per sweep; 0 when unowned.
inline int catCount(int catLevel) {
    if (catLevel <= 0) return 0;
    return tune::CatBaseCount + tune::CatCountStep * (catLevel - 1);
}

// Seconds between sweeps; 0 when unowned.
inline float catInterval(int catLevel) {
    if (catLevel <= 0) return 0.f;
    return std::max(tune::CatMinInterval, tune::CatBaseInterval - tune::CatIntervalStep * static_cast<float>(catLevel - 1));
}

// Game speed while the brake is held; independent of slow-mo.
inline float brakeScale() {
    return 1.f - tune::BrakeCut;
}

// Real seconds of braking per attempt; 0 when unowned.
inline float brakeBudget(int brakeLevel) {
    if (brakeLevel <= 0) return 0.f;
    return tune::BrakeSecondsPerLevel * static_cast<float>(brakeLevel);
}

inline std::size_t draftCardCount(bool hasDraftCount) {
    return static_cast<std::size_t>(hasDraftCount ? tune::DraftCountCards : tune::DefaultDraftCards);
}

} // namespace augment::formula
