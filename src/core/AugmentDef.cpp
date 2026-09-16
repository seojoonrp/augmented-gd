#include "AugmentDef.hpp"

#include <algorithm>

namespace augment {

std::string const& AugmentDef::describe(int level) const {
    int idx = std::clamp(level, 1, this->maxLevel()) - 1;
    return descriptions[idx];
}

std::vector<AugmentDef> const& allAugments() {
    static std::vector<AugmentDef> const pool = {
        { ids::Shield, "Shield", {
            "1 shield per attempt. When it breaks, you are noclipped for 3s.",
            "2 shields per attempt. When one breaks, you are noclipped for 3s.",
            "3 shields per attempt. When one breaks, you are noclipped for 3s.",
        }},
        { ids::SlowMo, "Slow-Mo", {
            "Game runs at 93% speed. Press X to toggle.",
            "Game runs at 86% speed. Press X to toggle.",
            "Game runs at 79% speed. Press X to toggle.",
        }},
        { ids::Checkpoint, "Checkpoint", {
            "Press Z to place 1 checkpoint per attempt. Die once and respawn there.",
            "Press Z to place up to 2 checkpoints per attempt. Die once and respawn at the last one.",
            "Press Z to place up to 3 checkpoints per attempt. Die once and respawn at the last one.",
        }},
        { ids::Foresight, "Foresight", {
            "Shows hitboxes.",
        }},
        { ids::Unmirror, "Unmirror", {
            "Removes every mirror portal from the level.",
        }},
        { ids::Blunt, "Blunt", {
            "Hazard hitboxes shrink to 80% of their size.",
            "Hazard hitboxes shrink to 60% of their size.",
            "Hazard hitboxes shrink to 40% of their size.",
        }},
    };
    return pool;
}

AugmentDef const* findAugment(std::string const& id) {
    for (auto const& def : allAugments()) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

} // namespace augment
