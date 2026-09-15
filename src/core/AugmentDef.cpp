#include "AugmentDef.hpp"

namespace augment {

std::vector<AugmentDef> const& allAugments() {
    // TODO: replace dummies with the real ~10 augment pool.
    static std::vector<AugmentDef> const pool = {
        { "dummy-jump",   "Feather",   "Placeholder: +{lvl} jump boost.",     3 },
        { "dummy-shield", "Shield",    "Placeholder: {lvl} extra hit(s).",    3 },
        { "dummy-speed",  "Slow-Mo",   "Placeholder: -{lvl}0% game speed.",   3 },
    };
    return pool;
}

AugmentDef const* findAugment(std::string const& id) {
    for (auto const& def : allAugments()) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

std::string describeAtLevel(AugmentDef const& def, int level) {
    std::string out = def.description;
    std::string const key = "{lvl}";
    std::string const val = std::to_string(level);
    for (auto pos = out.find(key); pos != std::string::npos; pos = out.find(key, pos + val.size())) {
        out.replace(pos, key.size(), val);
    }
    return out;
}

} // namespace augment
