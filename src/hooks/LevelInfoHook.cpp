// The mod's button on the level info screen: starts a run, or asks whether to
// continue / restart the one in progress. A round green button with the mod's
// mark in it (resources/ui/aug-logo.png, the user's own art).

#include "../game/AugmentManager.hpp"
#include "../ui/RunResumePopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

using namespace geode::prelude;
using namespace augment;

namespace {
    // The button rides beside GD's copy button, on its row (the user's ask,
    // 2026-09-27): one button width to its right, so the two read as a pair
    // without ours joining the column and being laid out by it.
    constexpr float kGapFromCopy = 56.f;
    // No copy button on this level (node-ids only names one when GD made it):
    // stand this far right of the column instead, at mid-height.
    constexpr float kFallbackX = 78.f;
    // On top of the 65 % of the circle the glyph is fitted to. 1.2 was a
    // round too big once the file's own padding came down to 2 %, so the
    // user settled on the plain fit.
    constexpr float kLogoScale = 1.0f;
}

class $modify(AugLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        auto logo = CCSprite::create("aug-logo.png"_spr);
        if (!logo) log::warn("AUG button: no logo sprite, the circle will be empty");
        auto spr = CircleButtonSprite::create(logo, CircleBaseColor::Green, CircleBaseSize::Medium);
        spr->setTopRelativeScale(kLogoScale);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AugLevelInfoLayer::onAugment));
        btn->setID("augment-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("augment-menu"_spr);
        menu->setPosition(this->augmentButtonSpot());
        menu->addChild(btn);
        this->addChild(menu);
        log::info("AUG button: circle at ({:.0f}, {:.0f})", menu->getPositionX(), menu->getPositionY());

        return true;
    }

    // Beside GD's copy button, in this layer's space. node-ids names that
    // button inside `left-side-menu`; without either (an old node-ids, or a
    // level GD gives no copy button) the fallback is the column's own row.
    CCPoint augmentButtonSpot() {
        float midY = CCDirector::get()->getWinSize().height / 2;
        auto column = this->getChildByID("left-side-menu");
        if (!column) {
            log::info("AUG button: no left-side-menu, using the fallback spot");
            return { kFallbackX, midY };
        }
        auto copyBtn = column->getChildByID("copy-button");
        if (!copyBtn) {
            log::info("AUG button: no copy-button on this level, using the column's mid-height");
            return { column->getPositionX() + kGapFromCopy, midY };
        }
        auto spot = this->convertToNodeSpace(column->convertToWorldSpace(copyBtn->getPosition()));
        return { spot.x + kGapFromCopy, spot.y };
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
