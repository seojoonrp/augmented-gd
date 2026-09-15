// M2: "AUG" button on the level info screen that starts (or previews) a run.

#include "../core/AugmentManager.hpp"
#include "../ui/AugmentDraftPopup.hpp"

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
        auto levelName = std::string(m_level->m_levelName);
        int levelID = m_level->m_levelID.value();
        log::info("AUG pressed on '{}' (id {})", levelName, levelID);

        std::string body;
        if (mgr.isRunFor(levelID)) {
            body = fmt::format(
                "Run in progress on <cy>{}</c>\n"
                "Deaths: <cr>{}</c>   Augments: <cg>{}</c>\n\n"
                "<cl>Continue</c> keeps your augments.\n<cr>Restart</c> starts a fresh run.",
                levelName, mgr.deaths(), mgr.augments().size()
            );
        }
        else {
            body = fmt::format(
                "Start an augmented run on <cy>{}</c>?\n\n"
                "Die, draft an augment, get stronger.\n"
                "Augments persist until you beat the level.",
                levelName
            );
        }

        Ref<GJGameLevel> level = m_level;
        bool const continuing = mgr.isRunFor(levelID);
        createQuickPopup(
            "Augment Mode",
            body,
            continuing ? "Restart" : "Preview",
            continuing ? "Continue" : "Start",
            [this, level, continuing](FLAlertLayer*, bool btn2) {
                auto& mgr = AugmentManager::get();
                if (continuing) {
                    if (!btn2) mgr.startRun(level);
                    this->onPlay(nullptr);
                    return;
                }
                if (btn2) {
                    mgr.startRun(level);
                    this->onPlay(nullptr);
                }
                else {
                    // Preview: just show the draft UI, don't apply anything.
                    auto popup = AugmentDraftPopup::create(mgr.rollDraft(3), [](std::string const& id) {
                        log::info("Preview pick (not applied): {}", id);
                    });
                    if (popup) popup->show();
                }
            }
        );
    }
};
