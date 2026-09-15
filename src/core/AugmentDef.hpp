#pragma once

#include <string>
#include <vector>

namespace augment {

// Static definition of one augment. Runtime state (current level, etc.)
// lives in AugmentManager, not here.
struct AugmentDef {
    std::string id;
    std::string name;
    std::string description; // may contain "{lvl}" which is replaced with the level being shown
    int maxLevel = 3;
};

// The full augment pool. Dummy entries for now; real augments come later.
std::vector<AugmentDef> const& allAugments();

AugmentDef const* findAugment(std::string const& id);

// Description text with "{lvl}" substituted.
std::string describeAtLevel(AugmentDef const& def, int level);

} // namespace augment
