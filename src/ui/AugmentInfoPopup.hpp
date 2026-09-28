#pragma once

#include "../core/AugmentDef.hpp"

#include <Geode/ui/Popup.hpp>

namespace augment {

// One augment's card, centred and enlarged, opened from a tile of the pause
// menu's run summary: the draft card's look with the text of what it does at
// the level held (AugmentDef::describeAt), `Lv N` in the footer (gold once
// maxed) and that many stars. The close button or Esc goes back.
class AugmentInfoPopup : public geode::Popup {
public:
    static AugmentInfoPopup* create(AugmentDef const& def, int level);

protected:
    bool init(AugmentDef const& def, int level);
};

} // namespace augment
