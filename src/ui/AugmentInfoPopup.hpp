#pragma once

#include "../core/AugmentDef.hpp"

#include <Geode/ui/Popup.hpp>

namespace augment {

// One augment's card at the level held, opened from a run summary tile.
class AugmentInfoPopup : public geode::Popup {
public:
    static AugmentInfoPopup* create(AugmentDef const& def, int level);

protected:
    bool init(AugmentDef const& def, int level);
};

} // namespace augment
