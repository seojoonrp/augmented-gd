// PlayLayer integration: the level lifecycle (init / reset / death / frame /
// quit) handed to the LevelSession, which fans it out to the augments in
// src/augments/. Nothing augment-specific lives here.

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
        s->hud()->attachGauge(m_percentageLabel);
        s->setMarks(ProgressMarks::create(m_progressBar, m_progressFill));
        log::info("Attached gauge + marks to the progress bar at {}", where);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        auto s = this->session();
        if (!s || !s->runLevel()) return;
        this->attachToProgressBar("setupHasCompleted");
        // The level's objects all exist now (GD calls this once they are
        // created, node-ids `refs/node-ids/src/PlayLayer.cpp:69-81`), and
        // the loading screen hides the index build.
        s->warmObjectIndex();
    }

    // ---------------------------------------------------------------- setup

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        auto& mgr = AugmentManager::get();
        int levelID = level ? level->m_levelID.value() : 0;
        // Only an AUG-button entry makes a run level; the flag is spent
        // whatever this load turns out to be.
        bool const armed = mgr.takeRunEntry(levelID);
        bool run = level && armed && mgr.isRunFor(levelID);
        if (level && !armed && mgr.isRunFor(levelID)) {
            log::info("PlayLayer::init: level {} has a run but was entered without AUG, playing normally", levelID);
        }

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
        auto s = this->session();
        // GD pokes the player with an invisible anticheat spike at the start
        // of every attempt. That call must go through untouched — blocking it
        // flags the level as hacked — so it is never swallowed; whether it is
        // a *death* is decided after the original call, by m_isDead.
        bool anticheat = object == m_anticheatSpike;
        bool ours = s && s->runAttempt() && (player == m_player1 || player == m_player2);
        // Read before the original: the percent belongs to the death.
        float percent = this->getCurrentPercent();

        // Shield (or a berserk smash) may swallow the hit, which means not
        // calling the original at all.
        if (ours && !anticheat && s->onHit(player, object)) return;

        PlayLayer::destroyPlayer(player, object);

        // A real death is the call the original leaves dead — the attempt-start
        // anticheat poke and a swallowed hit leave `m_isDead` false
        // (death-tracker's test, `refs/death-tracker/src/hooks/DTPlayLayer.cpp:241`).
        // Whichever player GD names ends the attempt, so player 2 counts too:
        // requiring player 1 dropped dual deaths entirely (user, 2026-09-27 —
        // a 60 % death paid no gauge and skipped the checkpoint respawn), and
        // countDeath() already keeps it to once per attempt.
        if (!player || !player->m_isDead) return;
        if (!ours) {
            if (s && s->runLevel()) {
                log::info(
                    "Death ignored at {:.1f}%: runAttempt {}, player {}",
                    percent, s->runAttempt(),
                    player == m_player1 ? "1" : (player == m_player2 ? "2" : "neither")
                );
            }
            return;
        }
        if (!s->countDeath()) return;

        log::info(
            "Death: player {} at {:.1f}%, object {}{}, dual {}",
            player == m_player1 ? 1 : 2, percent, object ? "named" : "none",
            anticheat ? " (anticheat!)" : "", m_gameState.m_isDualMode
        );

        // GD's own save and New Best! were kept out of this death
        // (GdRecordHook.cpp); the runs' record takes their place. Every
        // death counts here, a checkpoint respawn's too: the record is how
        // far a run got, not what the gauge has settled.
        int whole = static_cast<int>(percent);
        if (records::submit(m_level, whole)) records::showNewBest(this, whole);

        auto& mgr = AugmentManager::get();
        // The augments hear the death first, because startpos answers there
        // whether a checkpoint brings this attempt back: a death the run
        // comes back from charges nothing yet.
        bool respawning = s->onDeath();
        // The gauge and its cost as the bar showed them, so the reward
        // animates from there even when this death wraps the gauge and
        // raises the cost.
        float before = mgr.gauge();
        float cost = mgr.gaugeThreshold();
        auto r = mgr.onDeath(percent, respawning);
        if (r.deferred) {
            // Nothing is settled while the life goes on: no numbers, no
            // particles — just the HUD text.
            s->refreshHud(true);
        }
        else {
            s->rewardDeath(this->hudPointOf(player), before, cost, r);
        }
    }

    // Where a node of the object layer sits on the HUD (screen space):
    // through world space like Cat.cpp does for its targets.
    CCPoint hudPointOf(CCNode* node) {
        auto s = this->session();
        auto hud = s ? s->hud() : nullptr;
        auto parent = node ? node->getParent() : nullptr;
        if (!hud || !parent) {
            log::info("hudPointOf: no {}, falling back to screen centre", hud ? "parent" : "hud");
            auto winSize = CCDirector::get()->getWinSize();
            return { winSize.width / 2, winSize.height / 2 };
        }
        return hud->convertToNodeSpace(parent->convertToWorldSpace(node->getPosition()));
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
        auto s = this->session();
        bool run = s && s->runAttempt();
        // A clear with augments is not a GD clear (user, 2026-09-28): GD
        // skips saving progress while m_isTestMode is on — CBF's safe mode,
        // `refs/click-between-frames/src/main.cpp:241-249` — and the runs'
        // record keeps the 100 instead. runAttempt() reads the flag, hence
        // the HideFromGd for GdRecordHook's checks inside the call.
        std::optional<records::HideFromGd> hide;
        bool wasTestMode = m_isTestMode;
        if (run) {
            hide.emplace();
            s->markRunCleared();
            records::submit(m_level, 100);
            m_isTestMode = true;
            log::info("levelComplete: run clear, test mode borrowed so GD saves nothing");
        }
        PlayLayer::levelComplete();
        m_isTestMode = wasTestMode;
        hide.reset();

        scales::resetTime();
        if (run) AugmentManager::get().endRun();
    }

    void onQuit() {
        auto s = this->session();
        // The session is gone by the time GD's onQuit runs; if GD saves
        // anything there, it must still not see a run attempt.
        std::optional<records::HideFromGd> hide;
        if (s && s->runAttempt()) hide.emplace();
        // Never leave the next scene frozen or slowed down, and never let the
        // hazard scale leak into the editor or the next level.
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

    // ---------------------------------------------------------------- per-frame

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto s = this->session();
        if (!s || !s->runLevel()) return;
        s->onFrame(dt);
        s->tickHud(dt);
    }
};
