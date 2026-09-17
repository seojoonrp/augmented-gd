// PlayLayer integration: death counting / draft popup, and the per-attempt
// mechanics of every augment. The two hitbox augments keep their GameObject
// hooks elsewhere — hazard-hitbox in HazardHitboxHook.cpp, wave-hitbox in
// PlayerHitboxHook.cpp — and this file owns the scales they read, because
// nerve makes both depend on how far into the level the player is.

#include "../core/AugmentManager.hpp"
#include "../ui/AugmentDraftPopup.hpp"
#include "../ui/RunHud.hpp"
#include "HazardHitboxHook.hpp"
#include "PlayerHitboxHook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/Keyboard.hpp>

#include <algorithm>
#include <random>

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

// nerve moves the hazard scale continuously, but re-applying it walks every
// object in the level, so the per-frame update only does that once the scale
// has drifted this far from what the objects currently carry.
constexpr float kHazardReapplyStep = 0.01f;

enum class Hotkey { SlowMo = 0, Checkpoint = 1 };
constexpr int HotkeyCount = 2;

char const* hotkeyName(Hotkey which) {
    return which == Hotkey::SlowMo ? "slowmo" : "checkpoint";
}

// Defined with the input paths at the bottom of the file.
bool routeHotkey(Hotkey which, char const* source);

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

        // Checkpoint: `level` placements per attempt (life from 0 %), each one
        // good for one respawn. `checkpoints` = placed and not yet used, oldest
        // first; GD's m_checkpointArray is kept in step with it at reset time.
        int checkpointsPlaced = 0;
        std::vector<Ref<CheckpointObject>> checkpoints;
        bool respawnPending = false;
        // The checkpoint used by the last respawn. GD may still point at it
        // this attempt, so it stays alive until the next reset.
        Ref<CheckpointObject> lastRespawn;

        RunHud* hud = nullptr;

        // One physical press can reach onHotkey through two input paths in
        // the same frame; the second one is dropped.
        unsigned lastHotkeyFrame[HotkeyCount] = { ~0u, ~0u };
        bool lastHotkeyHandled[HotkeyCount] = { false, false };

        // Foresight: our own draw node. objectsByX = non-decoration objects
        // sorted by x for cheap "what's near the player" queries (built on
        // first use, shared with cat).
        cocos2d::CCDrawNode* hitboxNode = nullptr;
        std::vector<GameObject*> objectsByX;

        // Cat: seconds since the last sweep, and what this attempt's sweeps
        // removed (restored by hand on reset, so GD's own reset semantics
        // don't matter). A removed object keeps the opacity it had.
        float catTimer = 0.f;
        struct CatRemoval { Ref<GameObject> obj; unsigned char opacity; };
        std::vector<CatRemoval> catRemoved;

        // hazard-hitbox: the hazard scale every object currently in the level carries
        // (radii are multiplied in place, so a change is applied as a ratio).
        float hazardApplied = 1.f;
    };

    bool isRunLevel() {
        return m_level && AugmentManager::get().isRunFor(m_level->m_levelID.value());
    }

    // wave-hitbox only bites while a player is in wave mode; in dual either
    // player being in wave is enough for the HUD to call it active.
    bool playerInWave() {
        return (m_player1 && m_player1->m_isDart) || (m_player2 && m_player2->m_isDart);
    }

    // Normal-mode only: practice/test attempts don't count and get no augments.
    bool isRunAttempt() {
        return this->isRunLevel() && !m_isPracticeMode && !m_isTestMode;
    }

    // ---------------------------------------------------------------- setup

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        // hazard-hitbox shrinks hitboxes as objects first compute them, so the scale
        // has to be in place before PlayLayer::init creates the first object.
        // A level always starts at 0 %, so nerve contributes nothing yet.
        {
            auto& mgr = AugmentManager::get();
            bool run = level && mgr.isRunFor(level->m_levelID.value());
            hazard::setScale(run ? mgr.hazardScale(0.f) : 1.f);
            player::setWaveScale(run ? mgr.waveScale(0.f) : 1.f);
            m_fields->hazardApplied = hazard::scale();
        }

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (this->isRunLevel()) {
            m_fields->hud = RunHud::create();
            CCNode* parent = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : this;
            parent->addChild(m_fields->hud, 1000);
            this->refreshHud();
        }

        // Hotkey path 1 (see the comment above $on_mod(Loaded) at the bottom).
        // Node-scoped: removed with the layer, so nothing captures `this`.
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-slowmo"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (!down || repeat) return ListenerResult::Propagate;
                return routeHotkey(Hotkey::SlowMo, "setting");
            }
        );
        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "keybind-checkpoint"),
            [](Keybind const&, bool down, bool repeat, double) {
                if (!down || repeat) return ListenerResult::Propagate;
                return routeHotkey(Hotkey::Checkpoint, "setting");
            }
        );
        log::info("Hotkey setting listeners attached to PlayLayer (run level: {})", this->isRunLevel());
        return true;
    }

    void addObject(GameObject* object) {
        PlayLayer::addObject(object);
        if (!object || !this->isRunLevel()) return;

        if (AugmentManager::get().has(ids::Unmirror) && isMirrorPortal(object)) {
            neutralizeMirrorPortal(object);
        }

        // hazard-hitbox: rects and oriented boxes are shrunk lazily by the GameObject
        // hooks; the radius is a plain field GD reads inline, so scale it
        // here. Dirtying makes sure anything GD cached during addObject is
        // recomputed through the hooks.
        float s = m_fields->hazardApplied;
        if (s < 1.f && hazard::isTarget(object)) {
            if (object->m_objectRadius > 0.f) {
                object->m_objectRadius *= s;
            }
            object->m_isObjectRectDirty = true;
            object->m_isOrientedBoxDirty = true;
        }
    }

    // 0..1. What nerve scales both hitbox shrinks by.
    float progressFraction() {
        return this->getCurrentPercent() / 100.f;
    }

    // hazard-hitbox / wave-hitbox: publish both scales for where the player is
    // right now and bring every object already in the level in line with the
    // hazard one. Cheap when nothing changed, so it runs on every reset (also
    // covers "run ended, still playing" -> 1.0).
    void applyHitboxScales() {
        auto f = m_fields.self();
        auto& mgr = AugmentManager::get();
        bool run = this->isRunLevel();
        float progress = this->progressFraction();

        // The player's scale is a single global the hook reads per call, so it
        // can follow the nerve boost exactly.
        player::setWaveScale(run ? mgr.waveScale(progress) : 1.f);

        float want = run ? mgr.hazardScale(progress) : 1.f;
        hazard::setScale(want);
        if (std::abs(want - f->hazardApplied) < 0.001f) return;

        int hazards = 0, radii = 0;
        if (m_objects) {
            float ratio = want / f->hazardApplied;
            for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                if (!hazard::isTarget(obj)) continue;
                hazards++;
                if (obj->m_objectRadius > 0.f) {
                    obj->m_objectRadius *= ratio;
                    radii++;
                }
                obj->m_isObjectRectDirty = true;
                obj->m_isOrientedBoxDirty = true;
            }
        }
        f->hazardApplied = want;
        log::info("HazardHitbox: scale {:.2f} applied to {} hazards ({} circular)", want, hazards, radii);
    }

    // Per frame. Only pays the full re-apply once the nerve boost has moved the
    // hazard scale by kHazardReapplyStep.
    void updateHitboxScales() {
        auto& mgr = AugmentManager::get();
        bool run = this->isRunLevel();
        float progress = this->progressFraction();

        player::setWaveScale(run ? mgr.waveScale(progress) : 1.f);

        float want = run ? mgr.hazardScale(progress) : 1.f;
        if (std::abs(want - m_fields->hazardApplied) >= kHazardReapplyStep) {
            this->applyHitboxScales();
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
                float bonus = mgr.onDeath(this->getCurrentPercent());
                if (bonus > 0.f && f->hud) {
                    f->hud->notice(fmt::format("NEW BEST  +{:.0f}", bonus), { 255, 220, 90 });
                }

                // Checkpoint: any unused placement means we come back to the
                // newest one. Our own refs decide, not GD's array: GD drops a
                // checkpoint placed < 0.1 s before the death (removePlacedCheckpoint).
                if (mgr.has(ids::StartPos)) {
                    f->respawnPending = !f->checkpoints.empty();
                    log::info(
                        "Checkpoint: death with {}/{} placed, {} ready, GD array {} -> {}",
                        f->checkpointsPlaced, mgr.levelOf(ids::StartPos), f->checkpoints.size(),
                        this->gdCheckpointCount(), f->respawnPending ? "respawn" : "restart from 0"
                    );
                }
            }
        }

        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        auto& mgr = AugmentManager::get();
        auto f = m_fields.self();

        this->applyHitboxScales();

        bool fromCheckpoint = f->respawnPending && !f->checkpoints.empty() && this->isRunAttempt();
        if (f->respawnPending && !fromCheckpoint) {
            log::info("Checkpoint: respawn dropped (ready {}, runAttempt {})", f->checkpoints.size(), this->isRunAttempt());
        }
        f->respawnPending = false;
        f->deathCounted = false;
        f->noclipTimer = 0.f;
        f->lastRespawn = nullptr;
        this->catRestore();

        if (fromCheckpoint) {
            Ref<CheckpointObject> target = f->checkpoints.back();
            f->checkpoints.pop_back();

            // Borrow the practice-mode respawn path for exactly this reset.
            // GD respawns at m_currentCheckpoint / the last array entry, so
            // both are pointed at `target` first.
            this->syncCheckpointArray(target);
            m_currentCheckpoint = target;
            bool wasPractice = m_isPracticeMode;
            m_isPracticeMode = true;
            PlayLayer::resetLevel();
            m_isPracticeMode = wasPractice;
            this->consumeCheckpoint(target);

            log::info(
                "Respawned from checkpoint ({} ready, {}/{} placed this attempt)",
                f->checkpoints.size(), f->checkpointsPlaced, mgr.levelOf(ids::StartPos)
            );
            if (f->hud) f->hud->notice("CHECKPOINT", { 120, 255, 120 });
        }
        else {
            // Fresh attempt: refill everything.
            f->shieldsUsed = 0;
            f->checkpointsPlaced = 0;
            f->checkpoints.clear();
            // Same as qolmod's StartposSwitcher: a null current checkpoint
            // makes GD start from the start position.
            m_currentCheckpoint = nullptr;
            if (this->gdCheckpointCount() > 0) this->removeAllCheckpoints();
            PlayLayer::resetLevel();
        }

        this->applyHitboxScales();
        this->refreshHud();

        // A checkpoint respawn is still the same attempt: the draft waits for
        // the reset that starts over from 0. isRunning() is false during
        // PlayLayer::init (scene not yet on screen); a draft pending from a
        // previous visit likewise waits for the next reset.
        if (!fromCheckpoint && this->isRunLevel() && mgr.hasPendingDraft() && this->isRunning()) {
            mgr.clearPendingDraft();
            this->showDraft();
        }
    }

    // ---------------------------------------------------------------- checkpoint

    int gdCheckpointCount() {
        return m_checkpointArray ? static_cast<int>(m_checkpointArray->count()) : 0;
    }

    CheckpointObject* gdLastCheckpoint() {
        // getLastCheckpoint() is inline and dereferences the array unguarded.
        return m_checkpointArray ? this->getLastCheckpoint() : nullptr;
    }

    // GD respawns at the last entry of m_checkpointArray. Normally that already
    // is `target` (GD kept our placements). If GD dropped them on the
    // normal-mode death, rebuild the array from our refs so the older
    // checkpoints stay available for later respawns too.
    void syncCheckpointArray(CheckpointObject* target) {
        auto f = m_fields.self();
        int count = this->gdCheckpointCount();
        bool inSync = count == static_cast<int>(f->checkpoints.size()) + 1 && this->gdLastCheckpoint() == target;
        if (inSync) return;

        if (count > 0) this->removeAllCheckpoints();
        for (auto& cp : f->checkpoints) this->storeCheckpoint(cp);
        this->storeCheckpoint(target);
        log::info("Checkpoint: rebuilt GD array ({} -> {} entries)", count, this->gdCheckpointCount());
    }

    // A respawn uses its checkpoint up. removeCheckpoint(false) drops the
    // newest entry — it is what GD itself calls (removePlacedCheckpoint) to
    // undo a checkpoint placed right before a death — so the next death goes
    // to the previous one. The object stays alive in lastRespawn.
    void consumeCheckpoint(CheckpointObject* target) {
        auto f = m_fields.self();
        f->lastRespawn = target;
        int before = this->gdCheckpointCount();
        if (this->gdLastCheckpoint() == target) this->removeCheckpoint(false);
        log::info(
            "Checkpoint: consumed (GD array {} -> {}{})",
            before, this->gdCheckpointCount(), this->gdLastCheckpoint() == target ? ", still on top!" : ""
        );
    }

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
        if (this->isRunLevel() && mgr.hasPendingDraft()) {
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
        // Never leave the next scene frozen or slowed down, and never let the
        // hazard scale leak into the editor or the next level.
        setGameSpeed(1.f);
        hazard::setScale(1.f);
        player::setWaveScale(1.f);
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
        if (mgr.has(ids::Cat)) this->catTick(dt);

        this->updateHitboxScales();
        this->applyTimeScale();
        this->refreshHud();
    }

    // Non-decoration objects sorted by x, built once per level.
    std::vector<GameObject*>& objectsByX() {
        auto f = m_fields.self();
        if (f->objectsByX.empty() && m_objects) {
            for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                if (obj->m_objectType == GameObjectType::Decoration || obj->m_isDecoration) continue;
                f->objectsByX.push_back(obj);
            }
            std::sort(f->objectsByX.begin(), f->objectsByX.end(), [](GameObject* a, GameObject* b) {
                return a->getPositionX() < b->getPositionX();
            });
            log::info("Tracking {} non-decoration objects by x", f->objectsByX.size());
        }
        return f->objectsByX;
    }

    // ---------------------------------------------------------------- cat

    // Every catInterval() seconds of play, remove catCount() random hazards
    // that are on screen and ahead of the player. Removal is GD's own
    // GameObject::destroyObject() (m_isDisabled + m_isDisabled2 + opacity 0,
    // Geode inline source): collisionCheckObjects skips objects with either
    // flag set (xdBot's trajectory sim relies on that), and opacity 0 takes
    // the sprite with it. Undone in catRestore() on every reset.
    void catTick(float dt) {
        auto f = m_fields.self();
        if (!this->isRunAttempt() || m_isPaused || !m_player1 || m_player1->m_isDead) return;

        float interval = AugmentManager::get().catInterval();
        if (interval <= 0.f) return;
        f->catTimer += dt;
        if (f->catTimer < interval) return;
        f->catTimer = 0.f;
        this->catSweep();
    }

    void catSweep() {
        auto f = m_fields.self();
        auto& mgr = AugmentManager::get();
        int want = mgr.catCount();
        if (want <= 0 || !m_objectLayer || !m_player1) return;

        // "In view" = the object's screen position is inside the window (plus
        // a margin), computed through the real node transform so camera zoom,
        // offset and rotation all count. "Ahead" = further along in the
        // object layer's x than the player (behind them in platformer mode
        // when they are heading left).
        constexpr float kMargin = 30.f;
        auto win = CCDirector::get()->getWinSize();
        CCRect screen{ -kMargin, -kMargin, win.width + 2 * kMargin, win.height + 2 * kMargin };
        float px = m_player1->getPositionX();
        bool aheadIsLeft = m_player1->m_isGoingLeft;

        // Bound the scan by the screen's extent in object-layer x, whatever
        // the camera does.
        float lo = px, hi = px;
        for (auto corner : { CCPoint{ screen.getMinX(), screen.getMinY() }, CCPoint{ screen.getMaxX(), screen.getMinY() },
                             CCPoint{ screen.getMinX(), screen.getMaxY() }, CCPoint{ screen.getMaxX(), screen.getMaxY() } }) {
            float x = m_objectLayer->convertToNodeSpace(corner).x;
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
        if (aheadIsLeft) hi = px; else lo = px;

        std::vector<GameObject*> candidates;
        auto& objs = this->objectsByX();
        auto it = std::lower_bound(objs.begin(), objs.end(), lo, [](GameObject* o, float x) {
            return o->getPositionX() < x;
        });
        for (; it != objs.end() && (*it)->getPositionX() <= hi; ++it) {
            auto obj = *it;
            if (!hazard::isTarget(obj) || obj == m_anticheatSpike) continue;
            if (obj->m_isDisabled || obj->m_isDisabled2) continue;
            CCNode* parent = obj->getParent() ? obj->getParent() : m_objectLayer;
            CCPoint onScreen = parent->convertToWorldSpace({ obj->getPositionX(), obj->getPositionY() });
            if (!screen.containsPoint(onScreen)) continue;
            candidates.push_back(obj);
        }

        static std::mt19937 rng{ std::random_device{}() };
        std::shuffle(candidates.begin(), candidates.end(), rng);
        int removed = 0;
        for (auto obj : candidates) {
            if (removed >= want) break;
            f->catRemoved.push_back({ obj, obj->getOpacity() });
            obj->destroyObject();
            removed++;
        }
        log::info(
            "Cat: removed {}/{} of {} hazards in view at {:.1f}% (scan x {:.0f}..{:.0f}, {} removed this attempt)",
            removed, want, candidates.size(), this->getCurrentPercent(), lo, hi, f->catRemoved.size()
        );
        if (removed > 0 && f->hud) f->hud->notice(fmt::format("CAT  -{}", removed), { 255, 180, 230 });
    }

    // Put back everything the cat took this attempt. Runs before GD's own
    // reset so any per-object reset GD does still gets the last word.
    void catRestore() {
        auto f = m_fields.self();
        f->catTimer = 0.f;
        if (f->catRemoved.empty()) return;
        int stillDisabled = 0;
        for (auto& r : f->catRemoved) {
            if (!r.obj) continue;
            if (r.obj->m_isDisabled || r.obj->m_isDisabled2) stillDisabled++;
            r.obj->m_isDisabled = false;
            r.obj->m_isDisabled2 = false;
            r.obj->setOpacity(r.opacity);
        }
        log::info("Cat: restored {} hazards ({} were still disabled)", f->catRemoved.size(), stillDisabled);
        f->catRemoved.clear();
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
        auto& objs = this->objectsByX();
        auto it = std::lower_bound(objs.begin(), objs.end(), lo, [](GameObject* o, float x) {
            return o->getPositionX() < x;
        });
        for (; it != objs.end() && (*it)->getPositionX() <= hi; ++it) {
            auto obj = *it;
            if (!obj->isVisible() || obj->m_isHide) continue;
            // Disabled = removed by the cat (or toggled off by the level); GD
            // skips these in collision, so no box.
            if (obj->m_isDisabled || obj->m_isDisabled2) continue;
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

    // Every input path ends here. Returns Stop when the press was acted on.
    bool onHotkey(Hotkey which, char const* source) {
        auto f = m_fields.self();
        unsigned frame = CCDirector::sharedDirector()->getTotalFrames();
        int i = static_cast<int>(which);
        if (f->lastHotkeyFrame[i] == frame) {
            log::info("Hotkey {} via {} duplicate in frame {}, ignored", hotkeyName(which), source, frame);
            return f->lastHotkeyHandled[i] ? ListenerResult::Stop : ListenerResult::Propagate;
        }
        log::info("Hotkey {} via {} (frame {})", hotkeyName(which), source, frame);

        bool handled = which == Hotkey::SlowMo ? this->toggleSlowMo() : this->tryPlaceCheckpoint();
        f->lastHotkeyFrame[i] = frame;
        f->lastHotkeyHandled[i] = handled;
        return handled ? ListenerResult::Stop : ListenerResult::Propagate;
    }

    bool toggleSlowMo() {
        auto& mgr = AugmentManager::get();
        if (!this->isRunAttempt() || !mgr.has(ids::SlowMo)) {
            log::info("X ignored: runAttempt={} slowmoLv={}", this->isRunAttempt(), mgr.levelOf(ids::SlowMo));
            return false;
        }
        mgr.toggleSlowMo();
        log::info("Slow-mo toggled -> {}", mgr.slowMoEnabled() ? "ON" : "OFF");
        this->applyTimeScale();
        if (m_fields->hud) {
            m_fields->hud->notice(mgr.slowMoEnabled() ? "SLOW-MO ON" : "SLOW-MO OFF", { 255, 220, 120 });
        }
        return true;
    }

    // Returns true when the key was consumed (also when it only showed a
    // "none left" notice: the player has the augment, so Z is ours).
    bool tryPlaceCheckpoint() {
        auto& mgr = AugmentManager::get();
        auto f = m_fields.self();
        int lvl = mgr.levelOf(ids::StartPos);
        if (!this->isRunAttempt() || m_isPaused || lvl == 0 || !m_player1 || m_player1->m_isDead) {
            log::info("Z ignored: runAttempt={} paused={} cpLv={} dead={}",
                this->isRunAttempt(), m_isPaused, lvl, m_player1 ? m_player1->m_isDead : true);
            return false;
        }

        if (f->checkpointsPlaced >= lvl) {
            if (f->hud) f->hud->notice("NO CHECKPOINTS LEFT", { 255, 120, 120 });
            return true;
        }

        bool wasPractice = m_isPracticeMode;
        m_isPracticeMode = true;
        auto cp = this->markCheckpoint();
        m_isPracticeMode = wasPractice;

        if (cp) {
            f->checkpointsPlaced++;
            f->checkpoints.push_back(cp);
            log::info(
                "Checkpoint placed ({}/{}), {} ready, GD array {}",
                f->checkpointsPlaced, lvl, f->checkpoints.size(), this->gdCheckpointCount()
            );
            if (f->hud) f->hud->notice("CHECKPOINT PLACED", { 120, 255, 120 });
        }
        else {
            // GD refused (its own conditions, e.g. mid-dash). Say so, or the
            // player believes a checkpoint exists.
            log::info("markCheckpoint returned null at {:.1f}%", this->getCurrentPercent());
            if (f->hud) f->hud->notice("CAN'T PLACE HERE", { 255, 120, 120 });
        }
        return true;
    }

    // Debug aid: number key N grants one level of the N-th augment in the
    // table. Applies the same side effects a draft pick would.
    bool debugGrantAugment(int index) {
        auto const& defs = allAugments();
        if (index < 0 || index >= static_cast<int>(defs.size())) return false;
        if (!this->isRunLevel()) {
            log::info("Debug grant ignored: not a run level");
            return false;
        }
        auto& mgr = AugmentManager::get();
        auto const& def = defs[index];
        auto f = m_fields.self();

        if (mgr.levelOf(def.id) >= def.maxLevel) {
            log::info("Debug grant: '{}' already maxed", def.id);
            if (f->hud) f->hud->notice(fmt::format("{} MAXED", def.name), { 255, 120, 120 });
            return true;
        }
        int lvl = mgr.grant(def.id);
        log::info("Debug grant: '{}' -> level {}", def.id, lvl);

        if (def.id == ids::Unmirror) this->applyUnmirrorNow();
        if (def.id == ids::SlowMo) this->applyTimeScale();
        if (def.id == ids::HazardHitbox || def.id == ids::WaveHitbox || def.id == ids::Nerve) {
            this->applyHitboxScales();
        }
        this->refreshHud();
        if (f->hud) f->hud->notice(fmt::format("+{} Lv{} (DEBUG)", def.name, lvl), { 200, 160, 255 });
        return true;
    }

    // Debug aid: key 0 tops the gauge up so the next death drafts.
    bool debugFillGauge() {
        if (!this->isRunLevel()) {
            log::info("Debug fill ignored: not a run level");
            return false;
        }
        float added = AugmentManager::get().debugFillGauge();
        this->refreshHud();
        if (auto hud = m_fields->hud) {
            hud->notice(fmt::format("GAUGE FULL +{:.0f} (DEBUG)", added), { 200, 160, 255 });
        }
        return true;
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

        // Augment names are the Korean display names; the rest stays English
        // (the HUD is a debug readout).
        if (int lvl = mgr.levelOf(ids::Shield)) {
            if (f->noclipTimer > 0.f) {
                lines.push_back(fmt::format("{} Lv{}: NOCLIP {:.1f}s", augmentName(ids::Shield), lvl, f->noclipTimer));
            }
            else {
                lines.push_back(fmt::format("{} Lv{}: {}/{}", augmentName(ids::Shield), lvl, lvl - f->shieldsUsed, lvl));
            }
        }
        if (int lvl = mgr.levelOf(ids::SlowMo)) {
            lines.push_back(fmt::format(
                "{} Lv{}: {} ({:.0f}%)  [X]",
                augmentName(ids::SlowMo), lvl, mgr.slowMoEnabled() ? "ON" : "OFF", mgr.slowMoScale() * 100.f
            ));
        }
        if (int lvl = mgr.levelOf(ids::StartPos)) {
            lines.push_back(fmt::format(
                "{} Lv{}: {}/{} placed, {} ready  [Z]",
                augmentName(ids::StartPos), lvl, f->checkpointsPlaced, lvl, f->checkpoints.size()
            ));
        }
        if (mgr.has(ids::Foresight)) lines.push_back(augmentName(ids::Foresight));
        if (mgr.has(ids::Unmirror)) lines.push_back(augmentName(ids::Unmirror));
        // Both hitbox shrinks are read at the player's current position,
        // because nerve grows them as the level goes on.
        float progress = now / 100.f;
        if (int lvl = mgr.levelOf(ids::HazardHitbox)) {
            lines.push_back(fmt::format(
                "{} Lv{}: hazards {:.0f}%",
                augmentName(ids::HazardHitbox), lvl, mgr.hazardScale(progress) * 100.f
            ));
        }
        if (int lvl = mgr.levelOf(ids::WaveHitbox)) {
            lines.push_back(fmt::format(
                "{} Lv{}: player {:.0f}% in wave{}",
                augmentName(ids::WaveHitbox), lvl, mgr.waveScale(progress) * 100.f,
                this->playerInWave() ? "  ACTIVE" : ""
            ));
        }
        if (int lvl = mgr.levelOf(ids::Nerve)) {
            lines.push_back(fmt::format(
                "{} Lv{}: shrink x{:.2f}", augmentName(ids::Nerve), lvl, mgr.nerveBoost(progress)
            ));
        }
        if (mgr.has(ids::DraftCount)) {
            lines.push_back(fmt::format("{}: {} cards", augmentName(ids::DraftCount), mgr.draftCardCount()));
        }
        if (int lvl = mgr.levelOf(ids::Cat)) {
            lines.push_back(fmt::format(
                "{} Lv{}: {} per {:.1f}s, next in {:.1f}s, removed {}",
                augmentName(ids::Cat), lvl, mgr.catCount(), mgr.catInterval(),
                std::max(0.f, mgr.catInterval() - f->catTimer), f->catRemoved.size()
            ));
        }

        f->hud->setLines(lines);
    }

    // ---------------------------------------------------------------- draft

    // Shows one draft; when more are pending after the pick (a big new best
    // can earn several at once) the next popup opens right away and the game
    // stays paused in between.
    void showDraft() {
        auto& mgr = AugmentManager::get();
        auto choices = mgr.rollDraft(mgr.draftCardCount());
        if (choices.empty()) {
            log::info("Draft: nothing left to draft, {} pending dropped", mgr.pendingDrafts());
            while (mgr.hasPendingDraft()) mgr.clearPendingDraft();
            return;
        }
        log::info("Draft: showing {} cards, {} more pending", choices.size(), mgr.pendingDrafts());

        // Callback intentionally captures nothing: the PlayLayer may be gone
        // by the time it runs, so it only talks to the singleton and looks the
        // layer up fresh.
        auto popup = AugmentDraftPopup::create(choices, [](std::string const& id) {
            auto& mgr = AugmentManager::get();
            mgr.applyPick(id);

            auto pl = static_cast<AugPlayLayer*>(PlayLayer::get());
            if (pl) {
                if (id == ids::Unmirror) pl->applyUnmirrorNow();
                if (id == ids::HazardHitbox || id == ids::WaveHitbox || id == ids::Nerve) {
                    pl->applyHitboxScales();
                }
                pl->refreshHud();
            }

            if (pl && mgr.hasPendingDraft()) {
                mgr.clearPendingDraft();
                pl->showDraft();
                return;
            }
            mgr.resumeGameAfterDraft();
            if (mgr.cursorWasHidden()) CCEGLView::get()->showCursor(false);
        });
        if (!popup) return;

        // The director is about to be paused, which also freezes actions, so
        // the pop-in animation would never finish. Skip it.
        popup->m_noElasticity = true;
        popup->show();

        // Already paused for a previous draft in this chain: the cursor state
        // saved then is the one to restore, so don't overwrite it.
        if (mgr.isGamePausedForDraft()) return;

        // GD hides the cursor in levels; the draft needs it.
        auto view = CCEGLView::get();
        mgr.setCursorWasHidden(view->m_bShouldHideCursor);
        view->showCursor(true);

        mgr.pauseGameForDraft();
    }
};

// Hotkeys (keys come from the "keybind" settings in mod.json).
//
// Why the old listener never fired: the loader registers its own
// KeyboardInputEvent listener in queueMods(), before any mod binary is loaded,
// so at priority 0 it runs ahead of every listener a mod adds from $execute /
// $on_mod. That listener turns presses into KeybindSettingPressedEventV3 and
// returns Stop as soon as one setting's listener does. Custom Keybinds binds
// Z / X (practice checkpoints) on the PlayLayer and returns Stop whenever the
// level isn't paused, so in a level X and Z were consumed before they reached
// us (or CCKeyboardDispatcher, which is why hooking that saw nothing either).
//
// Two paths, each ahead of that:
//   1. our settings carry "priority": -5, so the loader dispatches them before
//      Custom Keybinds' for the same key; the node-scoped listener lives in
//      PlayLayer::init and dies with the layer.
//   2. a raw KeyboardInputEvent listener at priority -1, ahead of the loader's.
// Whichever runs first handles the press; the other one hits the frame guard.
namespace {
    // getSettingValue does the typeinfo cast for us; an empty result means the
    // setting couldn't be resolved, so fall back to the mod.json default.
    bool keybindMatches(char const* settingKey, enumKeyCodes fallback, Keybind const& pressed) {
        auto binds = Mod::get()->getSettingValue<std::vector<Keybind>>(settingKey);
        if (binds.empty()) return pressed == Keybind(fallback, KeyboardModifier::None);
        return std::ranges::contains(binds, pressed);
    }

    // Routes a press to the current run layer. Returns Stop only when the
    // press was acted on, so an idle key still reaches other mods.
    bool routeHotkey(Hotkey which, char const* source) {
        if (AugmentManager::get().isGamePausedForDraft()) {
            log::info("Hotkey {} via {} ignored: draft open", hotkeyName(which), source);
            return ListenerResult::Propagate;
        }
        auto pl = static_cast<AugPlayLayer*>(PlayLayer::get());
        if (!pl) {
            log::info("Hotkey {} via {} ignored: no PlayLayer", hotkeyName(which), source);
            return ListenerResult::Propagate;
        }
        return pl->onHotkey(which, source);
    }
}

$on_mod(Loaded) {
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        if (data.action != KeyboardInputData::Action::Press) return ListenerResult::Propagate;
        // Text fields get their keys untouched.
        if (CCIMEDispatcher::sharedDispatcher()->hasDelegate()) return ListenerResult::Propagate;

        Keybind pressed(data.key, data.modifiers);
        if (keybindMatches("keybind-slowmo", KEY_X, pressed)) return routeHotkey(Hotkey::SlowMo, "raw");
        if (keybindMatches("keybind-checkpoint", KEY_Z, pressed)) return routeHotkey(Hotkey::Checkpoint, "raw");

        // Debug: 1..9 grant augments (table order), Shift+1..9 the 10th
        // onwards, 0 fills the gauge. Never while a draft is up.
        bool shift = data.modifiers == KeyboardModifier::Shift;
        if (data.key >= KEY_Zero && data.key <= KEY_Nine && (data.modifiers == KeyboardModifier::None || shift)
            && AugmentManager::debugMode()
            && !AugmentManager::get().isGamePausedForDraft()) {
            if (auto pl = static_cast<AugPlayLayer*>(PlayLayer::get())) {
                bool handled = false;
                if (data.key == KEY_Zero) {
                    if (!shift) handled = pl->debugFillGauge();
                }
                else {
                    handled = pl->debugGrantAugment(data.key - KEY_One + (shift ? 9 : 0));
                }
                if (handled) return ListenerResult::Stop;
            }
        }
        return ListenerResult::Propagate;
    }, -1).leak();
    log::info("Hotkey raw listener registered (priority -1)");
}

// Slow-mo time scaling. Every scheduled update (PlayLayer::update included)
// receives the scaled dt.
class $modify(AugScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt * g_timeScale);
    }
};
