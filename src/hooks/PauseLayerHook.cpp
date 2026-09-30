// Pause menu of a run level: the round AUG button takes the practice button's
// spot and opens the run summary. Augments are off in practice, so a run has no
// use for it, but it stays if you're already in practice mode so you can get out.

#include "../game/AugmentManager.hpp"
#include "../game/LevelSession.hpp"
#include "../game/RunSummary.hpp"
#include "../ui/AugButton.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;
using namespace augment;

namespace {
    // GD's practice button height, for when there is none to match
    constexpr float kFallbackHeight = 57.f;
}

class $modify(AugPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto s = AugmentManager::get().session();
        if (!s || !s->runLevel()) return;
        auto menu = this->getChildByID("center-button-menu");
        if (!menu) {
            log::warn("PauseLayer: no center-button-menu, run button not added");
            return;
        }

        auto practice = menu->getChildByID("practice-button");
        auto play = menu->getChildByID("play-button");
        auto spr = augButtonSprite(CircleBaseSize::Big);
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
            // the "Practice" hint arrow for new players would point at nothing
            if (auto arrow = this->getChildByID("practice-arrow-text")) arrow->setVisible(false);
        }
        menu->updateLayout();
    }

    void onRunInfo(CCObject*) {
        summary::open();
    }
};
