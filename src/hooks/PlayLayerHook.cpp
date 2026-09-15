// PlayLayer integration: death counting / draft popup, and the per-attempt
// mechanics of every augment (shield, slow-mo, checkpoint, foresight, unmirror).

#include "../core/AugmentManager.hpp"
#include "../ui/AugmentDraftPopup.hpp"
#include "../ui/RunHud.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;
using namespace augment;

namespace {

// Slow-mo: scale the delta time the scheduler hands out (physics, actions and
// our own timers all slow down together) and match the FMOD master pitch so
// the music stays in sync. g_timeScale is written by the PlayLayer hook.
float g_timeScale = 1.f;

void setGameSpeed(float scale) {
    if (std::abs(g_timeScale - scale) < 0.001f) return;
    g_timeScale = scale;

    auto engine = FMODAudioEngine::sharedEngine();
    FMOD::ChannelGroup* master = nullptr;
    if (engine && engine->m_system && engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master) {
        master->setPitch(scale);
    }
    log::info("Game speed -> {:.2f}", scale);
}

bool isMirrorPortal(GameObject* obj) {
    return obj->m_objectType == GameObjectType::InverseMirrorPortal
        || obj->m_objectType == GameObjectType::NormalMirrorPortal;
}

// Turn a mirror portal into an inert, invisible decoration. Collision code
// dispatches on m_objectType, so Decoration means "never triggers".
void neutralizeMirrorPortal(GameObject* obj) {
    obj->m_objectType = GameObjectType::Decoration;
    obj->m_isHide = true;
    obj->setVisible(false);
    if (obj->m_glowSprite) obj->m_glowSprite->setVisible(false);
    if (obj->m_particle) obj->m_particle->setVisible(false);
}

} // namespace

class $modify(AugPlayLayer, PlayLayer) {
    struct Fields {
        // destroyPlayer can fire more than once per attempt; count only the first.
        bool deathCounted = false;

        // Shield: charges are derived from (level - shieldsUsed) so a shield
        // drafted mid-run is usable in the very next attempt.
        int shieldsUsed = 0;
        float noclipTimer = 0.f;

        // Checkpoint: one respawn per attempt; placements limited by level.
        int checkpointsPlaced = 0;
        bool respawnUsed = false;
        bool respawnPending = false;
        Ref<CheckpointObject> savedCheckpoint;

        RunHud* hud = nullptr;

        // Foresight: our own draw node + objects sorted by x for cheap
        // "what's near the player" queries.
        cocos2d::CCDrawNode* hitboxNode = nullptr;
        std::vector<GameObject*> objectsByX;
    };

    bool isRunLevel() {
        return m_level && AugmentManager::get().isRunFor(m_level->m_levelID.value());
    }

    // Normal-mode only: practice/test attempts don't count and get no augments.
    bool isRunAttempt() {
        return this->isRunLevel() && !m_isPracticeMode && !m_isTestMode;
    }

    // ---------------------------------------------------------------- setup

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (this->isRunLevel()) {
            m_fields->hud = RunHud::create();
            CCNode* parent = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : this;
            parent->addChild(m_fields->hud, 1000);
            this->refreshHud();
        }
        return true;
    }

    void addObject(GameObject* object) {
        PlayLayer::addObject(object);
        if (object && this->isRunLevel() && AugmentManager::get().has(ids::Unmirror) && isMirrorPortal(object)) {
            neutralizeMirrorPortal(object);
        }
    }

    // Called when Unmirror is drafted while the level is already loaded.
    void applyUnmirrorNow() {
        if (!m_objects) return;
        int count = 0;
        for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
            if (isMirrorPortal(obj)) {
                neutralizeMirrorPortal(obj);
                count++;
            }
        }
        log::info("Unmirror: neutralized {} mirror portal(s)", count);
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

        if (this->isRunAttempt() && (player == m_player1 || player == m_player2)) {
            auto& mgr = AugmentManager::get();
            auto f = m_fields.self();

            // Shield: still inside the noclip window -> ignore the hit entirely.
            if (f->noclipTimer > 0.f) return;

            int shields = mgr.levelOf(ids::Shield) - f->shieldsUsed;
            if (shields > 0) {
                f->shieldsUsed++;
                f->noclipTimer = tune::NoclipSeconds;
                log::info("Shield broke ({} left), noclip for {}s", shields - 1, tune::NoclipSeconds);
                if (f->hud) f->hud->notice("SHIELD BROKEN", { 120, 200, 255 });
                return;
            }

            if (player == m_player1 && !f->deathCounted) {
                f->deathCounted = true;
                mgr.onDeath(this->getCurrentPercent());

                // Checkpoint: remember where to come back to. GD may clear the
                // checkpoint array during a normal-mode death, so keep a ref.
                if (mgr.has(ids::Checkpoint) && !f->respawnUsed) {
                    if (auto cp = this->getLastCheckpoint()) {
                        f->savedCheckpoint = cp;
                        f->respawnPending = true;
                    }
                }
            }
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        auto& mgr = AugmentManager::get();
        auto f = m_fields.self();

        bool fromCheckpoint = f->respawnPending && f->savedCheckpoint && this->isRunAttempt();
        f->respawnPending = false;
        f->deathCounted = false;
        f->noclipTimer = 0.f;

        if (fromCheckpoint) {
            // Borrow the practice-mode respawn path for exactly this reset.
            f->respawnUsed = true;
            if (m_checkpointArray && m_checkpointArray->count() == 0) {
                this->storeCheckpoint(f->savedCheckpoint);
            }
            bool wasPractice = m_isPracticeMode;
            m_isPracticeMode = true;
            PlayLayer::resetLevel();
            m_isPracticeMode = wasPractice;
            log::info("Respawned from checkpoint");
            if (f->hud) f->hud->notice("CHECKPOINT", { 120, 255, 120 });
        }
        else {
            // Fresh attempt: refill everything.
            f->shieldsUsed = 0;
            f->checkpointsPlaced = 0;
            f->respawnUsed = false;
            f->savedCheckpoint = nullptr;
            if (m_checkpointArray && m_checkpointArray->count() > 0) {
                this->removeAllCheckpoints();
            }
            PlayLayer::resetLevel();
        }

        this->refreshHud();

        // isRunning() is false during PlayLayer::init (scene not yet on
        // screen); a draft pending from a previous visit waits for the next
        // reset instead of attaching itself to the old scene.
        if (this->isRunLevel() && mgr.hasPendingDraft() && this->isRunning()) {
            mgr.clearPendingDraft();
            this->showDraft();
        }
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        setGameSpeed(1.f);
        if (this->isRunAttempt()) {
            AugmentManager::get().endRun();
        }
    }

    void onQuit() {
        // Never leave the next scene frozen or slowed down.
        setGameSpeed(1.f);
        AugmentManager::get().resumeGameAfterDraft();
        PlayLayer::onQuit();
    }

    // ---------------------------------------------------------------- per-frame

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!this->isRunLevel()) return;

        auto& mgr = AugmentManager::get();
        auto f = m_fields.self();

        if (f->noclipTimer > 0.f) {
            f->noclipTimer -= dt;
            if (f->noclipTimer < 0.f) f->noclipTimer = 0.f;
        }

        if (mgr.has(ids::Foresight)) this->drawHitboxes();

        this->applyTimeScale();
        this->refreshHud();
    }

    // Foresight. GD's own hitbox drawing is gated by an inlined
    // "practice mode && show-hitboxes" check that can't be reached from a
    // hook (OpenHack byte-patches it), so we draw the boxes ourselves.
    void drawHitboxes() {
        auto f = m_fields.self();
        if (!m_objectLayer || !m_objects) return;

        if (!f->hitboxNode) {
            f->hitboxNode = CCDrawNode::create();
            f->hitboxNode->setID("foresight-hitboxes"_spr);
            m_objectLayer->addChild(f->hitboxNode, 1000);

            for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                if (obj->m_objectType == GameObjectType::Decoration || obj->m_isDecoration) continue;
                f->objectsByX.push_back(obj);
            }
            std::sort(f->objectsByX.begin(), f->objectsByX.end(), [](GameObject* a, GameObject* b) {
                return a->getPositionX() < b->getPositionX();
            });
            log::info("Foresight: tracking {} objects", f->objectsByX.size());
        }

        auto node = f->hitboxNode;
        node->clear();
        node->setVisible(true);
        if (!m_player1) return;

        // GD-style: thin outlines, no fill. Blue = solid, red = hazard,
        // green = everything else that interacts (portals, pads, rings, coins).
        constexpr float kBorder = 0.25f;
        ccColor4F const noFill = { 0.f, 0.f, 0.f, 0.f };
        auto colorFor = [](GameObjectType type) -> std::optional<ccColor4F> {
            switch (type) {
                case GameObjectType::Solid:
                case GameObjectType::Slope:
                case GameObjectType::Breakable:
                    return ccColor4F{ 0.f, 0.25f, 1.f, 1.f };
                case GameObjectType::Hazard:
                case GameObjectType::AnimatedHazard:
                    return ccColor4F{ 1.f, 0.f, 0.f, 1.f };
                case GameObjectType::Decoration:
                case GameObjectType::Special:
                case GameObjectType::Modifier:
                case GameObjectType::EnterEffectObject:
                case GameObjectType::CollisionObject:
                    return std::nullopt;
                default:
                    return ccColor4F{ 0.f, 1.f, 0.f, 1.f };
            }
        };
        auto drawObject = [&](GameObject* obj, ccColor4F color) {
            if (obj->m_objectRadius > 0.f) {
                node->drawCircle(obj->getPosition(), obj->m_objectRadius, noFill, kBorder, color, 32);
            }
            else {
                node->drawRect(obj->getObjectRect(), noFill, kBorder, color);
            }
        };

        // Objects within roughly one screen ahead / a bit behind the player.
        float px = m_player1->getPositionX();
        float const lo = px - 240.f, hi = px + 720.f;
        auto& objs = f->objectsByX;
        auto it = std::lower_bound(objs.begin(), objs.end(), lo, [](GameObject* o, float x) {
            return o->getPositionX() < x;
        });
        for (; it != objs.end() && (*it)->getPositionX() <= hi; ++it) {
            auto obj = *it;
            if (!obj->isVisible() || obj->m_isHide) continue;
            if (auto color = colorFor(obj->m_objectType)) drawObject(obj, *color);
        }

        // Player: yellow outer box plus the smaller inner box GD uses for
        // solid collisions.
        ccColor4F const playerColor = { 1.f, 1.f, 0.f, 1.f };
        auto drawPlayer = [&](PlayerObject* p) {
            auto rect = p->getObjectRect();
            node->drawRect(rect, noFill, kBorder, playerColor);
            float k = p->m_vehicleSize >= 1.f ? 0.25f : 0.4f;
            CCRect inner{
                rect.origin.x + rect.size.width * (1.f - k) / 2.f,
                rect.origin.y + rect.size.height * (1.f - k) / 2.f,
                rect.size.width * k, rect.size.height * k
            };
            node->drawRect(inner, noFill, kBorder, playerColor);
        };
        drawPlayer(m_player1);
        if (m_player2 && m_gameState.m_isDualMode) drawPlayer(m_player2);
    }

    void pauseGame(bool unfocused) {
        PlayLayer::pauseGame(unfocused);
        // Pause menu at normal speed; postUpdate re-applies slow-mo on resume.
        setGameSpeed(1.f);
    }

    void applyTimeScale() {
        float want = 1.f;
        if (this->isRunAttempt() && !m_isPaused && AugmentManager::get().slowMoEnabled()) {
            want = AugmentManager::get().slowMoScale();
        }
        setGameSpeed(want);
    }

    // ---------------------------------------------------------------- input

    void toggleSlowMo() {
        auto& mgr = AugmentManager::get();
        if (!this->isRunAttempt() || !mgr.has(ids::SlowMo)) {
            log::info("X ignored: runAttempt={} slowmoLv={}", this->isRunAttempt(), mgr.levelOf(ids::SlowMo));
            return;
        }
        mgr.toggleSlowMo();
        log::info("Slow-mo toggled -> {}", mgr.slowMoEnabled() ? "ON" : "OFF");
        this->applyTimeScale();
        if (m_fields->hud) {
            m_fields->hud->notice(mgr.slowMoEnabled() ? "SLOW-MO ON" : "SLOW-MO OFF", { 255, 220, 120 });
        }
    }

    void tryPlaceCheckpoint() {
        auto& mgr = AugmentManager::get();
        auto f = m_fields.self();
        int lvl = mgr.levelOf(ids::Checkpoint);
        if (!this->isRunAttempt() || m_isPaused || lvl == 0 || !m_player1 || m_player1->m_isDead) {
            log::info("Z ignored: runAttempt={} paused={} cpLv={} dead={}",
                this->isRunAttempt(), m_isPaused, lvl, m_player1 ? m_player1->m_isDead : true);
            return;
        }

        if (f->respawnUsed) {
            if (f->hud) f->hud->notice("NO RESPAWN LEFT", { 255, 120, 120 });
            return;
        }
        if (f->checkpointsPlaced >= lvl) {
            if (f->hud) f->hud->notice("NO CHECKPOINTS LEFT", { 255, 120, 120 });
            return;
        }

        bool wasPractice = m_isPracticeMode;
        m_isPracticeMode = true;
        auto cp = this->markCheckpoint();
        m_isPracticeMode = wasPractice;

        if (cp) {
            f->checkpointsPlaced++;
            log::info("Checkpoint placed ({}/{})", f->checkpointsPlaced, lvl);
            if (f->hud) f->hud->notice("CHECKPOINT PLACED", { 120, 255, 120 });
        }
        else {
            log::info("markCheckpoint returned null");
        }
    }

    // ---------------------------------------------------------------- hud

    void refreshHud() {
        auto f = m_fields.self();
        if (!f->hud) return;
        auto& mgr = AugmentManager::get();

        std::vector<std::string> lines;
        float now = this->getCurrentPercent();
        lines.push_back(fmt::format(
            "DRAFT {:.0f}/{:.0f}   deaths {}   now {:.1f}%   best {:.1f}%",
            mgr.gauge(), mgr.gaugeThreshold(), mgr.deaths(), now, std::max(now, mgr.bestPercent())
        ));

        if (int lvl = mgr.levelOf(ids::Shield)) {
            if (f->noclipTimer > 0.f) {
                lines.push_back(fmt::format("Shield Lv{}: NOCLIP {:.1f}s", lvl, f->noclipTimer));
            }
            else {
                lines.push_back(fmt::format("Shield Lv{}: {}/{}", lvl, lvl - f->shieldsUsed, lvl));
            }
        }
        if (int lvl = mgr.levelOf(ids::SlowMo)) {
            lines.push_back(fmt::format(
                "Slow-Mo Lv{}: {} ({:.0f}%)  [X]",
                lvl, mgr.slowMoEnabled() ? "ON" : "OFF", mgr.slowMoScale() * 100.f
            ));
        }
        if (int lvl = mgr.levelOf(ids::Checkpoint)) {
            lines.push_back(fmt::format(
                "Checkpoint Lv{}: {}/{} placed{}  [Z]",
                lvl, f->checkpointsPlaced, lvl, f->respawnUsed ? ", respawn used" : ""
            ));
        }
        if (mgr.has(ids::Foresight)) lines.push_back("Foresight");
        if (mgr.has(ids::Unmirror)) lines.push_back("Unmirror");

        f->hud->setLines(lines);
    }

    // ---------------------------------------------------------------- draft

    void showDraft() {
        auto& mgr = AugmentManager::get();
        auto choices = mgr.rollDraft(3);
        if (choices.empty()) return;

        // Callback intentionally captures nothing: the PlayLayer may be gone
        // by the time it runs, so it only talks to the singleton and looks the
        // layer up fresh.
        auto popup = AugmentDraftPopup::create(choices, [](std::string const& id) {
            auto& mgr = AugmentManager::get();
            mgr.applyPick(id);
            mgr.resumeGameAfterDraft();
            if (mgr.cursorWasHidden()) CCEGLView::get()->showCursor(false);

            if (auto pl = static_cast<AugPlayLayer*>(PlayLayer::get())) {
                if (id == ids::Unmirror) pl->applyUnmirrorNow();
                pl->refreshHud();
            }
        });
        if (!popup) return;

        // The director is about to be paused, which also freezes actions, so
        // the pop-in animation would never finish. Skip it.
        popup->m_noElasticity = true;
        popup->show();

        // GD hides the cursor in levels; the draft needs it.
        auto view = CCEGLView::get();
        mgr.setCursorWasHidden(view->m_bShouldHideCursor);
        view->showCursor(true);

        mgr.pauseGameForDraft();
    }
};

// Hotkeys: the keys are configured through the "keybind" settings in
// mod.json (rebindable in the mod's settings page). We read them from Geode's
// raw KeyboardInputEvent, which fires before any keyboard hooks, so other
// keybind mods can't swallow them.
namespace {
    // RTTI casts across the DLL boundary are unreliable, so use Geode's
    // typeinfo-based cast like Mod::getSettingValue does. If the setting
    // can't be resolved at all, fall back to the mod.json defaults.
    bool keybindMatches(char const* settingKey, enumKeyCodes fallback, KeyboardInputData const& data) {
        auto setting = cast::typeinfo_pointer_cast<KeybindSettingV3>(Mod::get()->getSetting(settingKey));
        if (!setting) {
            return data.key == fallback && data.modifiers == KeyboardModifier::None;
        }
        for (auto const& bind : setting->getValue()) {
            if (bind.key == data.key && bind.modifiers == data.modifiers) return true;
        }
        return false;
    }

    AugPlayLayer* activeRunLayer() {
        if (AugmentManager::get().isGamePausedForDraft()) return nullptr;
        return static_cast<AugPlayLayer*>(PlayLayer::get());
    }
}

$execute {
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        if (data.action != KeyboardInputData::Action::Press) return ListenerResult::Propagate;

        bool slowmo = keybindMatches("keybind-slowmo", KEY_X, data);
        bool checkpoint = keybindMatches("keybind-checkpoint", KEY_Z, data);
        if (data.key == KEY_X || data.key == KEY_Z) {
            log::info("Raw key {} down (mods {}), matches: slowmo={} checkpoint={}",
                static_cast<int>(data.key), static_cast<int>(data.modifiers.value), slowmo, checkpoint);
        }
        if (!slowmo && !checkpoint) return ListenerResult::Propagate;

        auto pl = activeRunLayer();
        log::info("Hotkey {} pressed, play layer = {}", slowmo ? "slowmo" : "checkpoint", fmt::ptr(pl));
        if (!pl) return ListenerResult::Propagate;

        if (slowmo) pl->toggleSlowMo();
        else pl->tryPlaceCheckpoint();
        return ListenerResult::Stop;
    }).leak();
}

// Slow-mo time scaling. Every scheduled update (PlayLayer::update included)
// receives the scaled dt.
class $modify(AugScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt * g_timeScale);
    }
};
