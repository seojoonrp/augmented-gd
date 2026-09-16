#pragma once

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
    // Stubs: in the table and draftable, but no hook reacts to them yet.
    constexpr char const* WaveHitbox   = "wave-hitbox";
    constexpr char const* Nerve        = "nerve";
    constexpr char const* DraftCount   = "draft-count";
}

// Static definition of one augment. Runtime state (current level, etc.)
// lives in AugmentManager, not here.
struct AugmentDef {
    std::string id;
    // Display name, Korean. Drawn with the mod's own font (GD's fonts have no
    // Hangul), so never feed it to a bigFont/goldFont/chatFont label.
    std::string name;
    int maxLevel = 1;
    // Shown on the card when drafting level 1.
    std::string initialDesc;
    // Shown when drafting level 2+ (same text for every level-up). Empty
    // when maxLevel == 1.
    std::string levelUpDesc;
    // true: picking records the level but nothing in the game changes.
    bool stub = false;

    std::string const& describe(int level) const;
};

std::vector<AugmentDef> const& allAugments();
AugmentDef const* findAugment(std::string const& id);
// Display name for an id; falls back to the id itself.
std::string augmentName(std::string const& id);

// Tuning constants shared by hooks. The card descriptions in AugmentDef.cpp
// quote these numbers as literal text, so change both together.
namespace tune {
    constexpr float NoclipSeconds = 1.5f;
    // Game speed at slow-mo level n: 1 - SlowMoStep * n.
    constexpr float SlowMoStep = 0.05f;
    // Hazard hitbox scale at hazard-hitbox level n: 1 - HazardStep * n.
    constexpr float HazardStep = 0.05f;
    // (stub) Player hitbox scale in wave mode at wave-hitbox level n: 1 - WaveStep * n.
    constexpr float WaveStep = 0.10f;
    // (stub) Nerve level 1..2: the two hitbox shrinks grow to
    // shrink * (1 + NerveMult[level - 1] * progress), progress in 0..1.
    constexpr float NerveMult[2] = { 1.f, 1.5f };
    // (stub) Cards per draft once draft-count is owned.
    constexpr int DraftCountCards = 4;
}

} // namespace augment
