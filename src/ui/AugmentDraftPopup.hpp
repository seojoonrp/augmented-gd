#pragma once

#include "../core/AugmentDef.hpp"

#include <Geode/ui/Popup.hpp>

#include <functional>
#include <string>
#include <vector>

namespace augment {

// Modal "pick one of three" popup. Has no close button and ignores Esc/back:
// the only way out is to pick a card.
class AugmentDraftPopup : public geode::Popup {
public:
    using PickCallback = std::function<void(std::string const& augmentID)>;

    // `choices` are shown left-to-right. `onPick` is invoked after the popup
    // has already been removed from the scene, so it must not touch the popup.
    static AugmentDraftPopup* create(std::vector<AugmentDef const*> choices, PickCallback onPick);

protected:
    bool init(std::vector<AugmentDef const*> choices, PickCallback onPick);

    void keyBackClicked() override {}
    void onClose(cocos2d::CCObject*) override {}

    cocos2d::CCNode* createCard(AugmentDef const& def, int currentLevel);
    void onCard(cocos2d::CCObject* sender);

    std::vector<AugmentDef const*> m_choices;
    PickCallback m_onPick;
};

} // namespace augment
