#pragma once

// One augment's in-level behaviour. LevelSession fans each event out to every
// augment in table order; all hooks default to no-ops. Levels live in the run
// (RunState), everything here is per level load.

#include "../input/Hotkeys.hpp"

#include <string>
#include <vector>

class GameObject;
class PlayerObject;

namespace augment {

class LevelSession;

class Augment {
public:
    // usually one id; HitboxScales answers for hazard-hitbox, wave-hitbox and nerve
    explicit Augment(std::vector<std::string> ids) : m_ids(std::move(ids)) {}
    virtual ~Augment() = default;

    std::vector<std::string> const& ids() const { return m_ids; }
    bool handles(std::string const& id) const;

    // --- level ---
    // Before PlayLayer::init, nothing exists yet. Publish globals objects read on creation here.
    virtual void onLevelInit(LevelSession&) {}
    // After PlayLayer::init: objects and HUD exist.
    virtual void onLevelReady(LevelSession&) {}
    // PlayLayer::addObject, run levels only.
    virtual void onObjectAdded(LevelSession&, GameObject*) {}
    // PlayLayer::onQuit, layer still alive.
    virtual void onQuit(LevelSession&) {}

    // --- attempt ---
    // In resetLevel, before GD resets (the dying attempt is still there).
    // true = this reset continues the attempt (checkpoint respawn).
    virtual bool onBeforeReset(LevelSession&) { return false; }
    // In resetLevel, after GD's reset. Per-attempt budgets refill only when !fromCheckpoint.
    virtual void onAttemptStart(LevelSession&, bool fromCheckpoint) {}
    // destroyPlayer on a run attempt, before the death counts. `object` is null
    // when GD names nothing (suicide, out of bounds). true = swallow the hit.
    virtual bool onHit(LevelSession&, PlayerObject*, GameObject* object) { return false; }
    // > 0 is asked before everyone else (berserk: a free smash, so the shield keeps its charge).
    virtual int hitPriority() const { return 0; }
    // Player 1 really died (once per attempt), before the gauge is charged.
    // true = the run comes back from it (checkpoint), so the gauge waits.
    virtual bool onDeath(LevelSession&) { return false; }
    // Checkpoint placed: snapshot per-attempt state. Only the newest one is ever respawned at.
    virtual void onCheckpointPlaced(LevelSession&) {}
    // Right after onAttemptStart(fromCheckpoint = true): restore the snapshot.
    virtual void onCheckpointRespawn(LevelSession&) {}
    // An augment just destroyed `count` hazards (cat, missile, berserk smash).
    virtual void onHazardsDestroyed(LevelSession&, int count) {}
    // postUpdate, run levels only. dt is already time-scaled.
    virtual void onFrame(LevelSession&, float dt) {}
    // PlayLayer::pauseGame.
    virtual void onPause(LevelSession&) {}

    // --- run ---
    // Draft pick or debug key, `level` = new level. Every augment hears every
    // grant, so cross effects (nerve -> hitbox scales) need no wiring.
    virtual void onGranted(LevelSession&, std::string const& id, int level) {}
    // Press (`down`) or release. true = consumed.
    virtual bool onHotkey(LevelSession&, Hotkey, bool down) { return false; }

    // --- HUD ---
    // Debug HUD row for `id`, English. Empty is fine.
    virtual std::string hudState(LevelSession&, std::string const& id) { return ""; }

private:
    std::vector<std::string> m_ids;
};

} // namespace augment
