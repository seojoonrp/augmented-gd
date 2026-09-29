// The mod's button on the level info screen: starts a run, or asks whether to
// continue / restart the one in progress. A round green button with the mod's
// mark in it (resources/ui/aug-logo.png, the user's own art).

#include "../game/AugmentManager.hpp"
#include "../ui/AugButton.hpp"
#include "../ui/RunResumePopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>

using namespace geode::prelude;
using namespace augment;

namespace {
    // Left of the difficulty face, level with GD's Play button (user,
    // 2026-09-29; it sat beside the copy button in the left column before).
    // The gap is from the face's edge; a difficulty name under the face
    // reaches ~5 pt further out, which the gap clears.
    constexpr float kGapFromDifficulty = 12.f;
    // Size: as wide as GD's buttons down the right edge (comments, like,
    // rate; node-ids' `right-side-menu`), measured from the first one found
    // (user, 2026-09-29: halfway to the Play button was a bit big). Drawn
    // from the Large circle (321 uhd px = 80 pt) so the texture only shrinks.
    constexpr char const* kSideButtons[] = { "info-button", "like-button", "rate-button", "leaderboards-button" };
    constexpr float kFallbackSideWidth = 45.f;   // unverified guess, used only if none is found
    // No difficulty sprite to stand beside: roughly where it would put us.
    constexpr float kFallbackX = 122.f;
    // On top of the 65 % of the circle the glyph is fitted to. 1.2 was a
    // round too big once the file's own padding came down to 2 %, so the
    // user settled on the plain fit.
    constexpr float kLogoScale = 1.0f;

    // Set while our own button runs GD's onPlay, so the onPlay hook can tell
    // it from GD's Play button.
    bool g_playingAsRun = false;
}

class $modify(AugLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        CCNode* playBtn = nullptr;
        if (m_playBtnMenu) {
            playBtn = m_playBtnMenu->getChildByID("play-button");
            if (!playBtn) playBtn = m_playBtnMenu->getChildByType<CCMenuItemSpriteExtra>(0);
        }
        char const* sideFrom = nullptr;
        float const width = this->sideButtonWidth(sideFrom);

        auto spr = augButtonSprite(CircleBaseSize::Large, kLogoScale);
        spr->setScale(width / spr->getContentSize().width);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AugLevelInfoLayer::onAugment));
        btn->setID("augment-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("augment-menu"_spr);
        menu->setPosition(this->augmentButtonSpot(playBtn, width / 2));
        menu->addChild(btn);
        this->addChild(menu);
        log::info(
            "AUG button: {:.0f} pt wide (as {}), centre ({:.0f}, {:.0f}){}",
            width, sideFrom ? sideFrom : "the fallback, no side button found",
            menu->getPositionX(), menu->getPositionY(), playBtn ? "" : ", play button not found"
        );

        return true;
    }

    // Width of GD's buttons down the right edge; `from` names the one measured.
    float sideButtonWidth(char const*& from) {
        if (auto menu = this->getChildByID("right-side-menu")) {
            for (auto id : kSideButtons) {
                if (auto btn = menu->getChildByID(id)) {
                    from = id;
                    return btn->getScaledContentSize().width * menu->getScale();
                }
            }
        }
        from = nullptr;
        return kFallbackSideWidth;
    }

    // Left of the difficulty face, at the Play button's height, in this
    // layer's space.
    CCPoint augmentButtonSpot(CCNode* playBtn, float radius) {
        auto const winSize = CCDirector::get()->getWinSize();
        float y = winSize.height * 0.66f;
        if (playBtn && playBtn->getParent()) {
            y = this->convertToNodeSpace(playBtn->getParent()->convertToWorldSpace(playBtn->getPosition())).y;
        }
        else {
            log::info("AUG button: no play button, using a fixed height");
        }
        if (!m_difficultySprite || !m_difficultySprite->getParent()) {
            log::info("AUG button: no difficulty sprite, using the fallback x");
            return { kFallbackX, y };
        }
        auto const box = m_difficultySprite->boundingBox();
        auto const left = this->convertToNodeSpace(
            m_difficultySprite->getParent()->convertToWorldSpace({ box.getMinX(), box.getMidY() })
        );
        return { left.x - kGapFromDifficulty - radius, y };
    }

    void onAugment(CCObject*) {
        auto& mgr = AugmentManager::get();
        int levelID = m_level->m_levelID.value();
        log::info("AUG pressed on '{}' (id {})", std::string(m_level->m_levelName), levelID);

        // No run on this level yet: start one straight away.
        if (!mgr.isRunFor(levelID)) {
            mgr.startRun(m_level);
            this->playAsRun();
            return;
        }

        // The popup sits on top of this layer, so `this` outlives it.
        Ref<GJGameLevel> level = m_level;
        auto popup = RunResumePopup::create(
            [this, level] {
                AugmentManager::get().startRun(level);
                this->playAsRun();
            },
            [this] { this->playAsRun(); }
        );
        if (popup) popup->show();
    }

    // GD's own play, with the next PlayLayer of this level armed as a run
    // level (AugmentManager::armRunEntry).
    void playAsRun() {
        AugmentManager::get().armRunEntry(m_level->m_levelID.value());
        g_playingAsRun = true;
        this->onPlay(nullptr);
        g_playingAsRun = false;
    }

    // GD's Play button: a normal attempt, whatever run the level has parked.
    void onPlay(CCObject* sender) {
        if (!g_playingAsRun) AugmentManager::get().disarmRunEntry();
        LevelInfoLayer::onPlay(sender);
    }
};
