// The AUG button on the level info screen: starts a run, or asks first (RunPromptPopup).

#include "../game/AugmentManager.hpp"
#include "../ui/AugButton.hpp"
#include "../ui/RunPromptPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>

using namespace geode::prelude;
using namespace augment;

namespace {
    // left of the difficulty face, level with Play. measured from the face's
    // edge; also clears a difficulty name under it (~5 pt wider)
    constexpr float kGapFromDifficulty = 12.f;
    // as wide as GD's buttons down the right edge (first one found). drawn from
    // the Large circle so the texture only ever shrinks
    constexpr char const* kSideButtons[] = { "info-button", "like-button", "rate-button", "leaderboards-button" };
    constexpr float kFallbackSideWidth = 45.f;
    // no difficulty sprite, roughly where it would put us
    constexpr float kFallbackX = 122.f;
    constexpr float kLogoScale = 1.0f;

    // set while our button runs GD's onPlay, so the onPlay hook can tell it apart
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
        float const width = this->sideButtonWidth();

        auto spr = augButtonSprite(CircleBaseSize::Large, kLogoScale);
        spr->setScale(width / spr->getContentSize().width);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AugLevelInfoLayer::onAugment));
        btn->setID("augment-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("augment-menu"_spr);
        menu->setPosition(this->augmentButtonSpot(playBtn, width / 2));
        menu->addChild(btn);
        this->addChild(menu);

        return true;
    }

    float sideButtonWidth() {
        if (auto menu = this->getChildByID("right-side-menu")) {
            for (auto id : kSideButtons) {
                if (auto btn = menu->getChildByID(id)) {
                    return btn->getScaledContentSize().width * menu->getScale();
                }
            }
        }
        return kFallbackSideWidth;
    }

    // in this layer's space
    CCPoint augmentButtonSpot(CCNode* playBtn, float radius) {
        auto const winSize = CCDirector::get()->getWinSize();
        float y = winSize.height * 0.66f;
        if (playBtn && playBtn->getParent()) {
            y = this->convertToNodeSpace(playBtn->getParent()->convertToWorldSpace(playBtn->getPosition())).y;
        }
        if (!m_difficultySprite || !m_difficultySprite->getParent()) {
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

        // the popups sit on this layer, so `this` outlives them
        Ref<GJGameLevel> level = m_level;
        auto startHere = [this, level] {
            AugmentManager::get().startRun(level);
            this->playAsRun();
        };

        if (mgr.isRunFor(levelID)) {
            if (auto popup = RunPromptPopup::resume(startHere, [this] { this->playAsRun(); })) popup->show();
            return;
        }
        // only one run at a time, so starting here drops the one parked on another level
        if (mgr.isRunActive()) {
            if (auto popup = RunPromptPopup::replace(mgr.levelName(), startHere)) popup->show();
            return;
        }
        startHere();
    }

    // GD's own play, with the next PlayLayer of this level armed as a run
    void playAsRun() {
        AugmentManager::get().armRunEntry(m_level->m_levelID.value());
        g_playingAsRun = true;
        this->onPlay(nullptr);
        g_playingAsRun = false;
    }

    // GD's Play button is always a normal attempt, even with a run parked here
    void onPlay(CCObject* sender) {
        if (!g_playingAsRun) AugmentManager::get().disarmRunEntry();
        LevelInfoLayer::onPlay(sender);
    }
};
