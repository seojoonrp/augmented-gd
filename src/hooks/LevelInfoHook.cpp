// "AUG" button on the level info screen: starts a run, or asks whether to
// continue / restart the one in progress.

#include "../game/AugmentManager.hpp"
#include "../ui/RunResumePopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>

using namespace geode::prelude;
using namespace augment;

class $modify(AugLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        auto spr = ButtonSprite::create("AUG", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AugLevelInfoLayer::onAugment));
        btn->setID("augment-button"_spr);

        // node-ids gives us the left column of buttons; fall back to our own
        // menu if it's missing for some reason.
        if (auto menu = this->getChildByID("left-side-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        else {
            auto fallback = CCMenu::create();
            fallback->setID("augment-menu"_spr);
            fallback->setPosition({ 30.f, 30.f });
            fallback->addChild(btn);
            this->addChild(fallback);
        }

        return true;
    }

    void onAugment(CCObject*) {
        auto& mgr = AugmentManager::get();
        int levelID = m_level->m_levelID.value();
        log::info("AUG pressed on '{}' (id {})", std::string(m_level->m_levelName), levelID);

        // No run on this level yet: start one straight away.
        if (!mgr.isRunFor(levelID)) {
            mgr.startRun(m_level);
            this->onPlay(nullptr);
            return;
        }

        // The popup sits on top of this layer, so `this` outlives it.
        Ref<GJGameLevel> level = m_level;
        auto popup = RunResumePopup::create(
            [this, level] {
                AugmentManager::get().startRun(level);
                this->onPlay(nullptr);
            },
            [this] { this->onPlay(nullptr); }
        );
        if (popup) popup->show();
    }
};
