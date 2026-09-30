#pragma once

#include "../core/AugmentDef.hpp"

#include <Geode/ui/Popup.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace augment {

// Pick one of N, no way out but picking. Animations run from visit() on a
// real clock: the director is paused during a draft, so actions don't run.
class AugmentDraftPopup : public geode::Popup {
public:
    using PickCallback = std::function<void(std::string const& augmentID)>;

    // onPick runs after the popup is gone, so it mustn't touch it
    static AugmentDraftPopup* create(std::vector<AugmentDef const*> choices, PickCallback onPick);

protected:
    bool init(std::vector<AugmentDef const*> choices, PickCallback onPick);

    void keyBackClicked() override {}
    void onClose(cocos2d::CCObject*) override {}
    void visit() override;

    cocos2d::CCNode* createCard(AugmentDef const& def, int currentLevel);
    void onCard(cocos2d::CCObject* sender);
    void stepReveal();
    // hovered card rises a bit, its hit area stays put
    void stepHover();
    // true once the popup has closed
    bool stepPick();
    void finishPick();

    struct RevealCard {
        cocos2d::CCNode* item = nullptr;    // fixed footprint / hit area
        cocos2d::CCNode* visual = nullptr;
        cocos2d::CCPoint from;  // row centre, item space
        cocos2d::CCPoint to;    // resting spot, item space
        float fromRotation = 0.f;
        float lift = 0.f;       // hover rise
        cocos2d::CCPoint pickFrom;   // hover lift included
        float pickFromScale = 1.f;
    };

    std::vector<AugmentDef const*> m_choices;
    PickCallback m_onPick;
    std::vector<RevealCard> m_reveal;
    std::chrono::steady_clock::time_point m_revealStart;
    std::chrono::steady_clock::time_point m_lastHover;
    std::chrono::steady_clock::time_point m_pickStart;
    bool m_revealing = false;
    bool m_picking = false;
    int m_hovered = -1;
    int m_picked = -1;
    float m_cardScale = 1.f;
    // lives in the chosen card's holder
    cocos2d::CCDrawNode* m_pickRing = nullptr;
};

} // namespace augment
