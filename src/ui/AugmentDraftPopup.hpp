#pragma once

#include "../core/AugmentDef.hpp"

#include <Geode/ui/Popup.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace augment {

// Modal "pick one of N" popup. Has no close button and ignores Esc/back:
// the only way out is to pick a card. The cards fan out from the centre when
// the popup opens; the reveal is driven from visit() with a real clock
// because the director is paused during a draft, which freezes cocos actions.
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
    void visit() override;

    // The card's visual (full-size, scaled to fit); the menu item wrapping it
    // keeps the final position and size so the touch area never moves.
    cocos2d::CCNode* createCard(AugmentDef const& def, int currentLevel);
    void onCard(cocos2d::CCObject* sender);
    void stepReveal();

    struct RevealCard {
        cocos2d::CCNode* visual = nullptr;
        cocos2d::CCPoint from;  // menu-row centre, in the item's coordinates
        cocos2d::CCPoint to;    // resting position, in the item's coordinates
        float fromRotation = 0.f;
    };

    std::vector<AugmentDef const*> m_choices;
    PickCallback m_onPick;
    std::vector<RevealCard> m_reveal;
    std::chrono::steady_clock::time_point m_revealStart;
    bool m_revealing = false;
    float m_cardScale = 1.f;
};

} // namespace augment
