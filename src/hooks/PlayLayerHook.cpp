// M4: count deaths during an active run and show the draft popup on respawn.

#include "../core/AugmentManager.hpp"
#include "../ui/AugmentDraftPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;
using namespace augment;

class $modify(AugPlayLayer, PlayLayer) {
    struct Fields {
        // destroyPlayer can fire more than once per attempt; count only the first.
        bool deathCounted = false;
    };

    bool isRunLevel() {
        return m_level && AugmentManager::get().isRunFor(m_level->m_levelID.value());
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);

        if (player != m_player1) return;           // ignore player 2 / anything else
        if (m_fields->deathCounted) return;
        if (m_isPracticeMode || m_isTestMode) return;
        if (!this->isRunLevel()) return;

        m_fields->deathCounted = true;
        AugmentManager::get().onDeath(this->getCurrentPercent());
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->deathCounted = false;

        auto& mgr = AugmentManager::get();
        log::info("resetLevel: runLevel={} pending={}", this->isRunLevel(), mgr.hasPendingDraft());
        if (!this->isRunLevel() || !mgr.hasPendingDraft()) return;
        mgr.clearPendingDraft();
        this->showDraft();
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        if (this->isRunLevel() && !m_isPracticeMode && !m_isTestMode) {
            AugmentManager::get().endRun();
        }
    }

    void onQuit() {
        // If the player somehow leaves while the draft is up, don't leave the
        // director frozen for the next scene.
        AugmentManager::get().resumeGameAfterDraft();
        PlayLayer::onQuit();
    }

    void showDraft() {
        auto& mgr = AugmentManager::get();
        auto choices = mgr.rollDraft(3);
        log::info("showDraft: {} choices", choices.size());
        if (choices.empty()) return;

        // Callback intentionally captures nothing: the PlayLayer may be gone
        // by the time it runs, so it only talks to the singleton.
        auto popup = AugmentDraftPopup::create(choices, [](std::string const& id) {
            auto& mgr = AugmentManager::get();
            mgr.applyPick(id);
            mgr.resumeGameAfterDraft();
        });
        if (!popup) return;

        // The director is about to be paused, which also freezes actions, so
        // the pop-in animation would never finish. Skip it.
        popup->m_noElasticity = true;
        popup->show();
        log::info("draft popup shown, parent={}", fmt::ptr(popup->getParent()));

        mgr.pauseGameForDraft();
    }
};
