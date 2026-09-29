// The pause menu of a run level: the mod's round button (the AUG button's
// face) takes the practice button's place in node-ids' `center-button-menu`
// and opens the run summary (RunInfoPopup). A practice attempt is not a run
// attempt — augments are off in it — so a run has no use for the practice
// button; it stays only when the player is already in practice mode, or
// they could not get out.

#include "../game/AugmentManager.hpp"
#include "../game/LevelSession.hpp"
#include "../game/RunSummary.hpp"
#include "../ui/AugButton.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;
using namespace augment;

namespace {
    // GD's practice button height, for when there is none to match.
    constexpr float kFallbackHeight = 57.f;
}

class $modify(AugPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto s = AugmentManager::get().session();
        if (!s || !s->runLevel()) return;
        auto menu = this->getChildByID("center-button-menu");
        if (!menu) {
            log::info("Pause: no center-button-menu, run button not added");
            return;
        }

        auto practice = menu->getChildByID("practice-button");
        auto play = menu->getChildByID("play-button");
        auto spr = augButtonSprite(CircleBaseSize::Big);
        // As tall as the button it stands in for.
        float const height = practice ? practice->getScaledContentSize().height : kFallbackHeight;
        spr->setScale(height / spr->getContentSize().height);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AugPauseLayer::onRunInfo));
        btn->setID("run-info-button"_spr);

        bool const inPractice = s->layer()->m_isPracticeMode;
        if (practice) menu->insertBefore(btn, practice);
        else if (play) menu->insertBefore(btn, play);
        else menu->addChild(btn);
        if (practice && !inPractice) {
            practice->removeFromParent();
            // GD's "Practice" arrow for new players points at the button.
            if (auto arrow = this->getChildByID("practice-arrow-text")) arrow->setVisible(false);
        }
        menu->updateLayout();
        log::info(
            "Pause: run button added ({}, {:.0f} pt tall)",
            !practice ? "no practice button" : (inPractice ? "beside practice, in practice mode" : "in place of practice"),
            height
        );
    }

    void onRunInfo(CCObject*) {
        summary::open();
    }
};
