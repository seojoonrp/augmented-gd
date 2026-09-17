#include "Augments.hpp"

#include <algorithm>

namespace augment {

bool Augment::handles(std::string const& id) const {
    return std::find(m_ids.begin(), m_ids.end(), id) != m_ids.end();
}

std::vector<std::unique_ptr<Augment>> makeAllAugments() {
    std::vector<std::unique_ptr<Augment>> all;
    all.push_back(makeShield());
    all.push_back(makeSlowMo());
    all.push_back(makeStartPos());
    all.push_back(makeForesight());
    all.push_back(makeUnmirror());
    all.push_back(makeHitboxScales());
    all.push_back(makeDraftCount());
    all.push_back(makeCat());
    return all;
}

} // namespace augment
