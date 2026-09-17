// PlayLayer integration: the level lifecycle (init / reset / death / frame /
// quit) handed to the LevelSession, which fans it out to the augments in
// src/augments/. Nothing augment-specific lives here.

#include "../game/AugmentManager.hpp"
#include "../game/DraftSession.hpp"
#include "../game/LevelSession.hpp"
#include "../game/Scales.hpp"
#include "../input/Hotkeys.hpp"
#include "../ui/ProgressMarks.hpp"
#include "../ui/RunHud.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/Keyboard.hpp>

using namespace geode::prelude;
using namespace augment;

class $modify(AugPlayLayer, PlayLayer) {
    // The session for this layer, or null (not a run level / layer replaced).
    LevelSession* session() {
        return AugmentManager::get().sessionFor(this);
    }

    bool isRunLevel() {
        auto s = this->session();
        return s && s->runLevel();
    }

    // GD creates m_progressBar in setupHasCompleted for online levels (init
    // passes dontCreateObjects), so this runs from both and does its work
    // the first time the bar exists.
    void attachToProgressBar(char const* where) {
        auto s = this->session();
        if (!s || !s->hud() || s->marks()) return;
        if (!m_progressBar) {
            log::info("ProgressBar not there yet at {}", where);
            return;
        }
        s->hud()->attachGauge(m_progressBar, m_progressFill, m_percentageLabel);
        s->setMarks(ProgressMarks::create(m_progressBar, m_progressFill));
        log::info("Attached gauge + marks to the progress bar at {}", where);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        if (this->isRunLevel()) this->attachToProgressBar("setupHasCompleted");
    }

    // ---------------------------------------------------------------- setup

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        auto& mgr = AugmentManager::get();
        int levelID = level ? level->m_levelID.value() : 0;
        bool run = level && mgr.isRunFor(levelID);

        // The session exists before PlayLayer::init so the augments see the
        // objects it creates (hitbox scales must be in place for the first
        // one) — onLevelInit is their chance to publish globals.
        if (run) mgr.beginLevel(this, levelID).onLevelInit();
        else scales::resetAll();

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (auto s = this->session()) {
            auto hud = RunHud::create();
            CCNode* parent = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : this;
            parent->addChild(hud, 1000);
            s->setHud(hud);
            this->attachToProgressBar("init");
            s->onLevelReady();
            s->refreshHud(true);
        }

        // Hotkey path 1 (see input/Hotkeys.cpp). Node-scoped: removed with
        // the layer, so nothing captures `this`.
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-slowmo"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (!down || repeat) return ListenerResult::Propagate;
                return hotkeys::route(Hotkey::SlowMo, true, "setting") ? ListenerResult::Stop : ListenerResult::Propagate;
            }
        );
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-checkpoint"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (!down || repeat) return ListenerResult::Propagate;
                return hotkeys::route(Hotkey::Checkpoint, true, "setting") ? ListenerResult::Stop : ListenerResult::Propagate;
            }
        );
        // Brake is a held key: the release is routed too.
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-brake"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (repeat) return ListenerResult::Propagate;
                return hotkeys::route(Hotkey::Brake, down, "setting") ? ListenerResult::Stop : ListenerResult::Propagate;
            }
        );
        log::info("Hotkey setting listeners attached to PlayLayer (run level: {})", this->isRunLevel());
        return true;
    }

    void addObject(GameObject* object) {
        PlayLayer::addObject(object);
        if (!object) return;
        if (auto s = this->session(); s && s->runLevel()) s->onObjectAdded(object);
    }

    // ---------------------------------------------------------------- death

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // GD pokes the player with an invisible anticheat spike at the start
        // of every attempt. That call must go through untouched: it is not a
        // death, and blocking it flags the level as hacked.
        if (object == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        auto s = this->session();
        if (s && s->runAttempt() && (player == m_player1 || player == m_player2)) {
            // Shield may swallow the hit.
            if (s->onHit(player)) return;

            // destroyPlayer can fire more than once per attempt; count once.
            if (player == m_player1 && s->countDeath()) {
                float bonus = AugmentManager::get().onDeath(this->getCurrentPercent());
                if (bonus > 0.f) s->notice(fmt::format("NEW BEST  +{:.0f}", bonus), { 255, 220, 90 });
                s->onDeath();
            }
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        auto s = this->session();
        // An augment may turn this reset into a respawn (startpos); it sets
        // GD up before the call and cleans up after.
        bool resume = s ? s->onBeforeReset() : false;

        PlayLayer::resetLevel();

        if (!s) return;
        s->onAttemptStart(resume);
        s->refreshHud(true);

        // A checkpoint respawn is still the same attempt: the draft waits for
        // the reset that starts over from 0. isRunning() is false during
        // PlayLayer::init (scene not yet on screen); a draft pending from a
        // previous visit likewise waits for the next reset.
        if (!resume && s->runLevel() && AugmentManager::get().hasPendingDraft() && this->isRunning()) {
            draft::showNext();
        }
    }

    // ---------------------------------------------------------------- lifecycle

    // The level is on screen and about to move: the place for the opening
    // draft (queued by startRun) or one left pending from an earlier visit.
    // The reset inside init skips drafts because the scene is not running
    // yet, so this is the first chance after the fade-in. (unverified: GD
    // is assumed to call startGame once per level load; the pending flag
    // makes a second call harmless.)
    void startGame() {
        PlayLayer::startGame();
        auto& mgr = AugmentManager::get();
        log::info("startGame: run level {}, draft pending {}", this->isRunLevel(), mgr.hasPendingDraft());
        if (this->isRunLevel() && mgr.hasPendingDraft()) draft::showNext();
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        scales::resetTime();
        if (auto s = this->session(); s && s->runAttempt()) {
            AugmentManager::get().endRun();
        }
    }

    void onQuit() {
        // Never leave the next scene frozen or slowed down, and never let the
        // hazard scale leak into the editor or the next level.
        if (auto s = this->session()) s->onQuit();
        scales::resetAll();
        draft::abandon();
        AugmentManager::get().endLevel();
        PlayLayer::onQuit();
    }

    void pauseGame(bool unfocused) {
        PlayLayer::pauseGame(unfocused);
        if (auto s = this->session()) s->onPause();
    }

    // ---------------------------------------------------------------- per-frame

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto s = this->session();
        if (!s || !s->runLevel()) return;
        s->onFrame(dt);
        s->tickHud(dt);
    }
};
