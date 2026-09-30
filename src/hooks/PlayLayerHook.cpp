// PlayLayer lifecycle, handed to the LevelSession (which passes it on to the augments).

#include "../game/AugmentManager.hpp"
#include "../game/DraftSession.hpp"
#include "../game/LevelSession.hpp"
#include "../game/Records.hpp"
#include "../game/Scales.hpp"
#include "../input/Hotkeys.hpp"
#include "../ui/ProgressMarks.hpp"
#include "../ui/RunHud.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/Keyboard.hpp>

#include <optional>

using namespace geode::prelude;
using namespace augment;

class $modify(AugPlayLayer, PlayLayer) {
    // null if this isn't a run level (or the layer was replaced)
    LevelSession* session() {
        return AugmentManager::get().sessionFor(this);
    }

    bool isRunLevel() {
        auto s = this->session();
        return s && s->runLevel();
    }

    // online levels only get m_progressBar in setupHasCompleted (init passes
    // dontCreateObjects), so this is called from both and attaches once
    void attachToProgressBar() {
        auto s = this->session();
        if (!s || !s->hud() || s->marks() || !m_progressBar) return;
        s->hud()->attachGauge(m_percentageLabel);
        s->setMarks(ProgressMarks::create(m_progressBar, m_progressFill));
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        auto s = this->session();
        if (!s || !s->runLevel()) return;
        this->attachToProgressBar();
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        auto& mgr = AugmentManager::get();
        int levelID = level ? level->m_levelID.value() : 0;
        // only the AUG button arms a run entry, and any load uses it up
        bool const armed = mgr.takeRunEntry(levelID);
        bool run = level && armed && mgr.isRunFor(levelID);

        // session before the original init, so hitbox scales are set before
        // the first object gets created
        if (run) mgr.beginLevel(this, levelID).onLevelInit();
        else scales::resetAll();

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (auto s = this->session()) {
            auto hud = RunHud::create();
            CCNode* parent = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : this;
            parent->addChild(hud, 1000);
            s->setHud(hud);
            this->attachToProgressBar();
            s->onLevelReady();
            s->refreshHud(true);
        }

        // keybind settings, one of the two hotkey paths (see Hotkeys.cpp).
        // node listeners die with the layer, so no `this` capture
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-slowmo"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (!down || repeat) return ListenerResult::Propagate;
                return hotkeys::route(Hotkey::SlowMo, true) ? ListenerResult::Stop : ListenerResult::Propagate;
            }
        );
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-checkpoint"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (!down || repeat) return ListenerResult::Propagate;
                return hotkeys::route(Hotkey::Checkpoint, true) ? ListenerResult::Stop : ListenerResult::Propagate;
            }
        );
        // brake is held, so releases go through too
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-brake"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (repeat) return ListenerResult::Propagate;
                return hotkeys::route(Hotkey::Brake, down) ? ListenerResult::Stop : ListenerResult::Propagate;
            }
        );
        return true;
    }

    void addObject(GameObject* object) {
        PlayLayer::addObject(object);
        if (!object) return;
        if (auto s = this->session(); s && s->runLevel()) s->onObjectAdded(object);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto s = this->session();
        // GD pokes the player with an invisible anticheat spike at attempt start.
        // it has to go through untouched, blocking it flags the level as hacked
        bool anticheat = object == m_anticheatSpike;
        bool ours = s && s->runAttempt() && (player == m_player1 || player == m_player2);
        // read before the original call
        float percent = this->getCurrentPercent();

        // shield / berserk swallow the hit by skipping the original
        if (ours && !anticheat && s->onHit(player, object)) return;

        PlayLayer::destroyPlayer(player, object);

        // only a real death leaves m_isDead set (not the anticheat poke or a swallowed hit).
        // either player counts in dual; countDeath() dedups per attempt.
        if (!player || !player->m_isDead) return;
        if (!ours || !s->countDeath()) return;

        // GD's save / New Best are blocked for run attempts (GdRecordHook), the
        // run record replaces them. checkpoint respawns count here too
        int whole = static_cast<int>(percent);
        if (records::submit(m_level, whole)) records::showNewBest(this, whole);

        auto& mgr = AugmentManager::get();
        // augments go first: startpos decides here if a checkpoint brings the
        // attempt back, and a death like that doesn't charge the gauge yet
        bool respawning = s->onDeath();
        // what the bar showed, so the reward animates from there even when
        // this death wraps the gauge and raises the cost
        float before = mgr.gauge();
        float cost = mgr.gaugeThreshold();
        auto r = mgr.onDeath(percent, respawning);
        if (r.deferred) {
            s->refreshHud(true);
        }
        else {
            s->rewardDeath(this->hudPointOf(player), before, cost, r);
        }
    }

    // object layer -> HUD space, through world space (like Cat.cpp)
    CCPoint hudPointOf(CCNode* node) {
        auto s = this->session();
        auto hud = s ? s->hud() : nullptr;
        auto parent = node ? node->getParent() : nullptr;
        if (!hud || !parent) {
            auto winSize = CCDirector::get()->getWinSize();
            return { winSize.width / 2, winSize.height / 2 };
        }
        return hud->convertToNodeSpace(parent->convertToWorldSpace(node->getPosition()));
    }

    void resetLevel() {
        auto s = this->session();
        // startpos can turn the reset into a checkpoint respawn
        bool resume = s ? s->onBeforeReset() : false;

        PlayLayer::resetLevel();

        if (!s) return;
        s->onAttemptStart(resume);
        s->refreshHud(true);

        // drafts wait for a restart from 0, not a respawn. isRunning() is false
        // during init, startGame picks that case up
        if (!resume && s->runLevel() && AugmentManager::get().hasPendingDraft() && this->isRunning()) {
            draft::showNext();
        }
    }

    // first point after the fade-in where a draft can open (the opening one,
    // or one left pending from an earlier visit)
    void startGame() {
        PlayLayer::startGame();
        auto& mgr = AugmentManager::get();
        if (this->isRunLevel() && mgr.hasPendingDraft()) draft::showNext();
    }

    void levelComplete() {
        auto s = this->session();
        bool run = s && s->runAttempt();
        // a run clear isn't a GD clear. GD saves nothing while m_isTestMode is on
        // (CBF's safe mode does the same), the run record keeps the 100 instead.
        // runAttempt() goes false with the flag set, hence HideFromGd for GdRecordHook
        std::optional<records::HideFromGd> hide;
        bool wasTestMode = m_isTestMode;
        if (run) {
            hide.emplace();
            s->markRunCleared();
            records::submit(m_level, 100);
            m_isTestMode = true;
        }
        PlayLayer::levelComplete();
        m_isTestMode = wasTestMode;
        hide.reset();

        scales::resetTime();
        if (run) AugmentManager::get().endRun();
    }

    void onQuit() {
        auto s = this->session();
        // the session is gone by the time GD's onQuit runs, keep hiding anyway
        std::optional<records::HideFromGd> hide;
        if (s && s->runAttempt()) hide.emplace();
        // don't leak frozen time or hitbox scales into the next scene / editor
        if (s) s->onQuit();
        scales::resetAll();
        draft::abandon();
        AugmentManager::get().endLevel();
        PlayLayer::onQuit();
    }

    void pauseGame(bool unfocused) {
        PlayLayer::pauseGame(unfocused);
        if (auto s = this->session()) s->onPause();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto s = this->session();
        if (!s || !s->runLevel()) return;
        s->onFrame(dt);
        s->tickHud(dt);
    }
};
