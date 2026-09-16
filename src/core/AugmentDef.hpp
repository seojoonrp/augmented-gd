#pragma once

#include <string>
#include <vector>

namespace augment {

// Stable IDs used by the hooks to look up run state.
namespace ids {
    constexpr char const* Shield     = "shield";
    constexpr char const* SlowMo     = "slowmo";
    constexpr char const* Checkpoint = "checkpoint";
    constexpr char const* Foresight  = "foresight";
    constexpr char const* Unmirror   = "unmirror";
    constexpr char const* Blunt      = "blunt";
}

// Static definition of one augment. Runtime state (current level, etc.)
// lives in AugmentManager, not here.
struct AugmentDef {
    std::string id;
    std::string name;
    // One entry per level (index 0 == level 1). Size doubles as max level.
    std::vector<std::string> descriptions;

    int maxLevel() const { return static_cast<int>(descriptions.size()); }
    std::string const& describe(int level) const;
};

std::vector<AugmentDef> const& allAugments();
AugmentDef const* findAugment(std::string const& id);

// Tuning constants shared by hooks and descriptions.
namespace tune {
    constexpr float NoclipSeconds = 3.f;
    // Game speed at slow-mo level 1..3.
    constexpr float SlowMoScale[3] = { 0.93f, 0.86f, 0.79f };
    // Hazard hitbox size at blunt level 1..3. Deliberately coarse for testing;
    // the real numbers will be smaller steps.
    constexpr float BluntScale[3] = { 0.8f, 0.6f, 0.4f };
}

} // namespace augment
